#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright © 2021-2022 Dmitry Salychev
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

#include <machine/bus.h>
#include <dev/dpaa2/dpaa2_mc.h>
#include <dev/dpaa2/dpaa2_swp.h>
#include <dev/dpaa2/dpaa2_bp.h>

/**
 * @brief QBMan software portal interface.
 *
 * Software portals are used by data path software executing on a processor core
 * to communicate with the Queue Manager (QMan) which acts as a central resource
 * in DPAA2, managing the queueing of data between multiple processor cores,
 * network interfaces, and hardware accelerators in a multicore SoC.
 */
INTERFACE dpaa2_swp;

/**
 * @brief Enqueue multiple frames to a frame queue using one Frame Queue ID.
 *
 * dev:		DPIO device.
 * fqid:	Frame Queue ID.
 * fd:		Frame descriptor to enqueue.
 * frames_n:	Number of frames to enqueue.
 */
METHOD int enq_multiple_fq {
	device_t		 dev;
	uint32_t		 fqid;
	struct dpaa2_fd		*fd;
	int			 frames_n;
}

/**
 * @brief Configure the channel data availability notification (CDAN)
 * in a particular WQ channel paired with DPIO.
 *
 * dev:		DPIO device.
 * ctx:		Context to configure data availability notifications (CDAN).
 */
METHOD int conf_wq_channel {
	device_t		 dev;
	struct dpaa2_io_notif_ctx *ctx;
};

/**
 * @brief Release one or more buffer pointers to a QBMan buffer pool.
 *
 * dev:		DPIO device.
 * bpid:	Buffer pool ID.
 * buf:		Array of buffers physical addresses.
 * buf_num:	Number of the buffers in the array.
 */
METHOD int release_bufs {
	device_t		 dev;
	uint16_t		 bpid;
	bus_addr_t		*buf;
	uint32_t		 buf_num;
};

/**
 * @brief Query current configuration/state of the buffer pool.
 *
 * dev:		DPIO device.
 * bpid:	Buffer pool ID.
 * conf:	Configuration/state of the buffer pool.
 */
METHOD int query_bp {
	device_t		 dev;
	uint16_t		 bpid;
	struct dpaa2_bp_conf	*conf;
}
