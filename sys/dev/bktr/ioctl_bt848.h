/*
 * extensions to ioctl_meteor.h for the bt848 cards
 *
 * $Id$
 */

/*
 * tuner types for the 
 */
#define TUNERTYPE_NABCST	1
#define TUNERTYPE_CABLEIRC	2
#define TUNERTYPE_CABLEHRC	3
#define TUNERTYPE_WEUROPE	4


/*
 * XXX: this is a hack, should be in ioctl_meteor.h
 * here to avoid touching that file for now...
 */
#define	TVTUNER_SETCHNL    _IOW('x', 32, unsigned int)	/* set channel */
#define	TVTUNER_GETCHNL    _IOR('x', 32, unsigned int)	/* get channel */
#define	TVTUNER_SETTYPE    _IOW('x', 33, unsigned int)	/* set tuner type */
#define	TVTUNER_GETTYPE    _IOR('x', 33, unsigned int)	/* get tuner type */
#define	TVTUNER_GETSTATUS  _IOR('x', 34, unsigned int)	/* get tuner status */


/*
 * XXX: more bad magic,
 *      we need to fix the METEORGINPUT to return something public
 *      duplicate them here for now...
 */
#define	METEOR_DEV0		0x00001000
#define	METEOR_DEV1		0x00002000
#define	METEOR_DEV2		0x00004000
