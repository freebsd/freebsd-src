#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "sdbm/sdbm.h"

typedef struct {
	DBM * 	dbp ;
	SV *    filter_fetch_key ;
	SV *    filter_store_key ;
	SV *    filter_fetch_value ;
	SV *    filter_store_value ;
	int     filtering ;
	} SDBM_File_type;

typedef SDBM_File_type * SDBM_File ;
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
	    sv_setsv(DEFSV, save_defsv) ;				\
	    SvREFCNT_dec(save_defsv) ;				\
	    db->filtering = FALSE ;				\
	    /*printf("end of filtering %s\n", name) ;*/		\
	}

#define sdbm_TIEHASH(dbtype,filename,flags,mode) sdbm_open(filename,flags,mode)
#define sdbm_FETCH(db,key)			sdbm_fetch(db->dbp,key)
#define sdbm_STORE(db,key,value,flags)		sdbm_store(db->dbp,key,value,flags)
#define sdbm_DELETE(db,key)			sdbm_delete(db->dbp,key)
#define sdbm_EXISTS(db,key)			sdbm_exists(db->dbp,key)
#define sdbm_FIRSTKEY(db)			sdbm_firstkey(db->dbp)
#define sdbm_NEXTKEY(db,key)			sdbm_nextkey(db->dbp)


MODULE = SDBM_File	PACKAGE = SDBM_File	PREFIX = sdbm_

SDBM_File
sdbm_TIEHASH(dbtype, filename, flags, mode)
	char *		dbtype
	char *		filename
	int		flags
	int		mode
	CODE:
	{
	    DBM * 	dbp ;

	    RETVAL = NULL ;
	    if ((dbp = sdbm_open(filename,flags,mode))) {
	        RETVAL = (SDBM_File)safemalloc(sizeof(SDBM_File_type)) ;
    	        Zero(RETVAL, 1, SDBM_File_type) ;
		RETVAL->dbp = dbp ;
	    }
	    
	}
	OUTPUT:
	  RETVAL

void
sdbm_DESTROY(db)
	SDBM_File	db
	CODE:
	  sdbm_close(db->dbp);
	  if (db->filter_fetch_key)
	    SvREFCNT_dec(db->filter_fetch_key) ;
	  if (db->filter_store_key)
	    SvREFCNT_dec(db->filter_store_key) ;
	  if (db->filter_fetch_value)
	    SvREFCNT_dec(db->filter_fetch_value) ;
	  if (db->filter_store_value)
	    SvREFCNT_dec(db->filter_store_value) ;
	  safefree(db) ;

datum_value
sdbm_FETCH(db, key)
	SDBM_File	db
	datum_key	key

int
sdbm_STORE(db, key, value, flags = DBM_REPLACE)
	SDBM_File	db
	datum_key	key
	datum_value	value
	int		flags
    CLEANUP:
	if (RETVAL) {
	    if (RETVAL < 0 && errno == EPERM)
		croak("No write permission to sdbm file");
	    croak("sdbm store returned %d, errno %d, key \"%s\"",
			RETVAL,errno,key.dptr);
	    sdbm_clearerr(db->dbp);
	}

int
sdbm_DELETE(db, key)
	SDBM_File	db
	datum_key	key

int
sdbm_EXISTS(db,key)
	SDBM_File	db
	datum_key	key

datum_key
sdbm_FIRSTKEY(db)
	SDBM_File	db

datum_key
sdbm_NEXTKEY(db, key)
	SDBM_File	db
	datum_key	key

int
sdbm_error(db)
	SDBM_File	db
	CODE:
	RETVAL = sdbm_error(db->dbp) ;
	OUTPUT:
	  RETVAL

int
sdbm_clearerr(db)
	SDBM_File	db
	CODE:
	RETVAL = sdbm_clearerr(db->dbp) ;
	OUTPUT:
	  RETVAL


#define setFilter(type)					\
	{						\
	    if (db->type)				\
	        RETVAL = sv_mortalcopy(db->type) ;	\
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
	SDBM_File	db
	SV *		code
	SV *		RETVAL = &PL_sv_undef ;
	CODE:
	    setFilter(filter_fetch_key) ;

SV *
filter_store_key(db, code)
	SDBM_File	db
	SV *		code
	SV *		RETVAL =  &PL_sv_undef ;
	CODE:
	    setFilter(filter_store_key) ;

SV *
filter_fetch_value(db, code)
	SDBM_File	db
	SV *		code
	SV *		RETVAL =  &PL_sv_undef ;
	CODE:
	    setFilter(filter_fetch_value) ;

SV *
filter_store_value(db, code)
	SDBM_File	db
	SV *		code
	SV *		RETVAL =  &PL_sv_undef ;
	CODE:
	    setFilter(filter_store_value) ;

