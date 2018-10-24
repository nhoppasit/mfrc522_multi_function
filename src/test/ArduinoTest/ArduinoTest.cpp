#include <arduino.h>
#include <MFRC522.h>

/*--------------------------------------------------------------------------------------
 CODE CONTROL
 --------------------------------------------------------------------------------------*/
#define TEST_LOGIC 0
#define PRINT_TIME 0
#define PRINT_LOG 1
#define PRINT_LOG_ACK 0
#define SET_BLINK false
//
/*--------------------------------------------------------------------------------------
 INCLUDE
 --------------------------------------------------------------------------------------*/
// #include <SPI.h>
// #include <RFID.h>
// #include <Wire.h>
// #include <LiquidCrystal_I2C.h>
//
/*--------------------------------------------------------------------------------------
 DEFINES
 --------------------------------------------------------------------------------------*/
#define MAX_DATA_SIZE 64 // Serial data size
//
#define STX 0x02 // Start text charactor
#define SEP 0x1C // Field separator charactor
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
// CPU tick
#define INTERVAL         50 // ms
//
// TASK TIMING
#define TIME_BLINK     1000 // ms
#define TIME_CLEAR_DR  1000 // ms
#define TIME_ACK       1000 // ms
//
// Device number
const char txType[] = "MF";
const char txDest[] = "0001";
const char txSource[] = "0000";
//
/*--------------------------------------------------------------------------------------
 VARIABLES
 --------------------------------------------------------------------------------------*/
// Transport header
char transType[2];
char transDestination[4];
char transSource[4];
//
// Presentation header
char formatVersion;
char responseIndicator;
char transactionCode[2];
char responseCode[2];
char moreIndicator;
//
//Serial control parameters
int inByte = 0;         // incoming serial byte
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
// Blink
boolean Blink = false; // ใช้จำสถานะไฟกระพริบ
//
// Timer tick
unsigned long previousMillis;
boolean       tick_state;
//
int taskBlink_CNT;
int taskACK_CNT;
int taskCLRDR_CNT;
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
}
//
//
/*--------------------------------------------------------------------------------------
 === LOOP                                                                            ===
 --------------------------------------------------------------------------------------*/
void tick();
void serialReceive();
void taskACKTime(boolean);
void taskCLRDR(boolean);
void validateTxType(boolean);
void validateTxDest(boolean);
void clearReceiving();
void loop()
{
	tick(); // นับเวลาตลอดเวลา
	serialReceive(); // จัดการข้อมูลที่เข้ามาทุกๆ Byte
	//
	// --- Communication timed tasks -------------------------
	taskACKTime(tick_state && dataSending);
	taskCLRDR(tick_state && dataReceived);
	//
	// Serial respond functions
	// sendStatus(dataReceived); // ข้อมูลใน incoming buffer ถูกล้างด้วย
	validateTxType(dataReceived);
	validateTxDest(dataReceived);
	//
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
/* // 1. อ่าน Member Tag [RFID-1] และ Bike Tag [RFID-2]
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
} */
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
        //lcd.backlight();
        Blink=true;
      }
      else
      {
        //lcd.noBacklight();
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
//--------------------------------------------------
// Event สำหรับรับค่าจาก Serial ทุกๆ byte
//--------------------------------------------------
void serialReceive()
{
	if (Serial.available() > 0) // if we get a valid byte, go ahead:
	{
		inByte = Serial.read(); // get incoming byte:
#if TEST_LOGIC<PRINT_LOG
		Serial.print("In-byte = 0x");
		Serial.println(inByte, HEX);
#endif
		//
		// ------------------------------------------------
		// Not data comming or receiving
		//    - sending
		//    - received
		//    - wait new message
		// ------------------------------------------------
		if(!dataComing)
		{
			//
			// -----------------------------------------
			// รับ ACK / NAK / OTHERS
			// -----------------------------------------
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
			//
			// -----------------------------------
			// Wait for data executation. BUSY!
			// -----------------------------------
			if(dataReceived && inByte==STX)
			{
#if TEST_LOGIC<PRINT_LOG
				Serial.println("Busy!");
#endif
				sendBIZ();
			}
			//
			//
			// -----------------------------------
			// Jump out to process the data
			// -----------------------------------
			if(dataReceived) return;
			//
			//
			// -----------------------------------
			// After process the received data.
			// Check STX.
			// Reset for data receiving
			// -----------------------------------
			if(inByte==STX)
			{
#if TEST_LOGIC<PRINT_LOG
				Serial.print("STX = 0x");
				Serial.println(inByte, HEX);
#endif
				resetReceivingCTRL();
				dataReceived = false;
				dataComing = true; // <-- Only one logic turn to receive message
				return;
			}
			//
		} // <-- End if(!dataComing)
		//
		// ------------------------------------
		// Message coming...
		// These logic has no return-command.
		// ------------------------------------
		if(dataComing)
		{
			if(0<=byteIndex && byteIndex<=1) // LLLL ...................
			{
				if(byteIndex==0) LLLL = inByte*10;
				else LLLL = LLLL + inByte;
				LRC = LRC^LLLL;
#if TEST_LOGIC<PRINT_LOG
				Serial.print("LLLL = ");
				Serial.println(LLLL);
				Serial.print("LRC = 0x");
				Serial.println(LRC, HEX);
#endif
			}
			else if(2<=byteIndex && byteIndex<=3) // transType: Transport header type ...................
			{
				transType[byteIndex-2] = (char)inByte;
				LRC = LRC^inByte;
#if TEST_LOGIC<PRINT_LOG
				Serial.print("transType[");
				Serial.print(byteIndex-2, DEC);
				Serial.print("] = ");
				Serial.println(transType[byteIndex-2]);
				Serial.print("LRC = 0x");
				Serial.println(LRC, HEX);
#endif
			}
			else if(4<=byteIndex && byteIndex<=7) // transDestination: Transport destination ...................
			{
				transDestination[byteIndex-4] = (char)inByte;
				LRC = LRC^inByte;
#if TEST_LOGIC<PRINT_LOG
				Serial.print("transDestination[");
				Serial.print(byteIndex-4, DEC);
				Serial.print("] = ");
				Serial.println(transDestination[byteIndex-4]);
				Serial.print("LRC = 0x");
				Serial.println(LRC, HEX);
#endif
			}
			else if(8<=byteIndex && byteIndex<=11) // transSource: Transport source ...................
			{
				transSource[byteIndex-8] = (char)inByte;
				LRC = LRC^inByte;
#if TEST_LOGIC<PRINT_LOG
				Serial.print("transSource[");
				Serial.print(byteIndex-8, DEC);
				Serial.print("] = ");
				Serial.println(transSource[byteIndex-8]);
				Serial.print("LRC = 0x");
				Serial.println(LRC, HEX);
#endif
			}

			/* //$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
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
			} */
			else if(11<byteIndex && byteIndex<LLLL+12) // Data ...................
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
			else if(byteIndex==LLLL+12) // ETX ...................
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
			else if(byteIndex==LLLL+13) // Incoming LRC ...................
			{
				if(LRC==inByte) // LRC commited. <--- Good : go ahead
				{
#if TEST_LOGIC<PRINT_LOG
					Serial.println("LRC commited.");
#endif
					dataComing = false;
					dataReceived = true;
					taskCLRDR_CNT = 0; // Reset เพื่อนับเวลา ออก
#if TEST_LOGIC<PRINT_LOG
					Serial.println("Message completed.");
#endif
					Serial.write(ACK); // <-- Send ACK
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
			//
		} // End Message coming...
	} // End Serial avariable...
}
//
// .......................................................................................
// Message execute
// .......................................................................................
//
void validateTxType(boolean _flag)
{
	if(_flag)
	{
		if(!(transType[0]==txType[0] &&
			 transType[1]==txType[1]))
		{
			clearReceiving();
#if TEST_LOGIC<PRINT_LOG
			Serial.println("WRONG Transaction header type!");
			Serial.println("Data terminated.");
#endif
		}
	}
}
void validateTxDest(boolean _flag)
{
	if(_flag)
	{
		if(!(transDestination[0]==txDest[0] &&
			 transDestination[1]==txDest[1] &&
			 transDestination[2]==txDest[2] &&
			 transDestination[3]==txDest[3]))
		{
			clearReceiving();
#if TEST_LOGIC<PRINT_LOG
			Serial.println("WRONG Transaction destination!");
			Serial.println("Data terminated.");
#endif
		}
	}
}
//
/* // ส่งสถานะ
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
 */


