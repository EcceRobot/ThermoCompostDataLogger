//NOTE: Non utilizzare gli ingressi analogici A4 e A5 in quanto utilizzati dallo shield per la comunicazione I2C con RTC
// the battery will last 5 or more years

//***********************************************************************************//
//****************************************LIBRERIE***********************************//
//***********************************************************************************//
//questa libreria serve per comunicare con il protocollo OneWire utilizzato dai sensori
#include <OneWire.h>
//questa è opzionale, permetti di usare dei comandi più semplici per interrogare i sensori di temperatura che comunicano in OneWire
#include <DallasTemperature.h>
//PER IL DATA LOGGER
#include <SPI.h>
#include <SD.h>
#include <RTClib.h>


//***********************************************************************************//
//************************************VARIABILI GLOBALI******************************//
//***********************************************************************************//


//AREA CONFIGURAZIONE UTENTE

//***********************************
//numero di sonde collegate:        *
int N_sonde_PH = 1;  // 0 to 1      *
int N_sonde_TH = 5;  // 0 to 5      *
int N_sonde_ORP = 1; // 0 to 1      *
int N_sonde_GFM = 1; // 0 to 1      *
//***********************************

// change this to match your SD shield or module;
// Arduino Ethernet shield: pin 4
// Adafruit SD shields and modules: pin 10
// Sparkfun SD shield: pin 8
// MKRZero SD: SDCARD_SS_PIN
const int Datalogger_chipSelect = 10;

//nome che avrà il file prodotto
//!!! Attenzione alla lunghezza del nome !!!
String filename = "data_67.csv";

//PER LA SONDA DI pH
const int PH_SensorPin = A0;                   // pin a cui ho collegato il cavo di segnale della sonda di pH
const float PH_offset = 0.0;                // offset di misura della sonda
const float PH_coeff = 3.5 * 5.0 / 1024;    // coefficiente di moltiplicazione, 3.5 coeff particolare per questa sonda; *5.0/1024 per la lettura in millivolt

//PER LA SONDA DI ORP
const int ORP_SensorPin = A1;                   // pin a cui ho collegato il cavo di segnale della sonda di ORP
const float ORP_offset = 0.0;                // offset di misura della sonda
const float ORP_coeff = 1 * 5.0 / 1024;    // coefficiente di moltiplicazione, 3.5 coeff particolare per questa sonda; *5.0/1024 per la lettura in millivolt

//PER LA SONDA GFM - Gas Flow Meter
const int GFM_SensorPin = A2;                   // pin a cui ho collegato il cavo di segnale della sonda di Gas Flow
const float GFM_FFS = 50;                       // fare riferimento alla relativa formula ed alle indicazioni della mail CRC-TECHUK@honeywell.com From: CRC-TECHUK (UK07) <CRC-TECHUK@honeywell.com> Date: 2018-01-05 14:03 GMT+01:00 Subject: RE: ARDUINO CODE FOR A GAS FLOW METER
const float GFM_Vs = 5;


//PER LE SONDE DI TEMPERSTURA DS18B20
const int pin_SondeTemp = 8;

//media e tempistiche campionamento
const int campioni_media = 5;             // numero di lettura da campionare, di cui fare poi la media
const int t_delay = 1000;                   // ogni quanto tempo [ms] invio il dato lungo al seriale


//TERMINE AREA CONFIGURAZIONE UTENTE



//*******************************************************************************************************************************************

//definisco l'oggetto dataFile per popolare il file all'interno della SD Card 
File dataFile;
//definisco l'oggetto rtc per richiedere il timestamp da inserire nel datalog
RTC_DS1307 rtc;
//definisco l'oggetto OneWire
OneWire OneWire_SondeTemp(pin_SondeTemp);
//definisco oggetto DallasTemeprature (così che richiedere la temperatura alle sonde sia più agevole)
DallasTemperature SondeTemp(&OneWire_SondeTemp);

String dataString = "";
String datalog_head = "";
String timestamp = "";

int n_sondeTH_trovate;
float Lettura_PH = 0;
float avg_Lettura_PH = 0;
float Lettura_ORP = 0;
float avg_Lettura_ORP = 0;
float Lettura_GFM = 0;
float avg_Lettura_GFM = 0;
float Lettura_TH0 = 0;
float Lettura_TH1 = 0;
float Lettura_TH2 = 0;
float Lettura_TH3 = 0;
float Lettura_TH4 = 0;
float avg_Lettura_TH0 = 0;
float avg_Lettura_TH1 = 0;
float avg_Lettura_TH2 = 0;
float avg_Lettura_TH3 = 0;
float avg_Lettura_TH4 = 0;

int count = 1;;

//***********************************************************************************//
//****************************************SETUP***************************************//
//***********************************************************************************//



void setup()
{
  pinMode (PH_SensorPin, INPUT);
  pinMode (ORP_SensorPin, INPUT);
  pinMode (GFM_SensorPin, INPUT);

  Serial.begin(9600);

/*************************************************************************************/
    if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }
  
  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running!");
  }
  
  // following line sets the RTC to the date & time this sketch was compiled
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
    
/*************************************************************************************/

  Serial.print("Initializing SD card...");

  // see if the card is present and can be initialized:
  if (!SD.begin(Datalogger_chipSelect)) {
    Serial.println("Card failed, or not present");
    return;                                         // don't do anything more:
  }
  Serial.println("card initialized.");


  Serial.print("\n\n\n\nEsempio lettura temperatura da piu' sonde\n");

  //inizializzo comunicazione con le sonde DS18B20
  SondeTemp.begin();


  // identifico il numero di sonde sul bus OneWire
  Serial.print("**********************\n");
  Serial.print("* Trovate ");
  n_sondeTH_trovate = int(SondeTemp.getDeviceCount());
  Serial.print(n_sondeTH_trovate);
  Serial.print(" sonda/e DS18B20. *\n");
  Serial.print("**********************\n");

  if (!SD.exists(filename)) {
    dataFile = SD.open(filename, FILE_WRITE);

    // if the file is available, write to it:
    if (dataFile) {

      // creo l'opportuna intestazione del file CSV
      datalog_head.concat("timestamp, unix_timestamp,");
      if (N_sonde_PH == 1) datalog_head.concat("pH");
      if (N_sonde_TH  > 0 && N_sonde_PH == 1) datalog_head.concat(",");
      if (N_sonde_TH  > 0) datalog_head.concat("th0");
      if (N_sonde_TH  > 1) datalog_head.concat(",");
      if (N_sonde_TH  > 1) datalog_head.concat("th1");
      if (N_sonde_TH  > 2) datalog_head.concat(",");
      if (N_sonde_TH  > 2) datalog_head.concat("th2");
      if (N_sonde_TH  > 3) datalog_head.concat(",");
      if (N_sonde_TH  > 3) datalog_head.concat("th3");
      if (N_sonde_TH  > 4) datalog_head.concat(",");
      if (N_sonde_TH  > 4) datalog_head.concat("th4");
      if (N_sonde_ORP == 1) datalog_head.concat(",");
      if (N_sonde_ORP == 1) datalog_head.concat("ORP");
      if (N_sonde_GFM == 1) datalog_head.concat(",");
      if (N_sonde_GFM == 1) datalog_head.concat("GFM");

      dataFile.println(datalog_head);
      dataFile.close();
    }
  }

}


//***********************************************************************************//
//****************************************LOOP***************************************//
//***********************************************************************************//


void loop() {

    DateTime now = rtc.now();
    timestamp ="";
    String tempString ="";
    timestamp.concat(now.year());
    timestamp.concat('/');
    if(now.month()<10) timestamp.concat('0');
    timestamp.concat(now.month());
    timestamp.concat('/');
    if(now.day()<10) timestamp.concat('0');
    timestamp.concat(now.day());
    timestamp.concat(' ');
    if(now.hour()<10) timestamp.concat('0');
    timestamp.concat(now.hour());
    timestamp.concat(':');
    if(now.minute()<10) timestamp.concat('0');
    timestamp.concat(now.minute());
    timestamp.concat(':');
    if(now.second()<10) timestamp.concat('0');
    timestamp.concat(now.second());
    timestamp.concat(',');
    timestamp.concat(now.unixtime());

    
    
  Serial.println("In loop");

  //PER LA SONDA TH
  if (N_sonde_TH > 0) {
    SondeTemp.requestTemperatures();
    Lettura_TH0 = SondeTemp.getTempCByIndex(0);
    Lettura_TH1 = SondeTemp.getTempCByIndex(1);
    Lettura_TH2 = SondeTemp.getTempCByIndex(2);
    Lettura_TH3 = SondeTemp.getTempCByIndex(3);
    Lettura_TH4 = SondeTemp.getTempCByIndex(4);
  }


  //PER LA SONDA pH
  if (N_sonde_PH == 1) Lettura_PH = analogRead(PH_SensorPin);

  //PER LA SONDA ORP
  if (N_sonde_ORP == 1) Lettura_ORP = analogRead(ORP_SensorPin);

  //PER LA SONDA GFM
  if (N_sonde_GFM == 1) Lettura_GFM = analogRead(GFM_SensorPin);

  //calcolo la media dei valori
  avg_Lettura_PH  = (avg_Lettura_PH  * (count - 1) + Lettura_PH)  / count;
  avg_Lettura_ORP  = (avg_Lettura_ORP  * (count - 1) + Lettura_ORP)  / count;
  avg_Lettura_GFM  = (avg_Lettura_GFM  * (count - 1) + Lettura_GFM)  / count;
  avg_Lettura_TH0 = (avg_Lettura_TH0 * (count - 1) + Lettura_TH0) / count;
  avg_Lettura_TH1 = (avg_Lettura_TH1 * (count - 1) + Lettura_TH1) / count;
  avg_Lettura_TH2 = (avg_Lettura_TH2 * (count - 1) + Lettura_TH2) / count;
  avg_Lettura_TH3 = (avg_Lettura_TH3 * (count - 1) + Lettura_TH3) / count;
  avg_Lettura_TH4 = (avg_Lettura_TH4 * (count - 1) + Lettura_TH4) / count;


  count++;

  //se ho analizzato il numero di punti richiesto allora invio il dato in seriale
  // e resto in pausa per il tempo t_delay
  if (count == campioni_media) {



    // dato un input analogico 0-1023 (di cui ho ricavato la media) lo trasformo in una misura fisica
    // secondo le caratterisitche del sensore indicate nell'area configurazione utente
    avg_Lettura_PH = avg_Lettura_PH * PH_coeff + PH_offset;
    avg_Lettura_ORP = avg_Lettura_ORP * ORP_coeff + ORP_offset;
    avg_Lettura_GFM = GFM_FFS * ((avg_Lettura_GFM*5/1024) / GFM_Vs - 0.5) / 0.4;   //in realtà quello che ottengo dalla formula è FA, ma per comodità lo appoggio sulla stessa variabile usata per le letture analogiche

    //PER IL DATA LOGGER

    // make a string for assembling the data to log:
    dataString = "";

    dataString.concat(timestamp);
    dataString.concat(',');
    if (N_sonde_PH == 1) dataString.concat(String(avg_Lettura_PH));
    if (N_sonde_TH  > 0 && N_sonde_PH == 1) dataString.concat(",");
    if (N_sonde_TH  > 0) dataString.concat(String(avg_Lettura_TH0));
    if (N_sonde_TH  > 1) dataString.concat(",");
    if (N_sonde_TH  > 1) dataString.concat(String(avg_Lettura_TH1));
    if (N_sonde_TH  > 2) dataString.concat(",");
    if (N_sonde_TH  > 2) dataString.concat(String(avg_Lettura_TH2));
    if (N_sonde_TH  > 3) dataString.concat(",");
    if (N_sonde_TH  > 3) dataString.concat(String(avg_Lettura_TH3));
    if (N_sonde_TH  > 4) dataString.concat(",");
    if (N_sonde_TH  > 4) dataString.concat(String(avg_Lettura_TH4));
    if (N_sonde_ORP == 1) dataString.concat(",");
    if (N_sonde_ORP == 1) dataString.concat(String(avg_Lettura_ORP));
    if (N_sonde_GFM == 1) dataString.concat(",");
    if (N_sonde_GFM == 1) dataString.concat(String(avg_Lettura_GFM));



    // open the file. note that only one file can be open at a time,
    // so you have to close this one before opening another.
    dataFile = SD.open(filename, FILE_WRITE);

    // if the file is available, write to it:
    if (dataFile) {

      dataFile.println(dataString);
      dataFile.close();
    }
    // if the file isn't open, pop up an error:
    else {
      Serial.println("error opening file");
    }
    // print to the serial port too:
    Serial.println(dataString);


    delay(t_delay);
    //resetto tutto per poi ricominciare a campionare i nuovi valori che arriveranno
    count = 1;
    avg_Lettura_PH = 0;
    avg_Lettura_TH0 = 0;
    avg_Lettura_TH1 = 0;
    avg_Lettura_TH2 = 0;
    avg_Lettura_TH3 = 0;
    avg_Lettura_TH4 = 0;
    avg_Lettura_ORP = 0;
  }
}
