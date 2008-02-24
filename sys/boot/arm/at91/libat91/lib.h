/*-
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/boot/arm/at91/libat91/lib.h,v 1.4 2006/12/20 18:19:52 imp Exp $
 */

#ifndef ARM_BOOT_LIB_H
#define ARM_BOOT_LIB_H

int getc(int);
void putchar(int);
void xputchar(int);
void printf(const char *fmt,...);

/* The following function write eeprom at ee_addr using data 	*/
/*  from data_add for size bytes.				*/
int ReadEEPROM(unsigned eeoff, char *data_addr, unsigned size);
void WriteEEPROM(unsigned eeoff, char *data_addr, unsigned size);
void InitEEPROM(void);

/* XMODEM protocol */
int xmodem_rx(char *dst);

/*  */
void start_wdog(int n);
void reset(void);

/* Delay us */
void Delay(int us);

#define ToASCII(x) ((x > 9) ? (x + 'A' - 0xa) : (x + '0'))

int p_IsWhiteSpace(char cValue);
unsigned p_HexCharValue(char cValue);
unsigned p_ASCIIToHex(const char *buf);
unsigned p_ASCIIToDec(const char *buf);

void p_memset(char *buffer, char value, int size);
int p_strlen(const char *buffer);
char *strcpy(char *to, const char *from);
void memcpy(void *to, const void *from, unsigned size);
int p_memcmp(const char *to, const char *from, unsigned size);
int strcmp(const char *to, const char *from);

#endif
