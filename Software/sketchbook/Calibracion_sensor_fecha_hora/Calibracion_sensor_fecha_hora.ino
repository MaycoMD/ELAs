#include "SoftwareSerialMod.h"
#include "SDI12Mod.h"
#include <SPI.h>

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

//MAPEO DE PINES:
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

#define tipoSensor 0                  // tipo de sensor a utilizar (0~3)
#define delaySensor 5                 // pre-calentamiento del sensor (en segundos)
#define frecuencia 5                 // frecuencia de transmisión de los datos (en minutos)

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
int valorSensor;
float valorTension = 0;
String fechaYhora;
String valorSenial;
volatile bool interrupcion = false;
volatile bool timerInt = false;
String respuesta = "";

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
  delay(500);
  Serial.begin(9600);
  while (!Serial) {}
  Serial.setTimeout(10000); //tiempo en milisegundos
  Serial.println("Iniciando sistema");
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

  digitalWrite(RELEpin, HIGH);

  calibrar_sensor();

  switch (tipoSensor)
  {
    case 0: sensor_420ma();
      break;
    case 1: sensor_SDI12();
      break;
    default: break;
  }

  Serial.println("Reiniciando Telit");
  if (reset_telit())
  {
    get_fecha_hora();
    set_fecha_hora();
    Serial.println();
    apagar_telit();
  }

}
/*--------------------- FIN CONFIGURACIONES INICIALES --------------------------------*/

/*---------------------------PROGRAMA PRINCIPAL --------------------------------------*/
void loop()
{

}
/*---------------------------- FIN PROGRAMA PRINCIPAL --------------------------------*/


/*-------------------------- FUNCIONES CONTROL MÓDULO TELIT --------------------------*/

bool comandoAT(String comando, char resp[3], byte contador)
{
  mySerial.begin(9600);
  char c;
  respuesta = "ERROR";

  while (mySerial.available() > 0)
  {
    char basura = mySerial.read();
  }

  while ((respuesta.indexOf(resp) == -1) && (contador != 0))
  {
    delay(2000);
    respuesta = "";
    contador--;
    mySerial.flush();
    //mySerial.print('\b');
    mySerial.println(comando);
    Serial.print(comando);
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
//-----------------------------------------------------------------------------
//Despierta al módulo GSM/GPRS mediante el pin de reset y envía el
//comando AT para verificar que está funcionando normalmente.
bool reset_telit(void)
{
  digitalWrite(RSTpin, HIGH);
  delay(220);
  digitalWrite(RSTpin, LOW);
  delay(5000);
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
//-----------------------------------------------------------------------------
//transmite vía comunicación serie (UART) el comando AT adecuado para apagar
//el módulo GSM
bool apagar_telit(void)
{
  if (comandoAT("AT#SYSHALT", "OK", 10))
  {
    return true;
  }
  return false;
}
//-----------------------------------------------------------------------------
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
//-----------------------------------------------------------------------------
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
/*------------------------ FIN FUNCIONES CONTROL MÓDULO TELIT ------------------------*/

/*--------------------------------- SENSOR 4-20mA ------------------------------------*/
void sensor_420ma(void)
{
  float valorSensorTemp = 0;
  int valorSensorInt = 0;
  float m = valorMax / 820.0;
  float b = (203.0 * valorMax) / 820.0;
  for (byte i = 0; i < 32; i++)
  {
    valorSensorTemp += analogRead(s420);
    delay(10);
  }
  valorSensorTemp /= 32.0;
  valorSensorTemp = (valorSensorTemp * m) - b;
  valorSensor = valorSensorTemp;
  return;
}

void calibrar_sensor(void)
{
  delay(1000);
  respuesta = "";
  Serial.print("Calibrar sensor? <s/n>: ");
  respuesta = Serial.readStringUntil(CR);
  Serial.println();
  if (respuesta.indexOf("s") != -1)
  {
    respuesta = "";
    Serial.println("Modo calibracion. Presione 'e' para salir");
    delay(delaySensor * 1000);
    while (respuesta.indexOf('e') == -1)
    {
      delay(500);
      switch (tipoSensor)
      {
        case 0: sensor_420ma();
          break;
        case 1: sensor_SDI12();
          break;
        case 2: sensor_OTT();
          break;
        default: break;
      }
      Serial.println(valorSensor);
      if (Serial.available() > 0)
      {
        respuesta = Serial.readStringUntil(CR);
      }
    }
  }

  return;
}
/*-------------------------------- FIN SENSOR 4-20mA ---------------------------------*/


/*--------------------------------- SENSOR SDI-12 ------------------------------------*/
void sensor_SDI12(void)
{
  mySDI12.begin();
  String sdiResponse = "";
  mySDI12.sendCommand("0M!");
  delay(30);
  while (mySDI12.available())  // build response string
  {
    char c = mySDI12.read();
    if ((c != '\n') && (c != '\r'))
    {
      sdiResponse += c;
      delay(5);
    }
  }
  mySDI12.clearBuffer();
  delay(1000);                 // delay between taking reading and requesting data
  sdiResponse = "";           // clear the response string

  // next command to request data from last measurement
  mySDI12.sendCommand("0D0!");
  delay(30);                     // wait a while for a response

  while (mySDI12.available())
  { // build string from response
    char c = mySDI12.read();
    if ((c != '\n') && (c != '\r')) {
      sdiResponse += c;
      delay(5);
    }
  }
  if (sdiResponse.length() > 1)
  {
    sdiResponse = sdiResponse.substring(3, 7);
    valorSensor = sdiResponse.toInt();
  }
  mySDI12.clearBuffer();
  mySDI12.end();
  return;
}
/*--------------------------------- FIN SENSOR SDI-12 --------------------------------*/


/*-------------------------- FUNCIONES SENSOR LIMNIMÉTRICO OTT -----------------------*/
void sensor_OTT(void)
{
  x_final = 0;
  y_final = 0;
  int k = 32;
  int d = 5;

  for (int i = 0; i < k; i++)
  {
    x_final += analogRead(OTTa);
    y_final += analogRead(OTTb);
  }
  x_final = x_final / k;
  y_final = y_final / k;

  if (((x_final - x_inicial) < d) && ((x_final - x_inicial) > -d))
  {
    x_final = x_inicial;
  }
  if (((y_final - y_inicial) < d) && ((y_final - y_inicial) > -d))
  {
    y_final = y_inicial;
  }

  if (((x_inicial - x_final) > 0) && ((y_inicial - y_final) > 0))
  {
    cuadrante_final = 1;
  }
  else if (((x_inicial - x_final) < 0) && ((y_inicial - y_final) > 0))
  {
    cuadrante_final = 2;
  }
  else if (((x_inicial - x_final) < 0) && ((y_inicial - y_final) < 0))
  {
    cuadrante_final = 3;
  }
  else if (((x_inicial - x_final) > 0) && ((y_inicial - y_final) < -0))
  {
    cuadrante_final = 4;
  }
  else
  {
    cuadrante_final = cuadrante_final;
  }

  switch (cuadrante_inicial)
  {
    case 1: if (cuadrante_final == 2)
      {
        valorSensor += 55;
      }
      else if (cuadrante_final == 4)
      {
        valorSensor -= 55;
      }
      break;
    case 2: if (cuadrante_final == 3)
      {
        valorSensor += 55;
      }
      else if (cuadrante_final == 1)
      {
        valorSensor -= 55;
      }
      break;
    case 3: if (cuadrante_final == 4)
      {
        valorSensor += 55;
      }
      else if (cuadrante_final == 2)
      {
        valorSensor -= 55;
      }
      break;
    case 4: if (cuadrante_final == 1)
      {
        valorSensor += 55;
      }
      else if (cuadrante_final == 3)
      {
        valorSensor -= 55;
      }
      break;
  }

  x_inicial = x_final;
  y_inicial = y_final;
  cuadrante_inicial = cuadrante_final;

  //Serial.print(x_final);
  //Serial.print("\t");
  //Serial.print(y_final);
  //Serial.print("\t");
  //Serial.print(cuadrante_final);
  //Serial.print("\t");
  //Serial.println(valorSensor);
  return;
}
/*------------------------ FIN FUNCIONES SENSOR LIMNIMÉTRICO OTT ---------------------*/

