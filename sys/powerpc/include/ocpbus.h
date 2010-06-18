/*-
 * Copyright (c) 2006 Juniper Networks
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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

#ifndef _MACHINE_OCPBUS_H_
#define	_MACHINE_OCPBUS_H_

#define	OCPBUS_IVAR_DEVTYPE	1
#define	OCPBUS_IVAR_CLOCK	2
#define	OCPBUS_IVAR_HWUNIT	3
#define	OCPBUS_IVAR_MACADDR	4

/* Device types. */
#define	OCPBUS_DEVTYPE_PIC	1
#define	OCPBUS_DEVTYPE_TSEC	2
#define	OCPBUS_DEVTYPE_UART	3
#define	OCPBUS_DEVTYPE_QUICC	4
#define	OCPBUS_DEVTYPE_PCIB	5 
#define	OCPBUS_DEVTYPE_LBC	6
#define	OCPBUS_DEVTYPE_I2C	7
#define	OCPBUS_DEVTYPE_SEC	8

/* PIC IDs */
#define	OPIC_ID			0
#define	ATPIC_ID		1

#endif /* _MACHINE_OCPBUS_H_ */
