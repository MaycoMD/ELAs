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

char LF = 10;
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
  Serial.println();
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
  comandoAT("AT", "OK", 1);
  comandoAT("ATE0", "OK", 1);
  comandoAT("AT+CFUN=1", "OK", 1);
  comandoAT("AT&K0", "OK", 1);
  comandoAT("ATV1", "OK", 1);
  comandoAT("AT#SELINT=2", "OK", 1);
  comandoAT("AT+GMI", "OK", 1);
  comandoAT("AT+GMM", "OK", 1);
  comandoAT("AT+GMR", "OK", 1);
  //comandoAT("ATQ1","OK",1);
  //comandoAT("AT#MWI=0","OK",1);
  comandoAT("AT+CSDF=1,2", "OK", 1);
  comandoAT("AT+CSQ", "OK", 1);
  comandoAT("AT+CCLK?", "OK", 1);
  comandoAT("AT+CCLK=\"2019/06/27,17:52:00+00\"","OK",1);
  //comandoAT("AT+CALA=\"19:30:00+00\",0,4,,\"0\",0", "OK", 1);

  // --------- SMS ----------
  comandoAT("AT#SMSMODE=1", "OK", 1);
  comandoAT("AT+CPMS=\"SM\",\"SM\",\"SM\"", "OK", 1);
  comandoAT("AT+CMGF=1", "OK", 10);
  comandoAT("AT+CSDH=0", "OK", 10);
  comandoAT("AT+CMGL", "OK", 10);
  comandoAT("AT+CMGD=1,4", "OK", 10);
  if (comandoAT("AT+CMGS=\"3513420474\",129", ">", 1))
  {
    String com = "SMS de prueba";
    com.concat(char(26));
    comandoAT(com, "+CMGS", 1);
  }

  // ----------- PERSONAL ------------
  comandoAT("AT#USERID=\"gprs\"", "OK", 10);
  comandoAT("AT#PASSW=\"gprs\"", "OK", 10);
  comandoAT("AT+CGDCONT=1,\"IP\",\"gprs.personal.com\"", "OK", 10);

  // ------------ CLARO --------------
  //comandoAT("AT#USERID=\"clarogprs\"", "OK", 10);
  //comandoAT("AT#PASSW=\"clarogprs999\"", "OK", 10);
  //comandoAT("AT+CGDCONT=1,\"IP\",\"igprs.claro.com.ar\"", "OK", 10);

  comandoAT("AT#E2SMSRI=1150", "OK", 10); // 0 -> disabled, 50-1150 (ms) enabled
  comandoAT("AT+CNMI=1,1,0,0,0", "OK", 10);
  comandoAT("AT&P0", "OK", 10);
  comandoAT("AT&W0", "OK", 10);
  do
  {
    comandoAT("AT+CSQ", "OK", 10);
  }
  while (respuesta.indexOf("99") != -1);

  if (comandoAT("AT#GPRS=1", "OK", 100))
  {
    // ----------- CONEXIÓN FTP ------------
    //    if (comandoAT("AT#FTPTO=5000", "OK", 10))
    //    {
    //      if (comandoAT("AT#FTPOPEN=\"200.16.30.250\",\"estaciones\",\"es2016$..\",1", "OK", 10))
    //      {
    //        if (comandoAT("AT#FTPTYPE=1", "OK", 10))
    //        {
    //          if (comandoAT("AT#FTPAPP=\"" + ID + "/datos\",1", "OK", 10))
    //          {
    //            if (comandoAT("AT#FTPAPPEXT=21,1", ">", 1))
    //            {
    //              comandoAT("Transmision de prueba", "OK", 1);
    //            }
    //          }
    //        }
    //        comandoAT("AT#FTPCLOSE", "OK", 10);
    //      }
    //    }

    // ---------- CONEXIÓN HTTP -----------
    comandoAT("AT#HTTPCFG=0,\"new.omixom.com\",8001,0,,,0,120,1", "OK", 10);
    //comandoAT("AT#HTTPQRY=?", "OK", 10);
    comandoAT("AT#HTTPQRY=0,0,\"/weatherstation/updateweatherstation.jsp?ID=70009&PASSWORD=vwrnlDhZtz&senial=25&nivel_rio=6&nivel_bat=12&dateutc=2019-05-07%2021:40:00\"", "RING", 10); // Máx. 150 caracteres
  }
  comandoAT("AT#GPRS=0", "OK", 1);
  comandoAT("AT#SYSHALT", "OK", 10);
  while (1) {}
}

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
    delay(500);
    respuesta = "";
    contador--;
    mySerial.flush();
    mySerial.println(comando);
    Serial.print(comando);
    while ((respuesta.indexOf(resp) == -1) && (respuesta.indexOf("ERROR") == -1))
    {
      int i = 8000;
      while (i)
      {
        i--;
        while (mySerial.available())
        {
          c = mySerial.read();
          respuesta += c;
          delay(10);
          i=0;
        }
        delay(10);       
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
