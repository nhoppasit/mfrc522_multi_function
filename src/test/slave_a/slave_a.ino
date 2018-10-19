/*--------------------------------------------------------------------------------------
 CODE CONTROL
 --------------------------------------------------------------------------------------*/
#define TEST_LOGIC 0
#define PRINT_TIME 0
#define PRINT_LOG 0
#define PRINT_LOG_ACK 0
#define SET_BLINK false
//
/*--------------------------------------------------------------------------------------
 INCLUDE
 --------------------------------------------------------------------------------------*/
#include <SPI.h>
#include <RFID.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
//
/*--------------------------------------------------------------------------------------
 DEFINES
 --------------------------------------------------------------------------------------*/
#define SLOT_NBR 0x51 // SLOT NUMBER
//
#define MAX_DATA_SIZE 24 // Serial data size
//
#define SEP 0x1C // End text charactor
#define ETX 0x03 // End text charactor
#define ACK 0x06 // Acknowlage
#define NAK 0x15 // Non-acknowlage
#define BIZ 0xBC // Busy!
//
// Field type meaning
#define MASTER_CONTACT   '?'   // '?' = 0x3F : Send slot status.
#define APPROVED_CMD     '$'   // '$' = 0x24 : Rental command = Approved + Credit balance.
#define REJECTED_CMD     0xCE  // 0xCE : Rental command = Rejected
#define INVALID_CMD      0xD6  // 0xD6 : Rental command = Invalid
//
// Sensor bit flag
#define BIKE_SEN_BIT 0x01  
//
// RFID MFRC522 
#define SS1_PIN 10
#define SS2_PIN 4
#define RST_PIN 9
//
// Relay
#define RLY_ON   LOW
#define RLY_OFF  HIGH
//
// CPU tick
#define INTERVAL         50 // ms
//
// TASK TIMING
#define TIME_RFID_MB    100 // ms
#define TIME_SENSOR     100 // ms
#define TIME_RFID_BIKE  100 // ms // 200
#define TIME_LCD_BIKE   200 // ms
#define TIME_BLINK     1000 // ms
#define TIME_CLEAR_DR  5000 // ms
#define TIME_ACK       5000 // ms
#define TIME_RENT2     6000 // ms
#define TIME_RENT3    12000 // ms
//
//
/*--------------------------------------------------------------------------------------
 VARIABLES
 --------------------------------------------------------------------------------------*/
//Sensor status
byte sensorByte = 0x00; // B0111 => (MSB) MemberTagExist | BikeTagExist | BikeSensor (LSB)
//
//Serial control parameters
int inByte = 0;         // incoming serial byte
boolean Addressed = false;
boolean dataComing = false;
boolean dataReceived = false;
boolean dataSending = false;
int byteIndex = -1;
int LLLL = 0; 
int LRC = 0;
int ACK_state = -4;
//
// Data message
int fieldType = 0;
int dataBytes[MAX_DATA_SIZE];
//
// Send buffer
int  outIDX = 0;
byte outBytes[MAX_DATA_SIZE]; 
byte outLRC = 0x00;
//
// RFID 
RFID rfid1(SS1_PIN, RST_PIN); 
RFID rfid2(SS2_PIN, RST_PIN);
boolean rfid1_exist  = false; // MEMBER
boolean rfid2_exist  = false; // BIKE
boolean member_exist = false; // ใช้สำหรับ Virtual Host
byte mbTag[5];
byte bikeTag[5];
//
// Member cards
const byte member_card [2][5]={
  {
    0x32,0x10,0x9A,0xB5,0x0D                                                                                                                                        }
  ,
  {
    0xC4,0xEB,0xCC,0xCF,0x2C                                                                                                                                        }
};
//
// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);
String msg1;
String msg2;
//
const String space             = "                ";
//
const String bike_empty        = "  SLOT EMPTY    ";
const String bike_filled       = " READY TO RENTAL";
//
const String transferring1     = "TRANSACTION     ";
const String transferring2     = "TRANSFFERING.   ";
const String transferring3     = "TRANSFFERING..  ";
const String transferring4     = "TRANSFFERING... ";
const String transferring5     = "TRANSFFERING....";
//
const String member_approved1  = "     MEMBER     ";
const String member_approved2  = "    APPROVED    ";
//
const String trans_failed1     = "* TRANSFERING  *";
const String trans_failed2     = "* FAILED!!!    *";
//
const String trans_timeout1    = "| TRANSFERING  |";
const String trans_timeout2    = "| FAILED!!!    |";
//
const String trans_reject1     = "   BALANCE      ";
const String trans_reject2     = "  NOT ENOUGH !  ";
//
const String member_invalid1   = "     MEMBER     ";
const String member_invalid2   = "   NOT VALID !  ";
//
const String try_again1        = "   PLEASE TRY   ";
const String try_again2        = "   AGAIN.       ";
//
const String done1             = "  THANK YOU     ";
const String done2             = "BE CAREFULL(^_^)";
//
// Blink
boolean Blink = false; // ใช้จำสถานะไฟกระพริบ
//
// Digitals / Sensors
const int bikeSensor = 2;
const int led1  = 12;
const int led2  = 13;
const int led3  = 14;
const int relay = 3;
//
boolean bikeParked = false;
//
// Timer tick
unsigned long previousMillis;
boolean       tick_state;
//
// Task : Ready to rental : ใช้ร่วมกับ rentaling = false
int taskRFID_CNT;
int taskLCDBike_CNT;
int taskBlink_CNT;
int taskACK_CNT;
int taskSensor_CNT;
int taskCLRDR_CNT;
//
int taskRent2_CNT;
int taskRent3_CNT;
//
// Task : Rentaling : ใช้ร่วมกับ rentaling = true และ rentalState = {1,2,3...}
boolean rentaling = false;
int rentalState = -1;
boolean mbSent = false;
//
//
/*--------------------------------------------------------------------------------------
 SETUP
 --------------------------------------------------------------------------------------*/
void setup()
{
  // Serial
  // start serial port at 9600 bps:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
  //
  // RFID
  SPI.begin(); 
  rfid1.init(); // Member
  rfid2.init(); // Bike
  //
  // LCD
  lcd.begin();
  lcd.backlight(); 
  //lcd.noBacklight();
  lcd.clear();
  //
  // OUTPUT PIN
  pinMode(relay, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(led3, OUTPUT);
  //
  // INPUT PIN
  pinMode(bikeSensor, INPUT);  
  // 
  digitalWrite(relay, RLY_OFF);
  //
  //
}
//
//
/*--------------------------------------------------------------------------------------
 === LOOP                                                                            ===
 --------------------------------------------------------------------------------------*/
void loop()
{  
  tick(); // นับเวลาตลอดเวลา
  serialReceive(); // จัดการข้อมูลที่เข้ามาทุกๆ Byte
  //
  // --- Timed tasks -------------------------
  taskRFID(tick_state && !rentaling);
  taskSensor(tick_state && !rentaling);
  taskLCD_Bike(tick_state && !rentaling);
  taskBlink(tick_state && !rentaling && SET_BLINK);
  taskACKTime(tick_state && dataSending);
  taskCLRDR(tick_state && dataReceived);
  //
  // --- Loop / interrupt tasks --------------
  // demo_stand();
  //
  // Serial respond
  sendStatus(dataReceived); // ข้อมูลใน incoming buffer ถูกล้างด้วย
  //
  // RENTALING
  // มีบัตร member : rentalState = 1
  doRent1(rfid1_exist && rfid2_exist && !rentaling); // ตรวจให้มีจักรยานอยู่ด้วยจริงๆ
  //
  // ตั้งเวลารอให้ master ติดต่อเข้ามาอ่านสถานะ : rentalState = 2
  doRent2(tick_state && rentaling && rentalState==2);
  //
  // ตั้งเวลารอให้ master แจ้งผลอนุมัติ/ปฎิเสธ : rentalState = 3
  doRent3(tick_state && rentaling && rentalState==3);
  //
  // ป้องกันกรณีลืมล้างสถานะ
  //forceClearBuff(dataReceived);
}
//
//
/*--------------------------------------------------------------------------------------
 PROCEDUES
 --------------------------------------------------------------------------------------*/
//
// .......................................................................................
// CPU Tick
// .......................................................................................
//
void tick()
{
  tick_state = false;
  if(millis() - previousMillis > INTERVAL)
  {       
    previousMillis = millis();    
    tick_state = true;
  }
}
//
// .......................................................................................
// TASKs
// .......................................................................................
//
// 1. อ่าน Member Tag [RFID-1] และ Bike Tag [RFID-2]
void taskRFID(boolean _flag)
{
  if(_flag)
  {
    taskRFID_CNT++;
    if(taskRFID_CNT == TIME_RFID_MB/INTERVAL) // Member
    {    
      if (rfid1.isCard()) 
      {
        if (rfid1.readCardSerial()) 
        {
          rfid1_exist=true;
          for(byte i=0;i<5;i++)
          {
            mbTag[i] = rfid1.serNum[i];
          }          
        } 
      }           
      rfid1.halt();
    } // if - อ่าน RFID-1 : MEMBER

    else if(taskRFID_CNT >= TIME_RFID_BIKE/INTERVAL) // Bike
    {  
      taskRFID_CNT = 0; // Reset counter <--
      if (rfid2.isCard()) 
      {
        if (rfid2.readCardSerial()) 
        {
          rfid2_exist=true;
          for(byte i=0;i<5;i++)
          {
            bikeTag[i] = rfid2.serNum[i];
          }
        }
        else
        {
          rfid2_exist=false;
          rfid1_exist=false;
        }
      }    
      else
      {
        rfid2_exist=false;
        rfid1_exist=false;
      }   
      rfid2.halt(); 
    } // if - อ่าน RFID-2 : Bike
  }
}
//
// 2. แสดงจักรยาน บน LCD ตรวจสอบด้วย RFID-2
//----------------------------------------------
void taskLCD_Bike(boolean _flag)
{
  if(_flag)
  {
    taskLCDBike_CNT++;
    if(taskLCDBike_CNT > TIME_LCD_BIKE/INTERVAL)
    {    
      taskLCDBike_CNT = 0; // Reset counter <--
      if(rfid2_exist)
      {
        //lcd.begin(); lcd.clear();        
        msg1=bike_filled;
        msg2=space;
        lcd.setCursor(0,0); // (cur,line)
        lcd.print(msg1);
        lcd.setCursor(0,1);
        lcd.print(msg2);    
      }
      else
      {
        //lcd.begin(); lcd.clear();
        msg1=bike_empty;
        msg2=space;
        lcd.setCursor(0,0); // (cur,line)
        lcd.print(msg1);
        lcd.setCursor(0,1);
        lcd.print(msg2);  
      }
    } 
  }  
}
//
// 3. LCD BLINK-BLINK
void taskBlink(boolean _flag)
{
  if(_flag)
  {
    taskBlink_CNT++;
    if(taskBlink_CNT > TIME_BLINK/INTERVAL)
    {
      taskBlink_CNT = 0; // Reset counter <-- 
      if(!Blink)
      {      
        lcd.backlight();
        Blink=true;
      }
      else
      {      
        lcd.noBacklight();
        Blink=false;
      }
    }
  }  
}
//
// 4. ACK TIME
void taskACKTime(boolean _flag)
{
  if(_flag)
  {    
    taskACK_CNT++;
    if(taskACK_CNT > TIME_ACK/INTERVAL)
    {
      taskACK_CNT = 0; // Reset counter <--
      ACK_state = -4;
      dataSending = false;
      dataComing = false;
      dataReceived = false;
#if TEST_LOGIC<PRINT_LOG_ACK
      Serial.println("ACK timed out!");
#endif
    }
  }
}
//
// 5. Bike sensor
void taskSensor(boolean _flag)
{
  if(_flag)
  {
    taskSensor_CNT++;
    if(taskSensor_CNT > TIME_SENSOR/INTERVAL)
    {
      taskSensor_CNT = 0; // Reset conter <--
      sensorByte = 0x00; 
      bikeParked = rfid2_exist;
      if(bikeParked) sensorByte  = sensorByte | BIKE_SEN_BIT; // Bike sensor        
    }
  }
}
//
// 6. Clear data received
void taskCLRDR(boolean _flag)
{
  if(_flag)
  {    
    taskCLRDR_CNT++;
    if(taskCLRDR_CNT > TIME_ACK/INTERVAL)
    {
      taskCLRDR_CNT = 0; // Reset counter <--
      clearReceiving();
#if TEST_LOGIC<PRINT_LOG
      Serial.println("Data expired!");
#endif
    }
  }
}
//
//
// .......................................................................................
// Interruption
// .......................................................................................
//
// rentalState = 1 : มีบัตร member ไปขั้นตอนถัดไป rentalState = 2
void doRent1(boolean _flag)
{
  if(_flag) 
  {
    rentaling = true;
    rentalState = 2;
    // แสดงข้อความ กำลังส่งข้อมูล...
    msg1=transferring1;
    msg2=transferring2;
    lcd.setCursor(0,0); // (cur,line)
    lcd.print(msg1);
    lcd.setCursor(0,1);
    lcd.print(msg2); 
  }
}
//
// ตั้งเวลารอให้ master ติดต่อเข้ามาอ่านสถานะ : rentalState = 2
void doRent2(boolean _flag)
{
  if(_flag)
  {
    taskRent2_CNT++;
    if((taskRent2_CNT > TIME_RENT2/INTERVAL) | 
      ((ACK_state==NAK | ACK_state==-1) & mbSent))
    {
      taskRent2_CNT = 0; // Reset <--
      //
      rentaling = false;
      rentalState = -1;
      mbSent = false;
      //      
      clearReceiving();
      //
      taskRFID_CNT = 0;
      taskLCDBike_CNT = 0;
      taskBlink_CNT = 0;
      taskACK_CNT = 0;
      taskSensor_CNT = 0;
      //
      taskRent2_CNT = 0;
      //
      bikeParked = false;
      rfid1_exist = false;
      rfid2_exist = false;
      //
      // แสดงผลความผิดพลาด และหยุดไว้สักครู่
      msg1=trans_failed1;
      msg2=trans_failed2;
      lcd.setCursor(0,0); // (cur,line)
      lcd.print(msg1);
      lcd.setCursor(0,1);
      lcd.print(msg2); 
      delay(2000);
      msg1=try_again1;
      msg2=try_again2;
      lcd.setCursor(0,0); // (cur,line)
      lcd.print(msg1);
      lcd.setCursor(0,1);
      lcd.print(msg2); 
      delay(2000);
    }
    else if(ACK_state==ACK & mbSent)
    {   
      mbSent = false; // หมดหน้าที่แล้ว   
      rentalState = 3; // ไปขั้นตอนถัดไป rentalState = 3 : ตั้งเวลารอให้ master แจ้งผลอนุมัติ/ปฎิเสธ
      taskRent3_CNT = 0;
      //
      msg2=transferring3;
      lcd.setCursor(0,1);
      lcd.print(msg2); 
    }
  }
}
//
// ตั้งเวลารอให้ master แจ้งผลอนุมัติ/ปฎิเสธ : rentalState = 3
void doRent3(boolean _flag)
{
  if(_flag)
  {
    taskRent3_CNT++;
    if((taskRent3_CNT > TIME_RENT3/INTERVAL) | 
      (dataReceived & (fieldType==REJECTED_CMD | fieldType==INVALID_CMD)))
    {
      taskRent3_CNT = 0; // Reset <--
      //
      // แสดงผลความผิดพลาด และหยุดไว้สักครู่
      if(fieldType==REJECTED_CMD)
      {
        msg1=trans_reject1;
        msg2=trans_reject2;
        lcd.setCursor(0,0); // (cur,line)
        lcd.print(msg1);
        lcd.setCursor(0,1);
        lcd.print(msg2); 
      }
      else if(fieldType==INVALID_CMD)
      {
        msg1=member_invalid1;
        msg2=member_invalid2;
        lcd.setCursor(0,0);//cur,line
        lcd.print(msg1);
        lcd.setCursor(0,1);
        lcd.print(msg2);        
      }
      else // Timed out
      {
        msg1=trans_timeout1;
        msg2=trans_timeout2;
        lcd.setCursor(0,0); // (cur,line)
        lcd.print(msg1);
        lcd.setCursor(0,1);
        lcd.print(msg2); 
      }
      //
      // Clear all parameters
      rentaling = false;
      rentalState = -1;
      mbSent = false;
      //      
      clearReceiving();
      //
      taskRFID_CNT = 0;
      taskLCDBike_CNT = 0;
      taskBlink_CNT = 0;
      taskACK_CNT = 0;
      taskSensor_CNT = 0;
      //
      taskRent2_CNT = 0;
      taskRent3_CNT = 0;
      //
      bikeParked = false;
      rfid1_exist = false;
      rfid2_exist = false;
      //
      // Say thanks
      delay(2000);
      msg1=try_again1;
      msg2=try_again2;
      lcd.setCursor(0,0); // (cur,line)
      lcd.print(msg1);
      lcd.setCursor(0,1);
      lcd.print(msg2); 
      delay(2000);
    }
    else if(dataReceived & fieldType==APPROVED_CMD)
    {      
      rentalState = 4; // ไปขั้นตอนถัดไป rentalState = 4 : ตั้งเวลารอให้ master แจ้งผลอนุมัติ/ปฎิเสธ
      //
      msg1=member_approved1;
      msg2=member_approved2;
      lcd.setCursor(0,0); // (cur,line)
      lcd.print(msg1);
      lcd.setCursor(0,1);
      lcd.print(msg2); 
      //
      // UNLOCK
      digitalWrite(relay, RLY_ON);
      delay(1000); 
      digitalWrite(relay, RLY_OFF);
      delay(1000);
      //
      // SAY THANKS.
      msg1=done1;
      msg2=done2;
      lcd.setCursor(0,0);//cur,line
      lcd.print(msg1);
      lcd.setCursor(0,1);
      lcd.print(msg2);   
      delay(2000); 
      // 
      // CLEAR
      rentaling = false;
      rentalState = -1;
      mbSent = false;
      //      
      clearReceiving();
      //
      taskRFID_CNT = 0;
      taskLCDBike_CNT = 0;
      taskBlink_CNT = 0;
      taskACK_CNT = 0;
      taskSensor_CNT = 0;
      //
      taskRent2_CNT = 0;
      taskRent3_CNT = 0;
      //
      bikeParked = false;
      rfid1_exist = false;
      rfid2_exist = false;
    }
  }
}
//
// .......................................................................................
// Serial event
// .......................................................................................
//
// ล้างตัวแปรควบคุมสำหรับรับค่าจาก Serial
void resetReceivingCTRL()
{
  byteIndex = 0;
  fieldType = 0x00;
  LLLL = 0;
  LRC = 0;
#if TEST_LOGIC<PRINT_LOG
  Serial.println("Data receiving controls are reset."); 
#endif  
}
//
// ล้างตัวแปรสถานะการรับค่าจาก Serial
void clearReceiving()
{
  Addressed = false;
  dataComing = false;
  dataReceived = false;
  resetReceivingCTRL();
}
//
// ส่งคำปฏิเสธ
void sendNAK()
{
  Serial.write(NAK);
#if TEST_LOGIC<PRINT_LOG
  Serial.println("[NAK]");
#endif
}
//
// ส่งคำเตือนไม่ว่าง
void sendBIZ()
{
  Serial.write(BIZ);
#if TEST_LOGIC<PRINT_LOG
  Serial.println("[BIZ]");
#endif
}
//
// Event สำหรับรับค่าจาก Serial ทุกๆ byte
void serialReceive()
{  
  if (Serial.available() > 0) // if we get a valid byte, go ahead:
  {    
    inByte = Serial.read(); // get incoming byte:
    //
#if TEST_LOGIC<PRINT_LOG
    Serial.print("In-byte = 0x");
    Serial.println(inByte, HEX);
#endif
    //
    if(!dataComing)
    {
      //
      // รับ ACK / NAK / OTHERS
      if(dataSending)
      {
        dataSending = false;
        if(inByte==ACK) ACK_state = ACK;
        else if(inByte==NAK) ACK_state = NAK;
        else ACK_state = -1;        
#if TEST_LOGIC<PRINT_LOG_ACK
        Serial.println("ACK received.");
        Serial.print("ACK = ");
        Serial.println(ACK_state, HEX);
#endif
        return;
      }
      //
      // -----------------------------------
      // Wait for data executation. BUSY!
      // -----------------------------------
      if(dataReceived && inByte==SLOT_NBR) 
      {
#if TEST_LOGIC<PRINT_LOG
        Serial.println("Busy!");
#endif
        sendBIZ();
      }
      if(dataReceived) return;

      // -----------------------------------
      // Addressing.
      // Reset for data receiving
      // -----------------------------------
      if(!Addressed && inByte==SLOT_NBR)
      {
        Addressed = true;

#if TEST_LOGIC<PRINT_LOG
        Serial.print("SLOT number = 0x");
        Serial.println(inByte, HEX);
        Serial.println("Right SLOT."); 
#endif
        return;
      }

      // ---------------------------------
      // Failed! STX / SEPARATOR
      // ---------------------------------
      if(Addressed && (inByte!=SEP))
      {
        Addressed = false;
        dataReceived = false;
        dataComing = false;

#if TEST_LOGIC<PRINT_LOG
        Serial.print("SEP = 0x");
        Serial.println(inByte, HEX);
        Serial.println("SEP failed!");
#endif
        return;
      }

      // ---------------------------------
      // COMMIT STX / SEPARATOR
      // ---------------------------------
      if(Addressed && (inByte==SEP))
      {
#if TEST_LOGIC<PRINT_LOG
        Serial.print("SEP = 0x");
        Serial.println(inByte, HEX);
#endif
        resetReceivingCTRL();
        Addressed = false;
        dataReceived = false;
        dataComing = true;

        return;
      }

    } // <-- if(!dataComing)

    // ------------------------------------
    // Message coming...
    // These logic has no return-command.
    // ------------------------------------
    if(dataComing)
    {      
      if(byteIndex==0) // Field type ...................
      {     
        LRC = LRC^SLOT_NBR;
        LRC = LRC^SEP;
        fieldType = inByte;
        LRC = LRC^fieldType;

#if TEST_LOGIC<PRINT_LOG
        Serial.print("Field type = 0x");
        Serial.println(fieldType, HEX);
        Serial.print("LRC = 0x");
        Serial.println(LRC, HEX);
#endif
      }

      else if(byteIndex==1) // LLLL ...................
      {
        LLLL = inByte;
        LRC = LRC^LLLL;

#if TEST_LOGIC<PRINT_LOG
        Serial.print("LLLL = ");
        Serial.println(LLLL);
        Serial.print("LRC = 0x");
        Serial.println(LRC, HEX);
#endif
      } 

      else if(1<byteIndex && byteIndex<LLLL+2) // Data ...................
      {
        int idx = byteIndex - 2;
        dataBytes[idx] = inByte;
        LRC = LRC^dataBytes[idx];
#if TEST_LOGIC<PRINT_LOG
        Serial.print("data [");
        Serial.print(idx);
        Serial.print("] = 0x");
        Serial.println(dataBytes[idx], HEX);
        Serial.print("LRC = 0x");
        Serial.println(LRC, HEX);
#endif
      }   
      else if(byteIndex==LLLL+2) // ETX ...................
      {
#if TEST_LOGIC<PRINT_LOG
        Serial.print("Byte index = ");
        Serial.println(byteIndex);
#endif
        if(inByte==ETX) // ETX came. <--- Good : go ahead
        {
          LRC = LRC^ETX;
#if TEST_LOGIC<PRINT_LOG
          Serial.println("ETX came.");
          Serial.print("LRC = 0x");
          Serial.println(LRC, HEX);
#endif
        }
        else // ETX failed. <--- Reject : reset
        {
#if TEST_LOGIC<PRINT_LOG
          Serial.println("ETX failed!");
#endif
          clearReceiving();
          sendNAK();
        }               
      }

      else if(byteIndex==LLLL+3) // Incoming LRC ...................
      {
        if(LRC==inByte) // LRC commited. <--- Good : go ahead
        {
#if TEST_LOGIC<PRINT_LOG
          Serial.println("LRC commited.");
#endif
          Addressed = false;
          dataComing = false;
          dataReceived = true;
          taskCLRDR_CNT = 0; // Reset เพื่อนับเวลา ออก
          
#if TEST_LOGIC<PRINT_LOG
          Serial.println("Message completed."); 
#endif
          Serial.write(ACK);
#if TEST_LOGIC<PRINT_LOG
          Serial.println("[ACK]");
#endif
        }
        else // LRC failed. <--- Reject : reset
        {
#if TEST_LOGIC<PRINT_LOG
          Serial.println("LRC failed.");
#endif
          clearReceiving();
          sendNAK();
        }
      }      

      // Update index and check its limit  ...................
      byteIndex++;
      if(byteIndex>MAX_DATA_SIZE) clearReceiving(); // Clear all controls

    } // Message coming...
  } // Serial avariable...
}
//
// .......................................................................................
// Message execute
// .......................................................................................
//
// ส่งสถานะแทนจอด
void sendStatus(boolean _flag)
{
  if(_flag)
  {
    if(fieldType==MASTER_CONTACT)
    {
      //      
      clearReceiving();
      //
      outLRC = 0x00;
      // SLOT number
      outIDX = 0; 
      outBytes[outIDX] = SLOT_NBR;
      outLRC = outLRC^outBytes[outIDX];
#if TEST_LOGIC<PRINT_LOG
      Serial.print("outBytes[");
      Serial.print(outIDX);
      Serial.print("] = 0x");
      Serial.println(outBytes[outIDX], HEX);
      Serial.print("LRC = 0x");
      Serial.println(outLRC, HEX);
#endif      
      // SEP
      outIDX++;
      outBytes[outIDX] = SEP;
      outLRC = outLRC^outBytes[outIDX];
#if TEST_LOGIC<PRINT_LOG
      Serial.print("outBytes[");
      Serial.print(outIDX);
      Serial.print("] = 0x");
      Serial.println(outBytes[outIDX], HEX);
      Serial.print("LRC = 0x");
      Serial.println(outLRC, HEX);
#endif      
      // '?' : Status
      outIDX++;
      outBytes[outIDX] = MASTER_CONTACT;
      outLRC = outLRC^outBytes[outIDX];
#if TEST_LOGIC<PRINT_LOG
      Serial.print("outBytes[");
      Serial.print(outIDX);
      Serial.print("] = 0x");
      Serial.println(outBytes[outIDX], HEX);
      Serial.print("LRC = 0x");
      Serial.println(outLRC, HEX);
#endif              
      // LLLL
      outIDX++;
      outBytes[outIDX] = 1;
      if(rfid2_exist) // Bike tag
      { 
        outBytes[outIDX] = outBytes[outIDX] + 5; // Bike tag
        if(rfid1_exist) 
          outBytes[outIDX] = outBytes[outIDX] + 5; // Member tag ต้องมีข้อมูล Bike Tag ด้วย       
      }
      outLRC = outLRC^outBytes[outIDX];
#if TEST_LOGIC<PRINT_LOG
      Serial.print("outBytes[");
      Serial.print(outIDX);
      Serial.print("] = 0x");
      Serial.println(outBytes[outIDX], HEX);
      Serial.print("LRC = 0x");
      Serial.println(outLRC, HEX);
#endif      
      // Byte sensor
      outIDX++;
      outBytes[outIDX] = sensorByte;
      outLRC = outLRC^outBytes[outIDX];
#if TEST_LOGIC<PRINT_LOG
      Serial.print("outBytes[");
      Serial.print(outIDX);
      Serial.print("] = 0x");
      Serial.println(outBytes[outIDX], HEX);
      Serial.print("LRC = 0x");
      Serial.println(outLRC, HEX);
#endif      
      // Bike tag
      if(rfid2_exist)
      {
        for(int itag=0;itag<5;itag++)
        {
          outIDX++;
          outBytes[outIDX] = bikeTag[itag];
          outLRC = outLRC^outBytes[outIDX];
#if TEST_LOGIC<PRINT_LOG
          Serial.print("outBytes[");
          Serial.print(outIDX);
          Serial.print("] = 0x");
          Serial.println(outBytes[outIDX], HEX);
          Serial.print("LRC = 0x");
          Serial.println(outLRC, HEX);
#endif      
        }
        // Member tag ต้องมี Bike tag ด้วย
        mbSent = false;
        if(rfid1_exist)
        {
          for(int itag=0;itag<5;itag++)
          {
            outIDX++;
            outBytes[outIDX] = mbTag[itag];
            outLRC = outLRC^outBytes[outIDX];

#if TEST_LOGIC<PRINT_LOG
            Serial.print("outBytes[");
            Serial.print(outIDX);
            Serial.print("] = 0x");
            Serial.println(outBytes[outIDX], HEX);
            Serial.print("LRC = 0x");
            Serial.println(outLRC, HEX);
#endif      
          }
          mbSent = true;
        }
      }
      //
      // ETX
      outIDX++;
      outBytes[outIDX] = ETX;
      outLRC = outLRC^outBytes[outIDX];
#if TEST_LOGIC<PRINT_LOG
      Serial.print("outBytes[");
      Serial.print(outIDX);
      Serial.print("] = 0x");
      Serial.println(outBytes[outIDX], HEX);
      Serial.print("LRC = 0x");
      Serial.println(outLRC, HEX);
#endif      
      // LRC
      outIDX++;
      outBytes[outIDX] = outLRC;
#if TEST_LOGIC<PRINT_LOG
      Serial.print("outBytes[");
      Serial.print(outIDX);
      Serial.print("] = 0x");
      Serial.println(outBytes[outIDX], HEX);
      Serial.print("LRC = 0x");
      Serial.println(outLRC, HEX);
#endif      
      outIDX++;
      Serial.write(outBytes, outIDX);
      taskACK_CNT = 0; // Reset ack time out counter
      ACK_state = -4;
      dataSending = true;      
    }
  }
}





































































