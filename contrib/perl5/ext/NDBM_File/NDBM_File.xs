#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include <ndbm.h>

typedef DBM* NDBM_File;
#define dbm_TIEHASH(dbtype,filename,flags,mode) dbm_open(filename,flags,mode)
#define dbm_FETCH(db,key)			dbm_fetch(db,key)
#define dbm_STORE(db,key,value,flags)		dbm_store(db,key,value,flags)
#define dbm_DELETE(db,key)			dbm_delete(db,key)
#define dbm_FIRSTKEY(db)			dbm_firstkey(db)
#define dbm_NEXTKEY(db,key)			dbm_nextkey(db)

MODULE = NDBM_File	PACKAGE = NDBM_File	PREFIX = dbm_

NDBM_File
dbm_TIEHASH(dbtype, filename, flags, mode)
	char *		dbtype
	char *		filename
	int		flags
	int		mode

void
dbm_DESTROY(db)
	NDBM_File	db
	CODE:
	dbm_close(db);

datum
dbm_FETCH(db, key)
	NDBM_File	db
	datum		key

int
dbm_STORE(db, key, value, flags = DBM_REPLACE)
	NDBM_File	db
	datum		key
	datum		value
	int		flags
    CLEANUP:
	if (RETVAL) {
	    if (RETVAL < 0 && errno == EPERM)
		croak("No write permission to ndbm file");
	    croak("ndbm store returned %d, errno %d, key \"%s\"",
			RETVAL,errno,key.dptr);
	    dbm_clearerr(db);
	}

int
dbm_DELETE(db, key)
	NDBM_File	db
	datum		key

datum
dbm_FIRSTKEY(db)
	NDBM_File	db

datum
dbm_NEXTKEY(db, key)
	NDBM_File	db
	datum		key

int
dbm_error(db)
	NDBM_File	db

void
dbm_clearerr(db)
	NDBM_File	db

