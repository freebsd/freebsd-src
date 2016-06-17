/* -*- Mode: c++ -*-
 *
 *  Copyright 2000 Massachusetts Institute of Technology
 *
 *  Permission to use, copy, modify, distribute, and sell this software and its
 *  documentation for any purpose is hereby granted without fee, provided that
 *  the above copyright notice appear in all copies and that both that
 *  copyright notice and this permission notice appear in supporting
 *  documentation, and that the name of M.I.T. not be used in advertising or
 *  publicity pertaining to distribution of the software without specific,
 *  written prior permission.  M.I.T. makes no representations about the
 *  suitability of this software for any purpose.  It is provided "as is"
 *  without express or implied warranty.
 *
 */

#ifndef OMNIMETER_H
#define OMNIMETER_H
// use the address of the second socket for both sockets
// (divide address space in half and use offsets to wrap second card accesses back to start of address space)
// Following values for programming Cirrus Logic chip
#define Socket1Base 0x40

#define SocketMemoryWindowLen    (0x00400000)
#define Socket0MemoryWindowStart (0x00800000)
#define Socket1MemoryWindowStart (Socket0MemoryWindowStart + SocketMemoryWindowLen)

#define SocketIOWindowLen        (0x00008000)
#define Socket1IOWindowStart     (SocketIOWindowLen)
#define Socket1IOWindowOffset    (0x00010000 - Socket1IOWindowStart)

// Following values for run-time access

//#define PCCardBase     (0xe4000000) //jca (0x30000000)
//#define PCCardBase     (0x30000000)
#define PCCardBase     (0xe0000000)  //jag

#define PCCard0IOBase (PCCardBase)
//#define PCCard0AttrBase (0xec000000) //jca (PCCardBase + 0x08000000)
#define PCCard0AttrBase (0xe8000000)
//#define PCCard0AttrBase (PCCardBase + 0x08000000)
//#define PCCard0MemBase (0xf4000000) //jca (PCCardBase + 0x0C000000)
//#define PCCard0MemBase (PCCardBase + 0x0C000000)
#define PCCard0MemBase (0xf0000000)

//#define PCCard1IOBase (PCCardBase + SocketIOWindowLen)  //jag
#define PCCard1IOBase (0xe4000000)
//#define PCCard1AttrBase (0xec000000 + SocketMemoryWindowLen)  //jag
#define PCCard1AttrBase (0xec000000)
//#define PCCard1MemBase (0xf4000000 + SocketMemoryWindowLen)  //jag
#define PCCard1MemBase (0xf4000000)

#define PCCardIndexRegister (PCCard0IOBase + 0x000003E0) //altered
#define PCCardDataRegister  (PCCardIndexRegister + 1)

/* interrupts */
#define PIN_cardInt2	13
#define PIN_cardInt1	5

void SMBOn(unsigned char SMBaddress);
void SetSMB(unsigned char SMBaddress, unsigned int dacValue);

#define GPIO_key6	0x00040000
#define GPIO_scl	0x01000000  // output,   SMB clock
#define GPIO_sda	0x02000000  // bidirect, SMB data
#define SMB_LCDVEE 0x2C
#define DefaultLCDContrast	16

#define LEDBacklightOn()	ClearGPIOpin(GPIO_key6)
#define LEDBacklightOff()	SetGPIOpin(GPIO_key6)
#define LCDPowerOn()			SMBOn(SMB_LCDVEE)
#define LCDPowerOff()			SMBOff(SMB_LCDVEE)
#define SetLCDContrast(d)		SetSMB(SMB_LCDVEE, d)
#define WritePort32(port,value) (port = (value))
#define ReadPort32(port) (port)
#define SetGPIOpin(pin)		WritePort32(GPSR,pin)
#define ClearGPIOpin(pin)	WritePort32(GPCR,pin)

void jcaoutb(long p, unsigned char data);
unsigned char jcainb(long p);
void jcaoutw(long p, unsigned short data);
unsigned short jcainw_p(long p);

#endif
