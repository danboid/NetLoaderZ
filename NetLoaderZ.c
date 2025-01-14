/*
 *  NetLoaderZ
 *
 * NetLoaderZ is based upon NetLoader by ry755 and also uses code from
 * Radu Hristea's basic implementation of the ZModem protocol for AVR.
 *
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Uzebox is a reserved trade mark
*/

#include <stdbool.h>
#include <avr/io.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <uzebox.h>
#include <bootlib.h>
#include "uzenet.h"
#include "defs.h"

#include "data/tileset.inc"
#include "data/font-8x8-full.inc"

// Strings
static const char txt_sdno[] PROGMEM = "No SD card!";
static const char txt_filn[] PROGMEM = "File doesn't exist!";

long int currentChunk = 0;
int totalChunks = 0; // size of file in chunks
int sd_bufCount = 0; // used for saving the place of where to write in sd_buf
int sdSector = 0; // the current sector selected in the file, used for saving the first sector to extract the name, author, and year of the game

static const int chunkSize PROGMEM = 128; // how many bytes in one chunk, can be changed as long as it's the same amount as the computer script

bool headerSizeSet = false;
bool totalChunksAmountSet = false; // true if the computer script has sent the total chunk size of the file being sent

int byteCount = 0;
int append = 0;
int dataSize = 0;
int headerSize = 0;

char gameName[32];
char gameAuthor[32];
unsigned int gameYear0C = 0; // 0x0C in .uze header
unsigned int gameYear0D = 0; // 0x0D in .uze header
unsigned int gameYear = 0;

void vsyncCallback() {
	wifi_Tick();
}

void wifiCallback(s8 status) {
	Print(0,0,PSTR("wifi status: "));
	switch(status){
		case WIFI_STAT_READY:
			Print(0,1,PSTR("ready    "));
			break;
		case WIFI_STAT_CONNECTED:
			Print(0,1,PSTR("connected"));
			break;
		case WIFI_STAT_IP:
			Print(0,1,PSTR("has ip   "));
			break;
		default:
			break;
	}
}

void initializeESP() {
	if(wifi_Init(&wifiCallback)!=WIFI_OK){
		Print(0,3,PSTR("init error!"));
		while(1);
	}
	Print(0,3,PSTR("init done"));

	wifi_SendString_P(PSTR("AT+CWMODE=3\r\n"));
	wifi_WaitForString_P(PSTR("OK\r\n"),NULL);

	wifi_SendString_P(PSTR("AT+CIPMUX=1\r\n"));
	wifi_WaitForString_P(PSTR("OK\r\n"),NULL);

	wifi_SendString_P(PSTR("AT+CIPSERVER=1\r\n"));
	wifi_WaitForString_P(PSTR("OK\r\n"),NULL);

	wifi_SendString_P(PSTR("AT+CIPDINFO=0\r\n"));
	wifi_WaitForString_P(PSTR("OK\r\n"),NULL);
}

void updateUI() {
	int progressAmount = (currentChunk * 100) / totalChunks;

	int onesDigit = progressAmount % 10;
	int tensDigit = (progressAmount / 10) % 10;

	int progressBlockAmount = (onesDigit * 8) / 10;

	SetTile(9,20,9);
	SetTile(20,20,9);
	SetTile(9,19,11);
	SetTile(20,19,12);
	SetTile(9,21,13);
	SetTile(20,21,14);
	Fill(10,19,10,1,10);
	Fill(10,21,10,1,10);

	if (totalChunks != 0) {
		if (tensDigit != 0) SetTile(9+tensDigit,20,8);
		if (onesDigit != 0) SetTile(10+tensDigit,20,progressBlockAmount+1);

		PrintInt(15,22,progressAmount,false);
		PrintChar(16,22,'%');

		PrintInt(19,23,totalChunks,false);
		PrintInt(13,23,currentChunk,false);
		PrintChar(15,23,'/');
	}
}

void printGameInfo() {
	for (int i = 0; i < strlen(gameName); i++) {
		PrintChar(i+3,2,gameName[i]);
	}
	for (int i = 0; i < strlen(gameAuthor); i++) {
		PrintChar(i+3,5,gameAuthor[i]);
	}
	PrintLong(6,8,gameYear);

	SetTile(0,0,11);
	SetTile(29,0,12);
	SetTile(0,9,13);
	SetTile(29,9,14);
	Fill(1,0,28,1,10);
	Fill(1,9,28,1,10);
	Fill(0,1,1,8,9);
	Fill(29,1,1,8,9);

	Print(1,1,PSTR("Name"));
	Print(1,4,PSTR("Author"));
	Print(1,7,PSTR("Year"));

	SetTile(1,2,13);
	SetTile(1,5,13);
	SetTile(1,8,13);
}

//***
	bool GotSpecial;
	bool GotHeader;
	bool MoreData;
	bool AllowFileWrite;
	byte LastError;
	byte ch;
	byte NeedZACK;
	byte HeaderType;
	byte HeaderData[4];
	word NrBytes;
	dword GoodOffset;
	dword fileSize;
	byte fileName[13];
	byte ZMBuffer[ZMAX_BUF];
//***

//-----------------------------------------------------------------------------
// purpose:	receiving file
// params:	none ->	global var. "FileIndex" is the file to receive
// returns:	success (TRUE or FALSE)
byte rZModem_ZReceive(void)
//-----------------------------------------------------------------------------
{	//
	byte quit  = FALSE;
	byte tries = 0;
	//
	LastError = NO_ERROR;
	AllowFileWrite = FALSE;
	//
    // HERE: open the file
    //
	while(ALLOK && !quit && (tries++ < ZMAX_RETRY))
	{
		  //wdogtrig();
		  rZModem_getZMHeader();
		  if(ALLOK)
		  {
			 switch(HeaderType)
			 {
         	   case ZCAN:
         	      	return FALSE;
         	   break;

			   case ZRQINIT:
					rZModem_sendHexHeader(ZRINIT);
			   break;

			   case ZFILE://file transfer signaled
					NeedZACK = 0x00;
					rZModem_getData(); // get file info
					if(ALLOK)
					{
					   GoodOffset = 0;
					   AllowFileWrite = TRUE;
					   rZModem_receiveFile();//receive current file
					   rZModem_sendHexHeader(ZRINIT);
					}
					else if(LastError == ZMODEM_CRC)
					{
					   LastError = NO_ERROR;
					   rZModem_sendHexHeader(ZRINIT);
					}
			   break;

			   case ZFIN://other side has finish signaled
					rZModem_sendHexHeader(ZFIN);//reply
					rZModem_getOO(); //try to get "Over & Out" signal
					quit = TRUE;
			   break;
			 }
		  }
		  else
		  {
			 if((LastError == ZMODEM_TIMEOUT) || (LastError == ZMODEM_CRC) ||
				(LastError == ZMODEM_BADHEX)) // Receive
			 {
				 LastError = NO_ERROR;
				 rZModem_sendHexHeader(ZRINIT);
			 }
		  }
	}
	if(quit == FALSE)
	   rZModem_sendCAN();
    delay_ms(100);
    rComm_clearInBound(); // reject the garbage
	return quit;
}

//-----------------------------------------------------------------------------
void rZModem_getZMHeader(void)
//-----------------------------------------------------------------------------
{	//
    byte count = 0;
    //
	GotHeader = FALSE;
	rComm_readByte(&ch);
	if(ALLOK)
	{
	   while(ALLOK && (ch != ZPAD))
	   {
			 //wdogtrig();
			 if(ch == CAN)
			 {
			    if(++count == 5)
			    {
			       GotHeader = TRUE;
			       HeaderType = ZCAN;
			       return;
			    }
			 }
			 else
			    count = 0;
			 rComm_readByte(&ch);
	   }
	   if(ALLOK)
	   {
		  rComm_readByte(&ch);
		  while(ALLOK && (ch == ZPAD))
		  {
				//wdogtrig();
				rComm_readByte(&ch);
		  }
		  if(ALLOK && (ch == ZDLE))
		  {
			 rComm_readByte(&ch);
			 if(ALLOK)
			 {
				if(ch == ZBIN)
				   rZModem_getBinaryHeader();
				else
				   rZModem_getHexHeader();
			 }
		  }
	   }
	}
}

//-----------------------------------------------------------------------------
void rZModem_sendHexHeader(byte hType)
//-----------------------------------------------------------------------------
{	//
	word crc = 0;
	byte i;
	//
	//wdogtrig();
	switch(hType)
	{
	  case ZRINIT :
	  case ZFIN   :
	  case ZRQINIT:
	       for(i = 0; i < 4; i++)
	           HeaderData[i] = 0;
	       if(hType == ZRINIT)
	           HeaderData[3] = CANOVIO;
	  break;

	  case ZRPOS  :
	       /*seek(GoodOffset);*/
	  case ZEOF   :
	  case ZACK   :
	       for(i = 0; i < 4; i++)
	           HeaderData[i] = (byte)(GoodOffset >> (byte)(8 * i));
	  break;
	}
	// sending now ...
	putchar1(ZPAD);
	putchar1(ZPAD);
	putchar1(ZDLE);
    putchar1(ZHEX);
   	ch = hType;
    crc = rZModem_crcUpdate(crc, ch);
	rZModem_sendHexChar();
	for(i = 0; i < 4; i++)
	{
	   	//wdogtrig();
	   	ch = HeaderData[i];
		crc = rZModem_crcUpdate(crc, ch);
    	rZModem_sendHexChar();
	}
   	ch = (byte)(crc >> 8);
	rZModem_sendHexChar();
   	ch = (byte)crc;
	rZModem_sendHexChar();
	//**************************
	putchar1(CR);putchar1(LF);
	if(hType != ZFIN && hType != ZACK)
	   putchar1(XON);
}

//-----------------------------------------------------------------------------
void rZModem_getData(void)
//-----------------------------------------------------------------------------
{   //
	byte quit = FALSE;
	word crc = 0;
	word i = 0;
	//
	while(ALLOK && (i <= 1024) && !quit)
	{
		  //wdogtrig();
		  rZModem_getNextDLECh();
		  if(ALLOK)
		  {
			 if(GotSpecial)
			 {
				switch(ch)
				{
				  case ZCRCW:
				  	   NeedZACK |= 0x02;
				  case ZCRCE:
				       MoreData = FALSE;
				  break;
				}
			    crc = rZModem_crcUpdate(crc, ch);
				rZModem_getNextDLECh();
				if(ALLOK)
				{
				   crc = rZModem_crcUpdate(crc, ch);
				   rZModem_getNextDLECh();
				}
				if(ALLOK)
				{
				   crc = rZModem_crcUpdate(crc, ch);
				   if(crc)
					  LastError = ZMODEM_CRC;
				   else
				   {
					  GoodOffset += i;
					  quit = TRUE;
				   }
				}
			 }
			 else
			 {  // write this byte to file !
				if(AllowFileWrite)
				   rDummy_writeToFile(ch);
				crc = rZModem_crcUpdate(crc, ch);
				i++;
			 }
		  }
	}
	if(i > 1024)
	   LastError = ZMODEM_LONGSP;
	/****************************/
	if(!ALLOK)
	   MoreData = FALSE;
}

//-----------------------------------------------------------------------------
void rZModem_receiveFile(void)
//-----------------------------------------------------------------------------
{	//
	byte quit  = FALSE;
	byte tries = 0;
	//
	while(ALLOK && !quit && (tries++ < ZMAX_RETRY))
	{
		  //wdogtrig();
		  rZModem_sendHexHeader(ZRPOS);
		  rZModem_getZMHeader();
		  if(ALLOK)
		  {
			 switch(HeaderType)
			 {
			   case ZCAN:
			        LastError = ZMODEM_GOTZCAN;
			        return;
			   break;

			   case ZDATA:
				    rZModem_receiveData();
				    quit = TRUE;
			   break;
			 }
		  }
		  else
		  {
			 if((LastError == ZMODEM_TIMEOUT) || (LastError == ZMODEM_CRC) ||
			    (LastError == ZMODEM_BADHEX)) // receiveFile
			 {
				 LastError = NO_ERROR;
			 }
		  }
	}
}

//-----------------------------------------------------------------------------
void rZModem_receiveData(void)
//-----------------------------------------------------------------------------
{	//
	byte quit  = FALSE;
	byte tries = 0;
	//
	NeedZACK = 0x01;
	MoreData = TRUE;
	while(ALLOK && !quit)
	{
		//wdogtrig();
		if(MoreData)
		{
		   rZModem_getData();
		   if(ALLOK)
			  tries = 0;
		}
		else
		{
			if(NeedZACK == 0x03)
			{
			   rZModem_sendHexHeader(ZACK);
			   NeedZACK = 0x01;
			}
			rZModem_getZMHeader();
			if(ALLOK)
			{
			   switch(HeaderType)
			   {
			     case ZCAN:
			          LastError = ZMODEM_GOTZCAN;
			     break;

			     case ZDATA:
				      if(rZModem_positionMatch())
					     MoreData = TRUE;
			     break;

			     case ZEOF:
				      if(rZModem_positionMatch())
					     quit = TRUE;
			     break;
			   }
			}
		}
		if(!ALLOK)
		{
		   if((LastError == ZMODEM_TIMEOUT) || (LastError == ZMODEM_LONGSP) || // receiveData
			  (LastError == ZMODEM_CRC))
		   {
			   if(tries++ < ZMAX_RETRY) // receiveData
			   {
				  LastError = NO_ERROR;
				  MoreData = FALSE;
				  rZModem_sendHexHeader(ZRPOS);
			   }
		   }
		}
	}
	// HERE: close the file
	if(quit == FALSE)
	{
	   // HERE: delete the file if it is not OK !
	}
}

//-----------------------------------------------------------------------------
void rZModem_getOO(void)
//-----------------------------------------------------------------------------
{
	rComm_readByte(&ch); // get an 'O'
	if(ALLOK)
	   rComm_readByte(&ch); // another 'O'
	if(LastError == ZMODEM_TIMEOUT)
	   LastError = NO_ERROR;
}

int main() {
	ClearVram();
	SetTileTable(tileset);
	SetFontTilesIndex(TILESET_SIZE); // font starts at the end of the tileset and spriteset
	SetUserPreVsyncCallback(&vsyncCallback);

	// SD card stuff
	u8 res;
	sdc_struct_t sd_struct;
	u8 sd_buf[512];
	u32 t32;

	sd_struct.bufp = &(sd_buf[0]); // Assign a sector buffer

	u8 data[chunkSize];
	u8 firstSector[512];

	// Initialize SD card and filesystem
	res = FS_Init(&sd_struct);
	if (res != 0U){
		Print(0, 0, txt_sdno);
		PrintChar(0, 1, res + '0');
		while(1);
	}

	// Find the file on the SD card

	t32 = FS_Find(&sd_struct,
	    ((u16)('N') << 8) |
	    ((u16)('E')     ),
	    ((u16)('T') << 8) |
	    ((u16)('L')     ),
	    ((u16)('O') << 8) |
	    ((u16)('A')     ),
	    ((u16)('D') << 8) |
	    ((u16)(' ')     ),
	    ((u16)('B') << 8) |
	    ((u16)('I')     ),
	    ((u16)('N') << 8) |
	    ((u16)(0)       ));
	if (t32 == 0U){
		Print(0, 0, txt_filn);
		while(1);
	}

	FS_Select_Cluster(&sd_struct, t32);
	FS_Read_Sector(&sd_struct);

	initializeESP();

	ClearVram();

	for (int i = 0; i < chunkSize; i++) {
		data[i] = NULL; // fill the array with null characters for atoi later
	}

	while(1) { // Main loop
		byteCount = 0;
		append = 0;
		dataSize = 0;
		headerSize = 0;
		headerSizeSet = false;

		updateUI();

		if (wifi_UnreadCount() > 0 && !totalChunksAmountSet) { // get total amount of chunks
			while(wifi_UnreadCount() > 0) {
				u8 c = wifi_ReadChar();
				c = c & (0xff);

				if (c == ':' && !headerSizeSet) { // this is really hacky
					append = 1;
					headerSize = byteCount + 1; // add one to remove the ":"
					headerSizeSet = true;
				}

				if (append) {
					data[byteCount - headerSize] = c;
				}

				byteCount++;

				//WaitVsync(1);
				WaitUs(100);
			}
			totalChunks = atoi(data);
			totalChunksAmountSet = true;

			dataSize = byteCount - headerSize;
			byteCount = 0;

			// send size back to computer to verify that it was sent correctly
			char dataSizeStr[3];
			itoa(dataSize,dataSizeStr,10); // converts dataSize into a string
			wifi_SendString_P(PSTR("AT+CIPSEND=0,"));
			wifi_SendString(dataSizeStr);
			wifi_SendString_P(PSTR("\r\n"));
			wifi_WaitForString_P(PSTR("> "),NULL);
			WaitVsync(2);

			while (byteCount < dataSize) {
				wifi_SendChar(data[byteCount]);
				byteCount++;
			}

			wifi_SendString_P(PSTR("\r\n"));
			wifi_WaitForString_P(PSTR("SEND OK\r\n"),NULL);
			//////////////////////////////////////////////////////////////////
		}

		byteCount = 0;
		append = 0;
		dataSize = 0;
		headerSize = 0;
		headerSizeSet = false;

		if (wifi_UnreadCount() > 0 && totalChunksAmountSet) {
			while(wifi_UnreadCount() > 0) {
				u8 c = wifi_ReadChar();
				c = c & (0xff);

				if (c == ':' && !headerSizeSet) { // this is really hacky
					append = 1;
					headerSize = byteCount + 1; // add one to remove the ":"
					headerSizeSet = true;
				}

				if (append) {
					data[byteCount - headerSize] = c;
				}

				byteCount++;

				//WaitVsync(1);
				WaitUs(100);
			}

			dataSize = byteCount - headerSize;
			byteCount = 0;

			// send chunk back to computer to verify that it was sent correctly
			char dataSizeStr[3];
			itoa(dataSize,dataSizeStr,10); // converts dataSize into a string
			wifi_SendString_P(PSTR("AT+CIPSEND=0,"));
			wifi_SendString(dataSizeStr);
			wifi_SendString_P(PSTR("\r\n"));
			wifi_WaitForString_P(PSTR("> "),NULL);
			WaitVsync(2);

			while (byteCount < dataSize) {
				wifi_SendChar(data[byteCount]);
				byteCount++;
			}

			wifi_SendString_P(PSTR("\r\n"));
			wifi_WaitForString_P(PSTR("SEND OK\r\n"),NULL);
			///////////////////////////////////////////////////////////////////

			if (data[0] == 'D' && data[1] == 'O' && data[2] == 'N' && data[3] == 'E') { // bruh
				wifi_SendString_P(PSTR("AT+CIPSEND=0,4\r\n"));
				WaitVsync(2);

				wifi_SendString_P(PSTR("DONE\r\n"));
				wifi_WaitForString_P(PSTR("SEND OK\r\n"),NULL);

				res = FS_Write_Sector(&sd_struct); // write the last chunk to the file
				if (res != 0U){
					PrintChar(2, 25, res + '0');
					while(1);
				}

				FS_Reset_Sector(&sd_struct);
				Bootld_Request(&sd_struct);
				while(1);
			} else {
				byteCount = 0;
				while (byteCount < dataSize) {
					sd_buf[sd_bufCount + byteCount] = data[byteCount]; // write the data to the SD card buffer

					if (sdSector == 0) {
						firstSector[sd_bufCount + byteCount] = data[byteCount]; // save first sector
					}

					byteCount++;
				}
			}

			if (sd_bufCount == 512-chunkSize) { // 496 because 16 gets added to it with byteCount above
				sd_bufCount = 0;

				res = FS_Write_Sector(&sd_struct); // write the SD card buffer to the actual file
				if (res != 0U){
					PrintChar(2, 25, res + '0');
					while(1);
				}
				FS_Next_Sector(&sd_struct);
				FS_Read_Sector(&sd_struct);
				currentChunk++;
				sdSector++;
			} else {
				currentChunk++;
				sd_bufCount = sd_bufCount + chunkSize; // used for saving the place of where to write in sd_buf
			}

			if (sdSector == 1) { // we just got past the first sector (starting at 0), now extract the game info
				for (int i = 14; i < 45; i++) {
					gameName[i-14] = firstSector[i];
				}
				for (int i = 46; i < 77; i++) {
					gameAuthor[i-46] = firstSector[i];
				}
				gameYear0C = firstSector[12];
				gameYear0D = firstSector[13];
				gameYear = (gameYear0D<<8) | gameYear0C;

				printGameInfo();
			}
		}
		WaitVsync(1);
	}
}
