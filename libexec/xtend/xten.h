/*-
 * Copyright (c) 1992, 1993 Eugene W. Stark
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Eugene W. Stark.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY EUGENE W. STARK (THE AUTHOR) ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

extern char *X10housenames[];
extern char *X10cmdnames[];

#define ALLUNITSOFF	16
#define ALLLIGHTSON	17
#define UNITON		18
#define UNITOFF		19
#define DIM		20
#define BRIGHT		21
#define ALLLIGHTSOFF	22
#define EXTENDEDCODE	23
#define HAILREQUEST	24
#define HAILACKNOWLEDGE	25
#define PRESETDIM0	26
#define PRESETDIM1	27
#define EXTENDEDDATA	28
#define STATUSON	29
#define STATUSOFF	30
#define STATUSREQUEST	31

/*
 * Flags for first byte of received packet
 */

#define TW_RCV_LOCAL	1  /* The packet arrived during a local transmission */
#define TW_RCV_ERROR	2  /* An invalid/corrupted packet was received */

