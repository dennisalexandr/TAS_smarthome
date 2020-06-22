#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <MQTT.h>

WiFiClient net;
MQTTClient client;

const char ssid[] = "";
const char pass[] = "";
String unique;
String belongsto;
unsigned long lastMillis = 0;


int pirState = LOW;
int val = 0;

#define RST_PIN         22           // Configurable, see typical pin layout above
#define SS_PIN          21          // Configurable, see typical pin layout above
#define R_LDR           5
#define R_PIR           4
#define LDR             A0
#define PIR             15
#define R_SEL           14

MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance

void connect() {
  Serial.print("checking wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.print("\nconnecting...");
  while (!client.connect("ESP32", "kimkim2u", "kimkim3u")) { //client_id, username, password
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nconnected!");
}

void messageReceived(String &topic, String &payload) {
  Serial.println("incoming: " + topic + " - " + payload);    
  if(topic=="/master"){
    if(payload=="0"){
      Serial.println("Smarthome deactivated");
      esp_restart();
    }
  }
}

//*****************************************************************************************//
void setup() {
  Serial.begin(9600);                                           
  WiFi.begin(ssid, pass);
  pinMode(R_LDR,OUTPUT);
  pinMode(R_PIR,OUTPUT);
  pinMode(R_SEL,OUTPUT);
  pinMode(PIR,INPUT);
  pinMode(LDR,INPUT);
  client.begin("broker.shiftr.io", net);
  client.onMessage(messageReceived);
  connect();
  SPI.begin();                                                  // Init SPI bus
  mfrc522.PCD_Init();                                              // Init MFRC522 card
  Serial.println(F("Read personal data on a MIFARE PICC:"));    //shows in serial that it is ready to read
}

void nyalakanPir(){
  val = digitalRead(PIR);       // read input value
  if (val == HIGH) {            // check if the input is HIGH
    digitalWrite(R_PIR, LOW);   // turn LED ON
    client.publish("lampukamarmandi","hidup");
    if (pirState == LOW) {
      Serial.println("Motion detected!");
      pirState = HIGH;
    }
    
  } else {
    digitalWrite(R_PIR, HIGH); // turn LED OFF
    client.publish("lampukamarmandi","mati");
    if (pirState == HIGH){
      // we have just turned of
      Serial.println("Motion ended!");
      // We only want to print on the output change, not state
      pirState = LOW;
    }
  }
}

void nyalakanLdr(){  
  int ldrStatus = analogRead(LDR);  
  if (ldrStatus > 100&&ldrStatus <= 200) {  
    digitalWrite(R_LDR, LOW);
    client.publish("lamputeras","hidup"); 
    client.publish("cuaca","berawan");     
    Serial.print("Its DARK, Turn on the LED : ");    
    Serial.println(ldrStatus); 
    delay(2000);   
  }
  else if(ldrStatus <= 100&&ldrStatus >= 0){
      digitalWrite(R_LDR, LOW);
      client.publish("lamputeras","hidup"); 
      client.publish("cuaca","gelap");     
      Serial.print("Its DARK, Turn on the LED : ");    
      Serial.println(ldrStatus); 
      delay(2000);
    } 
  else {  
    digitalWrite(R_LDR, HIGH);
    client.publish("lamputeras","mati");  
    client.publish("cuaca","terik");  
    Serial.print("Its BRIGHT, Turn off the LED : ");    
    Serial.println(ldrStatus);  
    delay(2000); 
  }
}

void bacaKartu(){
  byte block;
  byte len;
  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;
  MFRC522::StatusCode status;

  Serial.println(F("**Card Detected:**"));
  //-------------------------------------------
  mfrc522.PICC_DumpDetailsToSerial(&(mfrc522.uid)); //dump some details about the card
  //mfrc522.PICC_DumpToSerial(&(mfrc522.uid));      //uncomment this to see all blocks in hex

  byte buffer1[18];
  block = 4;
  len = 18;

  //------------------------------------------- GET Unique
  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 4, &key, &(mfrc522.uid)); //line 834 of MFRC522.cpp file
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("Authentication failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }

  status = mfrc522.MIFARE_Read(block, buffer1, &len);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("Reading failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }

  //PRINT BALANCE
  unique="";
  for (uint8_t i = 1; i < 16; i++)  {
     unique+=(char)buffer1[i];
  }
  
  //---------------------------------------- GET belongsto
  byte buffer2[18];
  block = 1;

  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 1, &key, &(mfrc522.uid)); //line 834
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("Authentication failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }

  status = mfrc522.MIFARE_Read(block, buffer2, &len);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("Reading failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }

  //PRINT ID  
  belongsto="";
  for (uint8_t i = 0; i < 16; i++) {
    belongsto+=(char)buffer2[i];
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  Serial.println("");
  Serial.println("Nama Anda : "+belongsto);
  //------------------------------------------------------------------------------------------------------------------------
  Serial.println(F("\n**End Reading**\n"));

  delay(1000); //change value if you want to read cards faster

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();  
}

void nyalakanSelenoid(){
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    return;
  }
  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return;
  }
  unique="";
  bacaKartu();
  if(unique=="YnX6Gj9T       "){
    Serial.println("Pintu dibuka");
    client.publish("pintudibuka",belongsto);
    digitalWrite(R_SEL, LOW);
    delay(7000);
    digitalWrite(R_SEL, HIGH);
  }
}

void loop() {
  client.loop();
  delay(10);  // <- fixes some issues with WiFi stability

  if (!client.connected()) {
    connect();
  }

  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    return;
  }
  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return;
  }
  unique="";
  bacaKartu();
  
  if(unique=="YnX6Gj9T       "){
    client.publish("activatedby",belongsto);
    unique="";
    while(1){
      nyalakanSelenoid();
      // publish a message roughly every second.
      if (millis() - lastMillis > 1000) {
         lastMillis = millis();
         client.publish("/master", "1");    
         client.subscribe("/master");
      }
      nyalakanPir();
      nyalakanLdr();
      
    }    
  }else{
    Serial.print("Maaf, Anda bukan anggota dari keluarga ini");
  }
}
