/*	$NetBSD: installboot.h,v 1.3 1998/01/05 07:02:57 perry Exp $	*/

ino_t createfileondev __P((char *, char *, char *, int));
void cleanupfileondev __P((char *, char *, int));

char *getmountpoint __P((char *));
void cleanupmount __P((char *));

extern int verbose;
