/* 
  Copyright (C) 1989 by the Massachusetts Institute of Technology

   Export of this software from the United States of America is assumed
   to require a specific license from the United States Government.
   It is the responsibility of any person or organization contemplating
   export to obtain such a license before exporting.

WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
distribute this software and its documentation for any purpose and
without fee is hereby granted, provided that the above copyright
notice appear in all copies and that both that copyright notice and
this permission notice appear in supporting documentation, and that
the name of M.I.T. not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.  M.I.T. makes no representations about the suitability of
this software for any purpose.  It is provided "as is" without express
or implied warranty.

  */
        
#include "krb_locl.h"

RCSID("$Id: tf_util.c,v 1.24 1997/04/20 06:24:32 assar Exp $");


#define TOO_BIG -1
#define TF_LCK_RETRY ((unsigned)2)	/* seconds to sleep before
					 * retry if ticket file is
					 * locked */
#define	TF_LCK_RETRY_COUNT	(50)	/* number of retries	*/

#ifndef O_BINARY
#define O_BINARY 0
#endif

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
static  int fd = -1;
static	int curpos;				/* Position in tfbfr */
static	int lastpos;			/* End of tfbfr */
static	char tfbfr[BUFSIZ];		/* Buffer for ticket data */

static int tf_gets(char *s, int n);
static int tf_read(void *s, int n);

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
 *              u_int32_t            issue_date
 *
 * Short description of routines:
 *
 * tf_init() opens the ticket file and locks it.
 *
 * tf_get_pname() returns the principal's name.
 *
 * tf_put_pname() writes the principal's name to the ticket file.
 *
 * tf_get_pinst() returns the principal's instance (may be null).
 *
 * tf_put_pinst() writes the instance.
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
tf_init(char *tf_name, int rw)
{
  /* Unix implementation */
  int wflag;
  struct stat stat_buf;
  int i_retry;

  switch (rw) {
  case R_TKT_FIL:
    wflag = 0;
    break;
  case W_TKT_FIL:
    wflag = 1;
    break;
  default:
    if (krb_debug)
      krb_warning("tf_init: illegal parameter\n");
    return TKT_FIL_ACC;
  }
  if (lstat(tf_name, &stat_buf) < 0)
    switch (errno) {
    case ENOENT:
      return NO_TKT_FIL;
    default:
      return TKT_FIL_ACC;
    }
  /* The old code tried to guess when the calling program was
   * running set-uid, this is now removed - the kerberos library
   * does not (or shouldn't) know anything about user-ids.

   * All library functions now assume that the right userids are set
   * upon entry, therefore there is no need to test permissions like
   * before. If the file is openable, just open it.
   */

  if(!S_ISREG(stat_buf.st_mode))
    return TKT_FIL_ACC;


  /*
   * If "wflag" is set, open the ticket file in append-writeonly mode
   * and lock the ticket file in exclusive mode.  If unable to lock
   * the file, sleep and try again.  If we fail again, return with the
   * proper error message. 
   */

  curpos = sizeof(tfbfr);

    
  if (wflag) {
    fd = open(tf_name, O_RDWR | O_BINARY, 0600);
    if (fd < 0) {
      return TKT_FIL_ACC;
    }
    for (i_retry = 0; i_retry < TF_LCK_RETRY_COUNT; i_retry++) {
      if (k_flock(fd, K_LOCK_EX | K_LOCK_NB) < 0) {
	if (krb_debug)
	  krb_warning("tf_init: retry %d of write lock of `%s'.\n",
		      i_retry, tf_name);
	sleep (TF_LCK_RETRY);
      } else {
	return KSUCCESS;		/* all done */
      }
    }
    close (fd);
    fd = -1;
    return TKT_FIL_LCK;
  }
  /*
   * Otherwise "wflag" is not set and the ticket file should be opened
   * for read-only operations and locked for shared access. 
   */

  fd = open(tf_name, O_RDONLY | O_BINARY, 0600);
  if (fd < 0) {
    return TKT_FIL_ACC;
  }

  for (i_retry = 0; i_retry < TF_LCK_RETRY_COUNT; i_retry++) {
    if (k_flock(fd, K_LOCK_SH | K_LOCK_NB) < 0) {
      if (krb_debug)
	krb_warning("tf_init: retry %d of read lock of `%s'.\n",
		    i_retry, tf_name);
      sleep (TF_LCK_RETRY);
    } else {
      return KSUCCESS;		/* all done */
    }
  }
  /* failure */
  close(fd);
  fd = -1;
  return TKT_FIL_LCK;
}

/*
 * tf_create() should be called when creating a new ticket file.
 * The only argument is the name of the ticket file.
 * After calling this, it should be possible to use other tf_* functions.
 *
 * New algoritm for creating ticket file:
 * 1. try to erase contents of existing file.
 * 2. try to remove old file.
 * 3. try to open with O_CREAT and O_EXCL
 * 4. if this fails, someone has created a file in between 1 and 2 and
 *    we should fail.  Otherwise, all is wonderful.
 */

int
tf_create(char *tf_name)
{
  struct stat statbuf;
  char garbage[BUFSIZ];

  fd = open(tf_name, O_RDWR | O_BINARY, 0);
  if (fd >= 0) {
    if (fstat (fd, &statbuf) == 0) {
      int i;

      for (i = 0; i < statbuf.st_size; i += sizeof(garbage))
	write (fd, garbage, sizeof(garbage));
    }
    close (fd);
  }

  if (unlink (tf_name) && errno != ENOENT)
    return TKT_FIL_ACC;

  fd = open(tf_name, O_RDWR | O_CREAT | O_EXCL | O_BINARY, 0600);
  if (fd < 0)
    return TKT_FIL_ACC;
  if (k_flock(fd, K_LOCK_EX | K_LOCK_NB) < 0) {
    sleep(TF_LCK_RETRY);
    if (k_flock(fd, K_LOCK_EX | K_LOCK_NB) < 0) {
      close(fd);
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
tf_get_pname(char *p)
{
  if (fd < 0) {
    if (krb_debug)
      krb_warning("tf_get_pname called before tf_init.\n");
    return TKT_FIL_INI;
  }
  if (tf_gets(p, ANAME_SZ) < 2)	/* can't be just a null */
    {
      if (krb_debug) 
	krb_warning ("tf_get_pname: pname < 2.\n");
      return TKT_FIL_FMT;
    }
  return KSUCCESS;
}

/*
 * tf_put_pname() sets the principal's name in the ticket file. Call
 * after tf_create().
 */

int
tf_put_pname(char *p)
{
  unsigned count;

  if (fd < 0) {
    if (krb_debug)
      krb_warning("tf_put_pname called before tf_create.\n");
    return TKT_FIL_INI;
  }
  count = strlen(p)+1;
  if (write(fd,p,count) != count)
    return(KFAILURE);
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
tf_get_pinst(char *inst)
{
  if (fd < 0) {
    if (krb_debug)
      krb_warning("tf_get_pinst called before tf_init.\n");
    return TKT_FIL_INI;
  }
  if (tf_gets(inst, INST_SZ) < 1)
    {
      if (krb_debug)
	krb_warning("tf_get_pinst: inst_sz < 1.\n");
      return TKT_FIL_FMT;
    }
  return KSUCCESS;
}

/*
 * tf_put_pinst writes the principal's instance to the ticket file.
 * Call after tf_create.
 */

int
tf_put_pinst(char *inst)
{
  unsigned count;

  if (fd < 0) {
    if (krb_debug)
      krb_warning("tf_put_pinst called before tf_create.\n");
    return TKT_FIL_INI;
  }
  count = strlen(inst)+1;
  if (write(fd,inst,count) != count)
    return(KFAILURE);
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
tf_get_cred(CREDENTIALS *c)
{
  KTEXT   ticket = &c->ticket_st;	/* pointer to ticket */
  int     k_errno;

  if (fd < 0) {
    if (krb_debug)
      krb_warning ("tf_get_cred called before tf_init.\n");
    return TKT_FIL_INI;
  }
  if ((k_errno = tf_gets(c->service, SNAME_SZ)) < 2)
    switch (k_errno) {
    case TOO_BIG:
      if (krb_debug)
	krb_warning("tf_get_cred: too big service cred.\n");
    case 1:		/* can't be just a null */
      tf_close();
      if (krb_debug)
	krb_warning("tf_get_cred: null service cred.\n");
      return TKT_FIL_FMT;
    case 0:
      return EOF;
    }
  if ((k_errno = tf_gets(c->instance, INST_SZ)) < 1)
    switch (k_errno) {
    case TOO_BIG:
      if (krb_debug)
	krb_warning ("tf_get_cred: too big instance cred.\n");
      return TKT_FIL_FMT;
    case 0:
      return EOF;
    }
  if ((k_errno = tf_gets(c->realm, REALM_SZ)) < 2)
    switch (k_errno) {
    case TOO_BIG:
      if (krb_debug)
	krb_warning ("tf_get_cred: too big realm cred.\n");
    case 1:		/* can't be just a null */
      tf_close();
      if (krb_debug)
	krb_warning ("tf_get_cred: null realm cred.\n");
      return TKT_FIL_FMT;
    case 0:
      return EOF;
    }
  if (
      tf_read((c->session), DES_KEY_SZ) < 1 ||
      tf_read(&(c->lifetime), sizeof(c->lifetime)) < 1 ||
      tf_read(&(c->kvno), sizeof(c->kvno)) < 1 ||
      tf_read(&(ticket->length), sizeof(ticket->length))
      < 1 ||
      /* don't try to read a silly amount into ticket->dat */
      ticket->length > MAX_KTXT_LEN ||
      tf_read((ticket->dat), ticket->length) < 1 ||
      tf_read(&(c->issue_date), sizeof(c->issue_date)) < 1
      ) {
    tf_close();
    if (krb_debug)
      krb_warning ("tf_get_cred: failed tf_read.\n");
    return TKT_FIL_FMT;
  }
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
tf_close(void)
{
  if (!(fd < 0)) {
    k_flock(fd, K_LOCK_UN);
    close(fd);
    fd = -1;		/* see declaration of fd above */
  }
  memset(tfbfr, 0, sizeof(tfbfr));
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
tf_gets(char *s, int n)
{
  int count;

  if (fd < 0) {
    if (krb_debug)
      krb_warning ("tf_gets called before tf_init.\n");
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
tf_read(void *v, int n)
{
  char *s = (char *)v;
  int count;
    
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
tf_save_cred(char *service,	/* Service name */
	     char *instance,	/* Instance */
	     char *realm,	/* Auth domain */
	     unsigned char *session, /* Session key */
	     int lifetime,	/* Lifetime */
	     int kvno,		/* Key version number */
	     KTEXT ticket,	/* The ticket itself */
	     u_int32_t issue_date) /* The issue time */
{
  int count;			/* count for write */

  if (fd < 0) {			/* fd is ticket file as set by tf_init */
    if (krb_debug)
      krb_warning ("tf_save_cred called before tf_init.\n");
    return TKT_FIL_INI;
  }
  /* Find the end of the ticket file */
  lseek(fd, 0L, SEEK_END);

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
  if (write(fd, session, 8) != 8)
    goto bad;
  /* Lifetime */
  if (write(fd, &lifetime, sizeof(int)) != sizeof(int))
    goto bad;
  /* Key vno */
  if (write(fd, &kvno, sizeof(int)) != sizeof(int))
    goto bad;
  /* Tkt length */
  if (write(fd, &(ticket->length), sizeof(int)) !=
      sizeof(int))
    goto bad;
  /* Ticket */
  count = ticket->length;
  if (write(fd, ticket->dat, count) != count)
    goto bad;
  /* Issue date */
  if (write(fd, &issue_date, sizeof(issue_date)) != sizeof(issue_date))
    goto bad;

  return (KSUCCESS);
bad:
  return (KFAILURE);
}
	  
int
tf_setup(CREDENTIALS *cred, char *pname, char *pinst)
{
    int ret;
    ret = tf_create(tkt_string());
    if (ret != KSUCCESS)
	return ret;

    if (tf_put_pname(pname) != KSUCCESS ||
	tf_put_pinst(pinst) != KSUCCESS) {
	tf_close();
	return INTK_ERR;
    }

    ret = tf_save_cred(cred->service, cred->instance, cred->realm, 
		       cred->session, cred->lifetime, cred->kvno,
		       &cred->ticket_st, cred->issue_date);
    tf_close();
    return ret;
}

int
in_tkt(char *pname, char *pinst)
{
  int ret;
  
  ret = tf_create (tkt_string());
  if (ret != KSUCCESS)
    return ret;

    if (tf_put_pname(pname) != KSUCCESS ||
	tf_put_pinst(pinst) != KSUCCESS) {
	tf_close();
	return INTK_ERR;
    }

    tf_close();
    return KSUCCESS;
}
