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
 * NOTE: this file contains the sole public interface between the driver
 * and user applications.  Every effort should be made to retain comaptibility
 * with the decided upon standard interface shared between Linux and
 * FreeBSD.  Currently, FreeBSD uses a different (richer) interface than
 * the Linux.
 *
 * WARNING WARNING: The contents of this structure is in flux,
 *		    recompile often, the driver will change the version
 *		    number when the interface changes for now...
 */

#ifndef _QUICKCAM_H
#define _QUICKCAM_H 1

#ifndef	_IOR
/* SCO doesn't have _IOR/_IOW for ioctls, so fake it out to keep things clean*/
#define	_IOR(cat, func, data)	(((cat) << 8) | (func))
#define	_IOW(cat, func, data)	(((cat) << 8) | (func))
#endif

/*
 * ioctls
 */
#define QC_GET	_IOR('S', 1, struct qcam)	/* get parameter structure */
#define QC_SET	_IOW('S', 2, struct qcam)	/* set parameter structure */

#define	QC_IOCTL_VERSION	3		/* version of the structure */

struct qcam {
	int	qc_version;			/* version of qcam structure */
	int	qc_xsize;			/* size in pixels */
	int	qc_ysize;			/* size in pixels */
	int	qc_xorigin;			/* x origin */
	int	qc_yorigin;			/* y origin */
	int	qc_bpp;				/* bits per pixel (4 or 6) */
	int	qc_zoom;			/* zoom mode */
	int	qc_exposure;			/* length of exposure */
	u_char	qc_brightness;			/* 0..255 */
	u_char	qc_whitebalance;		/* 0..255 */
	u_char	qc_contrast;			/* 0..255 */
};

#define	QC_MAX_XSIZE		320		/* pixels */
#define	QC_MAX_YSIZE		240		/* pixels */

/*
 * zoom flags
 */
#define	QC_ZOOM_100		0x00		/* no zoom */
#define	QC_ZOOM_150		0x01		/* 1.5x */
#define	QC_ZOOM_200		0x02		/* 2.0x */

#endif
