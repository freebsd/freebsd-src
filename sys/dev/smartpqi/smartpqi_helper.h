/*-
 * Copyright 2016-2023 Microchip Technology, Inc. and/or its subsidiaries.
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


#ifndef _PQI_HELPER_H
#define _PQI_HELPER_H


inline uint64_t
pqisrc_increment_device_active_io(pqisrc_softstate_t *softs, pqi_scsi_dev_t *device)
{
#if PQISRC_DEVICE_IO_COUNTER
	/*Increment device active io count by one*/
	return OS_ATOMIC64_INC(&device->active_requests);
#endif
}

inline uint64_t
pqisrc_decrement_device_active_io(pqisrc_softstate_t *softs,  pqi_scsi_dev_t *device)
{
#if PQISRC_DEVICE_IO_COUNTER
	/*Decrement device active io count by one*/
	return OS_ATOMIC64_DEC(&device->active_requests);
#endif
}

inline void
pqisrc_init_device_active_io(pqisrc_softstate_t *softs, pqi_scsi_dev_t *device)
{
#if PQISRC_DEVICE_IO_COUNTER
	/* Reset device count to Zero */
	OS_ATOMIC64_INIT(&device->active_requests, 0);
#endif
}

inline uint64_t
pqisrc_read_device_active_io(pqisrc_softstate_t *softs, pqi_scsi_dev_t *device)
{
#if PQISRC_DEVICE_IO_COUNTER
	/* read device active count*/
	return OS_ATOMIC64_READ(&device->active_requests);
#endif
}
#endif  /* _PQI_HELPER_H */
