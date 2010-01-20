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
 * This software is derived from software provided by kwikbyte without
 * copyright as follows:
 *
 * No warranty, expressed or implied, is included with this software.  It is
 * provided "AS IS" and no warranty of any kind including statutory or aspects
 * relating to merchantability or fitness for any purpose is provided.  All
 * intellectual property rights of others is maintained with the respective
 * owners.  This software is not copyrighted and is intended for reference
 * only.
 *
 * $FreeBSD$
 */

#include "at91rm9200.h"
#include "at91rm9200_lowlevel.h"
#include "lib.h"

/*
 * int getc(int seconds)
 * 
 * Reads a character from the DBGU port, if one is available within about
 * seconds seconds.  It assumes that DBGU has already been initialized.
 */
int
getc(int seconds)
{
	AT91PS_USART pUSART = (AT91PS_USART)AT91C_BASE_DBGU;
	unsigned	thisSecond;

	// Clamp to 20s
	if (seconds > 20)
	    seconds = 20;
	thisSecond = GetSeconds();
	seconds = thisSecond + seconds;
	do {
		if ((pUSART->US_CSR & AT91C_US_RXRDY))
			return (pUSART->US_RHR & 0xFF);
		thisSecond = GetSeconds();
	} while (thisSecond != seconds);
	return (-1);
}
