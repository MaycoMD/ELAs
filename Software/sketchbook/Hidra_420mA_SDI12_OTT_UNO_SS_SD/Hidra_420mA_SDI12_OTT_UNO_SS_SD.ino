#include <Adafruit_SleepyDog.h>
#include <avr/sleep.h>
#include "SoftwareSerialMod.h"
#include "SDI12Mod.h"
#include <SPI.h>
#include <SD.h>

#define VBATpin A0
#define RELEpin A1
#define s420 A2
#define OTTb A3
#define OTTa A4
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


#define tipoSensor 0                  // tipo de sensor a utilizar (0~3)
#define delaySensor 90                 // pre-calentamiento del sensor (en segundos)
#define frecuencia 10                 // frecuencia de transmisión de los datos (en minutos)

// SENSORES:
// 0 -> 4-20 mA
// 1 -> SDI-12
// 2 -> OTT
// 3 -> Pulsos (No implementado)

// ESTACIONES
// ELR02 -> Inriville
// ELR03 -> Piedras Blancas
// ELR04 -> Alpa Corral
// ELF01 -> Cruz Alta
// ELF02 -> Saladillo

#define LF 10
#define CR 13

const float valorMax = 10000.0;             // máximo valor a medir por el sensor (en milimetros)
const String ID = "ELA00";                  // Identificador de la estación
String valorSensor;
float valorTension = 0;
String fechaYhora;
String valorSenial;
volatile bool rtcFlag = false;
volatile bool timerFlag = false;
volatile bool smsFlag = false;
volatile bool ledFlag = false;
String respuesta = "";
File dataFile;

unsigned int cuadrante_inicial;  // definiciones de variables
unsigned int cuadrante_final;    // utilizadas con el sensor
unsigned int x_inicial = 0;      // limnimétrico a flotador
unsigned int y_inicial = 0;      // OTT
unsigned int x_final;
unsigned int y_final;

SoftwareSerial mySerial = SoftwareSerial(RXpin, TXpin);
SDI12 mySDI12(SDIpin);

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

  attachInterrupt(digitalPinToInterrupt(SMSRCVpin), SMSint, HIGH);
  interrupts();

  iniciar_SD();
  terminar_SD();

  switch (tipoSensor)
  {
    case 0: sensor_420ma();
      break;
    case 1: sensor_SDI12();
      break;
    default: break;
  }
  sensor_bateria();
  if (reset_telit())
  {
    get_fecha_hora();
    set_alarma();
    get_senial();
    leer_sms();
    if (smsFlag)
    {
      enviar_sms();
      smsFlag = false;
    }
    borrar_sms();
    apagar_telit();
  }

  if (tipoSensor == 2)
  {
    attachInterrupt(digitalPinToInterrupt(RTCpin), RTCint, LOW);
    interrupts();
  }
}
/*--------------------- FIN CONFIGURACIONES INICIALES --------------------------------*/


/*---------------------------PROGRAMA PRINCIPAL --------------------------------------*/
void loop()
{
  Watchdog.reset();
  delay(5000);
  if (tipoSensor != 2)
  {
    Watchdog.disable();
    Serial.flush();
    sleep_enable();
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    attachInterrupt(digitalPinToInterrupt(RTCpin), RTCint, LOW);
    interrupts();
    sleep_cpu();
    sleep_disable();
  }
  if (tipoSensor == 2)
  {
    sensor_OTT();
  }

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
      set_alarma();
      sensor_bateria();
      switch (tipoSensor)
      {
        case 0: sensor_420ma();
          break;
        case 1: sensor_SDI12();
          break;
        default: break;
      }
      get_senial();
      guardar_datos_l();
      bool datosFlag = false;
      if (guardar_datos_d())
      {
        datosFlag = true;
      }
      if (conexion_gprs())
      {
        if (conexion_ftp())
        {
          if (datosFlag)
          {
            enviar_datos_sd();
          }
          else
          {
            enviar_datos();
          }
          desconexion_ftp();
        }
        desconexion_gprs();
      }
      apagar_telit();
      rtcFlag = false;
    }
    else
    {
      rtcFlag = false;
    }
    if (tipoSensor == 2)
    {
      attachInterrupt(digitalPinToInterrupt(RTCpin), RTCint, LOW);
      interrupts();
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

void blinkLED(void)
{
  if (ledFlag == LOW)
  {
    ledFlag = HIGH;
  }
  else
  {
    ledFlag = LOW;
  }
  digitalWrite(LEDpin, ledFlag);
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
    delay(1000);
    respuesta = "";
    contador--;
    mySerial.flush();
    //mySerial.print('\b');
    mySerial.println(comando);
    Serial.print(comando);
    Watchdog.reset();
    timerFlag = false;
    while ((respuesta.indexOf(resp) == -1) && (respuesta.indexOf("ERROR") == -1))
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
    Watchdog.disable();
    if (comandoAT("AT#GPRS=1", "OK", 10))
    {
      Watchdog.reset();
      Watchdog.enable(8000);
      Watchdog.reset();
      return true;
    }
  }
  return false;
}
//---------------------------------------------------------------------------------------
bool conexion_ftp()
{
  if (comandoAT("AT#FTPTO=5000", "OK", 10))
  {
    if (comandoAT("AT#FTPOPEN=\"200.16.30.250\",\"estaciones\",\"es2016$..\",1", "OK", 10)) // 0->Active Mode, 1->Passive Mode
    {
      if (comandoAT("AT#FTPTYPE=1", "OK", 10)) // 0->Binary, 1->ASCII
      {
        if (comandoAT("AT#FTPAPP=\"" + ID + "/datos\",1", "OK", 10))
        {
          return true;
        }
      }
    }
  }
  return false;
}
//---------------------------------------------------------------------------------------
bool enviar_datos(void)
{
  String datos;
  datos.concat(fechaYhora.substring(0, 16));
  datos.concat(",");
  //  if (valorSensor < 0)
  {
    datos.concat("ERROR");
  }
  //  else
  {
    datos.concat(valorSensor);
  }
  datos.concat(",");
  datos.concat(valorTension);
  datos.concat(",");
  datos.concat(valorSenial);
  datos.concat("\r");
  String largo = String(datos.length());

  if (comandoAT("AT#FTPAPPEXT=" + largo + ",1", ">", 1))
  {
    if (comandoAT(datos, "OK", 1))
    {
      return true;
    }
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
bool desconexion_ftp()
{
  if (comandoAT("AT#FTPCLOSE", "OK", 10))
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
//Establece el valor de fecha y hora ingresado por el usuario en el menú USB
//y setea dicho valor en el RTC del módulo TELIT
void set_fecha_hora(void)
{
  delay(1000);
  respuesta = "";
  Serial.print("Configurar fecha/hora? <s/n>: ");
  respuesta = Serial.readStringUntil(CR);
  if (respuesta.indexOf("s") != -1)
  {
    fechaYhora = "";
    Serial.println();
    Serial.setTimeout(20000);
    Serial.print("Introduzca la fecha y hora en formato <aaaa/mm/dd,hh:mm:ss>: ");
    fechaYhora = Serial.readStringUntil(CR);
    Serial.println();
    comandoAT("AT+CCLK=\"" + fechaYhora + "+00\"", "OK", 10);
  }
  return;
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
  delay(1000);
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
    valorSenial = respuesta.substring(index1 + 2, index2 + 2);
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
      smsFlag = true;
    }
    else
    {
      smsFlag = false;
    }
  }
  return;
}
//--------------------------------------------------------------------------------------
bool enviar_sms(void)
{
  if (comandoAT("AT+CMGF=1", "OK", 1))
  {
    if (comandoAT("AT+CMGS=\"3513420474\",129", ">", 1))
    {
      String datos = ID;
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
  digitalWrite(RELEpin, HIGH);
  delay(delaySensor * 1000);
  float valorSensorTemp = 0;
  float m = valorMax / 820.0;
  float b = (203.0 * valorMax) / 820.0;
  valorSensor = "";
  for (int i = 0; i < 64; i++)
  {
    valorSensorTemp += analogRead(s420);
    delay(1000);
  }
  valorSensorTemp /= 64.0;
  valorSensorTemp = (valorSensorTemp * m) - b;
  if (valorSensorTemp >= 10000.0)
  {
    valorSensor = String(valorSensorTemp).substring(0, 2);
    valorSensor.concat(".");
    valorSensor.concat(String(valorSensorTemp).substring(3, 5));
  }
  else if (valorSensorTemp < 10000.0 && valorSensorTemp >= 1000.0)
  {
    valorSensor = String(valorSensorTemp).substring(0, 1);
    valorSensor.concat(".");
    valorSensor.concat(String(valorSensorTemp).substring(1, 3));
  }
  else if (valorSensorTemp < 1000.0 && valorSensorTemp >= 0.0)
  {
    valorSensor.concat("0.");
    valorSensor.concat(String(valorSensorTemp).substring(0, 2));
  }
  else if (valorSensorTemp < 0.0)
  {
    valorSensor = "ERROR";
  }

  digitalWrite(RELEpin, LOW);
  Watchdog.enable(8000);
  Watchdog.reset();
  return;
}
/*-------------------------------- FIN SENSOR 4-20mA ---------------------------------*/


/*--------------------------------- SENSOR SDI-12 ------------------------------------*/
void sensor_SDI12(void)
{
  digitalWrite(RELEpin, HIGH);
  delay(delaySensor * 1000);
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
    valorSensor = respuesta;
  }
  mySDI12.clearBuffer();

  mySDI12.end();
  digitalWrite(RELEpin, LOW);
  return;
}
/*--------------------------------- FIN SENSOR SDI-12 --------------------------------*/


/*-------------------------- FUNCIONES SENSOR LIMNIMÉTRICO OTT -----------------------*/
void sensor_OTT(void)
{
  //  Watchdog.reset();
  //  //Timer1.restart();
  //  x_final = 0;
  //  y_final = 0;
  //  int k = 32;
  //  int d = 5;
  //
  //  for (int i = 0; i < k; i++)
  //  {
  //    x_final += analogRead(OTTa);
  //    y_final += analogRead(OTTb);
  //  }
  //  x_final = x_final / k;
  //  y_final = y_final / k;
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
  //    cuadrante_final = 1;
  //  }
  //  else if (((x_inicial - x_final) < 0) && ((y_inicial - y_final) > 0))
  //  {
  //    cuadrante_final = 2;
  //  }
  //  else if (((x_inicial - x_final) < 0) && ((y_inicial - y_final) < 0))
  //  {
  //    cuadrante_final = 3;
  //  }
  //  else if (((x_inicial - x_final) > 0) && ((y_inicial - y_final) < -0))
  //  {
  //    cuadrante_final = 4;
  //  }
  //  else
  //  {
  //    cuadrante_final = cuadrante_final;
  //  }
  //
  //  switch (cuadrante_inicial)
  //  {
  //    case 1: if (cuadrante_final == 2)
  //      {
  //        valorSensor += 55;
  //      }
  //      else if (cuadrante_final == 4)
  //      {
  //        valorSensor -= 55;
  //      }
  //      break;
  //    case 2: if (cuadrante_final == 3)
  //      {
  //        valorSensor += 55;
  //      }
  //      else if (cuadrante_final == 1)
  //      {
  //        valorSensor -= 55;
  //      }
  //      break;
  //    case 3: if (cuadrante_final == 4)
  //      {
  //        valorSensor += 55;
  //      }
  //      else if (cuadrante_final == 2)
  //      {
  //        valorSensor -= 55;
  //      }
  //      break;
  //    case 4: if (cuadrante_final == 1)
  //      {
  //        valorSensor += 55;
  //      }
  //      else if (cuadrante_final == 3)
  //      {
  //        valorSensor -= 55;
  //      }
  //      break;
  //  }
  //
  //  x_inicial = x_final;
  //  y_inicial = y_final;
  //  cuadrante_inicial = cuadrante_final;
  //
  //  //Serial.print(x_final);
  //  //Serial.print("\t");
  //  //Serial.print(y_final);
  //  //Serial.print("\t");
  //  //Serial.print(cuadrante_final);
  //  //Serial.print("\t");
  //  //Serial.println(valorSensor);
  return;
}
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


