/*
 * Copyright (c) 2000,2001 Jonathan Chen.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Structure definitions for the Cardbus Bus driver
 */
struct cardbus_devinfo {
	struct pci_devinfo pci;
	uint8_t        mprefetchable; /* bit mask of prefetchable BARs */
	uint8_t        mbelow1mb; /* bit mask of BARs which require below 1Mb */
	uint8_t        ibelow1mb; /* bit mask of BARs which require below 1Mb */
#define        BARBIT(RID) (1<<(((RID)-CARDBUS_BASE0_REG)/4))
	uint16_t	mfrid;		/* manufacturer id */
	uint16_t	prodid;		/* product id */
	u_int		funcid;		/* function id */
	union {
		struct {
			u_int		type;		/* UART type */
		} sio;
		struct {
			uint8_t	nid[6];		/* MAC address */
			uint8_t	tech;		/* technology */
			uint8_t	contype;	/* connector type */
			uint32_t	speed[3];	/* available speeds */
			uint8_t	media[4];	/* media types */
		} lan;
	} funce;
	uint32_t	fepresent;	/* bit mask of funce values present */
};
