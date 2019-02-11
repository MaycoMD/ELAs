/*  Este programa permite al usuario establecer un comunicación transparente entre una PC a través de su puerto serie y
    el módulo de comunicación celular TELIT, utilizando como intermediaria la placa Arduino UNO. El programa está verificado
    para los modelos GL865-QUAD, GL865-QUAD V3 y UL865-NAD.
    Es necesaria la utilización de un software capaz de emular una comunicación por puerto serie (HyperTerminal, minicom, etc.);
    y se debe configurar dicho software a 9600 baudios, 8N1, sin control de flujo.
*/
#include "SoftwareSerialMod.h"

#define VBATpin A0
#define RELE1pin A1
#define s420 A2
#define OTTb A3
#define OTTa A4
#define SDIpin A5
#define SMSRCVpin 2
#define RTCpin 3
#define SYSRSTpin 4
#define RELE2pin 5
#define RXpin 8
#define TXpin 9
#define RSTpin 10
#define PWRMONpin 11
#define LEDpin 13

byte tipoSensor = 1;                  // tipo de sensor a utilizar (0~3)
byte delaySensor = 5;                // pre-calentamiento del sensor (en segundos)
float valorMax = 10000.0;             // máximo valor a medir por el sensor (en milimetros)
byte frecuencia = 10;                 // frecuencia de transmisión de los datos (en minutos)
String ID = "ELF00";                  // Identificador de la estación
byte maxDatos = 6;                   // Cantidad máxima de datos a almacenar
byte contDatos = 0;
int valorSensor;
float valorTension = 0;
unsigned int TIEMPO = 1000;
char LF = 10;
char CR = 13;
String fechaYhora;
String valorSenial;
String datos = "";
bool interrupcion = false;
bool flag = false;
String respuesta = "";

SoftwareSerial mySerial = SoftwareSerial(RXpin, TXpin);

void setup()
{
  pinMode(SMSRCVpin, INPUT_PULLUP);
  delay(500);

  pinMode(VBATpin, INPUT);
  pinMode(s420, INPUT);
  pinMode(SMSRCVpin, INPUT_PULLUP);
  pinMode(RTCpin, INPUT);
  pinMode(PWRMONpin, INPUT);
  pinMode(RXpin, INPUT);

  pinMode(RSTpin, OUTPUT);
  pinMode(SYSRSTpin, OUTPUT);
  pinMode(RELE1pin, OUTPUT);
  pinMode(RELE2pin, OUTPUT);
  pinMode(LEDpin, OUTPUT);
  pinMode(TXpin, OUTPUT);

  digitalWrite(RSTpin, LOW);
  digitalWrite(SYSRSTpin, HIGH);
  digitalWrite(RELE1pin, LOW);
  digitalWrite(RELE2pin, LOW);
  digitalWrite(LEDpin, LOW);

  Serial.begin(9600);
  while (!Serial) {}
  Serial.setTimeout(15000); //tiempo en milisegundos
  Serial.println("Sistema reiniciado");
  mySerial.begin(9600);

  Serial.print("Reiniciando modulo Telit... ");
  digitalWrite(RSTpin, HIGH);
  delay(250);
  digitalWrite(RSTpin, LOW);
  delay(5000);
  Serial.println("hecho");
}

void loop()
{
  String com = "";
  Serial.println("Ingrese el comando");
  Serial.print("Comando AT: ");
  com = Serial.readStringUntil(CR); 
  comandoAT(com,"OK");
}

bool comandoAT(String comando, char resp[3])
{
  byte contador = 10;
  char c;
  respuesta = "ERROR";

  while (mySerial.available() > 0)
  {
    char basura = mySerial.read();
  }

  while ((respuesta.indexOf(resp) == -1) && (contador != 0))
  {
    delay(TIEMPO);
    respuesta = "";
    contador--;
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
    if (respuesta.indexOf("ERROR") != -1)
    {
      Serial.println(respuesta);
    }
    else if (respuesta.indexOf(resp) == -1)
    {
      Serial.println(" -> TIMEOUT!");
    }
  }
  if (contador != 0)
  {
    Serial.println(respuesta);
    return true;
  }
  else
  {
    return false;
  }
}

