/*
 * If we're doing flock(2) emulation, we need to get the LOCK_* #defines.
 * This stub <sys/file.h> includes the real one, and, if they're not in
 * it, we #define them here.
 */

#include </usr/include/sys/file.h>

#ifndef LOCK_SH
/* lock operations for flock(2) */
#define	LOCK_SH		0x01		/* shared file lock */
#define	LOCK_EX		0x02		/* exclusive file lock */
#define	LOCK_NB		0x04		/* don't block when locking */
#define	LOCK_UN		0x08		/* unlock file */
#endif
