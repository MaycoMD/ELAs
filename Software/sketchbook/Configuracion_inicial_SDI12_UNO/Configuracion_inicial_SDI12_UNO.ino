#include "SDI12Mod.h"           // SDI-12 modificada
#include <SPI.h>

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

//=============================== VARIABLES ================================
String respuesta = "";
String comando = "";

void setup()
{
  delay(500);
  Serial.begin(9600);
  while (!Serial) {}
  Serial.setTimeout(10000); //tiempo en milisegundos

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
}
/*--------------------- FIN CONFIGURACIONES INICIALES --------------------------------*/


/*---------------------------PROGRAMA PRINCIPAL --------------------------------------*/
void loop()
{
  digitalWrite(RELEpin, HIGH);
  delay(5000);

  comandoSDI12("0!");
  comandoSDI12("0V!");
  comandoSDI12("0M!");
  //comandoSDI12("0XSCS2.3!");    // Configura el sensor al nivel actual
  comandoSDI12("0XWSS0.305!");
  comandoSDI12("0XRSS!");
  comandoSDI12("0XWSO0.0!");
  comandoSDI12("0XRSO!");
  comandoSDI12("0XWIH10.0!");
  comandoSDI12("0XRIH!");
  comandoSDI12("0XWIL0.0!");
  comandoSDI12("0WRIL!");
  comandoSDI12("0XWLCD2!");
  comandoSDI12("0XRLCD!");

  while(1);
}

/*--------------------------------- SENSOR SDI-12 ------------------------------------*/
void comandoSDI12(String comando)
{
  SDI12 mySDI12(SDIpin);
  mySDI12.begin();

  respuesta = "";
  delay(1000);
  Serial.println(comando);
  mySDI12.sendCommand(comando);
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
  Serial.println(respuesta);
  mySDI12.clearBuffer();

  respuesta = "";
  delay(1000);
  Serial.println("0D0!");
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
  Serial.println(respuesta);
  mySDI12.clearBuffer();
  mySDI12.end();
  Serial.println();
  return;
}
/*-------------------------------- FIN SENSOR SDI-12 ---------------------------------*/

