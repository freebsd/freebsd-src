#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#ifdef NULL
#undef NULL  /* XXX Why? */
#endif
#ifdef I_DBM
#  include <dbm.h>
#else
#  ifdef I_RPCSVC_DBM
#    include <rpcsvc/dbm.h>
#  endif
#endif

#ifdef DBM_BUG_DUPLICATE_FREE 
/*
 * DBM on at least Ultrix and HPUX call dbmclose() from dbminit(),
 * resulting in duplicate free() because dbmclose() does *not*
 * check if it has already been called for this DBM.
 * If some malloc/free calls have been done between dbmclose() and
 * the next dbminit(), the memory might be used for something else when
 * it is freed.
 * Verified to work on ultrix4.3.  Probably will work on HP/UX.
 * Set DBM_BUG_DUPLICATE_FREE in the extension hint file.
 */
/* Close the previous dbm, and fail to open a new dbm */
#define dbmclose()	((void) dbminit("/tmp/x/y/z/z/y"))
#endif

#include <fcntl.h>

typedef void* ODBM_File;

#define odbm_FETCH(db,key)			fetch(key)
#define odbm_STORE(db,key,value,flags)		store(key,value)
#define odbm_DELETE(db,key)			delete(key)
#define odbm_FIRSTKEY(db)			firstkey()
#define odbm_NEXTKEY(db,key)			nextkey(key)

static int dbmrefcnt;

#ifndef DBM_REPLACE
#define DBM_REPLACE 0
#endif

MODULE = ODBM_File	PACKAGE = ODBM_File	PREFIX = odbm_

#ifndef NULL
#  define NULL 0
#endif

ODBM_File
odbm_TIEHASH(dbtype, filename, flags, mode)
	char *		dbtype
	char *		filename
	int		flags
	int		mode
	CODE:
	{
	    char *tmpbuf;
	    if (dbmrefcnt++)
		croak("Old dbm can only open one database");
	    New(0, tmpbuf, strlen(filename) + 5, char);
	    SAVEFREEPV(tmpbuf);
	    sprintf(tmpbuf,"%s.dir",filename);
	    if (stat(tmpbuf, &PL_statbuf) < 0) {
		if (flags & O_CREAT) {
		    if (mode < 0 || close(creat(tmpbuf,mode)) < 0)
			croak("ODBM_File: Can't create %s", filename);
		    sprintf(tmpbuf,"%s.pag",filename);
		    if (close(creat(tmpbuf,mode)) < 0)
			croak("ODBM_File: Can't create %s", filename);
		}
		else
		    croak("ODBM_FILE: Can't open %s", filename);
	    }
	    RETVAL = (void*)(dbminit(filename) >= 0 ? &dbmrefcnt : 0);
	    ST(0) = sv_mortalcopy(&PL_sv_undef);
	    sv_setptrobj(ST(0), RETVAL, dbtype);
	}

void
DESTROY(db)
	ODBM_File	db
	CODE:
	dbmrefcnt--;
	dbmclose();

datum
odbm_FETCH(db, key)
	ODBM_File	db
	datum		key

int
odbm_STORE(db, key, value, flags = DBM_REPLACE)
	ODBM_File	db
	datum		key
	datum		value
	int		flags
    CLEANUP:
	if (RETVAL) {
	    if (RETVAL < 0 && errno == EPERM)
		croak("No write permission to odbm file");
	    croak("odbm store returned %d, errno %d, key \"%s\"",
			RETVAL,errno,key.dptr);
	}

int
odbm_DELETE(db, key)
	ODBM_File	db
	datum		key

datum
odbm_FIRSTKEY(db)
	ODBM_File	db

datum
odbm_NEXTKEY(db, key)
	ODBM_File	db
	datum		key

