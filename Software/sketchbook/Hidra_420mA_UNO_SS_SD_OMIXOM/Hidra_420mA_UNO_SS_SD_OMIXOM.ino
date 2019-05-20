//============================= LIBRERÍAS ==================================
#include <Adafruit_SleepyDog.h> // Watchdog Timer
#include <avr/sleep.h>          // Sleep modes
#include "SoftwareSerialMod.h"  // Software UART modificada
#include "SDI12Mod.h"           // SDI-12 modificada
#include <EEPROM.h>
#include <SPI.h>
#include <SD.h>

//========================== CONFIGURACIÓN PINES ==========================
#define VBATpin A0
#define RELEpin A1
#define s420 A2
#define SDIpin A5
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
#define pTIPO 40

// MAPEO EEPROM
// 0 -> ID
// 5 -> Frecuecia de transmisión
// 10 -> Valor Máximo a medir
// 15 -> Minimo (calibración)
// 20 -> Maximo (calibración)
// 25 -> Pendiente (m)
// 30 -> Ordenada al origen (b)
// 35 -> Precalentamiento del sensor (delaySensor)
// 40 -> Tipo de sensor

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
String comando = "";

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
      byte tipoSensor;
      EEPROM.get(pTIPO, tipoSensor);
      switch (tipoSensor)
      {
        case 0: sensor_420ma();
          break;
        case 1: sensor_SDI12();
          break;
        default: break;
      }
      sensor_bateria();
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

bool comandoAT(char resp[3], byte contador)
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
    mySerial.println(comando);
    Serial.print(comando);
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
  comando = "AT";
  if (comandoAT("OK", 10))
  {
    delay(2000);
    comando = "AT+CFUN=1";
    if (comandoAT("OK", 10))
    {
      delay(2000);
      comando = "AT+CSDF=1,2";
      if (comandoAT("OK", 10))
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
  comando = "AT#GPRS=0";
  if (comandoAT("OK", 10))
  {
    comando = "AT#GPRS=1";
    if (comandoATnoWDT("OK", 10))
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
    if (SD.exists("datos"))
    {
      File dataFile;
      dataFile = SD.open("datos", FILE_READ);
      if (dataFile)
      {
        bool flag = true;
        while (dataFile.available() && flag)
        {
          char c = dataFile.read();
          if (c == 'A')
          {
            comando = "";
            while (c != '\r')
            {
              comando.concat(c);
              c = dataFile.read();
            }
            if (comandoAT("RING", 10))
            {
              if (respuesta.indexOf("201") == -1)
              {
                flag = false;
              }
            }
          }
        }
        if (flag == true)
        {
          dataFile.close();
          SD.remove("datos");
        }
      }
    }
    terminar_SD();
  }

  comando = "";
  unsigned long id;
  EEPROM.get(pID, id);
  fechaYhora.replace("/", "-");
  fechaYhora.replace(",", "%20");

  comando = "AT#HTTPQRY=0,0,\"/weatherstation/updateweatherstation.jsp?ID=";
  comando.concat(id);
  comando.concat("&PASSWORD=vwrnlDhZtz&senial=");
  comando.concat(valorSenial);
  comando.concat("&nivel_rio=");
  comando.concat(valorSensor);
  comando.concat("&nivel_bat=");
  comando.concat(valorTension);
  comando.concat("&dateutc=");
  comando.concat(fechaYhora);
  comando.concat("\"");

  if (comandoAT("RING", 10))
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
  comando = "AT#GPRS=0";
  if (comandoAT("OK", 10));
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
  comando = "AT+CFUN=0,0";
  if (comandoAT("OK", 10))
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
  comando = "AT+CCLK?";
  if (comandoAT("OK", 10))
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
  comando = "AT+CALA=\"";
  comando.concat(strHora);
  comando.concat(":");
  comando.concat(strMinutos);
  comando.concat(":00+00\",0,4,,\"0\",0");
  comandoAT("OK", 10);
  return;
}
//--------------------------------------------------------------------------------------
void get_senial(void)
{
  comando = "AT+CSQ";
  if (comandoAT("OK", 10))
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
  comando = "AT+CMGF=1";
  comandoAT("OK", 10);
  comando = "AT+CSDH=0";
  comandoAT("OK", 10);
  comando = "AT+CMGL";
  if (comandoAT("OK", 10))
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
      index1 = respuesta.indexOf("<id=");
      if (index1 != -1)
      {
        unsigned long id;
        int index2 = respuesta.indexOf(">");
        respuesta = respuesta.substring(index1 + 4, index2);
        id = long(respuesta.toInt());
        EEPROM.put(pID, id);
      }
      return true;
    }
  }
  return false;
}
//--------------------------------------------------------------------------------------
bool enviar_sms(void)
{
  comando = "AT+CMGF=1";
  if (comandoAT("OK", 1))
  {
    comando = "AT+CMGS=\"3513420474\",129";
    if (comandoAT(">", 1))
    {
      unsigned long id;
      EEPROM.get(pID, id);

      comando = "";
      comando.concat(id);
      comando.concat("\r");
      comando.concat(fechaYhora);
      comando.concat("\r");
      comando.concat(valorSensor);
      comando.concat("\r");
      comando.concat(valorTension);
      comando.concat("\r");
      comando.concat(valorSenial);
      comando.concat(char(26));

      if (comandoAT("+CMGS", 1))
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
  comando = "AT+CMGD=4";
  if (comandoAT("OK", 1))
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


/*--------------------------------- SENSOR SDI-12 ------------------------------------*/
void sensor_SDI12(void)
{
  byte delaySensor;
  EEPROM.get(pDELAY, delaySensor);
  SDI12 mySDI12(SDIpin);

  digitalWrite(RELEpin, HIGH);
  for (int i = 0; i < delaySensor; i++)
  {
    delay(1000);
  }
  mySDI12.begin();
  Watchdog.reset();
  respuesta = "";
  delay(1000);
  mySDI12.sendCommand("0M!");
  delay(30);
  while (mySDI12.available())  // build response string
  {
    char c = mySDI12.read();
    if ((c != '\n') && (c != '\r'))
    {
      respuesta += c;
      delay(5);
    }
  }
  mySDI12.clearBuffer();
  delay(1000);                 // delay between taking reading and requesting data
  respuesta = "";           // clear the response string

  // next command to request data from last measurement
  mySDI12.sendCommand("0D0!");
  delay(30);                     // wait a while for a response

  while (mySDI12.available())
  { // build string from response
    char c = mySDI12.read();
    if ((c != '\n') && (c != '\r')) {
      respuesta += c;
      delay(5);
    }
  }
  if (respuesta.length() > 1)
  {
    respuesta = respuesta.substring(3, 7);
    valorSensor = float(respuesta.toInt());
  }
  mySDI12.clearBuffer();

  mySDI12.end();
  digitalWrite(RELEpin, LOW);
  return;
}
/*--------------------------------- FIN SENSOR SDI-12 --------------------------------*/


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
      dataFile.print(comando);
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


