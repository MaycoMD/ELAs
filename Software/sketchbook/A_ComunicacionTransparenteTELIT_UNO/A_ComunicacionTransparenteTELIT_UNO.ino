/*  Este programa permite al usuario establecer un comunicación transparente entre una PC a través de su puerto serie y
    el módulo de comunicación celular TELIT, utilizando como intermediaria la placa Arduino UNO. El programa está verificado
    para los modelos GL865-QUAD, GL865-QUAD V3 y UL865-NAD.
    Es necesaria la utilización de un software capaz de emular una comunicación por puerto serie (HyperTerminal, minicom, etc.);
    y se debe configurar dicho software a 9600 baudios, 8N1, sin control de flujo.
*/

// at
//at+cclk?
//at+cclk="aaaa/mm/dd,hh:mm:ss+00" +3 de hora actual
//at+cclk?
//at#syshalt

#include "SoftwareSerialMod.h"

#define SMSRCVpin 2
#define RTCpin 3
#define SYSRSTpin 4
#define RXpin 8
#define TXpin 9
#define RSTpin 10
#define PWRMONpin 11
#define LEDpin 13

char LF = 10;
char CR = 13;
String comando = "";
char c;

SoftwareSerial mySerial = SoftwareSerial(RXpin, TXpin);

void setup()
{
  pinMode(SMSRCVpin, INPUT_PULLUP);
  delay(500);

  pinMode(SMSRCVpin, INPUT_PULLUP);
  pinMode(RTCpin, INPUT);
  pinMode(PWRMONpin, INPUT);
  pinMode(RXpin, INPUT);

  pinMode(RSTpin, OUTPUT);
  pinMode(SYSRSTpin, OUTPUT);
  pinMode(LEDpin, OUTPUT);
  pinMode(TXpin, OUTPUT);

  digitalWrite(RSTpin, LOW);
  digitalWrite(SYSRSTpin, HIGH);
  digitalWrite(LEDpin, LOW);

  Serial.begin(9600);
  while (!Serial) {}
  Serial.setTimeout(10000); //tiempo en milisegundos
  Serial.println("Sistema reiniciado");
  mySerial.begin(9600);

  Serial.print("Reiniciando modulo Telit... ");
  digitalWrite(RSTpin, HIGH);
  delay(250);
  digitalWrite(RSTpin, LOW);
  delay(5000);
  Serial.println("hecho");
  Serial.println("Inicio de la comunicacion");
}

void loop()
{
  if (mySerial.available())
  {
    c = mySerial.read();
    Serial.print(c);
  }
  if (Serial.available())
  {
    comando = Serial.readStringUntil(CR);
    mySerial.println(comando);
    Serial.print(comando);
  }
}

