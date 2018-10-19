#include <deprecated.h>
#include <MFRC522.h>
#include <MFRC522Extended.h>
#include <require_cpp11.h>


/* mifare ultralight example (25-02-2018)

     RFID-RC522 (SPI connexion)

     CARD RC522      Arduino (UNO)
       SDA  -----------  10 (Configurable, see SS_PIN constant)
       SCK  -----------  13
       MOSI -----------  11
       MISO -----------  12
       IRQ  -----------
       GND  -----------  GND
       RST  -----------  9 (onfigurable, see RST_PIN constant)
       3.3V ----------- 3.3V

*/

#include <SPI.h>
#include <MFRC522.h>


#define RST_PIN    5   // 
#define SS_PIN    53    //

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance
MFRC522::StatusCode status; //variable to get card status

byte buffer[40];  //data transfer buffer (16+2 bytes data+CRC)
byte size = sizeof(buffer);

uint8_t pageAddr = 0x04;  //In this example we will write/read 16 bytes (page 6,7,8 and 9).
//Ultraligth mem = 16 pages. 4 bytes per page.
//Pages 0 to 4 are for special functions.

void setup() {
  Serial.begin(9600); // Initialize serial communications with the PC
  SPI.begin(); // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522 card
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max); //Set Antenna Gain to Max- this will increase reading distance
  Serial.println(F("Sketch has been started!"));
  memcpy(buffer, "backtohome.org/thaimissing?12345678", 40);
}

void loop() {
  // Serial.println(F("Loop ... "));
  // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent())
    return;

  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial())
    return;

  // Write data ***********************************************
  Serial.println(F("Writing data ... "));
  for (int i = 0; i < 10; i++) {
    //data is writen in blocks of 4 bytes (4 bytes per page)
    status = (MFRC522::StatusCode) mfrc522.MIFARE_Ultralight_Write(pageAddr + i, &buffer[i * 4], 4);
    Serial.println(pageAddr + i);
    if (status != MFRC522::STATUS_OK) {
      Serial.print(F("MIFARE_Read() failed: "));
      Serial.println(mfrc522.GetStatusCodeName(status));
      return;
    }
  }
  Serial.println(F("MIFARE_Ultralight_Write() OK "));
  Serial.println();


  // Read data ***************************************************
  Serial.println(F("Reading data ... "));
  //data in 4 block is readed at once.
  status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(pageAddr, buffer, &size);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("MIFARE_Read() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }

  Serial.print(F("Readed data: "));
  //Dump a byte array to Serial
  for (byte i = 0; i < 40; i++) {
    Serial.write(buffer[i]);
  }
  Serial.println();

  mfrc522.PICC_HaltA();

}
