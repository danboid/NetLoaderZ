//-----------------------------------------------------------------------------
// Project:		ZModem
// Version:		V2.02
// Date:		21 April 2002
// Author:		Radu Hristea (radu.hristea@aptrans-group.com; transeast@programmer.net)
// File:		defs.c
// Description:	This module contains all the defines used by the project
//-----------------------------------------------------------------------------

// frametypes
#define ZPAD			'*'
#define ZBIN 			'A'
#define ZHEX 			'B'
#define ZBIN32 			'C'

// headertypes
#define ZRQINIT			0	/* Request attention */
#define ZRINIT			1	/* Attention header */
#define ZSINIT			2	/* */
#define ZACK			3	/* Acknowlege request */
#define ZFILE			4	/* File name from sender */
#define ZSKIP			5	/* To sender: skip this file */
#define ZNAK			6	/* Last packet was garbled */
#define ZABORT			7	/* Abort batch transfers */
#define ZFIN			8	/* Finish session */
#define ZRPOS			9	/* Resume data trans at this position */
#define ZDATA			10	/* Data packet(s) follow */
#define ZEOF			11	/* End of file */
#define ZFERR			12	/* Fatal Read or Write error Detected */
#define ZCRC			13	/* Request for file CRC and response */
#define ZCHALLENGE		14	/* Receiver's Challenge */
#define ZCOMPL			15	/* Request is complete */
#define ZCAN			16	/* Other end canned session with CAN*5 */
#define ZFREECNT		17	/* Request for free bytes on filesystem */
#define ZCOMMAND		18	/* Command from sending program */
#define ZSTDERR			19	/* Output to standard error, data follows */

// ZDLE sequences
#define ZCRCE 'h'	/* CRC next, frame ends, header packet follows */
#define ZCRCG 'i'	/* CRC next, frame continues nonstop */
#define ZCRCQ 'j'	/* CRC next, frame continues, ZACK expected */
#define ZCRCW 'k'	/* CRC next, ZACK expected, end of frame */
#define ZRUB0 'l'	/* Translate to rubout 0177 */
#define ZRUB1 'm'	/* Translate to rubout 0377 */

// misc ZModem properties
#define CANFC32		0x20
#define CANFDX		0x01
#define CANOVIO		0x02

// special chars and flags
#define ZDLE		0x18
#define CAN			0x18 // same as ZDLE
#define ZCNL		0x02
#define ZCBIN		0x01
#define ZCRECOV		0x03

// states of the zmodem state machine
#define SM_SENDZDATA	0	//sending / receiving file information
#define SM_SENDZEOF		1	//finishing file
#define SM_SENDDATA		2	//data transfer is running
#define SM_ACTIONRPOS	3	//reposition the filepointer during sending or receiving
#define SM_WAITZRINIT	4	//waiting for initalisation finish
#define SM_GETHEADER	5   //waiting for a header from the receiver

// errors used by this implementation
#define NO_ERROR    				0x00    //just ... no error  
#define ZMODEM_INIT					0x01	//zmodem-initialization failed
#define ZMODEM_POS					0x02	//force reposition
#define ZMODEM_ZDATA				0x03	//
#define ZMODEM_CRC  				0x04	//16-bit-checksum error
#define ZMODEM_LONGSP				0x05	//too long subpaket recieved
#define ZMODEM_CRC32				0x06	//32-bit-checksum error
#define ZMODEM_FILEDATA				0x07	//filedata has errors or missing
#define ZMODEM_BADHEX				0x08	//unexpected hex-char received (in a hex header)
#define ZMODEM_TIMEOUT				0x09	//by name
#define ZMODEM_GOTZCAN				0x0A	//cancel recieved (form other side)
#define ZMODEM_ABORTFROMOUTSIDE		0x0B	//user break
#define ZMODEM_ERROR_FILE			0x0C	//file handling error (during open, create, read, write)

/* my DEFS */
#define byte  unsigned char
#define word  unsigned int
#define dword unsigned long 
//
#define TRUE 		1
#define FALSE 		0  
//
#define ZMAX_BUF  		1024   	// nr of bytes in the TX buffer
#define ZMAX_RETRY	 	10		// nr of retries
//
#define FILE_SIZE     	55555L 	// dummy size...
//
#define CR				0x0D
#define LF				0x0A
#define XON				0x11  

// macros
#define ALLOK  			(LastError == NO_ERROR)
//#define wdogtrig()	wdt_enable(WDTO_1S);

// include the functions prototypes
#include "funct.h"

