/*
 * Copyright (c) 2000  Nick Sayer
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
 *
 */

#define CDEV_MAJOR	160

/*
 * Find the PCI device that holds the G10 register needed to map in the SPIC
 */
#define PIIX4_BUS	0
#define PIIX4_SLOT	7
#define PIIX4_FUNC	3
#define PIIX4_DEVID	0x71138086

#define G10A	(0x64)
#define G10L	(G10A + 2)

#define SPIC_IRQ_PORT	0x8034
#define SPIC_IRQ_SHIFT	22

/* Define SPIC model type */
#define SPIC_DEVICE_MODEL_TYPE1       1
#define SPIC_DEVICE_MODEL_TYPE2       2

/* type2 series specifics */
#define SPIC_SIRQ                     0x9b
#define SPIC_SLOB                     0x9c
#define SPIC_SHIB                     0x9d

/* ioports used for brightness and type2 events */
#define SPIC_DATA_IOPORT      0x62
#define SPIC_CST_IOPORT       0x66
