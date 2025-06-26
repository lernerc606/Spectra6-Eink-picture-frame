/*****************************************************************************
* | File        :   EPD_13in3e.cpp
* | Author      :   Originally by Waveshare team
* | Modified by :   lernerc606
* | Function    :   Electronic paper display driver
* | Description :   Modified to load images from SD card and display them;
* |                 tested with FireBeetle 2 ESP32-C6.
* ----------------
* | Original Version:   V1.0
* | Original Date   :   2018-11-29
* | Modified Date   :   2025-06-26
*
* Modifications:
* - Added support for loading raw bitmap images from SD card and displaying
*   them on a 13.3-inch Spectra 6 E-Ink display.
* - Image filenames are retrieved from a text file (order.txt); index.txt
*   tracks the current image and is updated to point to the next one.
* - The ESP32 enters deep sleep for the duration specified in SLEEP_TIME.
* - To maximize battery life, a MOSFET completely powers down the SD card
*   reader when idle.
*
* License:
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files(the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/
#include "EPD_13in3e.h"
#include "Debug.h"
#include <SD.h>  // SD card library for SPI
#include <SPI.h> // SPI library

// Define SPI pins for T8 V1.8

#define SD_SCK   6   // HSPI clock
#define SD_MISO  7   // HSPI MISO
#define SD_MOSI  21   // HSPI MOSI
#define SD_CS    8  // SD card chip selecf
#define SD_power 2  // SD card MOSFET switch

// --- Configuration ---
#define ORDER_FILE "/order.txt"     // Order file (must be in the root directory)
#define INDEX_FILE "/index.txt"     // Index file (must be in the root directory)
//#define SLEEP_TIME 30000000UL         // Deep sleep time in microseconds
#define SLEEP_TIME 86400000000UL
// ***** Move pictureList to global scope to save stack space *****
const int maxPictures = 365;  // Maximum number of pictures in the order file
String pictureList[maxPictures];  // Global array for picture file names


// const UBYTE spiCsPin[2] = {
// 		SPI_CS0, SPI_CS1
// };
const UBYTE PSR_V[2] = {
	0xDF, 0x69
};
const UBYTE PWR_V[6] = {
	0x0F, 0x00, 0x28, 0x2C, 0x28, 0x38
};
const UBYTE POF_V[1] = {
	0x00
};
const UBYTE DRF_V[1] = {
	0x00
};
const UBYTE CDI_V[1] = {
	0xF7
};
const UBYTE TCON_V[2] = {
	0x03, 0x03
};
const UBYTE TRES_V[4] = {
	0x04, 0xB0, 0x03, 0x20
};
const UBYTE CMD66_V[6] = {
	0x49, 0x55, 0x13, 0x5D, 0x05, 0x10
};
const UBYTE EN_BUF_V[1] = {
	0x07
};
const UBYTE CCSET_V[1] = {
	0x01
};
const UBYTE PWS_V[1] = {
	0x22
};
const UBYTE AN_TM_V[9] = {
	0xC0, 0x1C, 0x1C, 0xCC, 0xCC, 0xCC, 0x15, 0x15, 0x55
};


const UBYTE AGID_V[1] = {
	0x10
};

const UBYTE BTST_P_V[2] = {
	0xE8, 0x28
};
const UBYTE BOOST_VDDP_EN_V[1] = {
	0x01
};
const UBYTE BTST_N_V[2] = {
	0xE8, 0x28
};
const UBYTE BUCK_BOOST_VDDN_V[1] = {
	0x01
};
const UBYTE TFT_VCOM_POWER_V[1] = {
	0x02
};


static void EPD_13IN3E_CS_ALL(UBYTE Value)
{
    DEV_Digital_Write(EPD_CS_M_PIN, Value);
    DEV_Digital_Write(EPD_CS_S_PIN, Value);
}


static void EPD_13IN3E_SPI_Sand(UBYTE Cmd, const UBYTE *buf, UDOUBLE Len)
{
    DEV_SPI_WriteByte(Cmd);
    DEV_SPI_Write_nByte((UBYTE *)buf,Len);
}


/******************************************************************************
function :	Software reset
parameter:
******************************************************************************/
static void EPD_13IN3E_Reset(void)
{
    DEV_Digital_Write(EPD_RST_PIN, 1);
    DEV_Delay_ms(30);
    DEV_Digital_Write(EPD_RST_PIN, 0);
    DEV_Delay_ms(30);
    DEV_Digital_Write(EPD_RST_PIN, 1);
    DEV_Delay_ms(30);
    DEV_Digital_Write(EPD_RST_PIN, 0);
    DEV_Delay_ms(30);
    DEV_Digital_Write(EPD_RST_PIN, 1);
    DEV_Delay_ms(30);
}

/******************************************************************************
function :	send command
parameter:
     Reg : Command register
******************************************************************************/

static void EPD_13IN3E_SendCommand(UBYTE Reg)
{
    DEV_SPI_WriteByte(Reg);
}

/******************************************************************************
function :	send data
parameter:
    Data : Write data
******************************************************************************/
static void EPD_13IN3E_SendData(UBYTE Reg)
{
    DEV_SPI_WriteByte(Reg);
}
static void EPD_13IN3E_SendData2(const UBYTE *buf, uint32_t Len)
{
    DEV_SPI_Write_nByte((UBYTE *)buf,Len);
}

/******************************************************************************
function :	Wait until the busy_pin goes LOW
parameter:
******************************************************************************/
static void EPD_13IN3E_ReadBusyH(void)
{
    Debug("e-Paper busy\r\n");
	while(!DEV_Digital_Read(EPD_BUSY_PIN)) {      //LOW: busy, HIGH: idle
        DEV_Delay_ms(10);
        // Debug("e-Paper busy release\r\n");
    }
	DEV_Delay_ms(20);
    Debug("e-Paper busy release\r\n");
}


/******************************************************************************
function :  Turn On Display
parameter:
******************************************************************************/
static void EPD_13IN3E_TurnOnDisplay(void)
{
    printf("Write PON \r\n");
    EPD_13IN3E_CS_ALL(0);
    EPD_13IN3E_SendCommand(0x04); // POWER_ON
    EPD_13IN3E_CS_ALL(1);
    EPD_13IN3E_ReadBusyH();

    printf("Write DRF \r\n");
    DEV_Delay_ms(50);
    EPD_13IN3E_CS_ALL(0);
    EPD_13IN3E_SPI_Sand(DRF, DRF_V, sizeof(DRF_V));
    EPD_13IN3E_CS_ALL(1);
    EPD_13IN3E_ReadBusyH();

    printf("Write POF \r\n");
    EPD_13IN3E_CS_ALL(0);
    EPD_13IN3E_SPI_Sand(POF, POF_V, sizeof(POF_V));
    EPD_13IN3E_CS_ALL(1);
    // EPD_13IN3E_ReadBusyH();
    printf("Display Done!! \r\n");
}

/******************************************************************************
function :	Initialize the e-Paper register
parameter:
******************************************************************************/
void EPD_13IN3E_Init(void)
{
	EPD_13IN3E_Reset();
//    EPD_13IN3E_ReadBusyH();

    DEV_Digital_Write(EPD_CS_M_PIN, 0);
	EPD_13IN3E_SPI_Sand(AN_TM, AN_TM_V, sizeof(AN_TM_V));
    EPD_13IN3E_CS_ALL(1);

    EPD_13IN3E_CS_ALL(0);
	EPD_13IN3E_SPI_Sand(CMD66, CMD66_V, sizeof(CMD66_V));
    EPD_13IN3E_CS_ALL(1);

    EPD_13IN3E_CS_ALL(0);
	EPD_13IN3E_SPI_Sand(PSR, PSR_V, sizeof(PSR_V));
    EPD_13IN3E_CS_ALL(1);

    EPD_13IN3E_CS_ALL(0);
	EPD_13IN3E_SPI_Sand(CDI, CDI_V, sizeof(CDI_V));
    EPD_13IN3E_CS_ALL(1);

    EPD_13IN3E_CS_ALL(0);
	EPD_13IN3E_SPI_Sand(TCON, TCON_V, sizeof(TCON_V));
    EPD_13IN3E_CS_ALL(1);

    EPD_13IN3E_CS_ALL(0);
	EPD_13IN3E_SPI_Sand(AGID, AGID_V, sizeof(AGID_V));
    EPD_13IN3E_CS_ALL(1);

    EPD_13IN3E_CS_ALL(0);
	EPD_13IN3E_SPI_Sand(PWS, PWS_V, sizeof(PWS_V));
    EPD_13IN3E_CS_ALL(1);

    EPD_13IN3E_CS_ALL(0);
	EPD_13IN3E_SPI_Sand(CCSET, CCSET_V, sizeof(CCSET_V));
    EPD_13IN3E_CS_ALL(1);

    EPD_13IN3E_CS_ALL(0);
	EPD_13IN3E_SPI_Sand(TRES, TRES_V, sizeof(TRES_V));
    EPD_13IN3E_CS_ALL(1);

    DEV_Digital_Write(EPD_CS_M_PIN, 0);
	EPD_13IN3E_SPI_Sand(PWR_epd, PWR_V, sizeof(PWR_V));
    EPD_13IN3E_CS_ALL(1);

    DEV_Digital_Write(EPD_CS_M_PIN, 0);
	EPD_13IN3E_SPI_Sand(EN_BUF, EN_BUF_V, sizeof(EN_BUF_V));
    EPD_13IN3E_CS_ALL(1);

    DEV_Digital_Write(EPD_CS_M_PIN, 0);
	EPD_13IN3E_SPI_Sand(BTST_P, BTST_P_V, sizeof(BTST_P_V));
    EPD_13IN3E_CS_ALL(1);

    DEV_Digital_Write(EPD_CS_M_PIN, 0);
	EPD_13IN3E_SPI_Sand(BOOST_VDDP_EN, BOOST_VDDP_EN_V, sizeof(BOOST_VDDP_EN_V));
    EPD_13IN3E_CS_ALL(1);
	
    DEV_Digital_Write(EPD_CS_M_PIN, 0);
	EPD_13IN3E_SPI_Sand(BTST_N, BTST_N_V, sizeof(BTST_N_V));
    EPD_13IN3E_CS_ALL(1);

    DEV_Digital_Write(EPD_CS_M_PIN, 0);
	EPD_13IN3E_SPI_Sand(BUCK_BOOST_VDDN, BUCK_BOOST_VDDN_V, sizeof(BUCK_BOOST_VDDN_V));
    EPD_13IN3E_CS_ALL(1);

    DEV_Digital_Write(EPD_CS_M_PIN, 0);
	EPD_13IN3E_SPI_Sand(TFT_VCOM_POWER, TFT_VCOM_POWER_V, sizeof(TFT_VCOM_POWER_V));
    EPD_13IN3E_CS_ALL(1);
    
}

/******************************************************************************
function :  Clear screen
parameter:
******************************************************************************/
void EPD_13IN3E_Clear(UBYTE color)
{
    UDOUBLE Width, Height;
    UBYTE Color;
    Width = (EPD_13IN3E_WIDTH % 2 == 0)? (EPD_13IN3E_WIDTH / 2 ): (EPD_13IN3E_WIDTH / 2 + 1);
    Height = EPD_13IN3E_HEIGHT;
    Color = (color<<4)|color;
    
    UBYTE buf[Width/2];
    
    for (UDOUBLE j = 0; j < Width/2; j++) {
        buf[j] = Color;
    }
    
    DEV_Digital_Write(EPD_CS_M_PIN, 0);
    EPD_13IN3E_SendCommand(0x10);
    for (UDOUBLE j = 0; j < EPD_13IN3E_HEIGHT; j++) {
        EPD_13IN3E_SendData2(buf, Width/2);
        DEV_Delay_ms(1);
    }
    EPD_13IN3E_CS_ALL(1);

    DEV_Digital_Write(EPD_CS_S_PIN, 0);
    EPD_13IN3E_SendCommand(0x10);
    for (UDOUBLE j = 0; j < EPD_13IN3E_HEIGHT; j++) {
        EPD_13IN3E_SendData2(buf, Width/2);
        DEV_Delay_ms(1);
    }
    EPD_13IN3E_CS_ALL(1);
    
    EPD_13IN3E_TurnOnDisplay();
}

void EPD_13IN3E_Display(const UBYTE *Image)
{
    UDOUBLE Width, Width1, Height;
    Width = (EPD_13IN3E_WIDTH % 2 == 0)? (EPD_13IN3E_WIDTH / 2 ): (EPD_13IN3E_WIDTH / 2 + 1);
    Width1 = (Width % 2 == 0)? (Width / 2 ): (Width / 2 + 1);
    Height = EPD_13IN3E_HEIGHT;
    
    DEV_Digital_Write(EPD_CS_M_PIN, 0);
    EPD_13IN3E_SendCommand(0x10);
    for(UDOUBLE i=0; i<Height; i++ )
    {
        EPD_13IN3E_SendData2(Image + i*Width,Width1);
        DEV_Delay_ms(1);
    }
    EPD_13IN3E_CS_ALL(1);

    DEV_Digital_Write(EPD_CS_S_PIN, 0);
    EPD_13IN3E_SendCommand(0x10);
    for(UDOUBLE i=0; i<Height; i++ )
    {
        EPD_13IN3E_SendData2(Image + i*Width + Width1,Width1);
        DEV_Delay_ms(1);
    }
       
    EPD_13IN3E_CS_ALL(1);
    
    EPD_13IN3E_TurnOnDisplay();
}


void EPD_13IN3E_DisplayPart(const UBYTE *Image, UWORD xstart, UWORD ystart, UWORD image_width, UWORD image_heigh)
{
    UDOUBLE Width, Width1, Height;
    Width = (EPD_13IN3E_WIDTH % 2 == 0)? (EPD_13IN3E_WIDTH / 2 ): (EPD_13IN3E_WIDTH / 2 + 1);
    Width1 = (Width % 2 == 0)? (Width / 2 ): (Width / 2 + 1);
    Height = EPD_13IN3E_HEIGHT;
    
    UWORD Xend = ((xstart + image_width)%2 == 0)?((xstart + image_width) / 2 - 1): ((xstart + image_width) / 2 );
    UWORD Yend = ystart + image_heigh-1;
    xstart = xstart / 2;
    
    if(xstart > 300 )
    {
        Xend = Xend - 300;
        xstart = xstart - 300;
        DEV_Digital_Write(EPD_CS_M_PIN, 0);
        EPD_13IN3E_SendCommand(0x10);
        for (UDOUBLE i = 0; i < Height; i++) {
            for (UDOUBLE j = 0; j < Width1; j++) {
                EPD_13IN3E_SendData(0x11);
            }
            DEV_Delay_ms(1);
        }
        EPD_13IN3E_CS_ALL(1);
        
        
        DEV_Digital_Write(EPD_CS_S_PIN, 0);
        EPD_13IN3E_SendCommand(0x10);
        for (UDOUBLE i = 0; i < Height; i++) {
            for (UDOUBLE j = 0; j < Width1; j++) {
                if((i<Yend) && (i>=ystart) && (j<Xend) && (j>=xstart)) {
                    EPD_13IN3E_SendData(Image[(j-xstart) + (image_width/2*(i-ystart))]);
                }
                else
                    EPD_13IN3E_SendData(0x11);
            }
            DEV_Delay_ms(1);
        }
        EPD_13IN3E_CS_ALL(1);
    }
    else if(Xend < 300 )
    {
        DEV_Digital_Write(EPD_CS_M_PIN, 0);
        EPD_13IN3E_SendCommand(0x10);
        for (UDOUBLE i = 0; i < Height; i++) {
            for (UDOUBLE j = 0; j < Width1; j++) {
                if((i<Yend) && (i>=ystart) && (j<Xend) && (j>=xstart)) {
                    EPD_13IN3E_SendData(Image[(j-xstart) + (image_width/2*(i-ystart))]);
                }
                else
                    EPD_13IN3E_SendData(0x11);
            }
            DEV_Delay_ms(1);
        }
        EPD_13IN3E_CS_ALL(1);
        
        
        DEV_Digital_Write(EPD_CS_S_PIN, 0);
        EPD_13IN3E_SendCommand(0x10);
        for (UDOUBLE i = 0; i < Height; i++) {
            for (UDOUBLE j = 0; j < Width1; j++) {
                EPD_13IN3E_SendData(0x11);
            }
            DEV_Delay_ms(1);
        }
        EPD_13IN3E_CS_ALL(1);
    }
    else
    {
        DEV_Digital_Write(EPD_CS_M_PIN, 0);
        EPD_13IN3E_SendCommand(0x10);
        for (UDOUBLE i = 0; i < Height; i++) {
            for (UDOUBLE j = 0; j < Width1; j++) {
                if((i<Yend) && (i>=ystart) && (j>=xstart)) {
                    EPD_13IN3E_SendData(Image[(j-xstart) + (image_width/2*(i-ystart))]);
                }
                else
                    EPD_13IN3E_SendData(0x11);
            }
            DEV_Delay_ms(1);
        }
        EPD_13IN3E_CS_ALL(1);
        
        
        DEV_Digital_Write(EPD_CS_S_PIN, 0);
        EPD_13IN3E_SendCommand(0x10);
        for (UDOUBLE i = 0; i < Height; i++) {
            for (UDOUBLE j = 0; j < Width1; j++) {
                if((i<Yend) && (i>=ystart) && (j<Xend-300)) {
                    EPD_13IN3E_SendData(Image[(j+300-xstart) + (image_width/2*(i-ystart))]);
                }
                else
                    EPD_13IN3E_SendData(0x11);
            }
            DEV_Delay_ms(1);
        }
        EPD_13IN3E_CS_ALL(1);
    }

    EPD_13IN3E_TurnOnDisplay();
}



void EPD_13IN3E_Show6Block(void)
{
    unsigned long i, j, k;
    UWORD Width, Height;
    Width = (EPD_13IN3E_WIDTH % 2 == 0)? (EPD_13IN3E_WIDTH / 2 ): (EPD_13IN3E_WIDTH / 2 + 1);
    Height = EPD_13IN3E_HEIGHT;
    unsigned char const Color_seven[6] = 
    {EPD_13IN3E_BLACK, EPD_13IN3E_BLUE, EPD_13IN3E_GREEN,
    EPD_13IN3E_RED, EPD_13IN3E_YELLOW, EPD_13IN3E_WHITE};
    
    DEV_Digital_Write(EPD_CS_M_PIN, 0);
    EPD_13IN3E_SendCommand(0x10);
    for (k = 0; k < 6; k++) {
        for (j = 0; j < Height/6; j++) {
            for (i = 0; i < Width/2; i++) {
                EPD_13IN3E_SendData(Color_seven[k]|(Color_seven[k]<<4));
            }
        }
        DEV_Delay_ms(1);
    }
    EPD_13IN3E_CS_ALL(1);
    
    DEV_Digital_Write(EPD_CS_S_PIN, 0);
    EPD_13IN3E_SendCommand(0x10);
    for (k = 0; k < 6; k++) {
        for (j = 0; j < Height/6; j++) {
            for (i = 0; i < Width/2; i++) {
                EPD_13IN3E_SendData(Color_seven[k]|(Color_seven[k]<<4));
            }
        }
        DEV_Delay_ms(1);
    }
    EPD_13IN3E_CS_ALL(1);
    
    EPD_13IN3E_TurnOnDisplay();
}


/******************************************************************************
function :  Enter sleep mode
parameter:
******************************************************************************/
void EPD_13IN3E_Sleep(void)
{
    EPD_13IN3E_CS_ALL(0);
    EPD_13IN3E_SendCommand(0x07); // DEEP_SLEEP
    EPD_13IN3E_SendData(0XA5);
    EPD_13IN3E_CS_ALL(1);
}


void hilbernate(const UBYTE reason)
{
  // --- Put the display to sleep and shut down ---
  Serial.println("Putting display to sleep...");
  
  EPD_13IN3E_Sleep();
  DEV_Module_Exit();
   

 // --- Enter deep sleep ---
  Serial.println("Entering deep sleep...");


Serial.print("Wakeup reason: ");
Serial.println(esp_sleep_get_wakeup_cause());


  if (reason != 1) {
    Serial.println("Sleep after success...");
    esp_sleep_enable_timer_wakeup(SLEEP_TIME);
  } else {
    Serial.println("Sleep after error...");
  }
  delay(1000);
//Serial.end();
  esp_sleep_enable_timer_wakeup(SLEEP_TIME);
  delay(100);

  esp_deep_sleep_start();
}


// Utility function: ensures filename begins with '/'
String ensureLeadingSlash(const String &filename) {
  String name = filename;
  name.trim();
  // add .raw if it's missing
  if (!name.endsWith(".raw")) {
    name += ".raw";
  }
  // add leading slash if it's missing
  if (!name.startsWith("/")) {
    name = "/" + name;
  }
  return name;
}



void EPD_13IN3E_demo(void) {
  //hilbernate(0);
  pinMode(SD_power, OUTPUT); // SD3 / IO10
  digitalWrite(SD_power, HIGH);
  delay(100);
  
  Serial.println("Initializing SD card");
  // Initialize SD card.
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  if (!SD.begin(SD_CS, SPI)) {
    Serial.println("SD card initialization failed! Check connections and card format.");
    hilbernate(1);
  }
  Serial.println("SD card initialized successfully on HSPI!");

  File orderFile = SD.open(ORDER_FILE, FILE_READ);
  if (!orderFile) {
    Serial.print("Failed to open order file '");
    hilbernate(1);
  }
  String orderContent = "";
  while (orderFile.available()) {
    orderContent += (char)orderFile.read();
  }
  orderFile.close();
  int pictureCount = 0;
  int startIdx = 0;
  while (true) {
    int newLineIdx = orderContent.indexOf('\n', startIdx);
    String line;
    if (newLineIdx == -1) {
      line = orderContent.substring(startIdx);
      line.trim();
      if (line.length() > 0) {
        pictureList[pictureCount++] = ensureLeadingSlash(line);
      }
      break;
    }
    line = orderContent.substring(startIdx, newLineIdx);
    line.trim();
    if (line.length() > 0) {
      pictureList[pictureCount++] = ensureLeadingSlash(line);
    }
    startIdx = newLineIdx + 1;
    if (pictureCount >= maxPictures)
      break;
  }
  Serial.print("Found ");
  Serial.print(pictureCount);
  Serial.println(" picture(s) in order file.");

  // --- Read the current picture index from the index file ---
  int index = 0;
  File indexFile = SD.open(INDEX_FILE, FILE_READ);
  if (indexFile) {
    String idxStr = indexFile.readStringUntil('\n');
    idxStr.trim();
    if (idxStr.length() > 0) {
      index = idxStr.toInt();
    }
    indexFile.close();
  }
  Serial.print("Current picture index (from SD): ");
  Serial.println(index);

  // Wrap the index within range.
  index = index % pictureCount;
  String fileName = pictureList[index];
  Serial.print("Displaying picture: ");
  Serial.println(fileName);




  // Open the raw bitmap file stored on the SD card.
  // This file should contain 600 bytes per row (i.e. 4bpp for 1200 pixels per row)
  File file = SD.open(fileName, FILE_READ);
  if (!file) {
    Serial.print("Failed to open file "); Serial.print(fileName);
    Serial.println();
    hilbernate(1);
  }
  Serial.println("Opened for display update.");
  const size_t ROW_SIZE = 600;
  const size_t SEG_SIZE = 300;
  // Allocate buffers for one full row and for each segment.
  static uint8_t rowBuffer[ROW_SIZE];
  //DEV_Digital_Write(SD_CS, 1); 
  
  DEV_Digital_Write(EPD_CS_M_PIN, 0);
  EPD_13IN3E_SendCommand(0x10);
  //DEV_Delay_ms(1); 

  // Process each row of the display.
  uint32_t rowCount = 0;
  
  while (file.available() && (rowCount < EPD_13IN3E_HEIGHT)) {
     int bytesRead = file.read(rowBuffer, ROW_SIZE);
    if (bytesRead != ROW_SIZE) {
      Serial.println("Error: Incomplete row read from file.");
      hilbernate(1);
    }
   // DEV_Digital_Write(SD_CS, 1);
    EPD_13IN3E_SendData2(rowBuffer, SEG_SIZE); // Send 300 bytes
    DEV_Delay_ms(1);                            // Small delay for data integrity
    rowCount++;
  }
  EPD_13IN3E_CS_ALL(1);
  DEV_Digital_Write(EPD_CS_S_PIN, 0);
  EPD_13IN3E_SendCommand(0x10);
  rowCount = 0;
  file.seek(0);
  while (file.available() && (rowCount < EPD_13IN3E_HEIGHT)) {
    // Read one row (600 bytes) from the file.
    int bytesRead = file.read(rowBuffer, ROW_SIZE);
    if (bytesRead != ROW_SIZE) {
      Serial.println("Error: Incomplete row read from file.");
      hilbernate(1);
    }
    EPD_13IN3E_SendData2(rowBuffer + SEG_SIZE, SEG_SIZE); // Send 300 bytes
    DEV_Delay_ms(1);                            // Small delay for data integrity
    rowCount++;
  }
  file.close();
  Serial.print("Finished sending ");
  Serial.print(rowCount);
  Serial.println(" rows from file.");

  EPD_13IN3E_CS_ALL(1);
  // --- Update the index ---
  index = (index + 1) % pictureCount;
  File indexFileWrite = SD.open(INDEX_FILE, FILE_WRITE);
  indexFileWrite.print(index);
  indexFileWrite.close();
  Serial.print("Updated index stored on SD: ");
  Serial.println(index);

  // Finalize the update by refreshing the display.
  SPI.endTransaction();
  SD.end();
  delay(100);
  digitalWrite(SD_power, LOW);
  delay(100);
  EPD_13IN3E_TurnOnDisplay();
  
  // --- Put the display to sleep and shut down ---
  hilbernate(0);

}

