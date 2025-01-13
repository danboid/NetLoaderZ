//-----------------------------------------------------------------------------
// Project:		ZModem
// Version:		V2.02
// Date:		21 April 2002
// Author:		Radu Hristea (radu.hristea@aptrans-group.com; transeast@programmer.net)
// File:		funct.c
// Description:	This module contains the function prototypes
//-----------------------------------------------------------------------------

/* ZModem.c */
   // actions
byte rZModem_ZSend(void);
byte rZModem_ZReceive(void);
   // layer 3 methods
byte rZModem_sendFiles(void);
byte rZModem_sendFile(void);
void rZModem_receiveFile(void);
void rZModem_receiveData(void);
   // layer 2 methods
void rZModem_sendFileInfo(void);
void rZModem_sendCAN(void);
void rZModem_getZMHeader(void);
void rZModem_getOO(void);
   // layer 1 methods
void rZModem_sendHexHeader(byte hType);
void rZModem_sendBinHeader(byte hType);
void rZModem_sendData(byte frameEnd);   
void rZModem_getHexHeader(void);
void rZModem_getBinaryHeader(void);
void rZModem_getData(void);
byte rZModem_positionMatch(void);
   // layer 0 methods
void rZModem_sendHexChar(void);
void rZModem_sendDLEChar(void);
void rZModem_getNextHexCh(void);
void rZModem_getNextDLECh(void); 
   // CRC computing
word rZModem_crcUpdate(word crc, byte serialData);   
   
/* Comm.c */
byte rComm_getUART(byte* buffer);
byte rComm_readByte(byte* buffer);
void rComm_clearInBound(void); 
   
/* Dummy.c */
void rDummy_readFromFile(byte* Buffer, word nrByteToRead, word* nrByteRead); 
void rDummy_writeToFile(byte c);  

