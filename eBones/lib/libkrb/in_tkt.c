/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: kt.c,v 4.9 89/10/25 19:03:35 qjb Exp $
 *	$FreeBSD$
 */

#if 0
#ifndef lint
static char rcsid[] =
"$FreeBSD$";
#endif /* lint */
#endif

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <krb.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef TKT_SHMEM
#include <sys/param.h>
#endif

extern int krb_debug;

/*
 * in_tkt() is used to initialize the ticket store.  It creates the
 * file to contain the tickets and writes the given user's name "pname"
 * and instance "pinst" in the file.  in_tkt() returns KSUCCESS on
 * success, or KFAILURE if something goes wrong.
 */

int
in_tkt(pname,pinst)
    char *pname;
    char *pinst;
{
    int tktfile;
    uid_t me, metoo;
    struct stat buf;
    int count;
    char *file = TKT_FILE;
    int fd;
    register int i;
    char charbuf[BUFSIZ];
#ifdef TKT_SHMEM
    char shmidname[MAXPATHLEN];
#endif /* TKT_SHMEM */

    me = getuid ();
    metoo = geteuid();
    if (lstat(file,&buf) == 0) {
	if (buf.st_uid != me && me == 0) {
	    unlink(file);
	} else {
	    if (buf.st_uid != me || !(buf.st_mode & S_IFREG) ||
		buf.st_mode & 077) {
		if (krb_debug)
		    fprintf(stderr,"Error initializing %s",file);
		return(KFAILURE);
	    }
	    /* file already exists, and permissions appear ok, so nuke it */
	    if ((fd = open(file, O_RDWR, 0)) < 0)
		goto out; /* can't zero it, but we can still try truncating it */

	    bzero(charbuf, sizeof(charbuf));

	    for (i = 0; i < buf.st_size; i += sizeof(charbuf))
		if (write(fd, charbuf, sizeof(charbuf)) != sizeof(charbuf)) {
		    (void) fsync(fd);
		    (void) close(fd);
		    goto out;
		}

	    (void) fsync(fd);
	    (void) close(fd);
	}
    }
 out:
    /* arrange so the file is owned by the ruid
       (swap real & effective uid if necessary).
       This isn't a security problem, since the ticket file, if it already
       exists, has the right uid (== ruid) and mode. */
    if (me != metoo) {
	if (setreuid(metoo, me) < 0) {
	    /* can't switch??? barf! */
	    if (krb_debug)
		perror("in_tkt: setreuid");
	    return(KFAILURE);
	} else
	    if (krb_debug)
		printf("swapped UID's %d and %d\n",metoo,me);
    }
    if ((tktfile = open(file,O_CREAT | O_TRUNC | O_WRONLY,0600)) < 0) {
	if (krb_debug)
	    fprintf(stderr,"Error initializing %s",TKT_FILE);
        return(KFAILURE);
    }
    if (me != metoo) {
	if (setreuid(me, metoo) < 0) {
	    /* can't switch??? barf! */
	    if (krb_debug)
		perror("in_tkt: setreuid2");
	    return(KFAILURE);
	} else
	    if (krb_debug)
		printf("swapped UID's %d and %d\n",me,metoo);
    }
    if (lstat(file,&buf) < 0) {
	if (krb_debug)
	    fprintf(stderr,"Error initializing %s",TKT_FILE);
        return(KFAILURE);
    }

    if (buf.st_uid != me || !(buf.st_mode & S_IFREG) ||
        buf.st_mode & 077) {
	if (krb_debug)
	    fprintf(stderr,"Error initializing %s",TKT_FILE);
        return(KFAILURE);
    }

    count = strlen(pname)+1;
    if (write(tktfile,pname,count) != count) {
        (void) close(tktfile);
        return(KFAILURE);
    }
    count = strlen(pinst)+1;
    if (write(tktfile,pinst,count) != count) {
        (void) close(tktfile);
        return(KFAILURE);
    }
    (void) close(tktfile);
#ifdef TKT_SHMEM
    (void) strcpy(shmidname, file);
    (void) strcat(shmidname, ".shm");
    return(krb_shm_create(shmidname));
#else /* !TKT_SHMEM */
    return(KSUCCESS);
#endif /* TKT_SHMEM */
}
