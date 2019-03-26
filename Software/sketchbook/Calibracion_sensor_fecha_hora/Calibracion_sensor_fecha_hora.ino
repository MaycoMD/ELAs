#include "SoftwareSerialMod.h"
#include "SDI12Mod.h"
#include <SPI.h>
#include <EEPROM.h>

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

#define pMIN 0
#define pMAX 5
#define pM 10
#define pB 15

// MAPEO EEPROM
// 0 -> Minimo
// 5 -> Maximo
// 10 -> Pendiente (m)
// 15 -> Ordenada al origen (b)

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

float valorMax = 10000.0;
float valorSensorMax;
float valorSensorMin;
float m = 0.0;
float b = 0.0;
String valorSensor;
float valorSensorTemp;
float valorTension = 0.0;
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

  delay(1000);
  if (encender_sensor())
  {
    digitalWrite(RELEpin, HIGH);
    calibracion_sensor_420ma();
    digitalWrite(RELEpin, LOW);
  }

  if (encender_telit())
  {
    if (reset_telit())
    {
      get_fecha_hora();
      set_fecha_hora();
      Serial.println();
      apagar_telit();
    }
  }

  Serial.println();
  EEPROM.get(pMIN, valorSensorMin);
  Serial.print("Valor minimo del sensor: ");
  Serial.println(valorSensorMin);
  EEPROM.get(pMAX, valorSensorMax);
  Serial.print("Valor maximo del sensor: ");
  Serial.println(valorSensorMax);
  EEPROM.get(pM, m);
  Serial.print("Pendiente: ");
  Serial.println(m);
  EEPROM.get(pB, b);
  Serial.print("Ordenada al origen: ");
  Serial.println(b);
}
/*--------------------- FIN CONFIGURACIONES INICIALES --------------------------------*/
void loop()
{
  delay(100);
  valorSensorTemp = 0.0;
  for (int i = 0; i < 64; i++)
  {
    valorSensorTemp += analogRead(s420);
    delay(100);
  }
  valorSensorTemp /= 64.0;
  Serial.print(valorSensorTemp);
  Serial.print(" -> ");
  valorSensorTemp = (valorSensorTemp * m) - b;
  valorSensor = String(valorSensorTemp);
  Serial.print(valorSensor);
  Serial.print(" -> ");
  valorSensorTemp /= 1000.0;
  if (valorSensorTemp >= 10.00)
  {
    valorSensor = "10.00";
  }
  else if (valorSensorTemp < 0.01)
  {
    valorSensor = "ERROR";
  }
  else
  {
    valorSensor = String(valorSensorTemp);
  }
  Serial.println(valorSensor);
}
//---------------------------------------------------------------------------------------
bool encender_sensor()
{
  bool n = 1;
  delay(1000);
  respuesta = "";
  while (n)
  {
    Serial.print("Encender sensor? <s/n>: ");
    respuesta = Serial.readStringUntil(CR);
    Serial.println();
    if (respuesta.indexOf("s") != -1)
    {
      n = 0;
      digitalWrite(RELEpin, HIGH);
      delay(delaySensor * 1000);
      Serial.println("Sensor encendido");
      return true;
    }
    else if (respuesta.indexOf("n") != -1)
    {
      n = 0;
      return false;
    }
  }
}
//---------------------------------------------------------------------------------------
void calibracion_sensor_420ma(void)
{
  valorSensorMin = 1023.0;
  valorSensorMax = 0.0;
  m = 0.0;
  b = 0.0;

  Serial.println("Coloque el sensor en el valor minimo");
  delay(20000);
  Serial.println("midiendo...");
  delay(5000);
  for (int j = 0; j < 10; j++)
  {
    valorSensorTemp = 0.0;
    for (int i = 0; i < 64; i++)
    {
      valorSensorTemp += analogRead(s420);
      delay(100);
    }
    valorSensorTemp /= 64.0;
    Serial.println(valorSensorTemp);
    if (valorSensorTemp < valorSensorMin)
    {
      valorSensorMin = valorSensorTemp;
    }
  }
  Serial.print("Minimo: ");
  Serial.println(valorSensorMin);
  EEPROM.put(pMIN, valorSensorMin);
  Serial.println("Almecenado en EEPROM");
  Serial.println();

  Serial.println("Coloque el sensor en el valor maximo");
  delay(20000);
  Serial.println("midiendo...");
  delay(5000);
  for (int j = 0; j < 10; j++)
  {
    valorSensorTemp = 0.0;
    for (int i = 0; i < 64; i++)
    {
      valorSensorTemp += analogRead(s420);
      delay(100);
    }
    valorSensorTemp /= 64.0;
    Serial.println(valorSensorTemp);
    if (valorSensorTemp > valorSensorMax)
    {
      valorSensorMax = valorSensorTemp;
    }
  }
  Serial.print("Maximo:");
  Serial.println(valorSensorMax);
  EEPROM.put(pMAX, valorSensorMax);
  Serial.println("Almecenado en EEPROM");
  Serial.println();

  m = valorMax / (valorSensorMax - valorSensorMin);
  Serial.print("Pendiente: ");
  Serial.println(m);
  EEPROM.put(pM, m);
  Serial.println("Almecenado en EEPROM");
  Serial.println();
  b = valorSensorMin * m;
  Serial.print("Ordenada al origen: ");
  Serial.println(b);
  EEPROM.put(pB, b);
  Serial.println("Almacenado en EEPROM");
  Serial.println();

  delay(1000);
  return;
}


/*-------------------------- FUNCIONES CONTROL MÓDULO TELIT --------------------------*/
bool encender_telit()
{
  bool n = 1;
  delay(1000);
  respuesta = "";
  while (n)
  {
    Serial.print("Reiniciar Telit?? <s/n>: ");
    respuesta = Serial.readStringUntil(CR);
    Serial.println();
    if (respuesta.indexOf("s") != -1)
    {
      n = 0;
      return true;
    }
    else if (respuesta.indexOf("n") != -1)
    {
      n = 0;
      return false;
    }
  }
}
//---------------------------------------------------------------------------------------

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

