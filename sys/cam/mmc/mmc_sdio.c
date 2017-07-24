/*-
 * Copyright (c) 2015 Ilya Bakulin <ilya@bakulin.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/interrupt.h>
#include <sys/sbuf.h>

#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/condvar.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_queue.h>
#include <cam/cam_periph.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_xpt_internal.h>
#include <cam/cam_debug.h>

#include <cam/mmc/mmc.h>
#include <cam/mmc/mmc_bus.h>
#include <cam/mmc/mmc_sdio.h>

#include <machine/stdarg.h>	/* for xpt_print below */
#include <machine/_inttypes.h>  /* for PRIu64 */
#include "opt_cam.h"


void
sdio_print_stupid_message(struct cam_periph *periph) {

	CAM_DEBUG(periph->path, CAM_DEBUG_INFO,
		  ("%s\n", __func__));
}

/*
 * f -  function to read from / write to
 * wr - is write
 * adr - address to r/w
 * data - actual data to write
 */
void sdio_fill_mmcio_rw_direct(union ccb *ccb, uint8_t f, uint8_t wr, uint32_t adr, uint8_t *data) {
        struct ccb_mmcio *mmcio;

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE,
		  ("sdio_fill_mmcio(f=%d, wr=%d, adr=%02x, data=%02x)\n", f, wr, adr, (data == NULL ? 0 : *data)));
	mmcio = &ccb->mmcio;

        mmcio->cmd.opcode = SD_IO_RW_DIRECT;
        mmcio->cmd.arg = SD_IO_RW_FUNC(f) | SD_IO_RW_ADR(adr);
        if (wr)
                mmcio->cmd.arg |= SD_IO_RW_WR | SD_IO_RW_RAW | SD_IO_RW_DAT(*data);
        mmcio->cmd.flags = MMC_RSP_R5 | MMC_CMD_AC;
        mmcio->cmd.data->len = 0;
}

uint8_t sdio_parse_mmcio_rw_direct(union ccb *ccb, uint8_t *data) {
        struct ccb_mmcio *mmcio;

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE,
		  ("sdio_parse_mmcio(datap=%p)\n", data));
	mmcio = &ccb->mmcio;

        if (mmcio->cmd.error)
                return (mmcio->cmd.error);
	if (mmcio->cmd.resp[0] & R5_COM_CRC_ERROR)
		return (MMC_ERR_BADCRC);
	if (mmcio->cmd.resp[0] & (R5_ILLEGAL_COMMAND | R5_FUNCTION_NUMBER))
		return (MMC_ERR_INVALID);
	if (mmcio->cmd.resp[0] & R5_OUT_OF_RANGE)
		return (MMC_ERR_FAILED);

	/* Just for information... */
	if (R5_IO_CURRENT_STATE(mmcio->cmd.resp[0]) != 1)
		printf("!!! SDIO state %d\n", R5_IO_CURRENT_STATE(mmcio->cmd.resp[0]));

	if (mmcio->cmd.resp[0] & R5_ERROR)
		printf("An error was detected!\n");

	if (mmcio->cmd.resp[0] & R5_COM_CRC_ERROR)
		printf("A CRC error was detected!\n");

        if (data != NULL)
                *data = (uint8_t) (mmcio->cmd.resp[0] & 0xff);
	return (MMC_ERR_NONE);

}
