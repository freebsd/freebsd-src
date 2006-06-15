/*-
 * Copyright (c) 2006 IronPort Systems
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define MFIQ_FREE	0
#define MFIQ_BIO	1
#define MFIQ_READY	2
#define MFIQ_BUSY	3
#define MFIQ_COUNT	4

struct mfi_qstat {
	uint32_t	q_length;
	uint32_t	q_max;
};

union mfi_statrequest {
	uint32_t		ms_item;
	struct mfi_qstat	ms_qstat;
};

#define MAX_LINUX_IOCTL_SGE	16

struct mfi_linux_ioc_packet {
	uint16_t	lioc_adapter_no;
	uint16_t	lioc_pad1;
	uint32_t	lioc_sgl_off;
	uint32_t	lioc_sge_count;
	uint32_t	lioc_sense_off;
	uint32_t	lioc_sense_len;
	union {
		uint8_t raw[128];
		struct mfi_frame_header hdr;
	} lioc_frame;

	struct iovec lioc_sgl[MAX_LINUX_IOCTL_SGE];
} __packed;

#define MFIIO_STATS	_IOWR('Q', 101, union mfi_statrequest)

struct mfi_linux_ioc_aen {
	uint16_t	laen_adapter_no;
	uint16_t	laen_pad1;
	uint32_t	laen_seq_num;
	uint32_t	laen_class_locale;
} __packed;
