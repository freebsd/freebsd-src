/*
	<dir.h> -- definitions for 4.2BSD-compatible directory access

	last edit:	09-Jul-1983	D A Gwyn
*/

#ifdef VMS
#ifndef FAB$C_BID
#include <fab.h>
#endif
#ifndef NAM$C_BID
#include <nam.h>
#endif
#ifndef RMS$_SUC
#include <rmsdef.h>
#endif
#include "dir.h"
#endif /* VMS */

#define DIRBLKSIZ	512		/* size of directory block */
#ifdef VMS
#define MAXNAMLEN	(DIR$S_NAME + 7) /* 80 plus room for version #.  */
#define MAXFULLSPEC	NAM$C_MAXRSS /* Maximum full spec */
#else
#define MAXNAMLEN	15		/* maximum filename length */
#endif /* VMS */
	/* NOTE:  MAXNAMLEN must be one less than a multiple of 4 */

struct direct				/* data from readdir() */
	{
	long		d_ino;		/* inode number of entry */
	unsigned short	d_reclen;	/* length of this record */
	unsigned short	d_namlen;	/* length of string in d_name */
	char		d_name[MAXNAMLEN+1];	/* name of file */
	};

typedef struct
	{
	int	dd_fd;			/* file descriptor */
	int	dd_loc;			/* offset in block */
	int	dd_size;		/* amount of valid data */
	char	dd_buf[DIRBLKSIZ];	/* directory block */
	}	DIR;			/* stream data from opendir() */

extern DIR		*opendir();
extern struct direct	*readdir();
extern long		telldir();
extern void		seekdir();
extern void		closedir();

#define rewinddir( dirp )	seekdir( dirp, 0L )
