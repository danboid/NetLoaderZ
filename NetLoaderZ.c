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
#include <uzebox.h>
#include <bootlib.h>
#include "uzenet.h"

#include "data/tileset.inc"
#include "data/font-8x8-full.inc"

// ZMODEM constants
#define ZPAD            '*'
#define ZDLE            0x18
#define ZDLEE           0x58
#define ZBIN            'A'
#define ZHEX            'B'
#define ZBIN32          'C'

// ZMODEM frame types
#define ZRQINIT         0
#define ZRINIT          1
#define ZSINIT          2
#define ZACK            3
#define ZFILE           4
#define ZSKIP           5
#define ZNAK            6
#define ZABORT          7
#define ZFIN            8
#define ZRPOS           9
#define ZDATA           10
#define ZEOF            11
#define ZFERR          12
#define ZCRC           13
#define ZCHALLENGE     14
#define ZCOMPL         15
#define ZCAN           16
#define ZFREECNT       17
#define ZCOMMAND       18
#define ZSTDERR        19

// ZMODEM frame end markers
#define ZCRCE          'h'  // Frame ends, header follows
#define ZCRCG          'i'  // Frame continues, no header follows
#define ZCRCQ          'j'  // Frame continues, header follows
#define ZCRCW          'k'  // Frame ends, header follows, wait for ZACK

// Buffer size as a constant
#define BUFFER_SIZE    1024

// Strings
static const char txt_sdno[] PROGMEM = "No SD card!";
static const char txt_filn[] PROGMEM = "File doesn't exist!";
static const char txt_zmodem[] PROGMEM = "Waiting for ZMODEM transfer...";

// Global variables
long int currentChunk = 0;
int totalChunks = 0;
int sd_bufCount = 0;
int sdSector = 0;

// Static buffer allocation
static uint8_t receive_buffer[BUFFER_SIZE];

char gameName[32];
char gameAuthor[32];
unsigned int gameYear0C = 0;
unsigned int gameYear0D = 0;
unsigned int gameYear = 0;

// UART initialization (matching Uzebox defaults)
void initializeUART(void) {
    // Configure UART: 115200 baud, 8-bit data, no parity, 1 stop bit
    #define BAUD 115200
    //#define F_CPU 28636363UL
    #include <util/setbaud.h>

    UBRR0H = UBRRH_VALUE;
    UBRR0L = UBRRL_VALUE;

    #if USE_2X
    UCSR0A |= (1 << U2X0);
    #else
    UCSR0A &= ~(1 << U2X0);
    #endif

    // Enable receiver and transmitter
    UCSR0B = (1<<RXEN0) | (1<<TXEN0);
    // Set frame format: 8 data bits, no parity, 1 stop bit
    UCSR0C = (1<<UCSZ01) | (1<<UCSZ00);
}

// CRC functions for ZMODEM
uint16_t crc16_ccitt(uint8_t *data, int length) {
    uint16_t crc = 0xFFFF;
    while (length--) {
        crc ^= *data++ << 8;
        for (uint8_t i = 0; i < 8; i++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
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

// ZMODEM protocol functions
uint8_t readZModemByte(void) {
    while (!(UCSR0A & (1<<RXC0)));  // Wait for data
    return UDR0;
}

void sendZModemByte(uint8_t byte) {
    if (byte == ZDLE || byte == 0x13 || byte == 0x11 || byte == 0x91 || byte == 0x93) {
        while (!(UCSR0A & (1<<UDRE0)));  // Wait for empty transmit buffer
        UDR0 = ZDLE;
        while (!(UCSR0A & (1<<UDRE0)));  // Wait again
        UDR0 = byte ^ 0x40;
    } else {
        while (!(UCSR0A & (1<<UDRE0)));  // Wait for empty transmit buffer
        UDR0 = byte;
    }
}

bool receiveZModemHeader(uint8_t *frameType) {
    uint8_t header[4];
    uint16_t crc;

    // Wait for ZPAD
    while (readZModemByte() != ZPAD);

    // Check frame type
    *frameType = readZModemByte();

    // Read 4 byte header
    for (int i = 0; i < 4; i++) {
        header[i] = readZModemByte();
    }

    // Read and verify CRC
    crc = (readZModemByte() << 8) | readZModemByte();
    return (crc == crc16_ccitt(header, 4));
}

void sendZModemHeader(uint8_t frameType) {
    sendZModemByte(ZPAD);
    sendZModemByte(ZDLE);
    sendZModemByte(ZBIN);
    sendZModemByte(frameType);

    // Send empty header
    for (int i = 0; i < 4; i++) {
        sendZModemByte(0);
    }

    // Send CRC
    uint16_t crc = crc16_ccitt((uint8_t[]){0,0,0,0}, 4);
    sendZModemByte(crc >> 8);
    sendZModemByte(crc & 0xFF);
}

bool receiveZModemData(uint8_t *buffer, int *length) {
    uint8_t byte;
    int count = 0;
    bool escaped = false;

    while (1) {
        byte = readZModemByte();

        if (byte == ZDLE) {
            escaped = true;
            continue;
        }

        if (escaped) {
            escaped = false;
            if (byte == ZCRCE || byte == ZCRCG || byte == ZCRCQ || byte == ZCRCW) {
                break;
            }
            byte ^= 0x40;
        }

        buffer[count++] = byte;
        if (count >= BUFFER_SIZE) break;
    }

    *length = count;
    return true;
}

int main() {
    ClearVram();
    SetTileTable(tileset);
    SetFontTilesIndex(TILESET_SIZE);

    // Initialize UART for ZMODEM
    initializeUART();

    // SD card initialization
    u8 res;
    sdc_struct_t sd_struct;
    u8 sd_buf[512];
    u32 t32;

    sd_struct.bufp = &(sd_buf[0]);

    // Initialize SD card and filesystem
    res = FS_Init(&sd_struct);
    if (res != 0U) {
        Print(0, 0, txt_sdno);
        PrintChar(0, 1, res + '0');
        while(1);
    }

    // Find/create file on SD card
    t32 = FS_Find(&sd_struct,
        ((u16)('N') << 8) | ((u16)('E')),
        ((u16)('T') << 8) | ((u16)('L')),
        ((u16)('O') << 8) | ((u16)('A')),
        ((u16)('D') << 8) | ((u16)(' ')),
        ((u16)('B') << 8) | ((u16)('I')),
        ((u16)('N') << 8) | ((u16)(0)));

    if (t32 == 0U) {
        Print(0, 0, txt_filn);
        while(1);
    }

    FS_Select_Cluster(&sd_struct, t32);
    FS_Read_Sector(&sd_struct);

    Print(0, 0, txt_zmodem);

    // ZMODEM receive loop
    uint8_t frameType;
    int dataLength;
    bool receiving = true;

    // Send ZRINIT
    sendZModemHeader(ZRINIT);

    while(receiving) {
        if (receiveZModemHeader(&frameType)) {
            switch(frameType) {
                case ZFILE:
                    // Handle file info
                    receiveZModemData(receive_buffer, &dataLength);
                    // Extract file info here
                    sendZModemHeader(ZRPOS);
                    break;

                case ZDATA:
                    // Receive data block
                    if (receiveZModemData(receive_buffer, &dataLength)) {
                        // Write to SD buffer
                        for (int i = 0; i < dataLength; i++) {
                            sd_buf[sd_bufCount++] = receive_buffer[i];

                            if (sd_bufCount == 512) {
                                res = FS_Write_Sector(&sd_struct);
                                if (res != 0U) {
                                    PrintChar(2, 25, res + '0');
                                    while(1);
                                }
                                FS_Next_Sector(&sd_struct);
                                FS_Read_Sector(&sd_struct);
                                sd_bufCount = 0;
                                currentChunk++;
                                sdSector++;

                                if (sdSector == 1) {
                                    // Extract game info from first sector
                                    memcpy(gameName, &receive_buffer[14], 31);
                                    memcpy(gameAuthor, &receive_buffer[46], 31);
                                    gameYear0C = receive_buffer[12];
                                    gameYear0D = receive_buffer[13];
                                    gameYear = (gameYear0D<<8) | gameYear0C;
                                    printGameInfo();
                                }
                            }
                        }
                        sendZModemHeader(ZACK);
                    }
                    break;

                case ZEOF:
                    // Write any remaining data
                    if (sd_bufCount > 0) {
                        res = FS_Write_Sector(&sd_struct);
                        if (res != 0U) {
                            PrintChar(2, 25, res + '0');
                            while(1);
                        }
                    }
                    sendZModemHeader(ZRINIT);
                    break;

                case ZFIN:
                    // Transfer complete
                    sendZModemHeader(ZFIN);
                    receiving = false;
                    break;
            }
        }

        updateUI();
        WaitVsync(1);
    }

    // Boot the loaded game
    FS_Reset_Sector(&sd_struct);
    Bootld_Request(&sd_struct);

    while(1);
    return 0;
}
