/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 Scott Long
 * Copyright (c) 2000 BSDi
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
 *	$FreeBSD$
 */

/*
 * Command queue statistics
 */
#define AACQ_FREE	0
#define AACQ_BIO	1
#define AACQ_READY	2
#define AACQ_BUSY	3
#define AACQ_COMPLETE	4
#define AACQ_COUNT	5	/* total number of queues */

struct aac_qstat {
    u_int32_t	q_length;
    u_int32_t	q_max;
};

/*
 * Statistics request
 */
union aac_statrequest {
    u_int32_t		as_item;
    struct aac_qstat	as_qstat;
};

#define AACIO_STATS		_IOWR('T', 101, union aac_statrequest)

#ifdef AAC_COMPAT_LINUX

/*
 * Ioctl commands likely to be submitted from a Linux management application.
 * These bit encodings are actually descended from Windows NT.  Ick.
 */

#define CTL_CODE(devType, func, meth, acc) (((devType) << 16) | ((acc) << 14) | ((func) << 2) | (meth))
#define METHOD_BUFFERED                 0
#define METHOD_IN_DIRECT                1
#define METHOD_OUT_DIRECT               2
#define METHOD_NEITHER                  3
#define FILE_ANY_ACCESS                 0
#define FILE_READ_ACCESS          	( 0x0001 )
#define FILE_WRITE_ACCESS         	( 0x0002 )
#define FILE_DEVICE_CONTROLLER          0x00000004

#define FSACTL_SENDFIB			CTL_CODE(FILE_DEVICE_CONTROLLER, 2050, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSACTL_AIF_THREAD		CTL_CODE(FILE_DEVICE_CONTROLLER, 2127, METHOD_NEITHER, FILE_ANY_ACCESS)
#define FSACTL_OPEN_GET_ADAPTER_FIB	CTL_CODE(FILE_DEVICE_CONTROLLER, 2100, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSACTL_GET_NEXT_ADAPTER_FIB	CTL_CODE(FILE_DEVICE_CONTROLLER, 2101, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSACTL_CLOSE_GET_ADAPTER_FIB	CTL_CODE(FILE_DEVICE_CONTROLLER, 2102, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSACTL_MINIPORT_REV_CHECK	CTL_CODE(FILE_DEVICE_CONTROLLER, 2107, METHOD_BUFFERED, FILE_ANY_ACCESS)

/*
 * Support for faking the "miniport" version.
 */
struct aac_rev_check {
    RevComponent	callingComponent;
    struct FsaRevision	callingRevision;
};

struct aac_rev_check_resp {
    int			possiblyCompatible;
    struct FsaRevision	adapterSWRevision;
};

/*
 * Context passed in by a consumer looking to collect an AIF.
 */
#define AAC_AIF_SILLYMAGIC	0xdeadbeef
struct get_adapter_fib_ioctl {
    u_int32_t	AdapterFibContext;
    int	  	Wait;
    caddr_t	AifFib;
};

#endif
