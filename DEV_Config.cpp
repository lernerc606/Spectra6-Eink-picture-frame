/*****************************************************************************
* | File      	:   DEV_Config.c
* | Author      :   Originally by Waveshare team
* | Modified by :   lernerc606
* | Function    :   Hardware underlying interface
* | Info        :
*----------------
* |	This version:   V1.0
* | Original Date   :   2020-02-19
* | Modified Date   :   2025-06-26
* | Info        :
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
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
#include "DEV_Config.h"
#include <SPI.h> // SPI library

void GPIO_Config(void)
{
    //SPI.begin(EPD_SCK_PIN, SD_MISO, EPD_MOSI_PIN);
    //SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    

    pinMode(EPD_BUSY_PIN,  INPUT);
    Serial.printf("init board3...\r\n");Serial.flush(); 
    pinMode(EPD_RST_PIN , OUTPUT);
    Serial.printf("init board4...\r\n");Serial.flush(); 
    pinMode(EPD_DC_PIN  , OUTPUT);
    Serial.printf("init board5...\r\n");Serial.flush(); 
    pinMode(EPD_PWR_PIN,  OUTPUT);
    Serial.printf("init board6...\r\n");Serial.flush(); 

    pinMode(EPD_SCK_PIN, OUTPUT);
    Serial.printf("init board7...\r\n");Serial.flush(); 
    pinMode(EPD_MOSI_PIN, OUTPUT);
    Serial.printf("init board8...\r\n");Serial.flush(); 
    pinMode(EPD_CS_M_PIN , OUTPUT);
    Serial.printf("init board9...\r\n");Serial.flush(); 
    pinMode(EPD_CS_S_PIN , OUTPUT);
    Serial.printf("init board10...\r\n");Serial.flush(); 

    digitalWrite(EPD_CS_M_PIN , HIGH);
    Serial.printf("init board11...\r\n");Serial.flush(); 
    digitalWrite(EPD_CS_S_PIN , HIGH);
    Serial.printf("init board12...\r\n");Serial.flush(); 
    digitalWrite(EPD_SCK_PIN, LOW);
    Serial.printf("init board13...\r\n");Serial.flush(); 
    delay(500);
    digitalWrite(EPD_PWR_PIN , HIGH); 
    delay(500);
    Serial.printf("init board14...\r\n");Serial.flush(); 
}

void GPIO_Mode(UWORD GPIO_Pin, UWORD Mode)
{
    if(Mode == 0) {
        pinMode(GPIO_Pin , INPUT);
	} else {
		pinMode(GPIO_Pin , OUTPUT);
	}
}
/******************************************************************************
function:	Module Initialize, the BCM2835 library and initialize the pins, SPI protocol
parameter:
Info:
******************************************************************************/
UBYTE DEV_Module_Init(void)
{
	//gpio
  Serial.printf("init board1...\r\n");
	GPIO_Config();
  Serial.printf("init board2...\r\n");

	return 0;
}

/******************************************************************************
function:
			SPI read and write
******************************************************************************/


void DEV_SPI_WriteByte(UBYTE data)
{
    for (int i = 0; i < 8; i++)
    {
        if ((data & 0x80) == 0) digitalWrite(EPD_MOSI_PIN, GPIO_PIN_RESET); 
        else                    digitalWrite(EPD_MOSI_PIN, GPIO_PIN_SET);

        data <<= 1;
        digitalWrite(EPD_SCK_PIN, GPIO_PIN_SET);     
        digitalWrite(EPD_SCK_PIN, GPIO_PIN_RESET);
    }

}

UBYTE DEV_SPI_ReadByte()
{
    UBYTE j=0xff;
    GPIO_Mode(EPD_MOSI_PIN, 0);
    for (int i = 0; i < 8; i++)
    {
        j = j << 1;
        if (digitalRead(EPD_MOSI_PIN))  j = j | 0x01;
        else                            j = j & 0xfe;
        
        digitalWrite(EPD_SCK_PIN, GPIO_PIN_SET);     
        digitalWrite(EPD_SCK_PIN, GPIO_PIN_RESET);
    }
    GPIO_Mode(EPD_MOSI_PIN, 1);
    return j;
}

void DEV_SPI_Write_nByte(UBYTE *pData, UDOUBLE len)
{
    for (int i = 0; i < len; i++)
        DEV_SPI_WriteByte(pData[i]);
}


void DEV_Module_Exit(void)
{
    digitalWrite(EPD_PWR_PIN , LOW);
    digitalWrite(EPD_RST_PIN , LOW);
}
