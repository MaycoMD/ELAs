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
// 35 -> Precalentamiento (delaySensor)

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

unsigned long ID;
unsigned int frecuencia;
byte delaySensor;
float valorMaxTotal;
float valorSensorMax;
float valorSensorMin;
float m = 0.0;
float b = 0.0;
String valorSensor;
float valorSensorTemp;
float valorTension = 0.0;
String fechaYhora;
String valorSenial;
String respuesta = "";

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

  Serial.println();
  EEPROM.get(pID, ID);
  Serial.print("Identificador de la estacion: ");
  Serial.println(ID);
  EEPROM.get(pFREC, frecuencia);
  Serial.print("Frecuencia de transmision: ");
  Serial.print(frecuencia);
  Serial.println(" minutos");
  EEPROM.get(pMAXT, valorMaxTotal);
  Serial.print("Valor maximo total: ");
  Serial.println(valorMaxTotal);
  EEPROM.get(pDELAY, delaySensor );
  Serial.print("Pre-calentamiento del sensor: ");
  Serial.print(delaySensor);
  Serial.println(" segundos");
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
  Serial.println();

  cambiar_id();
  cambiar_frecuencia();
  cambiar_maxtotal();
  cambiar_delay();

  if (encender_sensor())
  {
    if (calibrar_sensor())
    {
      calibracion_sensor_420ma();
    }
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
  EEPROM.get(pID, ID);
  Serial.print("Identificador de la estacion: ");
  Serial.println(ID);
  EEPROM.get(pFREC, frecuencia);
  Serial.print("Frecuencia de transmision: ");
  Serial.print(frecuencia);
  Serial.println(" minutos");
  EEPROM.get(pMAXT, valorMaxTotal);
  Serial.print("Valor maximo total: ");
  Serial.println(valorMaxTotal);
  EEPROM.get(pDELAY, delaySensor );
  Serial.print("Pre-calentamiento del sensor: ");
  Serial.print(delaySensor);
  Serial.println(" segundos");
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
  Serial.println();
}
/*--------------------- FIN CONFIGURACIONES INICIALES --------------------------------*/
void loop()
{
  digitalWrite(RELEpin, HIGH);
  delay(delaySensor * 1000);
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
void cambiar_id()
{
  bool n = 1;
  delay(1000);
  respuesta = "";
  Serial.println();
  while (n)
  {
    Serial.print("Modificar identificador? <s/n>: ");
    respuesta = Serial.readStringUntil(CR);
    Serial.println();
    if (respuesta.indexOf("s") != -1)
    {
      Serial.println();
      Serial.setTimeout(20000);
      Serial.print("Introduzca el nuevo identificador de la estacion: ");
      respuesta = Serial.readStringUntil(CR);
      Serial.println();
      ID = long(respuesta.toInt());
      Serial.print("Identificador: ");
      Serial.println(ID);
      EEPROM.put(pID, ID);
      Serial.println("Almacenado en EEPROM");
      return;
    }
    else if (respuesta.indexOf("n") != -1)
    {
      n = 0;
      return;
    }
  }
}
//---------------------------------------------------------------------------------------
void cambiar_frecuencia()
{
  bool n = 1;
  delay(1000);
  respuesta = "";
  Serial.println();
  while (n)
  {
    Serial.print("Modificar frecuencia? <s/n>: ");
    respuesta = Serial.readStringUntil(CR);
    Serial.println();
    if (respuesta.indexOf("s") != -1)
    {
      Serial.println();
      Serial.setTimeout(20000);
      Serial.print("Introduzca la nueva frecuencia (en minutos): ");
      respuesta = Serial.readStringUntil(CR);
      Serial.println();
      frecuencia = int(respuesta.toInt());
      Serial.print("Frecuencia: ");
      Serial.println(frecuencia);
      EEPROM.put(pFREC, frecuencia);
      Serial.println("Almacenado en EEPROM");
      return;
    }
    else if (respuesta.indexOf("n") != -1)
    {
      n = 0;
      return;
    }
  }
}
//---------------------------------------------------------------------------------------
void cambiar_maxtotal()
{
  bool n = 1;
  delay(1000);
  respuesta = "";
  Serial.println();
  while (n)
  {
    Serial.print("Modificar distancia maxima? <s/n>: ");
    respuesta = Serial.readStringUntil(CR);
    Serial.println();
    if (respuesta.indexOf("s") != -1)
    {
      Serial.println();
      Serial.setTimeout(20000);
      Serial.print("Introduzca la nueva distancia maxima (en milimetros): ");
      respuesta = Serial.readStringUntil(CR);
      Serial.println();
      valorMaxTotal = int(respuesta.toInt());
      Serial.print("Distancia maxima: ");
      Serial.println(valorMaxTotal);
      EEPROM.put(pMAXT, valorMaxTotal);
      Serial.println("Almacenado en EEPROM");
      return;
    }
    else if (respuesta.indexOf("n") != -1)
    {
      n = 0;
      return;
    }
  }
}
//---------------------------------------------------------------------------
void cambiar_delay()
{
  bool n = 1;
  delay(1000);
  respuesta = "";
  Serial.println();
  while (n)
  {
    Serial.print("Modificar precalentamiento? <s/n>: ");
    respuesta = Serial.readStringUntil(CR);
    Serial.println();
    if (respuesta.indexOf("s") != -1)
    {
      Serial.println();
      Serial.setTimeout(20000);
      Serial.print("Introduzca el nuevo tiempo de espera para el inicio del sensor (en segundos): ");
      respuesta = Serial.readStringUntil(CR);
      Serial.println();
      delaySensor = int(respuesta.toInt());
      Serial.print("Precalentamiento: ");
      Serial.print(delaySensor);
      Serial.println(" segundos");
      EEPROM.put(pDELAY, delaySensor);
      Serial.println("Almacenado en EEPROM");
      return;
    }
    else if (respuesta.indexOf("n") != -1)
    {
      n = 0;
      return;
    }
  }
}
//------------------------------------------------------------------------
bool encender_sensor()
{
  bool n = 1;
  delay(1000);
  respuesta = "";
  Serial.println();
  while (n)
  {
    Serial.print("Encender sensor? <s/n>: ");
    respuesta = Serial.readStringUntil(CR);
    Serial.println();
    if (respuesta.indexOf("s") != -1)
    {
      n = 0;
      Serial.println("Iniciando sensor... ");
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
bool calibrar_sensor()
{
  bool n = 1;
  delay(1000);
  respuesta = "";
  Serial.println();
  while (n)
  {
    Serial.print("Calibrar sensor? <s/n>: ");
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
  Serial.println("Almacenado en EEPROM");
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
  Serial.println("Almacenado en EEPROM");
  Serial.println();

  m = valorMaxTotal / (valorSensorMax - valorSensorMin);
  Serial.print("Pendiente: ");
  Serial.println(m);
  EEPROM.put(pM, m);
  Serial.println("Almacenado en EEPROM");
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
  Serial.println();
  while (n)
  {
    Serial.print("Reiniciar Telit? <s/n>: ");
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

bool comandoAT(String comando, char resp[5], byte contador)
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
    Serial.print("Introduzca la fecha y hora en formato <aaaa/mm/dd,hh:mm:ss> (UTC-00): ");
    fechaYhora = Serial.readStringUntil(CR);
    Serial.println();
    comandoAT("AT+CCLK=\"" + fechaYhora + "+00\"", "OK", 10);
  }
  return;
}
/*------------------------ FIN FUNCIONES CONTROL MÓDULO TELIT ------------------------*/

