/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: dest_tkt.c,v 4.9 89/10/02 16:23:07 jtkohl Exp $
 *	$FreeBSD$
 */

#if 0
#ifndef lint
static char *rcsid =
"$FreeBSD$";
#endif /* lint */
#endif

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <krb.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef TKT_SHMEM
#include <sys/param.h>
#endif
#include <errno.h>

/*
 * dest_tkt() is used to destroy the ticket store upon logout.
 * If the ticket file does not exist, dest_tkt() returns RET_TKFIL.
 * Otherwise the function returns RET_OK on success, KFAILURE on
 * failure.
 *
 * The ticket file (TKT_FILE) is defined in "krb.h".
 */

int
dest_tkt()
{
    char *file = TKT_FILE;
    int i,fd;
    extern int errno;
    struct stat statb;
    char buf[BUFSIZ];
#ifdef TKT_SHMEM
    char shmidname[MAXPATHLEN];
#endif /* TKT_SHMEM */

    errno = 0;
    if (lstat(file,&statb) < 0)
	goto out;

    if (!(statb.st_mode & S_IFREG)
#ifdef notdef
	|| statb.st_mode & 077
#endif
	)
	goto out;

    if ((fd = open(file, O_RDWR, 0)) < 0)
	goto out;

    bzero(buf, BUFSIZ);

    for (i = 0; i < statb.st_size; i += BUFSIZ)
	if (write(fd, buf, BUFSIZ) != BUFSIZ) {
	    (void) fsync(fd);
	    (void) close(fd);
	    goto out;
	}

    (void) fsync(fd);
    (void) close(fd);

    (void) unlink(file);

out:
    if (errno == ENOENT) return RET_TKFIL;
    else if (errno != 0) return KFAILURE;
#ifdef TKT_SHMEM
    /*
     * handle the shared memory case
     */
    (void) strcpy(shmidname, file);
    (void) strcat(shmidname, ".shm");
    if ((i = krb_shm_dest(shmidname)) != KSUCCESS)
	return(i);
#endif /* TKT_SHMEM */
    return(KSUCCESS);
}
