/*
 * Copyright (C) 2000
 * Dr. Duncan McLennan Barclay, dmlb@ragnet.demon.co.uk.
 *
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY DUNCAN BARCLAY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DUNCAN BARCLAY OR CONTRIBUTORS BE LIABLE
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

/*
 * Debugging odds and odds
 */

/*
 * RAY_DEBUG settings
 *
 *	RECERR		Recoverable error's, deprecated use RAY_RECERR macro
 *	SUBR		Subroutine entry
 *	BOOTPARAM	Startup CM dump
 *	STARTJOIN	State transitions for start/join
 *	CCS		CCS info
 *	IOCTL		IOCTL calls
 *	MBUF		MBUFs dumped - needs one of TX, RX, MGT, or CTL
 *	RX		packet types reported
 *	CM		common memory re-mapping
 *	COM		new command sleep/wakeup
 *	STOP		driver detaching
 *	CTL		CTL packets
 *	MGT		MGT packets
 *	TX		TX routine info
 *	DCOM		dump comq entries
 */
#define RAY_DBG_RECERR		0x0001
#define RAY_DBG_SUBR		0x0002
#define RAY_DBG_BOOTPARAM	0x0004
#define RAY_DBG_STARTJOIN	0x0008
#define RAY_DBG_CCS		0x0010
#define RAY_DBG_IOCTL		0x0020
#define RAY_DBG_MBUF		0x0080
#define RAY_DBG_RX		0x0100
#define RAY_DBG_CM		0x0200
#define RAY_DBG_COM		0x0400
#define RAY_DBG_STOP		0x0800
#define RAY_DBG_CTL		0x1000
#define RAY_DBG_MGT		0x2000
#define RAY_DBG_TX		0x4000
#define RAY_DBG_DCOM		0x8000
/* Cut and paste this into a kernel configuration file */
#if 0
#define RAY_DEBUG	(				\
 			/* RAY_DBG_SUBR		| */ 	\
			/* RAY_DBG_BOOTPARAM	| */	\
			/* RAY_DBG_STARTJOIN	| */	\
			/* RAY_DBG_CCS		| */	\
                        /* RAY_DBG_IOCTL	| */	\
                        /* RAY_DBG_MBUF		| */ 	\
                        /* RAY_DBG_RX		| */	\
                        /* RAY_DBG_CM		| */	\
                        /* RAY_DBG_COM		| */	\
                        /* RAY_DBG_STOP		| */	\
                        /* RAY_DBG_CTL		| */ 	\
                        /* RAY_DBG_MGT		| */  	\
                        /* RAY_DBG_TX		| */  	\
                        /* RAY_DBG_DCOM		| */  	\
			0				\
			)
#endif

#if RAY_DEBUG

#define RAY_DPRINTF(sc, mask, fmt, args...) do {if (RAY_DEBUG & (mask)) {\
    device_printf((sc)->dev, "%s(%d) " fmt "\n",			\
    	__FUNCTION__ , __LINE__ , ##args);				\
} } while (0)

/* This macro assumes that common memory is mapped into kernel space */
#define RAY_DHEX8(sc, mask, off, len, s) do { if (RAY_DEBUG & (mask)) {	\
    int i, j;								\
    device_printf((sc)->dev, "%s(%d) %s\n",				\
    	__FUNCTION__ , __LINE__ , (s));					\
    for (i = (off); i < (off)+(len); i += 8) {				\
	    printf(".  0x%04x ", i);					\
	    for (j = 0; j < 8; j++)					\
		    printf("%02x ", SRAM_READ_1((sc), i+j));		\
	    printf("\n");						\
    }									\
} } while (0)

#define RAY_DCOM(sc, mask, com, s) do { if (RAY_DEBUG & (mask)) {	\
    device_printf((sc)->dev, "%s(%d) %s com entry 0x%p\n",		\
        __FUNCTION__ , __LINE__ , (s) , (com));				\
    printf(".  c_mesg %s\n", (com)->c_mesg);				\
    printf(".  c_flags 0x%b\n", (com)->c_flags, RAY_COM_FLAGS_PRINTFB);	\
    printf(".  c_retval 0x%x\n", (com)->c_retval);			\
    printf(".  c_ccs 0x%0x index 0x%02x\n",				\
        (com)->c_ccs, RAY_CCS_INDEX((com)->c_ccs));			\
} } while (0)

#else
#define RAY_DPRINTF(sc, mask, fmt, args...)
#define RAY_DHEX8(sc, mask, off, len, s)
#define RAY_DCOM(sc, mask, com, s)
#endif /* RAY_DEBUG > 0 */

/*
 * These override macros defined in if_ray.c to turn them into
 * debugging ones.
 */
#if RAY_DEBUG & RAY_DBG_COM

#define RAY_COM_CHECK(sc, com) do { if (RAY_DEBUG & RAY_DBG_COM) {	\
    ray_com_ecf_check((sc), (com), __FUNCTION__ );			\
} } while (0)

#endif /* RAY_DEBUG & RAY_DBG_COM */

#if RAY_DEBUG & RAY_DBG_MBUF
#define RAY_MBUF_DUMP(sc, mask, m, s)	do { if (RAY_DEBUG & (mask)) {	\
    ray_dump_mbuf((sc), (m), (s));					\
} } while (0)
#endif /* RAY_DEBUG & RAY_DBG_MBUF */
