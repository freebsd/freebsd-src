/*
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: tf_util.c,v 4.9 90/03/10 19:19:45 jon Exp $
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
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <krb.h>

#ifdef TKT_SHMEM
#include <sys/param.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#endif /* TKT_SHMEM */

#define TOO_BIG -1
#define TF_LCK_RETRY ((unsigned)2)	/* seconds to sleep before
					 * retry if ticket file is
					 * locked */
extern int krb_debug;

#ifdef TKT_SHMEM
char *krb_shm_addr = 0;
static char *tmp_shm_addr = 0;
static char krb_dummy_skey[8] = {0,0,0,0,0,0,0,0};

#endif /* TKT_SHMEM */

/*
 * fd must be initialized to something that won't ever occur as a real
 * file descriptor. Since open(2) returns only non-negative numbers as
 * valid file descriptors, and tf_init always stuffs the return value
 * from open in here even if it is an error flag, we must
 * 	a. Initialize fd to a negative number, to indicate that it is
 * 	   not initially valid.
 *	b. When checking for a valid fd, assume that negative values
 *	   are invalid (ie. when deciding whether tf_init has been
 *	   called.)
 *	c. In tf_close, be sure it gets reinitialized to a negative
 *	   number.
 */
static  fd = -1;
static	curpos;				/* Position in tfbfr */
static	lastpos;			/* End of tfbfr */
static	char tfbfr[BUFSIZ];		/* Buffer for ticket data */

static int tf_read(char *s, int n);
static int tf_gets(char *s, int n);

/*
 * This file contains routines for manipulating the ticket cache file.
 *
 * The ticket file is in the following format:
 *
 *      principal's name        (null-terminated string)
 *      principal's instance    (null-terminated string)
 *      CREDENTIAL_1
 *      CREDENTIAL_2
 *      ...
 *      CREDENTIAL_n
 *      EOF
 *
 *      Where "CREDENTIAL_x" consists of the following fixed-length
 *      fields from the CREDENTIALS structure (see "krb.h"):
 *
 *              char            service[ANAME_SZ]
 *              char            instance[INST_SZ]
 *              char            realm[REALM_SZ]
 *              C_Block         session
 *              int             lifetime
 *              int             kvno
 *              KTEXT_ST        ticket_st
 *              long            issue_date
 *
 * Short description of routines:
 *
 * tf_init() opens the ticket file and locks it.
 *
 * tf_get_pname() returns the principal's name.
 *
 * tf_get_pinst() returns the principal's instance (may be null).
 *
 * tf_get_cred() returns the next CREDENTIALS record.
 *
 * tf_save_cred() appends a new CREDENTIAL record to the ticket file.
 *
 * tf_close() closes the ticket file and releases the lock.
 *
 * tf_gets() returns the next null-terminated string.  It's an internal
 * routine used by tf_get_pname(), tf_get_pinst(), and tf_get_cred().
 *
 * tf_read() reads a given number of bytes.  It's an internal routine
 * used by tf_get_cred().
 */

/*
 * tf_init() should be called before the other ticket file routines.
 * It takes the name of the ticket file to use, "tf_name", and a
 * read/write flag "rw" as arguments.
 *
 * It tries to open the ticket file, checks the mode, and if everything
 * is okay, locks the file.  If it's opened for reading, the lock is
 * shared.  If it's opened for writing, the lock is exclusive.
 *
 * Returns KSUCCESS if all went well, otherwise one of the following:
 *
 * NO_TKT_FIL   - file wasn't there
 * TKT_FIL_ACC  - file was in wrong mode, etc.
 * TKT_FIL_LCK  - couldn't lock the file, even after a retry
 */

int
tf_init(tf_name, rw)
    char   *tf_name;
    int rw;
{
    int     wflag;
    uid_t   me, getuid();
    struct stat stat_buf;
#ifdef TKT_SHMEM
    char shmidname[MAXPATHLEN];
    FILE *sfp;
    int shmid;
#endif

    switch (rw) {
    case R_TKT_FIL:
	wflag = 0;
	break;
    case W_TKT_FIL:
	wflag = 1;
	break;
    default:
	if (krb_debug) fprintf(stderr, "tf_init: illegal parameter\n");
	return TKT_FIL_ACC;
    }
    if (lstat(tf_name, &stat_buf) < 0)
	switch (errno) {
	case ENOENT:
	    return NO_TKT_FIL;
	default:
	    return TKT_FIL_ACC;
	}
    me = getuid();
    if ((stat_buf.st_uid != me && me != 0) ||
	((stat_buf.st_mode & S_IFMT) != S_IFREG))
	return TKT_FIL_ACC;
#ifdef TKT_SHMEM
    (void) strcpy(shmidname, tf_name);
    (void) strcat(shmidname, ".shm");
    if (stat(shmidname,&stat_buf) < 0)
	return(TKT_FIL_ACC);
    if ((stat_buf.st_uid != me && me != 0) ||
	((stat_buf.st_mode & S_IFMT) != S_IFREG))
	return TKT_FIL_ACC;
#endif /* TKT_SHMEM */

    /*
     * If "wflag" is set, open the ticket file in append-writeonly mode
     * and lock the ticket file in exclusive mode.  If unable to lock
     * the file, sleep and try again.  If we fail again, return with the
     * proper error message.
     */

    curpos = sizeof(tfbfr);

#ifdef TKT_SHMEM
    sfp = fopen(shmidname, "r");	/* only need read/write on the
					   actual tickets */
    if (sfp == 0)
    	return TKT_FIL_ACC;
    shmid = -1;
    {
	char buf[BUFSIZ];
	int val;			/* useful for debugging fscanf */
	/* We provide our own buffer here since some STDIO libraries
	   barf on unbuffered input with fscanf() */

	setbuf(sfp, buf);
	if ((val = fscanf(sfp,"%d",&shmid)) != 1) {
	    (void) fclose(sfp);
	    return TKT_FIL_ACC;
	}
	if (shmid < 0) {
	    (void) fclose(sfp);
	    return TKT_FIL_ACC;
	}
	(void) fclose(sfp);
    }
    /*
    * global krb_shm_addr is initialized to 0.  Ultrix bombs when you try and
    * attach the same segment twice so we need this check.
    */
    if (!krb_shm_addr) {
    	if ((krb_shm_addr = shmat(shmid,0,0)) == -1){
		if (krb_debug)
		    fprintf(stderr,
			    "cannot attach shared memory for segment %d\n",
			    shmid);
		krb_shm_addr = 0;	/* reset so we catch further errors */
		return TKT_FIL_ACC;
	    }
    }
    tmp_shm_addr = krb_shm_addr;
#endif /* TKT_SHMEM */

    if (wflag) {
	fd = open(tf_name, O_RDWR, 0600);
	if (fd < 0) {
	    return TKT_FIL_ACC;
	}
	if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
	    sleep(TF_LCK_RETRY);
	    if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
		(void) close(fd);
		fd = -1;
		return TKT_FIL_LCK;
	    }
	}
	return KSUCCESS;
    }
    /*
     * Otherwise "wflag" is not set and the ticket file should be opened
     * for read-only operations and locked for shared access.
     */

    fd = open(tf_name, O_RDONLY, 0600);
    if (fd < 0) {
	return TKT_FIL_ACC;
    }
    if (flock(fd, LOCK_SH | LOCK_NB) < 0) {
	sleep(TF_LCK_RETRY);
	if (flock(fd, LOCK_SH | LOCK_NB) < 0) {
	    (void) close(fd);
	    fd = -1;
	    return TKT_FIL_LCK;
	}
    }
    return KSUCCESS;
}

/*
 * tf_get_pname() reads the principal's name from the ticket file. It
 * should only be called after tf_init() has been called.  The
 * principal's name is filled into the "p" parameter.  If all goes well,
 * KSUCCESS is returned.  If tf_init() wasn't called, TKT_FIL_INI is
 * returned.  If the name was null, or EOF was encountered, or the name
 * was longer than ANAME_SZ, TKT_FIL_FMT is returned.
 */

int
tf_get_pname(p)
    char   *p;
{
    if (fd < 0) {
	if (krb_debug)
	    fprintf(stderr, "tf_get_pname called before tf_init.\n");
	return TKT_FIL_INI;
    }
    if (tf_gets(p, ANAME_SZ) < 2)	/* can't be just a null */
	return TKT_FIL_FMT;
    return KSUCCESS;
}

/*
 * tf_get_pinst() reads the principal's instance from a ticket file.
 * It should only be called after tf_init() and tf_get_pname() have been
 * called.  The instance is filled into the "inst" parameter.  If all
 * goes well, KSUCCESS is returned.  If tf_init() wasn't called,
 * TKT_FIL_INI is returned.  If EOF was encountered, or the instance
 * was longer than ANAME_SZ, TKT_FIL_FMT is returned.  Note that the
 * instance may be null.
 */

int
tf_get_pinst(inst)
    char   *inst;
{
    if (fd < 0) {
	if (krb_debug)
	    fprintf(stderr, "tf_get_pinst called before tf_init.\n");
	return TKT_FIL_INI;
    }
    if (tf_gets(inst, INST_SZ) < 1)
	return TKT_FIL_FMT;
    return KSUCCESS;
}

/*
 * tf_get_cred() reads a CREDENTIALS record from a ticket file and fills
 * in the given structure "c".  It should only be called after tf_init(),
 * tf_get_pname(), and tf_get_pinst() have been called. If all goes well,
 * KSUCCESS is returned.  Possible error codes are:
 *
 * TKT_FIL_INI  - tf_init wasn't called first
 * TKT_FIL_FMT  - bad format
 * EOF          - end of file encountered
 */

int
tf_get_cred(c)
    CREDENTIALS *c;
{
    KTEXT   ticket = &c->ticket_st;	/* pointer to ticket */
    int     k_errno;

    if (fd < 0) {
	if (krb_debug)
	    fprintf(stderr, "tf_get_cred called before tf_init.\n");
	return TKT_FIL_INI;
    }
    if ((k_errno = tf_gets(c->service, SNAME_SZ)) < 2)
	switch (k_errno) {
	case TOO_BIG:
	case 1:		/* can't be just a null */
	    tf_close();
	    return TKT_FIL_FMT;
	case 0:
	    return EOF;
	}
    if ((k_errno = tf_gets(c->instance, INST_SZ)) < 1)
	switch (k_errno) {
	case TOO_BIG:
	    return TKT_FIL_FMT;
	case 0:
	    return EOF;
	}
    if ((k_errno = tf_gets(c->realm, REALM_SZ)) < 2)
	switch (k_errno) {
	case TOO_BIG:
	case 1:		/* can't be just a null */
	    tf_close();
	    return TKT_FIL_FMT;
	case 0:
	    return EOF;
	}
    if (
	tf_read((char *) (c->session), KEY_SZ) < 1 ||
	tf_read((char *) &(c->lifetime), sizeof(c->lifetime)) < 1 ||
	tf_read((char *) &(c->kvno), sizeof(c->kvno)) < 1 ||
	tf_read((char *) &(ticket->length), sizeof(ticket->length))
	< 1 ||
    /* don't try to read a silly amount into ticket->dat */
	ticket->length > MAX_KTXT_LEN ||
	tf_read((char *) (ticket->dat), ticket->length) < 1 ||
	tf_read((char *) &(c->issue_date), sizeof(c->issue_date)) < 1
	) {
	tf_close();
	return TKT_FIL_FMT;
    }
#ifdef TKT_SHMEM
    bcopy(tmp_shm_addr,c->session,KEY_SZ);
    tmp_shm_addr += KEY_SZ;
#endif /* TKT_SHMEM */
    return KSUCCESS;
}

/*
 * tf_close() closes the ticket file and sets "fd" to -1. If "fd" is
 * not a valid file descriptor, it just returns.  It also clears the
 * buffer used to read tickets.
 *
 * The return value is not defined.
 */

void
tf_close()
{
    if (!(fd < 0)) {
#ifdef TKT_SHMEM
	if (shmdt(krb_shm_addr)) {
	    /* what kind of error? */
	    if (krb_debug)
		fprintf(stderr, "shmdt 0x%x: errno %d",krb_shm_addr, errno);
	} else {
	    krb_shm_addr = 0;
	}
#endif TKT_SHMEM
	(void) flock(fd, LOCK_UN);
	(void) close(fd);
	fd = -1;		/* see declaration of fd above */
    }
    bzero(tfbfr, sizeof(tfbfr));
}

/*
 * tf_gets() is an internal routine.  It takes a string "s" and a count
 * "n", and reads from the file until either it has read "n" characters,
 * or until it reads a null byte. When finished, what has been read exists
 * in "s". If it encounters EOF or an error, it closes the ticket file.
 *
 * Possible return values are:
 *
 * n            the number of bytes read (including null terminator)
 *              when all goes well
 *
 * 0            end of file or read error
 *
 * TOO_BIG      if "count" characters are read and no null is
 *		encountered. This is an indication that the ticket
 *		file is seriously ill.
 */

static int
tf_gets(s, n)
    register char *s;
    int n;
{
    register count;

    if (fd < 0) {
	if (krb_debug)
	    fprintf(stderr, "tf_gets called before tf_init.\n");
	return TKT_FIL_INI;
    }
    for (count = n - 1; count > 0; --count) {
	if (curpos >= sizeof(tfbfr)) {
	    lastpos = read(fd, tfbfr, sizeof(tfbfr));
	    curpos = 0;
	}
	if (curpos == lastpos) {
	    tf_close();
	    return 0;
	}
	*s = tfbfr[curpos++];
	if (*s++ == '\0')
	    return (n - count);
    }
    tf_close();
    return TOO_BIG;
}

/*
 * tf_read() is an internal routine.  It takes a string "s" and a count
 * "n", and reads from the file until "n" bytes have been read.  When
 * finished, what has been read exists in "s".  If it encounters EOF or
 * an error, it closes the ticket file.
 *
 * Possible return values are:
 *
 * n		the number of bytes read when all goes well
 *
 * 0		on end of file or read error
 */

static int
tf_read(s, n)
    register char *s;
    register int n;
{
    register count;

    for (count = n; count > 0; --count) {
	if (curpos >= sizeof(tfbfr)) {
	    lastpos = read(fd, tfbfr, sizeof(tfbfr));
	    curpos = 0;
	}
	if (curpos == lastpos) {
	    tf_close();
	    return 0;
	}
	*s++ = tfbfr[curpos++];
    }
    return n;
}

/*
 * tf_save_cred() appends an incoming ticket to the end of the ticket
 * file.  You must call tf_init() before calling tf_save_cred().
 *
 * The "service", "instance", and "realm" arguments specify the
 * server's name; "session" contains the session key to be used with
 * the ticket; "kvno" is the server key version number in which the
 * ticket is encrypted, "ticket" contains the actual ticket, and
 * "issue_date" is the time the ticket was requested (local host's time).
 *
 * Returns KSUCCESS if all goes well, TKT_FIL_INI if tf_init() wasn't
 * called previously, and KFAILURE for anything else that went wrong.
 */

int
tf_save_cred(service, instance, realm, session, lifetime, kvno,
	     ticket, issue_date)
    char   *service;		/* Service name */
    char   *instance;		/* Instance */
    char   *realm;		/* Auth domain */
    C_Block session;		/* Session key */
    int     lifetime;		/* Lifetime */
    int     kvno;		/* Key version number */
    KTEXT   ticket;		/* The ticket itself */
    long    issue_date;		/* The issue time */
{

    off_t   lseek();
    int     count;		/* count for write */
#ifdef TKT_SHMEM
    int	    *skey_check;
#endif /* TKT_SHMEM */

    if (fd < 0) {		/* fd is ticket file as set by tf_init */
	  if (krb_debug)
	      fprintf(stderr, "tf_save_cred called before tf_init.\n");
	  return TKT_FIL_INI;
    }
    /* Find the end of the ticket file */
    (void) lseek(fd, 0L, 2);
#ifdef TKT_SHMEM
    /* scan to end of existing keys: pick first 'empty' slot.
       we assume that no real keys will be completely zero (it's a weak
       key under DES) */

    skey_check = (int *) krb_shm_addr;

    while (*skey_check && *(skey_check+1))
	skey_check += 2;
    tmp_shm_addr = (char *)skey_check;
#endif /* TKT_SHMEM */

    /* Write the ticket and associated data */
    /* Service */
    count = strlen(service) + 1;
    if (write(fd, service, count) != count)
	goto bad;
    /* Instance */
    count = strlen(instance) + 1;
    if (write(fd, instance, count) != count)
	goto bad;
    /* Realm */
    count = strlen(realm) + 1;
    if (write(fd, realm, count) != count)
	goto bad;
    /* Session key */
#ifdef TKT_SHMEM
    bcopy(session,tmp_shm_addr,8);
    tmp_shm_addr+=8;
    if (write(fd,krb_dummy_skey,8) != 8)
	goto bad;
#else /* ! TKT_SHMEM */
    if (write(fd, (char *) session, 8) != 8)
	goto bad;
#endif /* TKT_SHMEM */
    /* Lifetime */
    if (write(fd, (char *) &lifetime, sizeof(int)) != sizeof(int))
	goto bad;
    /* Key vno */
    if (write(fd, (char *) &kvno, sizeof(int)) != sizeof(int))
	goto bad;
    /* Tkt length */
    if (write(fd, (char *) &(ticket->length), sizeof(int)) !=
	sizeof(int))
	goto bad;
    /* Ticket */
    count = ticket->length;
    if (write(fd, (char *) (ticket->dat), count) != count)
	goto bad;
    /* Issue date */
    if (write(fd, (char *) &issue_date, sizeof(long))
	!= sizeof(long))
	goto bad;

    /* Actually, we should check each write for success */
    return (KSUCCESS);
bad:
    return (KFAILURE);
}
