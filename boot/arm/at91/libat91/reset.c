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
 * $FreeBSD$
 */

#include "at91rm9200.h"
#include "lib.h"

/*
 * void reset()
 * 
 * Forces a reset of the system.  Uses watchdog timer of '1', which
 * corresponds to 128 / SLCK seconds (SLCK is 32,768 Hz, so 128/32768 is
 * 1 / 256 ~= 5.4ms
 */
void
reset(void)
{
	// The following should effect a reset.
	AT91C_BASE_ST->ST_WDMR = 1 | AT91C_ST_RSTEN;
	AT91C_BASE_ST->ST_CR = AT91C_ST_WDRST;
}

/*
 * void start_wdog()
 *
 * Starts a watchdog timer.  We force the boot process to get to the point
 * it can kick the watch dog part of the ST part for the OS's driver.
 */
void
start_wdog(int n)
{
	// The following should effect a reset after N seconds.
	AT91C_BASE_ST->ST_WDMR = (n * (32768 / 128)) | AT91C_ST_RSTEN;
	AT91C_BASE_ST->ST_CR = AT91C_ST_WDRST;
}
