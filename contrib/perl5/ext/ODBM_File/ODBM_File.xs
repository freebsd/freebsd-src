#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#ifdef I_DBM
/* If using the DB3 emulation, ENTER is defined both
 * by DB3 and Perl.  We drop the Perl definition now.
 * See also INSTALL section on DB3.
 * -- Stanislav Brabec <utx@penguin.cz> */
#  undef ENTER
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

typedef struct {
	void * 	dbp ;
	SV *    filter_fetch_key ;
	SV *    filter_store_key ;
	SV *    filter_fetch_value ;
	SV *    filter_store_value ;
	int     filtering ;
	} ODBM_File_type;

typedef ODBM_File_type * ODBM_File ;
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

ODBM_File
odbm_TIEHASH(dbtype, filename, flags, mode)
	char *		dbtype
	char *		filename
	int		flags
	int		mode
	CODE:
	{
	    char *tmpbuf;
	    void * dbp ;
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
	    dbp = (void*)(dbminit(filename) >= 0 ? &dbmrefcnt : 0);
	    RETVAL = (ODBM_File)safemalloc(sizeof(ODBM_File_type)) ;
    	    Zero(RETVAL, 1, ODBM_File_type) ;
	    RETVAL->dbp = dbp ;
	    ST(0) = sv_mortalcopy(&PL_sv_undef);
	    sv_setptrobj(ST(0), RETVAL, dbtype);
	}

void
DESTROY(db)
	ODBM_File	db
	CODE:
	dbmrefcnt--;
	dbmclose();
	safefree(db);

datum_value
odbm_FETCH(db, key)
	ODBM_File	db
	datum_key	key

int
odbm_STORE(db, key, value, flags = DBM_REPLACE)
	ODBM_File	db
	datum_key	key
	datum_value	value
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
	datum_key	key

datum_key
odbm_FIRSTKEY(db)
	ODBM_File	db

datum_key
odbm_NEXTKEY(db, key)
	ODBM_File	db
	datum_key	key


#define setFilter(type)					\
	{						\
	    if (db->type)				\
	        RETVAL = sv_mortalcopy(db->type) ; 	\
	    ST(0) = RETVAL ;				\
	    if (db->type && (code == &PL_sv_undef)) {	\
                SvREFCNT_dec(db->type) ;		\
	        db->type = Nullsv ;			\
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
	ODBM_File	db
	SV *		code
	SV *		RETVAL = &PL_sv_undef ;
	CODE:
	    setFilter(filter_fetch_key) ;

SV *
filter_store_key(db, code)
	ODBM_File	db
	SV *		code
	SV *		RETVAL =  &PL_sv_undef ;
	CODE:
	    setFilter(filter_store_key) ;

SV *
filter_fetch_value(db, code)
	ODBM_File	db
	SV *		code
	SV *		RETVAL =  &PL_sv_undef ;
	CODE:
	    setFilter(filter_fetch_value) ;

SV *
filter_store_value(db, code)
	ODBM_File	db
	SV *		code
	SV *		RETVAL =  &PL_sv_undef ;
	CODE:
	    setFilter(filter_store_value) ;

