#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "sdbm/sdbm.h"

typedef DBM* SDBM_File;
#define sdbm_TIEHASH(dbtype,filename,flags,mode) sdbm_open(filename,flags,mode)
#define sdbm_FETCH(db,key)			sdbm_fetch(db,key)
#define sdbm_STORE(db,key,value,flags)		sdbm_store(db,key,value,flags)
#define sdbm_DELETE(db,key)			sdbm_delete(db,key)
#define sdbm_FIRSTKEY(db)			sdbm_firstkey(db)
#define sdbm_NEXTKEY(db,key)			sdbm_nextkey(db)


MODULE = SDBM_File	PACKAGE = SDBM_File	PREFIX = sdbm_

SDBM_File
sdbm_TIEHASH(dbtype, filename, flags, mode)
	char *		dbtype
	char *		filename
	int		flags
	int		mode

void
sdbm_DESTROY(db)
	SDBM_File	db
	CODE:
	sdbm_close(db);

datum
sdbm_FETCH(db, key)
	SDBM_File	db
	datum		key

int
sdbm_STORE(db, key, value, flags = DBM_REPLACE)
	SDBM_File	db
	datum		key
	datum		value
	int		flags
    CLEANUP:
	if (RETVAL) {
	    if (RETVAL < 0 && errno == EPERM)
		croak("No write permission to sdbm file");
	    croak("sdbm store returned %d, errno %d, key \"%s\"",
			RETVAL,errno,key.dptr);
	    sdbm_clearerr(db);
	}

int
sdbm_DELETE(db, key)
	SDBM_File	db
	datum		key

datum
sdbm_FIRSTKEY(db)
	SDBM_File	db

datum
sdbm_NEXTKEY(db, key)
	SDBM_File	db
	datum		key

int
sdbm_error(db)
	SDBM_File	db

int
sdbm_clearerr(db)
	SDBM_File	db

