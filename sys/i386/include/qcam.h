/*
 * Copyright (C) 1996 by Thomas Davis
 * Copyright (C) 1996 by Scott Laird
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL SCOTT LAIRD BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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
