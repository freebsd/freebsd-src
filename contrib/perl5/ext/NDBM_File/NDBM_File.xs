#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
/* If using the DB3 emulation, ENTER is defined both
 * by DB3 and Perl.  We drop the Perl definition now.
 * See also INSTALL section on DB3.
 * -- Stanislav Brabec <utx@penguin.cz> */
#undef ENTER
#include <ndbm.h>

typedef struct {
	DBM * 	dbp ;
	SV *    filter_fetch_key ;
	SV *    filter_store_key ;
	SV *    filter_fetch_value ;
	SV *    filter_store_value ;
	int     filtering ;
	} NDBM_File_type;

typedef NDBM_File_type * NDBM_File ;
typedef datum datum_key ;
typedef datum datum_value ;

#define ckFilter(arg,type,name)					\
	if (db->type) {						\
	    SV * save_defsv ;					\
            /* printf("filtering %s\n", name) ;*/		\
	    if (db->filtering)					\
	        croak("recursion detected in %s", name) ;	\
	    db->filtering = TRUE ;				\
	    save_defsv = newSVsv(DEFSV) ;			\
	    sv_setsv(DEFSV, arg) ;				\
	    PUSHMARK(sp) ;					\
	    (void) perl_call_sv(db->type, G_DISCARD|G_NOARGS); 	\
	    sv_setsv(arg, DEFSV) ;				\
	    sv_setsv(DEFSV, save_defsv) ;			\
	    SvREFCNT_dec(save_defsv) ;				\
	    db->filtering = FALSE ;				\
	    /*printf("end of filtering %s\n", name) ;*/		\
	}


MODULE = NDBM_File	PACKAGE = NDBM_File	PREFIX = ndbm_

NDBM_File
ndbm_TIEHASH(dbtype, filename, flags, mode)
	char *		dbtype
	char *		filename
	int		flags
	int		mode
	CODE:
	{
	    DBM * 	dbp ;

	    RETVAL = NULL ;
	    if (dbp =  dbm_open(filename, flags, mode)) {
	        RETVAL = (NDBM_File)safemalloc(sizeof(NDBM_File_type)) ;
    	        Zero(RETVAL, 1, NDBM_File_type) ;
		RETVAL->dbp = dbp ;
	    }
	    
	}
	OUTPUT:
	  RETVAL

void
ndbm_DESTROY(db)
	NDBM_File	db
	CODE:
	dbm_close(db->dbp);
	safefree(db);

#define ndbm_FETCH(db,key)			dbm_fetch(db->dbp,key)
datum_value
ndbm_FETCH(db, key)
	NDBM_File	db
	datum_key	key

#define ndbm_STORE(db,key,value,flags)		dbm_store(db->dbp,key,value,flags)
int
ndbm_STORE(db, key, value, flags = DBM_REPLACE)
	NDBM_File	db
	datum_key	key
	datum_value	value
	int		flags
    CLEANUP:
	if (RETVAL) {
	    if (RETVAL < 0 && errno == EPERM)
		croak("No write permission to ndbm file");
	    croak("ndbm store returned %d, errno %d, key \"%s\"",
			RETVAL,errno,key.dptr);
	    dbm_clearerr(db->dbp);
	}

#define ndbm_DELETE(db,key)			dbm_delete(db->dbp,key)
int
ndbm_DELETE(db, key)
	NDBM_File	db
	datum_key	key

#define ndbm_FIRSTKEY(db)			dbm_firstkey(db->dbp)
datum_key
ndbm_FIRSTKEY(db)
	NDBM_File	db

#define ndbm_NEXTKEY(db,key)			dbm_nextkey(db->dbp)
datum_key
ndbm_NEXTKEY(db, key)
	NDBM_File	db
	datum_key	key

#define ndbm_error(db)				dbm_error(db->dbp)
int
ndbm_error(db)
	NDBM_File	db

#define ndbm_clearerr(db)			dbm_clearerr(db->dbp)
void
ndbm_clearerr(db)
	NDBM_File	db


#define setFilter(type)					\
	{						\
	    if (db->type)				\
	        RETVAL = sv_mortalcopy(db->type) ; 	\
	    ST(0) = RETVAL ;				\
	    if (db->type && (code == &PL_sv_undef)) {	\
                SvREFCNT_dec(db->type) ;		\
	        db->type = NULL ;			\
	    }						\
	    else if (code) {				\
	        if (db->type)				\
	            sv_setsv(db->type, code) ;		\
	        else					\
	            db->type = newSVsv(code) ;		\
	    }	    					\
	}



SV *
filter_fetch_key(db, code)
	NDBM_File	db
	SV *		code
	SV *		RETVAL = &PL_sv_undef ;
	CODE:
	    setFilter(filter_fetch_key) ;

SV *
filter_store_key(db, code)
	NDBM_File	db
	SV *		code
	SV *		RETVAL =  &PL_sv_undef ;
	CODE:
	    setFilter(filter_store_key) ;

SV *
filter_fetch_value(db, code)
	NDBM_File	db
	SV *		code
	SV *		RETVAL =  &PL_sv_undef ;
	CODE:
	    setFilter(filter_fetch_value) ;

SV *
filter_store_value(db, code)
	NDBM_File	db
	SV *		code
	SV *		RETVAL =  &PL_sv_undef ;
	CODE:
	    setFilter(filter_store_value) ;

