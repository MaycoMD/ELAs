//============================= LIBRERÍAS ==================================
#include <Adafruit_SleepyDog.h> // Watchdog Timer
#include <avr/sleep.h>          // Sleep modes
#include "SoftwareSerialMod.h"  // Software UART modificada
#include <EEPROM.h>
#include <SPI.h>
#include <SD.h>

//========================== CONFIGURACIÓN PINES ==========================
#define VBATpin A0
#define RELEpin A1
#define s420 A2
#define SMSRCVpin 2
#define RTCpin 3
#define SDCSpin 4
#define SYSRSTpin 5
#define RXpin 8
#define TXpin 9
#define RSTpin 10
#define PWRMONpin 11
#define LEDpin 13

//MAPEO DE PINES DIGITALES:
// 0 -> Hardware Rx
// 1 -> Hardware Tx
// 2 -> Rain pin (Arduino reset interrupt)
// 3 -> Telit Alarma RTC interrupt
// 4 -> SD_CS
// 5 -> Arduino hard reset
// 6 ->
// 7 -> (Telit power monitor)
// 8 -> Software Rx
// 9 -> Software Tx
// 10 -> Telit reset
// 11 -> SD_MOSI
// 12 -> SD_MISO
// 13 -> SD_CLK
// 14 -> GND
// A1 -> RELEpin
// A5 -> SDI12pin

//MAPEO DE PINES ANALÓGICOS:
// A0 -> VBATpin
// A2 -> s420
// A3 -> OTTa
// A4 -> OTTb

//============================= DIRECCIONES EEPROM =========================
#define pID 0
#define pFREC 5
#define pMAXT 10
#define pMIN 15
#define pMAX 20
#define pM 25
#define pB 30
#define pDELAY 35

// MAPEO EEPROM
// 0 -> ID
// 5 -> Frecuecia de transmisión
// 10 -> Valor Máximo a medir
// 15 -> Minimo (calibración)
// 20 -> Maximo (calibración)
// 25 -> Pendiente (m)
// 30 -> Ordenada al origen (b)
// 35 -> Precalentamiento del sensor (delaySensor)

//============================ ESTACIONES ============================

// ELR02 -> Inriville
// ELR03 -> Piedras Blancas
// ELR04 -> Alpa Corral
// ELF01 -> Cruz Alta
// ELF02 -> Saladillo

//=============================== VARIABLES ================================
#define LF 10
float valorSensor;
float valorTension;
String fechaYhora = "";
String valorSenial = "";
volatile bool rtcFlag = false;
String respuesta = "";
String datos = "";

//=============================== PROGRAMA ==================================
/*---------------------------- CONFIGURACIONES INICIALES -----------------------------*/
void setup()
{
  Watchdog.disable();
  delay(500);
  Serial.begin(9600);
  while (!Serial) {}
  Serial.setTimeout(10000); //tiempo en milisegundos

  Watchdog.enable(8000);
  Watchdog.reset();

  pinMode(VBATpin, INPUT);
  pinMode(s420, INPUT);
  pinMode(SMSRCVpin, INPUT);
  pinMode(RTCpin, INPUT);
  pinMode(PWRMONpin, INPUT);
  pinMode(RXpin, INPUT);

  pinMode(SDCSpin, OUTPUT);
  pinMode(RSTpin, OUTPUT);
  pinMode(SYSRSTpin, OUTPUT);
  pinMode(RELEpin, OUTPUT);
  pinMode(LEDpin, OUTPUT);
  pinMode(TXpin, OUTPUT);

  digitalWrite(RSTpin, LOW);
  digitalWrite(SYSRSTpin, LOW);
  digitalWrite(RELEpin, LOW);
  digitalWrite(LEDpin, LOW);

  int frecuencia;
  EEPROM.get(pFREC, frecuencia);
  if (frecuencia <= 5)
  {
    digitalWrite(RELEpin, HIGH);
  }

  attachInterrupt(digitalPinToInterrupt(SMSRCVpin), SMSint, HIGH);
  interrupts();

  iniciar_SD();
  terminar_SD();

  if (reset_telit())
  {
    if (leer_sms())
    {
      borrar_sms();
      sensor_420ma();
      sensor_bateria();
      get_fecha_hora();
      get_senial();
      enviar_sms();
    }
    get_fecha_hora();
    set_alarma();
    apagar_telit();
  }
}
/*--------------------- FIN CONFIGURACIONES INICIALES --------------------------------*/


/*---------------------------PROGRAMA PRINCIPAL --------------------------------------*/
void loop()
{
  Watchdog.reset();
  delay(5000);
  Watchdog.disable();
  Serial.flush();
  sleep_enable();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  attachInterrupt(digitalPinToInterrupt(RTCpin), RTCint, LOW);
  interrupts();
  sleep_cpu();
  sleep_disable();

  if (rtcFlag == true)
  {
    interrupts();
    delay(2000);
    if (digitalRead(RTCpin) == LOW)
    {
      Watchdog.enable(8000);
      Watchdog.reset();
      digitalWrite(LEDpin, LOW);
      reset_telit();
      get_fecha_hora();
      //sensor_420ma();
      //sensor_bateria();
      get_senial();

      if (conexion_gprs())
      {
        enviar_datos();
        desconexion_gprs();
      }

      get_fecha_hora();
      set_alarma();
      apagar_telit();
      rtcFlag = false;
    }
    else
    {
      rtcFlag = false;
    }
  }
}
/*---------------------------- FIN PROGRAMA PRINCIPAL --------------------------------*/


/*------------------------------- INTERRUPCIONES -------------------------------------*/
// Rutina de interrupción por la alarma programada en el RTC del módulo Telit
void RTCint()
{
  sleep_disable(); //fully awake now
  detachInterrupt(digitalPinToInterrupt(RTCpin));
  Watchdog.reset();
  delayMicroseconds(10000);
  if (digitalRead(RTCpin) == LOW)
  {
    rtcFlag = true;
  }
  interrupts();
  return;
}
//---------------------------------------------------------------------------------------
void SMSint()
{
  sleep_disable();
  noInterrupts();
  delayMicroseconds(10000);
  if (digitalRead(SMSRCVpin) == HIGH)
  {
    digitalWrite((SYSRSTpin), HIGH);
  }
  interrupts();
  return;
}
/*------------------------------- FIN INTERRUPCIONES ---------------------------------*/


/*-------------------------- FUNCIONES CONTROL MÓDULO TELIT --------------------------*/

bool comandoAT(String comando, char resp[3], byte contador)
{
  Watchdog.enable(8000);
  Watchdog.reset();
  SoftwareSerial mySerial = SoftwareSerial(RXpin, TXpin);
  mySerial.begin(9600);
  char c;
  respuesta = "ERROR";

  while (mySerial.available() > 0)
  {
    char basura = mySerial.read();
  }

  while ((respuesta.indexOf(resp) == -1) && (contador != 0))
  {
    delay(500);
    respuesta = "";
    contador--;
    mySerial.flush();
    mySerial.println(comando);
    Serial.print(comando);
    Watchdog.reset();
    while ((respuesta.indexOf(resp) == -1) && (respuesta.indexOf("ERROR") == -1))
    {
      do
      {
        c = LF;
        delay(20);
        if (mySerial.available())
        {
          c = mySerial.read();
          respuesta += c;
        }
      }
      while (c != LF);
    }
    Serial.println(respuesta);
  }

  while (mySerial.available() > 0)
  {
    char basura = mySerial.read();
  }
  mySerial.flush();
  mySerial.end();
  Serial.flush();
  if (respuesta.indexOf(resp) != -1)
  {
    return true;
  }
  else
  {
    return false;
  }
}
//--------------------------------------------------------------------------------------
bool comandoATnoWDT(char resp[5], byte contador)
{
  Watchdog.disable();
  SoftwareSerial mySerial = SoftwareSerial(RXpin, TXpin);
  mySerial.begin(9600);
  char c;
  respuesta = "ERROR";
  while (mySerial.available() > 0)
  {
    char basura = mySerial.read();
  }
  while ((respuesta.indexOf(resp) == -1) && (contador != 0))
  {
    delay(500);
    respuesta = "";
    contador--;
    mySerial.flush();
    mySerial.println(datos);
    Serial.print(datos);
    while ((respuesta.indexOf(resp) == -1) && (respuesta.indexOf("ERROR") == -1))
    {
      do
      {
        c = LF;
        delay(20);
        if (mySerial.available())
        {
          c = mySerial.read();
          respuesta += c;
        }
      }
      while (c != LF);
    }
    Serial.println(respuesta);
  }
  while (mySerial.available() > 0)
  {
    char basura = mySerial.read();
  }
  mySerial.flush();
  mySerial.end();
  Serial.flush();
  Watchdog.reset();
  Watchdog.enable(8000);
  Watchdog.reset();
  if (respuesta.indexOf(resp) != -1)
  {
    return true;
  }
  else
  {
    return false;
  }
}
//---------------------------------------------------------------------------------------
//Despierta al módulo GSM/GPRS mediante el pin de reset y envía el
//comando AT para verificar que está funcionando normalmente.
bool reset_telit(void)
{
  Watchdog.reset();
  digitalWrite(RSTpin, HIGH);
  delay(220);
  digitalWrite(RSTpin, LOW);
  delay(5000);
  Watchdog.reset();
  if (comandoAT("AT", "OK", 10))
  {
    delay(2000);
    if (comandoAT("AT+CFUN=1", "OK", 10))
    {
      delay(2000);
      if (comandoAT("AT+CSDF=1,2", "OK", 10))
      {
        return true;
      }
    }
  }
  return false;
}
//---------------------------------------------------------------------------------------
bool conexion_gprs(void)
{
  if (comandoAT("AT#GPRS=0", "OK", 10))
  {
    datos = "";
    datos = "AT#GPRS=1";
    if (comandoATnoWDT("OK", 25))
    {
      return true;
    }
  }
  return false;
}
//---------------------------------------------------------------------------------------
bool enviar_datos(void)
{
  if (iniciar_SD())
  {
    File dataFile;
    dataFile = SD.open("datos", FILE_READ);
    if (dataFile)
    {
      while (dataFile.available())
      {
        char c = dataFile.read();      
        if (c == 'A')
        {
          datos = "";
          while (c != '\r')
          {
            datos.concat(c);
            c = dataFile.read();
          }
          comandoATnoWDT("RING", 10);
        }
      }
      dataFile.close();
      SD.remove("datos");
    }
    terminar_SD();
  }

  datos = "";
  unsigned int ID;
  EEPROM.get(pID, ID);
  fechaYhora.replace("/", "-");
  fechaYhora.replace(",", "%20");

  datos = "AT#HTTPQRY=0,0,\"/weatherstation/updateweatherstation.jsp?ID=";
  datos.concat(ID);
  datos.concat("&PASSWORD=vwrnlDhZtz&senial=");
  datos.concat(valorSenial);
  datos.concat("&nivel_rio=");
  datos.concat(valorSensor);
  datos.concat("&nivel_bat=");
  datos.concat(valorTension);
  datos.concat("&dateutc=");
  datos.concat(fechaYhora);
  datos.concat("\"");

  if (comandoATnoWDT("RING", 10))
  {
    if (respuesta.indexOf("201") != -1)
    {
      return true;
    }
  }
  guardar_datos();
  return false;
}
//---------------------------------------------------------------------------------------
bool desconexion_gprs(void)
{
  if (comandoAT("AT#GPRS=0", "OK", 10));
  {
    return true;
  }
  return false;
}
//---------------------------------------------------------------------------------------
//transmite vía comunicación serie (UART) el comando AT adecuado para apagar
//el módulo GSM
bool apagar_telit(void)
{
  if (comandoAT("AT+CFUN=0,0", "OK", 10))
  {
    return true;
  }
  return false;
}
//---------------------------------------------------------------------------------------
//Obtiene la fecha y hora actual del RTC del TELIT y lo guarda en el string
//fechaYhora.
bool get_fecha_hora(void)
{
  if (comandoAT("AT+CCLK?", "OK", 10))
  {
    fechaYhora = respuesta.substring(10, 29);
    return true;
  }
  return false;
}
//---------------------------------------------------------------------------------------
//Función que setea la alarma del RTC automáticamente en caso de que el
//sistema se quede sin batería y se deje de posponer la alarma anterior. Esta
//rutina pregunta la hora actual del RTC del TELIT y calcula de acuerdo a la
//frecuencia de transmisión de los datos, la hora correspondiente a la siguiente
//alarma.
void set_alarma(void)
{
  String strHora = "0";
  String strMinutos = "0";
  int index = fechaYhora.indexOf(",");
  String strHora_temp = fechaYhora.substring(index + 1, index + 3);
  int hora = strHora_temp.toInt();
  String strMinutos_temp = fechaYhora.substring(index + 4, index + 6);
  int minutos = strMinutos_temp.toInt();
  bool seteada = 0;
  int frecuencia;
  EEPROM.get(pFREC, frecuencia);

  if (frecuencia < 60)
  {
    int j = 60 / frecuencia;
    int ifrec = 0;
    for (int i = 1; i < j; i++)
    {
      ifrec = frecuencia * i;
      if (minutos < ifrec)
      {
        strMinutos_temp = String(ifrec);
        if (ifrec < 10)
        {
          strMinutos += strMinutos_temp;
        }
        else
        {
          strMinutos = strMinutos_temp;
        }
        seteada = 1;
        break;
      }
    }
  }

  if (seteada == 0)
  {
    strMinutos = "00";
    hora++;
  }
  if (hora == 24)
  {
    strHora = "00";
  }
  else if (hora < 24 && hora > 9)
  {
    strHora = String(hora);
  }
  else
  {
    strHora_temp = String(hora);
    strHora += strHora_temp;
  }
  comandoAT("AT+CALA=\"" + strHora + ":" + strMinutos + ":00+00\",0,4,,\"0\",0", "OK", 10);
  return;
}
//--------------------------------------------------------------------------------------
void get_senial(void)
{
  if (comandoAT("AT+CSQ", "OK", 10))
  {
    int index1 = respuesta.indexOf(":");
    int index2 = respuesta.indexOf(",");
    valorSenial = respuesta.substring(index1 + 2, index2);
    return true;
  }
  return false;
}
//--------------------------------------------------------------------------------------
bool leer_sms()
{
  comandoAT("AT+CSDH=0", "OK", 10);
  if (comandoAT("AT+CMGL", "OK", 10))
  {
    if (respuesta.length() > 10)
    {
      int index1 = respuesta.indexOf("<frec=");
      if (index1 != -1)
      {
        int frecuencia;
        int index2 = respuesta.indexOf(">");
        respuesta = respuesta.substring(index1 + 6, index2);
        frecuencia = int(respuesta.toInt());
        EEPROM.put(pFREC, frecuencia);
      }
      return true;
    }
    else
    {
      return false;
    }
  }
}
//--------------------------------------------------------------------------------------
bool enviar_sms(void)
{
  if (comandoAT("AT+CMGF=1", "OK", 1))
  {
    if (comandoAT("AT+CMGS=\"3513420474\",129", ">", 1))
    {
      unsigned int ID;
      EEPROM.get(pID, ID);

      String datos = "";
      datos.concat(ID);
      datos.concat("\r");
      datos.concat(fechaYhora);
      datos.concat("\r");
      datos.concat(valorSensor);
      datos.concat("\r");
      datos.concat(valorTension);
      datos.concat("\r");
      datos.concat(valorSenial);
      datos.concat(char(26));

      if (comandoAT(datos, "+CMGS", 1))
      {
        return true;
      }
    }
  }
  return false;
}
//--------------------------------------------------------------------------------------
bool borrar_sms(void)
{
  if (comandoAT("AT+CMGD=4", "OK", 1))
  {
    return true;
  }
  return false;
}
/*------------------------ FIN FUNCIONES CONTROL MÓDULO TELIT ------------------------*/


/*--------------------------------- SENSOR 4-20mA ------------------------------------*/
void sensor_420ma(void)
{
  Watchdog.disable();

  float m;
  EEPROM.get(pM, m);
  float b;
  EEPROM.get(pB, b);
  int frecuencia;
  EEPROM.get(pFREC, frecuencia);
  byte delaySensor;
  EEPROM.get(pDELAY, delaySensor);

  if (frecuencia > 5)
  {
    digitalWrite(RELEpin, HIGH);
    for (int i = 0; i < delaySensor; i++)
    {
      delay(1000);
    }
  }

  for (int i = 0; i < 64; i++)
  {
    valorSensor += analogRead(s420);
    delay(1000);
  }
  valorSensor /= 64.0;
  valorSensor = (valorSensor * m) - b;
  valorSensor /= 1000.0;
  if (valorSensor >= 10.00)
  {
    valorSensor = 10.00;
  }
  else if (valorSensor < 0.01)
  {
    valorSensor = -1;
  }

  if (frecuencia > 5)
  {
    digitalWrite(RELEpin, LOW);
  }
  Watchdog.enable(8000);
  Watchdog.reset();
  return;
}
/*-------------------------------- FIN SENSOR 4-20mA ---------------------------------*/


/*-------------------------------- TENSIÓN BATERÍA -----------------------------------*/
void sensor_bateria()
{
  valorTension = 0;
  for (int i = 0; i < 32; i++)
  {
    valorTension += analogRead(VBATpin);
    delay(10);
  }
  valorTension /= 32.0;
  valorTension = ((valorTension * 5.0) / 1023.0) + 10.0 + 0.7;
  return;
}
/*------------------------------ FIN TENSIÓN BATERÍA ---------------------------------*/


/*----------------------------- FUNCIONES TARJETA SD ---------------------------------*/
bool guardar_datos()
{
  Watchdog.reset();
  bool flag = false;
  File dataFile;
  if (iniciar_SD())
  {
    dataFile = SD.open("datos", FILE_WRITE);
    if (dataFile)
    {
      dataFile.print(datos);
      dataFile.print("\r");
      flag = true;
      Serial.println("Archivo guardado");
    }
    dataFile.close();
  }
  terminar_SD();
  return flag;
}
//---------------------------------------------------------------------------------------
bool iniciar_SD()
{
  Watchdog.reset();
  if (SD.begin(SDCSpin))
  {
    return true;
  }
  else
  {
    Serial.println("error en SD");
    return false;
  }
}
//---------------------------------------------------------------------------------------
void terminar_SD()
{
  Watchdog.reset();
  SD.end();
  return;
}
/*--------------------------- FIN FUNCIONES TARJETA SD -------------------------------*/


