/*
 * Video spigot capture driver.
 *
 * Copyright (c) 1995, Jim Lowe.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. 2.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Version 1.2, Aug 30, 1995.
 */

#ifndef	_MACHINE_SPIGOT_H_
#define	_MACHINE_SPIGOT_H_

#include <sys/ioccom.h>

struct spigot_info {
	unsigned long	maddr;
	unsigned short	irq;
};

/*
 * Get memory address.
 */
#define SPIGOT_GET_INFO		_IOR('s', 4, struct spigot_info)
/*
 * Set up a user interrupt.
 */
#define	SPIGOT_SETINT		_IOW('s', 5, int)
/*
 * Allow/disallow access to the I/O Page.
 */
#define	SPIGOT_IOPL_ON		_IO ('s', 6)
#define	SPIGOT_IOPL_OFF		_IO ('s', 7)

#ifndef KERNEL
/*
 * Defines for spigot library.
 */
unsigned short *	spigot_open(char *dev);
void			spigot_close(void);
void			spigot_set_capture_size(int width, int vtof);
unsigned char		spigot_start_xfer(int num_frames);
void			spigot_stop_xfer(void);
unsigned char		spigot_status(void);

/*
 * Define the status bits.
 */
#define	SPIGOT_COLOR			0x01	/* Color present (No color) */
#define	SPIGOT_60HZ			0x02	/* 60 hz input signal (50hz) */
#define	SPIGOT_NO_HORIZONTAL_LOCK	0x04	/* Horizontal lock present */
#define	SPIGOT_HPLL_LOCKED		0x08	/* HPLL locked (HPLL unlocked)*/
#define	SPIGOT_VCR_MODE			0x10	/* VCR mode (TV mode) */
#define	SPIGOT_VSYNC_PRESENT		0x20	/* Vsync present */

/*
 * spigot_open() returns a data address pointing to the spigot data.
 * Each read from this address returns the next word.  The ``dev'' passed
 * is usually "/dev/spigot".  Data is described in the phillips desktop
 * video data handbook under the 7191 chip.  Formats may be either
 * YUV 4:2:2 or YUV 4:1:1.  A sample device driver for ``nv'' is included
 * with this code.
 *
 * spigot_close() cleans up and closes the device.
 *
 * spigot_set_capture_size() will set the capture window size.  Width should be
 * one of:	80, 160, 240, 320, or 640 for NTSC or
 *		96, 192, 288, 384 for PAL.
 * vtof is the Vertical top of frame offset and must be between 0 and 15 lines.
 *
 * spigot_start_xfer() will start a transfer from the 7191 to the data fifo.
 * spigot_stop_xfer() will clear the data fifo and abort any transfers.
 *
 * spigot_status() will return the above status bits.
 */
#endif /* !KERNEL */

#endif /* !_MACHINE_SPIGOT_H_ */
