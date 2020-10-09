/*
* Arduino Door Lock Access Control Project
*                
* by Dejan Nedelkovski, www.HowToMechatronics.com
* 
* Library: MFRC522, https://github.com/miguelbalboa/rfid
*/
#include <EEPROM.h>
#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>

const int RST_PIN   9
const int SS_PIN    10
const int PICC_id_length = 8;

byte readCard[PICC_id_length];
String tagID = "";
boolean successRead = false;
boolean correctTag = false;
boolean doorOpened = false;
LiquidCrystal_I2C lcd(0x27,16,2);
MFRC522 mfrc522(SS_PIN, RST_PIN);

// motor vars
const int stepPin = 3;
const int dirPin = 4; 
const int relayPin = 7; 

const int microDelay = 1000;
const int stepSize = 200;
const int motorRots = 5.8;

void setup() {
  // Initiating
  SPI.begin();        // SPI bus
  mfrc522.PCD_Init(); //  MFRC522
  lcd.init();
  lcd.backlight();

  pinMode(stepPin,OUTPUT);
  pinMode(dirPin,OUTPUT);

  reset_display();


}
void loop() {
  for ( uint8_t i = 0; i < PICC_id_length; i++) {  // Clear uid array
      mfrc522.uid.uidByte[i] = 0;
    }

  if ( ! mfrc522.PICC_IsNewCardPresent()) { //If a new PICC placed to RFID reader continue
    return;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {   //Since a PICC placed get Serial and continue
    return;
  }
  tagID = "";
  for ( uint8_t i = 0; i < PICC_id_length; i++) {  //
    readCard[i] = mfrc522.uid.uidByte[i];
    tagID.concat(String(mfrc522.uid.uidByte[i], HEX)); // Adds the PICC_id_length bytes in a single String variable
  }
  tagID.toUpperCase();
  mfrc522.PICC_HaltA(); // Stop reading
  correctTag = false;
  // Checks whether the scanned tag is the master tag
  if ( is_master_id( readCard ) ){
    lcd_message("Add/Remove Tag", 0);
    while (!successRead) {
      successRead = getID();
      if ( successRead == true) { // If new card is scanned
        
        if (remove_id(readCard)){
          lcd_message("Removed Tag", 1);
          delay(1000);
          reset_display();
          return;
        }
      add_EEPROM_id(readCard); // If card cannot be found and removed, add it
      lcd_message("Added Tag!", 1);
      delay(1000);
      reset_display();
      return;
      }
    }
    reset_display();
  }
  successRead = false;
  
  // Checks whether the scanned tag is authorized
  bool success = false;
  for (int i = 2; i < EEPROM.length() / PICC_id_length; i++){
    
    byte test [PICC_id_length];
    read_EEPROM_id(test, i);
    
    bool is_found = true;
    for (int n = 0; n < PICC_id_length; n++){
      if (test[n] != readCard[n]){
        is_found = false;
        break;
      }
      
    }

    if (is_found){
      open_door();
      success = true;
      break;
    }
    
  }
  if (success == false){
    lcd_message("Access Denied!", 1);
    delay(1000);
    reset_display();
  }
  
}

void motorStep(float mult = 1){
  if(mult < 0){ // Positive mult to open, negative to close
    digitalWrite(dirPin, LOW);
  }
  else{
    digitalWrite(dirPin, HIGH);
  }
  delay(5);

  mult = abs(mult);

  for(int i = 0; i < mult*stepSize; i++){
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(microDelay);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(microDelay);
  }
}

void open_door(){
  lcd_message("Access Granted!", 1);
  motorStep(motorRots);
  delay(3000);
  motorStep(-motorRots);
  reset_display();
}

uint8_t getID() {
  for ( uint8_t i = 0; i < PICC_id_length; i++) {  // Clear uid array
      mfrc522.uid.uidByte[i] = 0;
  }
  // Getting ready for Reading PICCs
  if ( ! mfrc522.PICC_IsNewCardPresent()) { //If a new PICC placed to RFID reader continue
    return 0;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {   //Since a PICC placed get Serial and continue
    return 0;
  }
  tagID = "";
  for ( uint8_t i = 0; i < PICC_id_length; i++) {  // Reads each byte of the ID
    readCard[i] = mfrc522.uid.uidByte[i]; // Adds the bytes in a global byte array
    tagID.concat(String(mfrc522.uid.uidByte[i], HEX)); // Adds the bytes in a global String variable
    
  }
  tagID.toUpperCase();
  //Serial.println("tagID:" + tagID);
  mfrc522.PICC_HaltA(); // Stop reading
  return 1;
}

void read_EEPROM_id(byte byteArray [], int index){ // reads ID at index and store it in byteArray
  for(int i = 0; i < PICC_id_length; i++){byteArray[i] = 0x00;}
  int start_pos = index * PICC_id_length;
  for (int i = start_pos; i <= start_pos + PICC_id_length; i++){
    byteArray[i - start_pos] = EEPROM.read(i);
  }
    
  if (index == 0){
    Serial.println("EEPROM index 0 is reserved");
  }
  
}
void clear_EEPROM_index(int index){
  if (index > 1){
    
    int start_pos = index * PICC_id_length;
    for (int i = start_pos; i <= start_pos + PICC_id_length; i++){
      EEPROM.write(i, 0xFF);
      
    }
    
  }
  else{
    Serial.println("EEPROM index 0/1 is reserved");
  }

}
void add_EEPROM_id(byte new_id []){
  for (int i = 2; i < EEPROM.length() / PICC_id_length; i++){
    byte test [PICC_id_length];
    read_EEPROM_id(test, i);

    bool is_Empty = true;
    for (int n = 0; n < PICC_id_length; n++){
      if (test[n] != 0xFF){
        is_Empty = false;
        break;
      }
    }

    if (is_Empty){
      write_EEPROM_id(new_id, i);
      break;
    }
    
  }
}
bool remove_id(byte remove_id []){
  bool success = false;
  for (int i = 2; i < EEPROM.length() / PICC_id_length; i++){
    byte test [PICC_id_length];
    read_EEPROM_id(test, i);

    bool is_found = true;
    for (int n = 0; n < PICC_id_length; n++){
      if (test[n] != remove_id[n]){
        is_found = false;
        break;
      }
    }

    if (is_found){
      clear_EEPROM_index(i);
      success = true;
      break;
    }
    
  }
  return success;
}
void write_EEPROM_id(byte id [], int index){
  if (index > 0){
    
    int start_pos = index * PICC_id_length;
    for (int i = start_pos; i <= start_pos + PICC_id_length; i++){
      EEPROM.write(i, id[i - start_pos]);
    }
    
  }
  else{
    Serial.println("EEPROM index 0 is reserved");
  }
}

bool is_master_id (byte id []){
  byte master_key [PICC_id_length];
  read_EEPROM_id(master_key, 1);

  bool is_master = true;
  for (int i = 0; i < PICC_id_length; i++){
    if (master_key[i] != id[i]){
      is_master = false;
      break;
    }
  }

  return is_master;
}

void dump_byte_array(byte *buffer, byte bufferSize) {
    for (byte i = 0; i < bufferSize; i++) {
        Serial.print(buffer[i] < 0x10 ? " 0" : " ");
        Serial.print(buffer[i], HEX);
    }
    Serial.println("");
}

void print_id(byte id []){
  for (int i = 0; i < PICC_id_length; i++){
    Serial.print(id[i], HEX);
    
  }
  Serial.println("");

}

void lcd_message(String message, int row){
  
  int start = 0;
  if (message.length() < 16){
    start = (16 - message.length())/2;
  }
  lcd.setCursor(start,row);
  
  for (int i = 0; i < message.length() && i <= 16; i++){
    lcd.print(message[i]);
  }
  //Serial.println("lcd message:" + message);
}

void reset_display(){
  lcd_message("  Scan your ID  ", 0);
  lcd_message("                ", 1);
}
