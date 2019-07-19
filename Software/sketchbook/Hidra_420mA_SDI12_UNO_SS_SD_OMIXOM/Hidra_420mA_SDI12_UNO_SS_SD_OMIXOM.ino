//============================= LIBRERÍAS ==================================
#include <Adafruit_SleepyDog.h> // Watchdog Timer
#include <avr/sleep.h>          // Sleep modes
#include "SoftwareSerialMod.h"  // Software UART modificada
#include "SDI12Mod.h"           // SDI-12 modificada
#include <EEPROM.h>
#include <SPI.h>
#include <SD.h>

//========================== CONFIGURACIÓN PINES ================================
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

//MAPEO DE PINES
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
// A0 -> VBATpin
// A1 -> RELEpin
// A2 -> s420
// A3 -> OTTa
// A4 -> OTTb
// A5 -> SDI12pin

//============================= DIRECCIONES EEPROM =====================================
#define pID 0
#define pFREC 10
#define pMAXT 15
#define pMIN 20
#define pMAX 25
#define pM 30
#define pB 35
#define pDELAY 40
#define pTIPO 45
#define pFLAG 50
#define pNUM 55

// MAPEO EEPROM
// 0 -> Identificador
// 5 -> Frecuecia de transmisión
// 10 -> Valor Máximo a medir
// 15 -> Minimo (calibración)
// 20 -> Maximo (calibración)
// 25 -> Pendiente (m)
// 30 -> Ordenada al origen (b)
// 35 -> Precalentamiento del sensor (delaySensor)
// 40 -> Tipo de sensor
// 45 -> Banderas
// 50 -> Número de teléfono al cual enviar SMS

//============================ ESTACIONES ==============================================
// 70001 -> Cruz Alta
// 70002 -> Saladillo
// 70003 -> Inriville
// 70004 -> LH
// 70005 -> LH
// 70006 -> LH
// ELR03 -> Piedras Blancas
// ELR04 -> Alpa Corral

//=============================== VARIABLES ============================================
float valorSensor;
float valorTension;
byte valorSenial;
String fechaYhora = "";
String respuesta = "";
String comando = "";
volatile bool rtcFlag = false;

//=============================== PROGRAMA =============================================
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

  byte frecuencia = 0;
  EEPROM.get(pFREC, frecuencia);
  byte tipoSensor = 0;
  EEPROM.get(pTIPO, tipoSensor);
  if (tipoSensor == 0)
  {
    if (frecuencia <= 5)
    {
      digitalWrite(RELEpin, HIGH);
    }
  }

  attachInterrupt(digitalPinToInterrupt(SMSRCVpin), SMSint, HIGH);
  interrupts();

  if (SD.begin(SDCSpin))
  {
    SD.end();
  }


  if (reset_telit())
  {
    byte bandera = 0;
    EEPROM.get(pFLAG,bandera);
    if(bandera==1)
    {
      bandera = 0;
      EEPROM.put(pFLAG,bandera);
    }
    else
    {
      borrar_sms();
    }
    if (leer_sms())
    {
      switch (tipoSensor)
      {
        case 0: sensor_420ma();
          break;
        case 1: sensor_sdi12();
          break;
        default: break;
      }
      sensor_bateria();
      get_fecha_hora();
      get_senial();
      set_alarma();
      enviar_sms();
    }
    borrar_sms();
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

  fechaYhora = "";
  respuesta = "";
  comando = "";

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
      fechaYhora.replace("/", "-");
      fechaYhora.replace(",", "%20");
      byte tipoSensor;
      EEPROM.get(pTIPO, tipoSensor);
      switch (tipoSensor)
      {
        case 0: sensor_420ma();
          break;
        case 1: sensor_sdi12();
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
      else
      {
        guardar_datos();
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
  sleep_disable();
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
    byte bandera = 1;
    EEPROM.put(pFLAG,bandera);
    delayMicroseconds(10000);
    digitalWrite((SYSRSTpin), HIGH);
  }
  interrupts();
  return;
}
/*------------------------------- FIN INTERRUPCIONES ---------------------------------*/


/*-------------------------- FUNCIONES CONTROL MÓDULO TELIT --------------------------*/
bool comandoAT(char resp[5], byte contador)
{
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
    Serial.print(comando);
    mySerial.flush();
    mySerial.println(comando);
    while ((respuesta.indexOf(resp) == -1) && (respuesta.indexOf("ERROR") == -1))
    {
      while (mySerial.available())
      {
        c = mySerial.read();
        respuesta += c;
        delay(5);
      }
    }
    Serial.println(respuesta);
  }

  while (mySerial.available() > 0)
  {
    char basura = mySerial.read();
  }
  mySerial.flush();
  mySerial.end();
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
    Watchdog.disable();
    if (comandoAT("OK", 20))
    {
      return true;
    }
  }
  return false;
}
//---------------------------------------------------------------------------------------
bool enviar_datos(void)
{
  if (SD.begin(SDCSpin))
  {
    if (SD.exists("DATOS"))
    {
      File dataFile = SD.open("DATOS", FILE_READ);
      if (dataFile)
      {
        while (dataFile.available())
        {
          char c = dataFile.read();
          if (c == 'A')
          {
            comando = "";
            while (c != ';')
            {
              comando.concat(c);
              c = dataFile.read();
            }
            comandoAT("201", 1);
          }
        }
        dataFile.close();
        SD.remove("DATOS");
      }
    }
    SD.end();
  }

  comando = "";
  unsigned long id;
  EEPROM.get(pID, id);

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
  if (comandoAT("201", 1))
  {
    return true;
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
  byte index = fechaYhora.indexOf(",");
  String strHora = fechaYhora.substring(index + 1, index + 3);
  byte hora = strHora.toInt();
  String strMinutos = fechaYhora.substring(index + 4, index + 6);
  byte minutos = strMinutos.toInt();
  bool seteada = 0;
  byte frecuencia = 0;
  EEPROM.get(pFREC, frecuencia);

  if (frecuencia < 60)
  {
    byte j = 60 / frecuencia;
    for (byte i = 1; i < j; i++)
    {
      if (minutos < frecuencia * i)
      {
        if (frecuencia * i < 10)
        {
          strMinutos = "0";
          strMinutos += String(frecuencia * i);
        }
        else
        {
          strMinutos = String(frecuencia * i);
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
  if (hora < 24 && hora > 9)
  {
    strHora = String(hora);
  }
  if (hora <= 9)
  {
    strHora = "0";
    strHora += String(hora);
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
    byte index1 = respuesta.indexOf(":");
    byte index2 = respuesta.indexOf(",");
    respuesta = respuesta.substring(index1 + 2, index2);
    valorSenial = int(respuesta.toInt());
    return true;
  }
  return false;
}
//--------------------------------------------------------------------------------------
bool leer_sms()
{
  comando = "AT+CMGF=1";
  comandoAT("OK", 10);
  comando = "AT+CMGR=1";
  if (comandoAT("OK", 10))
  {
    if (respuesta.length() > 10)
    {
      if (respuesta.indexOf("Reset") != -1)
      {
        return true;
      }
      byte index1;
      if (index1 = respuesta.indexOf("<f="))
      {
        byte frecuencia;
        byte index2 = respuesta.indexOf(">");
        respuesta = respuesta.substring(index1 + 3, index2);
        frecuencia = int(respuesta.toInt());
        EEPROM.put(pFREC, frecuencia);
        return true;
      }
//      else if (index1 = respuesta.indexOf("<i="))
//      {
//        unsigned long id;
//        byte index2 = respuesta.indexOf(">");
//        respuesta = respuesta.substring(index1 + 3, index2);
//        id = long(respuesta.toInt());
//        EEPROM.put(pID, id);
//        return true;
//      }
//      else if (index1 = respuesta.indexOf("<d="))
//      {
//        byte delaySensor;
//        byte index2 = respuesta.indexOf(">");
//        respuesta = respuesta.substring(index1 + 3, index2);
//        delaySensor = int(respuesta.toInt());
//        EEPROM.put(pDELAY, delaySensor);
//        return true;
//      }
    }
  }
  return false;
}
//--------------------------------------------------------------------------------------
bool enviar_sms(void)
{
  unsigned long numero = 3513420474;
  EEPROM.get(pNUM, numero);
  String alarma = comando.substring(9, 17);
  comando = "AT+CMGF=1";
  if (comandoAT("OK", 1))
  {
    comando = "AT+CMGS=\"";
    comando.concat(numero);
    comando.concat("\",129");
    if (comandoAT(">", 1))
    {
      unsigned long id;
      EEPROM.get(pID, id);
      byte frec;
      EEPROM.get(pFREC, frec);
      byte delaySensor;
      EEPROM.get(pDELAY, delaySensor);
      byte tipoSensor;
      EEPROM.get(pTIPO, tipoSensor);

      comando = "";
      comando.concat(id);
      comando.concat("\r");
      comando.concat(fechaYhora);
      comando.concat("\r");
      comando.concat(alarma);
      comando.concat("\r");
      comando.concat(valorSensor);
      comando.concat(" m\r");
      comando.concat(valorTension);
      comando.concat(" V\r");
      comando.concat(valorSenial);
      comando.concat(" ASU\r");
      comando.concat(frec);
      comando.concat(" min\r");
      comando.concat(tipoSensor);
      comando.concat("\r");
      comando.concat(delaySensor);
      comando.concat(" seg");
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
  comando = "AT+CMGD=1,4";
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
  Watchdog.reset();
  float m;
  EEPROM.get(pM, m);
  float b;
  EEPROM.get(pB, b);
  byte frecuencia;
  EEPROM.get(pFREC, frecuencia);
  byte delaySensor;
  EEPROM.get(pDELAY, delaySensor);

  if (frecuencia > 5)
  {
    digitalWrite(RELEpin, HIGH);
    for (byte i = 0; i < delaySensor; i++)
    {
      Watchdog.reset();
      delay(1000);
    }
  }

  for (byte i = 0; i < 64; i++)
  {
    valorSensor += analogRead(s420);
    Watchdog.reset();
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
    valorSensor = -1.00;
  }

  if (frecuencia > 5)
  {
    digitalWrite(RELEpin, LOW);
  }
  Watchdog.reset();
  return;
}
/*-------------------------------- FIN SENSOR 4-20mA ---------------------------------*/


/*--------------------------------- SENSOR SDI-12 ------------------------------------*/
void sensor_sdi12(void)
{
  Watchdog.reset();
  SDI12 mySDI12(SDIpin);
  byte delaySensor;
  EEPROM.get(pDELAY, delaySensor);
  mySDI12.begin();

  digitalWrite(RELEpin, HIGH);
  for (byte i = 0; i < delaySensor; i++)
  {
    Watchdog.reset();
    delay(1000);
  }

  respuesta = "";
  delay(1000);
  mySDI12.sendCommand("0M!");
  delay(30);
  while (mySDI12.available())
  {
    char c = mySDI12.read();
    if ((c != '\n') && (c != '\r'))
    {
      respuesta += c;
      delay(5);
    }
  }
  mySDI12.clearBuffer();

  respuesta = "";
  delay(1000);
  mySDI12.sendCommand("0D0!");
  delay(30);
  while (mySDI12.available())
  {
    char c = mySDI12.read();
    if ((c != '\n') && (c != '\r')) {
      respuesta += c;
      delay(5);
    }
  }

  if (respuesta.length() > 1)
  {
    byte index = respuesta.indexOf('.');
    respuesta = respuesta.substring(3, index + 3);
    valorSensor = respuesta.toFloat();
  }
  mySDI12.clearBuffer();

  mySDI12.end();
  digitalWrite(RELEpin, LOW);
  Watchdog.reset();
  return;
}
/*-------------------------------- FIN SENSOR SDI-12 ---------------------------------*/


/*-------------------------------- TENSIÓN BATERÍA -----------------------------------*/
void sensor_bateria()
{
  valorTension = 0;
  for (byte i = 0; i < 32; i++)
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

  if (SD.begin(SDCSpin))
  {
    File dataFile = SD.open("DATOS", FILE_WRITE);
    if (dataFile)
    {
      comando = "";
      unsigned long id;
      EEPROM.get(pID, id);

      dataFile.print("AT#HTTPQRY=0,0,\"/weatherstation/updateweatherstation.jsp?ID=");
      dataFile.print(id);
      dataFile.print("&PASSWORD=vwrnlDhZtz&senial=");
      dataFile.print(valorSenial);
      dataFile.print("&nivel_rio=");
      dataFile.print(valorSensor);
      dataFile.print("&nivel_bat=");
      dataFile.print(valorTension);
      dataFile.print("&dateutc=");
      dataFile.print(fechaYhora);
      dataFile.print("\";\r");
    }
    dataFile.close();
    SD.end();
    return true;
  }
  return false;
}
/*--------------------------- FIN FUNCIONES TARJETA SD -------------------------------*/


