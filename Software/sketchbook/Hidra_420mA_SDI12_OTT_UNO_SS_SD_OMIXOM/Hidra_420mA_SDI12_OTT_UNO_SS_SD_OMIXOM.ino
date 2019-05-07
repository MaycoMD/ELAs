//============================= LIBRERÍAS ==================================
#include <Adafruit_SleepyDog.h> // Watchdog Timer
#include <avr/sleep.h>          // Sleep modes
#include "SoftwareSerialMod.h"  // Software UART modificada
//#include "SDI12Mod.h"           // SDI-12 modificada
#include <EEPROM.h>
#include <SPI.h>
#include <SD.h>

//========================== CONFIGURACIÓN PINES ==========================
#define VBATpin A0
#define RELEpin A1
#define s420 A2
//#define OTTb A3
//#define OTTa A4
//#define SDIpin A5
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

// MAPEO EEPROM
// 0 -> ID
// 5 -> Frecuecia de transmisión
// 10 -> Valor Máximo a medir
// 15 -> Minimo (calibración)
// 20 -> Maximo (calibración)
// 25 -> Pendiente (m)
// 30 -> Ordenada al origen (b)


//======================== CONFIGURACIONES SENSORES ========================
//#define tipoSensor 0          // tipo de sensor a utilizar (0~3)
#define delaySensor 5        // pre-calentamiento del sensor (en segundos)

// SENSORES:
// 0 -> 4-20 mA
// 1 -> SDI-12
// 2 -> Thalimedes
// 3 -> Pulsos (No implementado)

// ESTACIONES
// ELR02 -> Inriville
// ELR03 -> Piedras Blancas
// ELR04 -> Alpa Corral
// ELF01 -> Cruz Alta
// ELF02 -> Saladillo

//=============================== VARIABLES ================================
#define LF 10
float valorSensor;
float valorTension = 0.0;
String fechaYhora;
String valorSenial;
volatile bool rtcFlag = false;
String respuesta = "";
File dataFile;

//int cuadrante_inicial;  // definiciones de variables
//int cuadrante_final;    // utilizadas con el sensor
//int x_inicial = 0;      // limnimétrico a flotador
//int y_inicial = 0;      // OTT

SoftwareSerial mySerial = SoftwareSerial(RXpin, TXpin);
//SDI12 mySDI12(SDIpin);

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
    //if (tipoSensor != 2)
    //{
    digitalWrite(RELEpin, HIGH);
    //}
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
      //      switch (tipoSensor)
      //      {
      //        case 0: sensor_420ma();
      //          break;
      //        case 1: sensor_SDI12();
      //          break;
      //        default: break;
      //      }
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

  //  if (tipoSensor == 2)
  //  {
  //    attachInterrupt(digitalPinToInterrupt(RTCpin), RTCint, LOW);
  //    interrupts();
  //  }
}
/*--------------------- FIN CONFIGURACIONES INICIALES --------------------------------*/


/*---------------------------PROGRAMA PRINCIPAL --------------------------------------*/
void loop()
{
  Watchdog.reset();
  delay(5000);
  //  if (tipoSensor != 2)
  //  {
  Watchdog.disable();
  Serial.flush();
  sleep_enable();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  attachInterrupt(digitalPinToInterrupt(RTCpin), RTCint, LOW);
  interrupts();
  sleep_cpu();
  sleep_disable();
  //  }
  //  if (tipoSensor == 2)
  //  {
  //    sensor_OTT();
  //  }

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
      //      switch (tipoSensor)
      //      {
      //        case 0: sensor_420ma();
      //          break;
      //        case 1: sensor_SDI12();
      //          break;
      //        default: break;
      //      }
      sensor_420ma();
      sensor_bateria();
      get_senial();
      get_fecha_hora();
      guardar_datos_l();
      bool datosFlag = false;
      //      if (guardar_datos_d())
      //      {
      //        datosFlag = true;
      //      }
      if (conexion_gprs())
      {
        if (datosFlag)
        {
          enviar_datos_sd();
        }
        else
        {
          enviar_datos();
        }
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
    //    if (tipoSensor == 2)
    //    {
    //      attachInterrupt(digitalPinToInterrupt(RTCpin), RTCint, LOW);
    //      interrupts();
    //    }
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
bool comandoATnoWDT(String comando, char resp[5], byte contador)
{
  Watchdog.disable();
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
  if (comandoAT("AT", "OK", 10))
  {
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
    if (comandoATnoWDT("AT#GPRS=1", "OK", 10))
    {
      return true;
    }
  }
  return false;
}
//---------------------------------------------------------------------------------------
bool enviar_datos(void)
{
  unsigned int ID;
  EEPROM.get(pID, ID);

  fechaYhora.replace("/", "-");
  fechaYhora.replace(",", "\20");

  String datos = "AT#HTTPQRY=0,0,\"/weatherstation/updateweatherstation.jsp?ID=";
  datos.concat(ID);
  datos.concat("&PASSWORD=vwrnlDhZtz&senial=");
  datos.concat(valorSenial);
  datos.concat("&nivel_rio=");
  datos.concat(valorSensor);
  datos.concat("&nivel_bat=");
  datos.concat(valorTension);
  datos.concat("&dateutc=");
  datos.concat(fechaYhora.substring(0, 19));
  datos.concat("\"");

  if (comandoAT(datos, "RING", 10))
  {
    return true;
  }
  return false;
}
//---------------------------------------------------------------------------------------
bool enviar_datos_sd(void)
{
  if (iniciar_SD())
  {
    if (SD.exists("datosD"))
    {
      dataFile = SD.open("datosD", FILE_READ);
      char c;
      if (dataFile)
      {
        unsigned long largo = dataFile.size();
        unsigned int times = (largo / 1500);
        unsigned int rest = largo % 1500;
        unsigned int bytes2send = 0;
        bool firstChar = true;
        if (rest > 0)
        {
          times++;
        }
        for (int i = 0; i < (times); i++)
        {
          if (i < (times - 1))
          {
            bytes2send = 1500;
          }
          else
          {
            bytes2send = largo;
          }

          bool eof = (i == (times - 1));
          String command = ("AT#FTPAPPEXT=");
          command.concat(String(bytes2send));
          command.concat(",");
          command.concat(eof);
          while (mySerial.available() > 0)
          {
            char basura = mySerial.read();
          }
          if (comandoAT(command, ">", 1))
          {
            mySerial.begin(9600);
            while ((dataFile.available()) && ((bytes2send != 0) && (largo != 0)))
            {
              largo--;
              bytes2send--;
              c = dataFile.read();
              if (firstChar)
              {
                firstChar = false;
                if (c == '2')
                {
                  mySerial.print(c);
                  Serial.print(c);
                }
              }
              else
              {
                mySerial.print(c);
                Serial.print(c);
              }
            }
          }
          else
          {
            dataFile.close();
            return false;
          }
          // verifica la respuesta del Telit al terminar de escribir en el archivo remoto
          respuesta = "";
          while ((respuesta.indexOf("OK") == -1) && (respuesta.indexOf("ERROR") == -1))
          {
            do
            {
              c = LF;
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
        mySerial.end();
      }
      dataFile.close();
      SD.remove("datosD");
      return true;
    }
    terminar_SD();
  }
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
void get_fecha_hora(void)
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
//void sensor_SDI12(void)
//{
//  digitalWrite(RELEpin, HIGH);
//  delay(delaySensor * 1000);
//  mySDI12.begin();
//  Watchdog.reset();
//  respuesta = "";
//  delay(1000);
//  mySDI12.sendCommand("0M!");
//  delay(30);
//  while (mySDI12.available())  // build response string
//  {
//    char c = mySDI12.read();
//    if ((c != '\n') && (c != '\r'))
//    {
//      respuesta += c;
//      delay(5);
//    }
//  }
//  mySDI12.clearBuffer();
//  delay(1000);                 // delay between taking reading and requesting data
//  respuesta = "";           // clear the response string
//
//  // next command to request data from last measurement
//  mySDI12.sendCommand("0D0!");
//  delay(30);                     // wait a while for a response
//
//  while (mySDI12.available())
//  { // build string from response
//    char c = mySDI12.read();
//    if ((c != '\n') && (c != '\r')) {
//      respuesta += c;
//      delay(5);
//    }
//  }
//  if (respuesta.length() > 1)
//  {
//    respuesta = respuesta.substring(3, 7);
//    valorSensor = respuesta;
//  }
//  mySDI12.clearBuffer();
//
//  mySDI12.end();
//  digitalWrite(RELEpin, LOW);
//  return;
//}
/*--------------------------------- FIN SENSOR SDI-12 --------------------------------*/


/*-------------------------- FUNCIONES SENSOR LIMNIMÉTRICO OTT -----------------------*/
//void sensor_OTT(void)
//{
//  Watchdog.reset();
//  int x_final = 0;
//  int y_final = 0;
//  const int k = 32;
//  const int d = 10;
//  float valorSensorTemp = (valorSensor.toInt() * 1000.00);
//  const int delta_cuadrante = 55;
//
//  for (int i = 0; i < k; i++)
//  {
//    x_final += analogRead(A3);
//    y_final += analogRead(A4);
//  }
//  x_final /= k;
//  y_final /= k;
//
//  if (((x_final - x_inicial) < d) && ((x_final - x_inicial) > -d))
//  {
//    x_final = x_inicial;
//  }
//  if (((y_final - y_inicial) < d) && ((y_final - y_inicial) > -d))
//  {
//    y_final = y_inicial;
//  }
//
//  if (((x_inicial - x_final) > 0) && ((y_inicial - y_final) > 0))
//  {
//    cuadrante_final = 100;
//  }
//  else if (((x_inicial - x_final) < 0) && ((y_inicial - y_final) > 0))
//  {
//    cuadrante_final = 200;
//  }
//  else if (((x_inicial - x_final) < 0) && ((y_inicial - y_final) < 0))
//  {
//    cuadrante_final = 300;
//  }
//  else if (((x_inicial - x_final) > 0) && ((y_inicial - y_final) < -0))
//  {
//    cuadrante_final = 400;
//  }
//  else
//  {
//    cuadrante_final = cuadrante_final;
//  }
//
//  switch (cuadrante_inicial)
//  {
//    case 100: if (cuadrante_final == 200)
//      {
//        valorSensorTemp += delta_cuadrante;
//      }
//      else if (cuadrante_final == 400)
//      {
//        valorSensorTemp -= delta_cuadrante;
//      }
//      break;
//    case 200: if (cuadrante_final == 300)
//      {
//        valorSensorTemp += delta_cuadrante;
//      }
//      else if (cuadrante_final == 100)
//      {
//        valorSensorTemp -= delta_cuadrante;
//      }
//      break;
//    case 300: if (cuadrante_final == 400)
//      {
//        valorSensorTemp += delta_cuadrante;
//      }
//      else if (cuadrante_final == 200)
//      {
//        valorSensorTemp -= delta_cuadrante;
//      }
//      break;
//    case 400: if (cuadrante_final == 100)
//      {
//        valorSensorTemp += delta_cuadrante;
//      }
//      else if (cuadrante_final == 300)
//      {
//        valorSensorTemp -= delta_cuadrante;
//      }
//      break;
//  }
//
//  valorSensor = String(valorSensorTemp / 1000.00);
//  x_inicial = x_final;
//  y_inicial = y_final;
//  cuadrante_inicial = cuadrante_final;
//
//  Serial.print(x_final);
//  Serial.print("\t");
//  Serial.print(y_final);
//  Serial.print("\t");
//  Serial.print(cuadrante_final);
//  Serial.print("\t");
//  Serial.print(valorSensorTemp);
//  Serial.print("\t");
//  Serial.println(valorSensor);
//
//  return;
//}
/*------------------------ FIN FUNCIONES SENSOR LIMNIMÉTRICO OTT ---------------------*/


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
bool guardar_datos_l()
{
  Watchdog.reset();
  bool flag = false;
  if (iniciar_SD())
  {
    dataFile = SD.open("datosL", FILE_WRITE);
    if (dataFile)
    {
      dataFile.print(fechaYhora.substring(0, 16));
      dataFile.print(",");
      dataFile.print(valorSensor);
      dataFile.print(",");
      dataFile.print(String(valorTension));
      dataFile.print(",");
      dataFile.print(valorSenial);
      dataFile.print("\r\n");
      flag = true;
    }
    else
    {
      //Serial.println("error abriendo datosL");
    }
    dataFile.close();
  }
  terminar_SD();
  return flag;
}
//--------------------------------------------------------------------------------------
bool guardar_datos_d()
{
  Watchdog.reset();
  bool flag = false;
  if (iniciar_SD())
  {
    dataFile = SD.open("datosD", FILE_WRITE);
    if (dataFile)
    {
      dataFile.print(fechaYhora.substring(0, 16));
      dataFile.print(",");
      dataFile.print(valorSensor);
      dataFile.print(",");
      dataFile.print(String(valorTension));
      dataFile.print(",");
      dataFile.print(valorSenial);
      dataFile.print("\r\n");
      flag = true;
    }
    else
    {
      //Serial.println("error abriendo datosD");
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
    dataFile = SD.open("datosL", FILE_WRITE);
    if (!dataFile)
    {
      Serial.println("error en datosL");
    }
    dataFile.close();
    dataFile = SD.open("datosD", FILE_WRITE);
    if (!dataFile)
    {
      Serial.println("error en datosD");
    }
    dataFile.close();
  }
  else
  {
    Serial.println("error en SD");
  }
  return;
}
//---------------------------------------------------------------------------------------
bool terminar_SD()
{
  Watchdog.reset();
  SD.end();
  return;
}
/*--------------------------- FIN FUNCIONES TARJETA SD -------------------------------*/


