/*
 * Copyright (c) 1996, David Greenman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#define CY_PCI_BASE_ADDR0		0x10
#define CY_PCI_BASE_ADDR1		0x14
#define CY_PCI_BASE_ADDR2		0x18

#define CY_PLX_9050_ICS			0x4c
#define CY_PLX_9060_ICS			0x68
#define CY_PLX_9050_ICS_IENABLE		0x040
#define CY_PLX_9050_ICS_LOCAL_IENABLE	0x001
#define CY_PLX_9060_ICS_IENABLE		0x100
#define CY_PLX_9060_ICS_LOCAL_IENABLE	0x800

/* Cyclom-Y Custom Register for PLX ID. */
#define	PLX_VER				0x3400
#define	PLX_9050			0x0b
#define	PLX_9060			0x0c
#define	PLX_9080			0x0d
