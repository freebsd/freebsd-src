/* Shared between kernel & process */

#ifndef	_SYS_WORMIO_H_
#define	_SYS_WORMIO_H_

#include <sys/ioccom.h>

/***************************************************************\
* Ioctls for the WORM drive					*
\***************************************************************/

/*
 * Quirk select: chose the set of quirk functions to use for this
 * device.
 */

struct wormio_quirk_select
{
	const char *vendor;	/* vendor name */
	const char *model;	/* model name */
};

#define WORMIOCQUIRKSELECT	_IOW('W', 10, struct wormio_quirk_select)

/*
 * Prepare disk-wide parameters.
 */

struct wormio_prepare_disk
{
	int dummy;		/* use dummy writes, laser turned off */
	int speed;		/* drive speed selection */
};

#define WORMIOCPREPDISK		_IOW('W', 20, struct wormio_prepare_disk)

/*
 * Prepare track-specific parameters.
 */

struct wormio_prepare_track
{
	int audio;		/* audio track (data track if 0) */
	int preemp;		/* audio with preemphasis */
};

#define WORMIOCPREPTRACK	_IOW('W', 21, struct wormio_prepare_track)

/*
 * Fixation: write leadins and leadouts.  Select table-of-contents
 * type for this session.  If onp is != 0, another session will be
 * opened.
 */

struct wormio_fixation
{
	int toc_type;		/* TOC type */
	int onp;		/* open next program area */
};

#define WORMIOCFIXATION		_IOW('W', 22, struct wormio_fixation)

#endif /* !_SYS_WORMIO_H_ */
