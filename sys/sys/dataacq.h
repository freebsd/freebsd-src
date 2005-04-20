/*
 * Header for general data acquisition definitions.
 *
 * $FreeBSD$
 */

#ifndef	_SYS_DATAACQ_H_
#define	_SYS_DATAACQ_H_

#include <sys/ioccom.h>

/* Period in microseconds between analog I/O samples.
 */
#define AD_MICRO_PERIOD_SET _IOW('A', 1, long)
#define AD_MICRO_PERIOD_GET _IOR('A', 2, long)

/* Gain list support.  Initially all gains are 1.  If the board
 * supports no gains at all then AD_NGAINS_GET will return a 0.
 *
 * AD_NGAINS_GET: Return the number of gains the board supports
 *
 * AD_SUPPORTED_GAINS: Get the supported gains.
 * The driver will copy out "ngains" doubles,
 * where "ngains" is obtained with AD_NGAINS_GET.
 *
 * AD_GAINS_SET: Set the gain list.  The driver will copy in "ngains" ints.
 *
 * AD_GAINS_GET: Get the gain list.  The driver will copy out "ngains" ints.
 */

#define AD_NGAINS_GET     _IOR('A', 3, int)
#define AD_NCHANS_GET     _IOR('A', 4, int)
#define AD_SUPPORTED_GAINS _IO('A', 5)
#define AD_GAINS_SET       _IO('A', 6)
#define AD_GAINS_GET       _IO('A', 7)

#endif /* !_SYS_DATAACQ_H_ */
