/*
 * Connectix QuickCam parallel-port camera video capture driver.
 * Copyright (c) 1996, Paul Traina.
 *
 * This driver is based in part on work
 * Copyright (c) 1996, Thomas Davis.
 *
 * QuickCam(TM) is a registered trademark of Connectix Inc.
 * Use this driver at your own risk, it is not warranted by
 * Connectix or the authors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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
 *
 * The following information is hardware dependant.  It should not be used
 * by user applications, see machine/qcam.h for the applications interface.
 */

#ifndef	_QCAMREG_H
#define	_QCAMREG_H

/*
 * Camera autodetection parameters
 */
#define	QC_PROBELIMIT		30		/* number of times to probe */
#define	QC_PROBECNTLOW		5		/* minimum transitions */
#define	QC_PROBECNTHI		25		/* maximum transitions */

/*
 * QuickCam camera commands
 */
#define	QC_BRIGHTNESS		0x0b
#define	QC_CONTRAST		0x19
#define	QC_WHITEBALANCE		0x1f
#define	QC_XFERMODE		0x07
#define	QC_XSIZE		0x13
#define	QC_YSIZE		0x11
#define	QC_YORG			0x0d
#define	QC_XORG			0x0f

/*
 * XFERmode register flags
 */
#define	QC_XFER_BIDIR		0x01		/* bidirectional transfer */
#define	QC_XFER_6BPP		0x02		/* 6 bits per pixel */
#define	QC_XFER_WIDE		0x00		/* wide angle */
#define	QC_XFER_NARROW		0x04		/* narrow */
#define	QC_XFER_TIGHT		0x08		/* very narrow */

/*
 * QuickCam default values (don't depend on these staying the same)
 */
#define	QC_DEF_XSIZE		160
#define	QC_DEF_YSIZE		120
#define	QC_DEF_XORG		7
#define	QC_DEF_YORG		1
#define	QC_DEF_BPP		6
#define	QC_DEF_CONTRAST		180
#define	QC_DEF_BRIGHTNESS	180
#define	QC_DEF_WHITEBALANCE	150
#define	QC_DEF_ZOOM		QC_ZOOM_100

/*
 * QuickCam parallel port handshake constants
 */
#define	QC_CTL_HIGHNIB		0x06
#define	QC_CTL_LOWNIB		0x0e
#define	QC_CTL_HIGHWORD		0x26
#define	QC_CTL_LOWWORD		0x2f

#endif	/* _QCAMREG_H */
