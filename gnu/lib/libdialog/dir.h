/*
 * include file for dir.c
 *
 * Copyright (c) 1995, Marc van Kempen
 *
 * All rights reserved.
 *
 * This software may be used, modified, copied, distributed, and
 * sold, in both source and binary form provided that the above
 * copyright and these terms are retained, verbatim, as the first
 * lines of this file.  Under no circumstances is the author
 * responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with
 * its use.
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/dirent.h>

typedef struct DirList {             /* structure to hold the directory entries */
    char        filename[MAXNAMLEN]; /* together with the stat-info per file */
    struct stat filestatus;          /* filename, or the name to which it points */
    int         link;                /* is it a link ? */
    char        *linkname;           /* the name of the file the link points to */
} DirList;

#ifndef TRUE
#define TRUE (1)
#endif
#ifndef FALSE
#define FALSE (0)
#endif

void get_dir(char *dirname, char *fmask, DirList **dir, int *n);
void get_filenames(DirList *d, int n, char ***names, int *nf);
void FreeDir(DirList *d, int n);
