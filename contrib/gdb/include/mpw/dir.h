/* The <dir.h> header gives the layout of a directory. */

#pragma once

#ifndef _DIR_H
#define _DIR_H

#ifndef _TYPES_H		/* not quite right */
#include <sys/types.h>
#endif

#define	DIRBLKSIZ	512	/* size of directory block */

#ifndef DIRSIZ
#define	DIRSIZ	14
#endif

struct direct {
  ino_t d_ino;
  char d_name[DIRSIZ];
};

#endif /* _DIR_H */
