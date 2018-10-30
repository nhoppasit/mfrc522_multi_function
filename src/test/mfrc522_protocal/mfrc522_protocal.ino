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
// Transaction code [2-BYTE] meaning
#define GENERIC_QUERY_1      '?'   // '?' = 0x3F : Send general status.
#define GENERIC_QUERY_2      '?'   
//
// Field type [2-BYTE] meaning
#define GENERIC_AGRUMENT_1   'G'   // '?' = 0x3F : Send slot status.
#define GENERIC_AGRUMENT_2   'A'   // '?' = 0x3F : Send slot status.
//
//
// CPU tick
#define INTERVAL         50 // ms
//
// TASK TIMING
#define TIME_BLINK     1000 // ms
#define TIME_CLEAR_DR  1000 // ms
#define TIME_ACK       1000 // ms
//
// Application code
#define TX_TYPE_1		'M'  // MIRROR FOUNDATION: MF
#define TX_TYPE_2		'F'  
//
// Device code
#define TX_DESTINATION_1		'0'  // TRANSACTION DESTINATION: 0001
#define TX_DESTINATION_2		'0'  
#define TX_DESTINATION_3		'0'  
#define TX_DESTINATION_4		'1'  
//
// RESPONSE
#define NEED_RESPONSE		'0' // require a response
#define A_RESPONSE			'1' // response 
#define NOT_RESPONSE		'2' // not response
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
void loop()
{  
	tick(); // นับเวลาตลอดเวลา
	serialReceive(); // จัดการข้อมูลที่เข้ามาทุกๆ Byte
	//
	// --- Communication timed tasks -------------------------  
	taskACKTime(tick_state && dataSending);
	taskCLRDR(tick_state && dataReceived);
	taskCLRDR(tick_state && dataComing);
	//
	// Message validation functions
	validateTxType(dataReceived);
	validateTxDest(dataReceived);
	validateResponse(dataReceived);
	//
	// Serial respond functions
	sendGenericQuery(dataReceived); // ข้อมูลใน incoming buffer ถูกล้างด้วย
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
			taskCLRDR_CNT = 0; // Clear task when data comming. If wrong length, it increses.
			
			if(0<=byteIndex && byteIndex<=1) // LLLL ...................
			{
				if(byteIndex==0) LLLL = inByte*10;
				else LLLL = LLLL + inByte;
				LRC = LRC^LLLL;
#if TEST_LOGIC<PRINT_LOG
				Serial.print("LLLL = ");
				Serial.println(LLLL);
#endif
			} 
			else if(2<=byteIndex && byteIndex<=3 && (byteIndex-2)<=LLLL) // transType: Transport header type ...................
			{     
				transType[byteIndex-2] = (char)inByte;
				LRC = LRC^inByte;
#if TEST_LOGIC<PRINT_LOG
				Serial.print("transType[");
				Serial.print(byteIndex-2, DEC);
				Serial.print("] = ");
				Serial.println(transType[byteIndex-2]);
#endif
			}
			else if(4<=byteIndex && byteIndex<=7 && (byteIndex-2)<=LLLL) // transDestination: Transport destination ...................
			{     
				transDestination[byteIndex-4] = (char)inByte;
				LRC = LRC^inByte;
#if TEST_LOGIC<PRINT_LOG
				Serial.print("transDestination[");
				Serial.print(byteIndex-4, DEC);
				Serial.print("] = ");
				Serial.println(transDestination[byteIndex-4]);
#endif
			}
			else if(8<=byteIndex && byteIndex<=11 && (byteIndex-2)<=LLLL) // transSource: Transport source ...................
			{     
				transSource[byteIndex-8] = (char)inByte;
				LRC = LRC^inByte;
#if TEST_LOGIC<PRINT_LOG
				Serial.print("transSource[");
				Serial.print(byteIndex-8, DEC);
				Serial.print("] = ");
				Serial.println(transSource[byteIndex-8]);
#endif
			}
			else if(12==byteIndex) // formatVersion: Presentation header format version ...................
			{     
				formatVersion = (char)inByte;
				LRC = LRC^inByte;
#if TEST_LOGIC<PRINT_LOG
				Serial.print("Format Version = ");
				Serial.println(formatVersion);
#endif
			}
			else if(13==byteIndex) // responseIndicator: Presentation header request-response indicator  ...................
			{     
				responseIndicator = (char)inByte;
				LRC = LRC^inByte;
#if TEST_LOGIC<PRINT_LOG
				Serial.print("Request-response indicator = ");
				Serial.println(responseIndicator);
#endif
			}
			else if(14<=byteIndex && byteIndex<=15) // transactionCode: Presentation header transaction Code ...................
			{     
				transactionCode[byteIndex-14] = (char)inByte;
				LRC = LRC^inByte;
#if TEST_LOGIC<PRINT_LOG
				Serial.print("Transaction Code[");
				Serial.print(byteIndex-14, DEC);
				Serial.print("] = ");
				Serial.println(transactionCode[byteIndex-14]);
#endif
			}
			else if(16<=byteIndex && byteIndex<=17) // responseCode: Presentation header response Code ...................
			{     
				responseCode[byteIndex-16] = (char)inByte;
				LRC = LRC^inByte;
#if TEST_LOGIC<PRINT_LOG
				Serial.print("Response Code[");
				Serial.print(byteIndex-16, DEC);
				Serial.print("] = ");
				Serial.println(responseCode[byteIndex-16]);
#endif
			}
			else if(18==byteIndex) // moreIndicator: Presentation header more indicator  ...................
			{     
				moreIndicator = (char)inByte;
				LRC = LRC^inByte;
#if TEST_LOGIC<PRINT_LOG
				Serial.print("More indicator = ");
				Serial.println(moreIndicator);
#endif
			}
			else if(19==byteIndex) // Field separator: Presentation header field separator  ...................
			{     
				if(inByte!=SEP) // SEP failed!
				{
#if TEST_LOGIC<PRINT_LOG
					Serial.println("SEP failed!");
#endif
					clearReceiving();
					sendNAK();
				}     
#if TEST_LOGIC<PRINT_LOG
				else
				{
					Serial.println("SEP came.");
				}
#endif				
				
			}			
			//
			//
			// ------------------- FIELD ELEMENT ----------------------
			else if(19<byteIndex && byteIndex<LLLL+20) // Data ...................
			{
				int idx = byteIndex - 2;
				dataBytes[idx] = inByte;
				LRC = LRC^dataBytes[idx];
#if TEST_LOGIC<PRINT_LOG
				Serial.print("data [");
				Serial.print(idx);
				Serial.print("] = 0x");
				Serial.println(dataBytes[idx], HEX);
#endif
			}  
			//
			//
			else if(byteIndex==LLLL+20) // ETX ...................
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
			else if(byteIndex==LLLL+21) // Incoming LRC ...................
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
			else if(LLLL<(byteIndex-2))
			{
#if TEST_LOGIC<PRINT_LOG
				Serial.println("Message length failed.");
#endif
				clearReceiving();
				sendNAK();
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
// Message validation
// .......................................................................................
//
void validateTxType(boolean _flag)
{
	if(_flag)
	{		
		if(!(transType[0]==TX_TYPE_1 && 
			 transType[1]==TX_TYPE_2)) 
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
		if(!(transDestination[0]==TX_DESTINATION_1 && 
			 transDestination[1]==TX_DESTINATION_2 && 
			 transDestination[2]==TX_DESTINATION_3 && 
			 transDestination[3]==TX_DESTINATION_4)) 
		{
			clearReceiving();
#if TEST_LOGIC<PRINT_LOG
			Serial.println("WRONG Transaction destination!");
			Serial.println("Data terminated.");
#endif      
		}
	}
}
void validateResponse(boolean _flag)
{
	if(_flag)
	{		
		if(responseIndicator==NOT_RESPONSE) 
		{
			clearReceiving();
#if TEST_LOGIC<PRINT_LOG
			Serial.println("This message is a request message which does not require a response.");
#endif      
		}
		if(responseIndicator==A_RESPONSE) 
		{
			clearReceiving();
#if TEST_LOGIC<PRINT_LOG
			Serial.println("This message is a response message.");
#endif      
		}
	}
}
// ----------------------------------------------------------------------
// RESPONSE FUNCTIONS
// ----------------------------------------------------------------------
//
// Generic query
void sendGenericQuery(boolean _flag)
{
  if(_flag)
  {
    if(transactionCode[0]==GENERIC_QUERY_1 && transactionCode[0]==GENERIC_QUERY_2 &&
		responseIndicator==NEED_RESPONSE)
    {
      //      
      clearReceiving();
      //
      outLRC = 0x00;
      // SLOT number
      outIDX = 0; 
      outBytes[outIDX] = TX_DESTINATION_1;
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
      outBytes[outIDX] = GENERIC_QUERY_1;
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
      if(1) // Bike tag
      { 
        outBytes[outIDX] = outBytes[outIDX] + 5; // Bike tag
        if(1) 
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
      outBytes[outIDX] = '1';
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
      if(1)
      {
        for(int itag=0;itag<5;itag++)
        {
          outIDX++;
          outBytes[outIDX] = 'X';
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
        if(1)
        {
          for(int itag=0;itag<5;itag++)
          {
            outIDX++;
            outBytes[outIDX] = 'Y';
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


