#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <gdbm.h>
#include <fcntl.h>

typedef struct {
	GDBM_FILE 	dbp ;
	SV *    filter_fetch_key ;
	SV *    filter_store_key ;
	SV *    filter_fetch_value ;
	SV *    filter_store_value ;
	int     filtering ;
	} GDBM_File_type;

typedef GDBM_File_type * GDBM_File ;
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



#define GDBM_BLOCKSIZE 0 /* gdbm defaults to stat blocksize */

typedef void (*FATALFUNC)();

#ifndef GDBM_FAST
static int
not_here(char *s)
{
    croak("GDBM_File::%s not implemented on this architecture", s);
    return -1;
}
#endif

/* GDBM allocates the datum with system malloc() and expects the user
 * to free() it.  So we either have to free() it immediately, or have
 * perl free() it when it deallocates the SV, depending on whether
 * perl uses malloc()/free() or not. */
static void
output_datum(pTHX_ SV *arg, char *str, int size)
{
#if !defined(MYMALLOC) || (defined(MYMALLOC) && defined(PERL_POLLUTE_MALLOC) && !defined(LEAKTEST))
	sv_usepvn(arg, str, size);
#else
	sv_setpvn(arg, str, size);
	safesysfree(str);
#endif
}

/* Versions of gdbm prior to 1.7x might not have the gdbm_sync,
   gdbm_exists, and gdbm_setopt functions.  Apparently Slackware
   (Linux) 2.1 contains gdbm-1.5 (which dates back to 1991).
*/
#ifndef GDBM_FAST
#define gdbm_exists(db,key) not_here("gdbm_exists")
#define gdbm_sync(db) (void) not_here("gdbm_sync")
#define gdbm_setopt(db,optflag,optval,optlen) not_here("gdbm_setopt")
#endif

static double
constant(char *name, int arg)
{
    errno = 0;
    switch (*name) {
    case 'A':
	break;
    case 'B':
	break;
    case 'C':
	break;
    case 'D':
	break;
    case 'E':
	break;
    case 'F':
	break;
    case 'G':
	if (strEQ(name, "GDBM_CACHESIZE"))
#ifdef GDBM_CACHESIZE
	    return GDBM_CACHESIZE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "GDBM_FAST"))
#ifdef GDBM_FAST
	    return GDBM_FAST;
#else
	    goto not_there;
#endif
	if (strEQ(name, "GDBM_FASTMODE"))
#ifdef GDBM_FASTMODE
	    return GDBM_FASTMODE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "GDBM_INSERT"))
#ifdef GDBM_INSERT
	    return GDBM_INSERT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "GDBM_NEWDB"))
#ifdef GDBM_NEWDB
	    return GDBM_NEWDB;
#else
	    goto not_there;
#endif
	if (strEQ(name, "GDBM_NOLOCK"))
#ifdef GDBM_NOLOCK
	    return GDBM_NOLOCK;
#else
	    goto not_there;
#endif
	if (strEQ(name, "GDBM_READER"))
#ifdef GDBM_READER
	    return GDBM_READER;
#else
	    goto not_there;
#endif
	if (strEQ(name, "GDBM_REPLACE"))
#ifdef GDBM_REPLACE
	    return GDBM_REPLACE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "GDBM_WRCREAT"))
#ifdef GDBM_WRCREAT
	    return GDBM_WRCREAT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "GDBM_WRITER"))
#ifdef GDBM_WRITER
	    return GDBM_WRITER;
#else
	    goto not_there;
#endif
	break;
    case 'H':
	break;
    case 'I':
	break;
    case 'J':
	break;
    case 'K':
	break;
    case 'L':
	break;
    case 'M':
	break;
    case 'N':
	break;
    case 'O':
	break;
    case 'P':
	break;
    case 'Q':
	break;
    case 'R':
	break;
    case 'S':
	break;
    case 'T':
	break;
    case 'U':
	break;
    case 'V':
	break;
    case 'W':
	break;
    case 'X':
	break;
    case 'Y':
	break;
    case 'Z':
	break;
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

MODULE = GDBM_File	PACKAGE = GDBM_File	PREFIX = gdbm_

double
constant(name,arg)
	char *		name
	int		arg


GDBM_File
gdbm_TIEHASH(dbtype, name, read_write, mode, fatal_func = (FATALFUNC)croak)
	char *		dbtype
	char *		name
	int		read_write
	int		mode
	FATALFUNC	fatal_func
	CODE:
	{
	    GDBM_FILE  	dbp ;

	    RETVAL = NULL ;
	    if ((dbp =  gdbm_open(name, GDBM_BLOCKSIZE, read_write, mode, fatal_func))) {
	        RETVAL = (GDBM_File)safemalloc(sizeof(GDBM_File_type)) ;
    	        Zero(RETVAL, 1, GDBM_File_type) ;
		RETVAL->dbp = dbp ;
	    }
	    
	}
	OUTPUT:
	  RETVAL
	

#define gdbm_close(db)			gdbm_close(db->dbp)
void
gdbm_close(db)
	GDBM_File	db
	CLEANUP:

void
gdbm_DESTROY(db)
	GDBM_File	db
	CODE:
	gdbm_close(db);
	safefree(db);

#define gdbm_FETCH(db,key)			gdbm_fetch(db->dbp,key)
datum_value
gdbm_FETCH(db, key)
	GDBM_File	db
	datum_key	key

#define gdbm_STORE(db,key,value,flags)		gdbm_store(db->dbp,key,value,flags)
int
gdbm_STORE(db, key, value, flags = GDBM_REPLACE)
	GDBM_File	db
	datum_key	key
	datum_value	value
	int		flags
    CLEANUP:
	if (RETVAL) {
	    if (RETVAL < 0 && errno == EPERM)
		croak("No write permission to gdbm file");
	    croak("gdbm store returned %d, errno %d, key \"%.*s\"",
			RETVAL,errno,key.dsize,key.dptr);
	}

#define gdbm_DELETE(db,key)			gdbm_delete(db->dbp,key)
int
gdbm_DELETE(db, key)
	GDBM_File	db
	datum_key	key

#define gdbm_FIRSTKEY(db)			gdbm_firstkey(db->dbp)
datum_key
gdbm_FIRSTKEY(db)
	GDBM_File	db

#define gdbm_NEXTKEY(db,key)			gdbm_nextkey(db->dbp,key)
datum_key
gdbm_NEXTKEY(db, key)
	GDBM_File	db
	datum_key	key

#define gdbm_reorganize(db)			gdbm_reorganize(db->dbp)
int
gdbm_reorganize(db)
	GDBM_File	db


#define gdbm_sync(db)				gdbm_sync(db->dbp)
void
gdbm_sync(db)
	GDBM_File	db

#define gdbm_EXISTS(db,key)			gdbm_exists(db->dbp,key)
int
gdbm_EXISTS(db, key)
	GDBM_File	db
	datum_key	key

#define gdbm_setopt(db,optflag, optval, optlen)	gdbm_setopt(db->dbp,optflag, optval, optlen)
int
gdbm_setopt (db, optflag, optval, optlen)
	GDBM_File	db
	int		optflag
	int		&optval
	int		optlen


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
	GDBM_File	db
	SV *		code
	SV *		RETVAL = &PL_sv_undef ;
	CODE:
	    setFilter(filter_fetch_key) ;

SV *
filter_store_key(db, code)
	GDBM_File	db
	SV *		code
	SV *		RETVAL =  &PL_sv_undef ;
	CODE:
	    setFilter(filter_store_key) ;

SV *
filter_fetch_value(db, code)
	GDBM_File	db
	SV *		code
	SV *		RETVAL =  &PL_sv_undef ;
	CODE:
	    setFilter(filter_fetch_value) ;

SV *
filter_store_value(db, code)
	GDBM_File	db
	SV *		code
	SV *		RETVAL =  &PL_sv_undef ;
	CODE:
	    setFilter(filter_store_value) ;

