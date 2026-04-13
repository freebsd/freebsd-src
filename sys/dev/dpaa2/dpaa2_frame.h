/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Dmitry Salychev
 * Copyright (c) 2026 Bjoern A. Zeeb
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

#ifndef _DPAA2_FRAME_H
#define _DPAA2_FRAME_H

#include <sys/types.h>
#include <sys/bus.h>
#include <sys/kassert.h>

#include "dpaa2_types.h"
#include "dpaa2_buf.h"

/*
 * Helper routines for the DPAA2 frames (e.g. descriptors, software/hardware
 * annotations, etc.).
 */

/*
 * DPAA2 frame descriptor size, field offsets and masks.
 *
 * See 3.1.1 Frame descriptor format,
 *     4.2.1.2.2 Structure of Frame Descriptors (FDs),
 * LX2160A DPAA2 Low-Level Hardware Reference Manual, Rev. 0, 06/2020
 */
#define DPAA2_FD_SIZE		32u
#define DPAA2_FD_FMT_MASK	(0x3u)
#define DPAA2_FD_FMT_SHIFT	(12)
#define DPAA2_FD_ERR_MASK	(0xFFu)
#define DPAA2_FD_ERR_SHIFT	(0)
#define DPAA2_FD_SL_MASK	(0x1u)
#define DPAA2_FD_SL_SHIFT	(14)
#define DPAA2_FD_LEN_MASK	(0x3FFFFu)
#define DPAA2_FD_OFFSET_MASK	(0x0FFFu)
#define DPAA2_FD_PTAC_PTV2_MASK	(0x1u)
#define DPAA2_FD_PTAC_PTV1_MASK	(0x2u)
#define DPAA2_FD_PTAC_PTA_MASK	(0x4u)
#define DPAA2_FD_PTAC_MASK	(0x7u)
#define DPAA2_FD_PTAC_SHIFT	(21)
#define DPAA2_FD_ASAL_MASK	(0xFu)
#define DPAA2_FD_ASAL_SHIFT	(16)

/*
 * DPAA2 frame annotation sizes
 *
 * NOTE: Accelerator-specific (HWA) annotation length is described in the 64-byte
 *       units by the FD[ASAL] bits and can be as big as 960 bytes. Current
 *       values describe what is actually supported by the DPAA2 drivers.
 *
 * See 3.1.1 Frame descriptor format,
 * LX2160A DPAA2 Low-Level Hardware Reference Manual, Rev. 0
 */
#define DPAA2_FA_SIZE			192u	/* DPAA2 frame annotation */
#define DPAA2_FA_SWA_SIZE		64u	/* SW frame annotation */
#define DPAA2_FA_HWA_SIZE		128u	/* HW frame annotation */
#define DPAA2_FA_WRIOP_SIZE		128u	/* WRIOP HW annotation */
#define DPAA2_FA_HWA_FAS_SIZE		8u	/* Frame annotation status */

/*
 * DPAA2 annotation valid bits in FD[FRC].
 *
 * See 7.31.2 WRIOP FD frame context (FRC),
 * LX2160A DPAA2 Low-Level Hardware Reference Manual, Rev. 0, 06/2020
 */
#define DPAA2_FD_FRC_FASV		(1 << 15)
#define DPAA2_FD_FRC_FAEADV		(1 << 14)
#define DPAA2_FD_FRC_FAPRV		(1 << 13)
#define DPAA2_FD_FRC_FAIADV		(1 << 12)
#define DPAA2_FD_FRC_FASWOV		(1 << 11)
#define DPAA2_FD_FRC_FAICFDV		(1 << 10)

/*
 * DPAA2 Frame annotation status word.
 *
 * See 7.34.3 Frame annotation status word (FAS),
 * LX2160A DPAA2 Low-Level Hardware Reference Manual, Rev. 0, 06/2020
 */
#define DPAA2_FAS_L3CV			(1 << 3) /* L3 csum validated */
#define DPAA2_FAS_L3CE			(1 << 2) /* L3 csum error */
#define DPAA2_FAS_L4CV			(1 << 1) /* L4 csum validated*/
#define DPAA2_FAS_L4CE			(1 << 0) /* L4 csum error */

/**
 * @brief DPAA2 frame descriptor.
 *
 * addr:		Memory address of the start of the buffer holding the
 *			frame data or the buffer containing the scatter/gather
 *			list.
 * data_length:		Length of the frame data (in bytes).
 * bpid_ivp_bmt:	Buffer pool ID (14 bit + BMT bit + IVP bit)
 * offset_fmt_sl:	Frame data offset, frame format and short-length fields.
 * frame_ctx:		Frame context. This field allows the sender of a frame
 *			to communicate some out-of-band information to the
 *			receiver of the frame.
 * ctrl:		Control bits (ERR, CBMT, ASAL, PTAC, DROPP, SC, DD).
 * flow_ctx:		Frame flow context. Associates the frame with a flow
 *			structure. QMan may use the FLC field for 3 purposes:
 *			stashing control, order definition point identification,
 *			and enqueue replication control.
 *
 * See 3.1.1 Frame descriptor format,
 *     4.2.1.2.2 Structure of Frame Descriptors (FDs),
 * LX2160A DPAA2 Low-Level Hardware Reference Manual, Rev. 0, 06/2020
 */
struct dpaa2_fd {
	uint64_t	addr;
	uint32_t	data_length;
	uint16_t	bpid_ivp_bmt;
	uint16_t	offset_fmt_sl;
	uint32_t	frame_ctx;
	uint32_t	ctrl;
	uint64_t	flow_ctx;
} __packed;
CTASSERT(sizeof(struct dpaa2_fd) == DPAA2_FD_SIZE);

/**
 * @brief WRIOP hardware frame annotation.
 *
 * See 7.34.2 WRIOP hardware frame annotation (FA),
 * LX2160A DPAA2 Low-Level Hardware Reference Manual, Rev. 0, 06/2020
 */
struct dpaa2_hwa_wriop {
	union {
		struct {
			uint64_t fas;
			uint64_t timestamp;
			/* XXX-DSL: more to add here... */
		} __packed;
		uint8_t raw[128];
	};
} __packed;
CTASSERT(sizeof(struct dpaa2_hwa_wriop) == DPAA2_FA_WRIOP_SIZE);

/**
 * @brief DPAA2 hardware frame annotation.
 *
 * See 3.4.1.2 Accelerator-specific annotation,
 * LX2160A DPAA2 Low-Level Hardware Reference Manual, Rev. 0, 06/2020 
 */
struct dpaa2_hwa {
	union {
		/* Keep fields common to all accelerators at the top. */
		struct {
			uint64_t fas;
		} __packed;
		/* Keep accelerator-specific annotations below. */
		struct dpaa2_hwa_wriop wriop;
	};
} __packed;
CTASSERT(sizeof(struct dpaa2_hwa) == DPAA2_FA_HWA_SIZE);

/**
 * @brief DPAA2 software frame annotation (pass-through annotation).
 *
 * See 3.4.1.1 Pass-through annotation,
 * LX2160A DPAA2 Low-Level Hardware Reference Manual, Rev. 0, 06/2020
 */
struct dpaa2_swa {
	union {
		struct {
			uint32_t	  magic;
			struct dpaa2_buf *buf;
		};
		struct {
			uint8_t pta1[32];
			uint8_t pta2[32];
		};
		uint8_t raw[64];
	};
} __packed;
CTASSERT(sizeof(struct dpaa2_swa) == DPAA2_FA_SWA_SIZE);

/**
 * @brief Frame annotation status word.
 *
 * See 7.34.3 Frame annotation status word (FAS),
 * LX2160A DPAA2 Low-Level Hardware Reference Manual, Rev. 0, 06/2020
 */
struct dpaa2_hwa_fas {
	uint8_t  _reserved1;
	uint8_t  ppid;
	uint16_t ifpid;
	uint32_t status;
} __packed;
CTASSERT(sizeof(struct dpaa2_hwa_fas) == DPAA2_FA_HWA_FAS_SIZE);

int  dpaa2_fd_build(device_t, const uint16_t, struct dpaa2_buf *,
    bus_dma_segment_t *, const int, struct dpaa2_fd *);

int  dpaa2_fd_err(struct dpaa2_fd *);
uint32_t dpaa2_fd_data_len(struct dpaa2_fd *);
int  dpaa2_fd_format(struct dpaa2_fd *);
bool dpaa2_fd_short_len(struct dpaa2_fd *);
int  dpaa2_fd_offset(struct dpaa2_fd *);

uint32_t dpaa2_fd_get_frc(struct dpaa2_fd *);
#ifdef _not_yet_
void dpaa2_fd_set_frc(struct dpaa2_fd *, uint32_t);
#endif

int  dpaa2_fa_get_swa(struct dpaa2_fd *, struct dpaa2_swa **);
int  dpaa2_fa_get_hwa(struct dpaa2_fd *, struct dpaa2_hwa **);
int  dpaa2_fa_get_fas(struct dpaa2_fd *, struct dpaa2_hwa_fas *);
#ifdef _not_yet_
int  dpaa2_fa_set_fas(struct dpaa2_fd *, struct dpaa2_hwa_fas *);
#endif

#endif /* _DPAA2_FRAME_H */
