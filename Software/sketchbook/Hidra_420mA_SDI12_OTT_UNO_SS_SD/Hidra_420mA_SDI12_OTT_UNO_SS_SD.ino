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
// 11 -> Telit power monitor (SD_MOSI)
// 12 -> (SD_MISO)
// 13 -> (SD_CLK)
// 14 -> GND

#define tipoSensor 1                  // tipo de sensor a utilizar (0~3)
#define delaySensor 5                 // pre-calentamiento del sensor (en segundos)
#define frecuencia 10                 // frecuencia de transmisión de los datos (en minutos)

// SENSORES:
// 0 -> 4-20 mA
// 1 -> SDI-12
// 2 -> OTT
// 3 -> Pulsos (No implementado)

// ESTACIONES
// ELR01 -> Quillinzo
// ELR02 -> Inriville
// ELR03 -> Piedras Blancas
// ELR04 -> Alpa Corral
// ELR05 -> Santa Rosa
// ELF01 -> Cruz Alta
// ELF02 -> Saladillo

#define LF 10
#define CR 13

const float valorMax = 10000.0;             // máximo valor a medir por el sensor (en milimetros)
const String ID = "ELR00";                  // Identificador de la estación
int valorSensor;
float valorTension = 0;
String fechaYhora;
String valorSenial;
bool interrupcion = false;
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
  Serial.setTimeout(15000); //tiempo en milisegundos
  //Serial.println();
  //Serial.print("Iniciando sistema... ");

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
  //Serial.println("hecho.");

  //Serial.print("Iniciando tarjeta SD... ");
  if (!SD.begin(SDCSpin))
  {
    //Serial.println("fallo");
  }
  //Serial.println("hecho.");

  //calibrar_sensor();
  //Serial.print("Leyendo sensores... ");
  switch (tipoSensor)
  {
    case 0: //sensor_420ma();
      break;
    case 1: sensor_SDI12();
      break;
    default: break;
  }
  sensor_bateria();
  //Serial.println("hecho.");

  if (reset_telit())
  {
    get_fecha_hora();
    //set_fecha_hora();
    set_alarma();
    get_senial();
    enviar_sms();
    borrar_sms();
  }
  apagar_telit();

  if (tipoSensor == 2)
  {
    //Serial.println("Habilitando interrupciones");
    attachInterrupt(digitalPinToInterrupt(RTCpin), RTCint, LOW);
    attachInterrupt(digitalPinToInterrupt(SMSRCVpin), SMSint, HIGH);
    interrupts();
  }
}
/*--------------------- FIN CONFIGURACIONES INICIALES --------------------------------*/

/*---------------------------PROGRAMA PRINCIPAL --------------------------------------*/
void loop()
{
  delay(5000);

  if (tipoSensor != 2)
  {
    //Serial.println("Habilitando interrupciones");
    //Serial.println("Iniciando modo de bajo consumo");
    Serial.flush();
    sleep_enable();
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    attachInterrupt(digitalPinToInterrupt(RTCpin), RTCint, LOW);
    attachInterrupt(digitalPinToInterrupt(SMSRCVpin), SMSint, HIGH);
    interrupts();
    sleep_cpu();
    sleep_disable();
  }
  if (tipoSensor == 2)
  {
    sensor_OTT();
  }

  if (interrupcion == true)
  {
    //Serial.print("(interrupcion) alarma -> ");
    delay(2000);
    if (digitalRead(RTCpin) == LOW)
    {
      //Serial.println("interrupcion por RTC");
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
      guardar_datos();
      imprimir_datos();
      if (conectar_telit())
      {
        if (enviar_datos())
        {
          SD.remove("datosD.txt");
        }
        if (!desconectar_telit())
        {
          reset_telit();
          get_fecha_hora();
          set_alarma();
        }
      }
      apagar_telit();
      interrupcion = false;
    }
    else
    {
      //Serial.println("falsa alarma");
      interrupcion = false;

    }
    if (tipoSensor == 2)
    {
      //Serial.println("Habilitando interrupciones");
      attachInterrupt(digitalPinToInterrupt(RTCpin), RTCint, LOW);
      attachInterrupt(digitalPinToInterrupt(SMSRCVpin), SMSint, HIGH);
      interrupts();
    }
  }
}
/*---------------------------- FIN PROGRAMA PRINCIPAL --------------------------------*/


/*--------------------------- INTERRUPCIONES EXTERNAS --------------------------------*/
// Rutina de interrupción por la alarma programada en el RTC del módulo Telit
void RTCint()
{
  sleep_disable(); //fully awake now
  detachInterrupt(digitalPinToInterrupt(RTCpin));
  delayMicroseconds(10000);
  if (digitalRead(RTCpin) == LOW)
  {
    interrupcion = true;
  }
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

bool comandoAT(String comando, char resp[3])
{
  delay(1000);
  mySerial.begin(9600);
  byte contador = 10;
  char c;
  respuesta = "ERROR";

  while (mySerial.available() > 0)
  {
    char basura = mySerial.read();
  }

  while ((respuesta.indexOf(resp) == -1) && (contador != 0))
  {
    if (contador != 10)
    {
      delay(5000);
    }
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
  if (contador != 0)
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
  //Serial.print("Reiniciando modulo Telit... ");
  delay(5000);

  digitalWrite(RSTpin, HIGH);
  delay(220);
  digitalWrite(RSTpin, LOW);
  delay(5000);

  //Serial.println("hecho.");

  if (comandoAT("AT", "OK"))
  {
    if (comandoAT("AT+CFUN=1", "OK"))
    {
      if (comandoAT("AT+CSDF=1,2", "OK"))
      {
        return true;
      }
    }
  }
  return false;
}
//----------------------------------------------------------------------------
bool despertar_telit(void)
{
  delay(2000);
  if (comandoAT("AT+CFUN=1", "OK"))
  {
    if (comandoAT("AT#WAKE=0", "OK"))
    {
      comandoAT("AT+CSDF=1,2", "OK");
    }
    return true;
  }
  return false;
}
//----------------------------------------------------------------------------
bool conectar_telit(void)
{
  if (comandoAT("AT#GPRS=0", "OK"))
  {
    if (comandoAT("AT#GPRS=1", "OK"))
    {
      if (comandoAT("AT#FTPTO=5000", "OK"))
      {
        if (comandoAT("AT#FTPOPEN=\"200.16.30.250\",\"estaciones\",\"es2016$..\",1", "OK")) // 0->Active Mode, 1->Passive Mode
        {
          if (comandoAT("AT#FTPTYPE=1", "OK")) // 0->Binary, 1->ASCII
          {
            if (comandoAT("AT#FTPAPP=\"" + ID + "/datos\",1", "OK"))
            {
              return true;
            }
          }
        }
      }
    }
  }
  return false;
}
//-----------------------------------------------------------------------------
//Inicializa el módulo GSM/GPRS y trasmite por UART los comandos adecuados
//para iniciar una conexión a internet y transmitir los datos al servidor web.
//Si se logra establecer una conexión GPRS, envía de a uno los datos
//almacenados en orden en el array de datos guardados.
bool enviar_datos(void)
{
  if (SD.exists("datosD.txt"))
  {
    File dataFile = SD.open("datosD.txt");
    char c;
    if (dataFile)
    {
      unsigned long largo = dataFile.size();
      unsigned int times = (largo / 1500);
      unsigned int rest = largo % 1500;
      unsigned int bytes2send = 0;
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
        if (comandoAT(command, ">"))
        {
          mySerial.begin(9600);
          while ((dataFile.available()) && ((bytes2send != 0) && (largo != 0)))
          {
            largo--;
            bytes2send--;
            c = dataFile.read();
            mySerial.print(c);
            Serial.print(c);
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
    return true;
  }
  return false;
}
//-----------------------------------------------------------------------------
bool desconectar_telit(void)
{
  if (comandoAT("AT#FTPCLOSE", "OK"))
  {
    if (comandoAT("AT#GPRS=0", "OK"));
    {
      return true;
    }
  }
  return false;
}
//-----------------------------------------------------------------------------
//transmite vía comunicación serie (UART) el comando AT adecuado para apagar
//el módulo GSM
bool apagar_telit(void)
{
  if (comandoAT("AT+CFUN=0,0", "OK"))
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
  if (comandoAT("AT+CCLK?", "OK"))
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
  Serial.print("Configurar fecha/hora? <s/n>: ");
  if (Serial.readStringUntil("s"))
  {
    Serial.println();
    Serial.setTimeout(20000);
    Serial.print("Introduzca la fecha y hora en formato <aaaa/mm/dd,hh:mm:ss>: ");
    fechaYhora = Serial.readStringUntil(CR);
    Serial.println();
    comandoAT("AT+CCLK=\"" + fechaYhora + "+00\"", "OK");
  }
  return;
}
//-----------------------------------------------------------------------------
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
  comandoAT("AT+CALA=\"" + strHora + ":" + strMinutos + ":00+00\",0,4,,\"0\",0", "OK");
  return;
}

//---------------------------------------------------------------------------------------
void get_senial(void)
{
  if (comandoAT("AT+CSQ", "OK"))
  {
    int index = respuesta.indexOf(":");
    valorSenial = respuesta.substring(index + 2, index + 6);
    return true;
  }
  return false;
}
//---------------------------------------------------------------------------------------
bool enviar_sms(void)
{
  if (comandoAT("AT+CMGF=1", "OK"))
  {
    if (comandoAT("AT+CMGS=\"3513420474\",129", ">"))
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
      if (comandoAT(datos, "+CMGS"))
      {
        return true;
      }
    }
  }
  return false;
}
//---------------------------------------------------------------------------------------
bool borrar_sms(void)
{
  if (comandoAT("AT+CMGD=4", "OK"))
  {
    return true;
  }
  return false;
}
/*------------------------ FIN FUNCIONES CONTROL MÓDULO TELIT ------------------------*/

/*--------------------------------- SENSOR 4-20mA ------------------------------------*/
void sensor_420ma(void)
{
  digitalWrite(RELEpin, HIGH);
  delay(delaySensor * 1000);
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
  digitalWrite(RELEpin, LOW);
  return;
}

void calibrar_sensor(void)
{
  Serial.print("Calibrar sensor? <s/n>: ");
  respuesta = Serial.readStringUntil(CR);
  Serial.println();
  if (respuesta.indexOf("s") != -1)
  {
    Serial.println("Modo calibracion. Presione 'e' para salir");
    digitalWrite(RELEpin, HIGH);
    delay(delaySensor * 1000);
    while (respuesta.indexOf('e') == -1)
    {
      delay(1000);
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
    digitalWrite(RELEpin, LOW);
  }
  return;
}
/*-------------------------------- FIN SENSOR 4-20mA ---------------------------------*/


/*--------------------------------- SENSOR SDI-12 ------------------------------------*/
void sensor_SDI12(void)
{
  digitalWrite(RELEpin, HIGH);
  delay(delaySensor * 1000);
  mySDI12.begin();

  String sdiResponse = "";
  delay(1000);
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
  digitalWrite(RELEpin, LOW);
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
  //  Serial.print("Nivel de bateria: ");
  //  Serial.print(valorTension);
  //  Serial.println(" V");
  return;
}
/*------------------------------ FIN TENSIÓN BATERÍA ---------------------------------*/

/*------------------------------------------------------------------------------------*/
void imprimir_datos()
{
  Serial.println(ID + ": " +
                 fechaYhora + "," +
                 valorSensor + "," +
                 valorTension + "," +
                 valorSenial);
  return;
}
/*------------------------------------------------------------------------------------*/


/*----------------------------- FUNCIONES TARJETA SD ---------------------------------*/
void guardar_datos()
{
  File dataFile;

  dataFile = SD.open("datosD.txt", FILE_WRITE);
  if (dataFile)
  {
    dataFile.print(fechaYhora.substring(0, 16));
    dataFile.print(",");
    dataFile.print(String(valorSensor));
    dataFile.print(",");
    dataFile.print(String(valorTension));
    dataFile.print(",");
    dataFile.print(valorSenial);
    dataFile.print("\r\n");
    dataFile.close();
  }
  //  else
  //  {
  //    Serial.println("error opening datosD.txt");
  //  }

  dataFile = SD.open("datosL.txt", FILE_WRITE);
  if (dataFile)
  {
    dataFile.print(fechaYhora.substring(0, 16));
    dataFile.print(",");
    dataFile.print(String(valorSensor));
    dataFile.print(",");
    dataFile.print(String(valorTension));
    dataFile.print(",");
    dataFile.print(valorSenial);
    dataFile.print("\r\n");
    dataFile.close();
  }
  //  else
  //  {
  //    Serial.println("error opening datosL.txt");
  //  }
  return;
}
/*--------------------------- FIN FUNCIONES TARJETA SD -------------------------------*/

