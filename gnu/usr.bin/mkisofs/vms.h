/*
 * Header file mkisofs.h - assorted structure definitions and typecasts.

   Written by Eric Youngdale (1993).

  */

#ifdef VMS
#define stat(X,Y) VMS_stat(X,Y)
#define lstat VMS_stat

/* gmtime not available under VMS - make it look like we are in Greenwich */
#define gmtime localtime

#define S_ISBLK(X) (0)
#define S_ISCHR(X) (0)
#define S_ISREG(X)  (((X) & S_IFMT) == S_IFREG)
#define S_ISDIR(X)  (((X) & S_IFMT) == S_IFDIR)
#endif
