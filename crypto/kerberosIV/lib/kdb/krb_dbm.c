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

#include "kdb_locl.h"

RCSID("$Id: krb_dbm.c,v 1.37 1999/09/16 20:41:49 assar Exp $");

#include <xdbm.h>

#define KERB_DB_MAX_RETRY 5

#ifdef DEBUG
extern int debug;
extern long kerb_debug;
extern char *progname;
#endif

static int init = 0;
static char default_db_name[] = DBM_FILE;
static char *current_db_name = default_db_name;

static struct timeval timestamp;/* current time of request */
static int non_blocking = 0;

/*
 * This module contains all of the code which directly interfaces to
 * the underlying representation of the Kerberos database; this
 * implementation uses a DBM or NDBM indexed "file" (actually
 * implemented as two separate files) to store the relations, plus a
 * third file as a semaphore to allow the database to be replaced out
 * from underneath the KDC server.
 */

/*
 * Locking:
 * 
 * There are two distinct locking protocols used.  One is designed to
 * lock against processes (the admin_server, for one) which make
 * incremental changes to the database; the other is designed to lock
 * against utilities (kdb_util, kpropd) which replace the entire
 * database in one fell swoop.
 *
 * The first locking protocol is implemented using flock() in the 
 * krb_dbl_lock() and krb_dbl_unlock routines.
 *
 * The second locking protocol is necessary because DBM "files" are
 * actually implemented as two separate files, and it is impossible to
 * atomically rename two files simultaneously.  It assumes that the
 * database is replaced only very infrequently in comparison to the time
 * needed to do a database read operation.
 *
 * A third file is used as a "version" semaphore; the modification
 * time of this file is the "version number" of the database.
 * At the start of a read operation, the reader checks the version
 * number; at the end of the read operation, it checks again.  If the
 * version number changed, or if the semaphore was nonexistant at
 * either time, the reader sleeps for a second to let things
 * stabilize, and then tries again; if it does not succeed after
 * KERB_DB_MAX_RETRY attempts, it gives up.
 * 
 * On update, the semaphore file is deleted (if it exists) before any
 * update takes place; at the end of the update, it is replaced, with
 * a version number strictly greater than the version number which
 * existed at the start of the update.
 * 
 * If the system crashes in the middle of an update, the semaphore
 * file is not automatically created on reboot; this is a feature, not
 * a bug, since the database may be inconsistant.  Note that the
 * absence of a semaphore file does not prevent another _update_ from
 * taking place later.  Database replacements take place automatically
 * only on slave servers; a crash in the middle of an update will be
 * fixed by the next slave propagation.  A crash in the middle of an
 * update on the master would be somewhat more serious, but this would
 * likely be noticed by an administrator, who could fix the problem and
 * retry the operation.
 */


/*
 * Utility routine: generate name of database file.
 */

static char *
gen_dbsuffix(char *db_name, char *sfx)
{
    char *dbsuffix;
    
    if (sfx == NULL)
	sfx = ".ok";

    asprintf (&dbsuffix, "%s%s", db_name, sfx);
    if (dbsuffix == NULL) {
	fprintf (stderr, "gen_dbsuffix: out of memory\n");
	exit(1);
    }
    return dbsuffix;
}

static void
decode_princ_key(datum *key, char *name, char *instance)
{
    strlcpy (name, key->dptr, ANAME_SZ);
    strlcpy (instance, (char *)key->dptr + ANAME_SZ, INST_SZ);
}

static void
encode_princ_contents(datum *contents, Principal *principal)
{
    contents->dsize = sizeof(*principal);
    contents->dptr = (char *) principal;
}

static void
decode_princ_contents (datum *contents, Principal *principal)
{
    memcpy(principal, contents->dptr, sizeof(*principal));
}

static void
encode_princ_key (datum *key, char *name, char *instance)
{
    static char keystring[ANAME_SZ + INST_SZ];

    memset(keystring, 0, ANAME_SZ + INST_SZ);
    strncpy(keystring, name, ANAME_SZ);
    strncpy(&keystring[ANAME_SZ], instance, INST_SZ);
    key->dptr = keystring;
    key->dsize = ANAME_SZ + INST_SZ;
}

static int dblfd = -1;		/* db LOCK fd */
static int mylock = 0;
static int inited = 0;

static int
kerb_dbl_init(void)
{
    if (!inited) {
	char *filename = gen_dbsuffix (current_db_name, ".ok");
	if ((dblfd = open(filename, O_RDWR)) < 0) {
	    fprintf(stderr, "kerb_dbl_init: couldn't open %s\n", filename);
	    fflush(stderr);
	    perror("open");
	    exit(1);
	}
	free(filename);
	inited++;
    }
    return (0);
}

static void
kerb_dbl_fini(void)
{
    close(dblfd);
    dblfd = -1;
    inited = 0;
    mylock = 0;
}

static int
kerb_dbl_lock(int mode)
{
    int flock_mode;
    
    if (!inited)
	kerb_dbl_init();
    if (mylock) {		/* Detect lock call when lock already
				 * locked */
	fprintf(stderr, "Kerberos locking error (mylock)\n");
	fflush(stderr);
	exit(1);
    }
    switch (mode) {
    case KERB_DBL_EXCLUSIVE:
	flock_mode = LOCK_EX;
	break;
    case KERB_DBL_SHARED:
	flock_mode = LOCK_SH;
	break;
    default:
	fprintf(stderr, "invalid lock mode %d\n", mode);
	abort();
    }
    if (non_blocking)
	flock_mode |= LOCK_NB;
    
    if (flock(dblfd, flock_mode) < 0) 
	return errno;
    mylock++;
    return 0;
}

static void
kerb_dbl_unlock(void)
{
    if (!mylock) {		/* lock already unlocked */
	fprintf(stderr, "Kerberos database lock not locked when unlocking.\n");
	fflush(stderr);
	exit(1);
    }
    if (flock(dblfd, LOCK_UN) < 0) {
	fprintf(stderr, "Kerberos database lock error. (unlocking)\n");
	fflush(stderr);
	perror("flock");
	exit(1);
    }
    mylock = 0;
}

int
kerb_db_set_lockmode(int mode)
{
    int old = non_blocking;
    non_blocking = mode;
    return old;
}

/*
 * initialization for data base routines.
 */

int
kerb_db_init(void)
{
    init = 1;
    return (0);
}

/*
 * gracefully shut down database--must be called by ANY program that does
 * a kerb_db_init 
 */

void
kerb_db_fini(void)
{
}

/*
 * Set the "name" of the current database to some alternate value.
 *
 * Passing a null pointer as "name" will set back to the default.
 * If the alternate database doesn't exist, nothing is changed.
 */

int
kerb_db_set_name(char *name)
{
    DBM *db;

    if (name == NULL)
	name = default_db_name;
    db = dbm_open(name, 0, 0);
    if (db == NULL)
	return errno;
    dbm_close(db);
    kerb_dbl_fini();
    current_db_name = name;
    return 0;
}

/*
 * Return the last modification time of the database.
 */

time_t
kerb_get_db_age(void)
{
    struct stat st;
    char *okname;
    time_t age;
    
    okname = gen_dbsuffix(current_db_name, ".ok");

    if (stat (okname, &st) < 0)
	age = 0;
    else
	age = st.st_mtime;

    free (okname);
    return age;
}

/*
 * Remove the semaphore file; indicates that database is currently
 * under renovation.
 *
 * This is only for use when moving the database out from underneath
 * the server (for example, during slave updates).
 */

static time_t
kerb_start_update(char *db_name)
{
    char *okname = gen_dbsuffix(db_name, ".ok");
    time_t age = kerb_get_db_age();
    
    if (unlink(okname) < 0
	&& errno != ENOENT) {
	    age = -1;
    }
    free (okname);
    return age;
}

static int
kerb_end_update(char *db_name, time_t age)
{
    int fd;
    int retval = 0;
    char *new_okname = gen_dbsuffix(db_name, ".ok#");
    char *okname = gen_dbsuffix(db_name, ".ok");
    
    fd = open (new_okname, O_CREAT|O_RDWR|O_TRUNC, 0600);
    if (fd < 0)
	retval = errno;
    else {
	struct stat st;
	struct utimbuf tv;
	/* make sure that semaphore is "after" previous value. */
	if (fstat (fd, &st) == 0
	    && st.st_mtime <= age) {
	    tv.actime = st.st_atime;
	    tv.modtime = age;
	    /* set times.. */
	    utime (new_okname, &tv);
	    fsync(fd);
	}
	close(fd);
	if (rename (new_okname, okname) < 0)
	    retval = errno;
    }

    free (new_okname);
    free (okname);

    return retval;
}

static time_t
kerb_start_read(void)
{
    return kerb_get_db_age();
}

static int
kerb_end_read(time_t age)
{
    if (kerb_get_db_age() != age || age == -1) {
	return -1;
    }
    return 0;
}

/*
 * Create the database, assuming it's not there.
 */
int
kerb_db_create(char *db_name)
{
    char *okname = gen_dbsuffix(db_name, ".ok");
    int fd;
    int ret = 0;
#ifdef NDBM
    DBM *db;

    db = dbm_open(db_name, O_RDWR|O_CREAT|O_EXCL, 0600);
    if (db == NULL)
	ret = errno;
    else
	dbm_close(db);
#else
    char *dirname = gen_dbsuffix(db_name, ".dir");
    char *pagname = gen_dbsuffix(db_name, ".pag");

    fd = open(dirname, O_RDWR|O_CREAT|O_EXCL, 0600);
    if (fd < 0)
	ret = errno;
    else {
	close(fd);
	fd = open (pagname, O_RDWR|O_CREAT|O_EXCL, 0600);
	if (fd < 0)
	    ret = errno;
	else
	    close(fd);
    }
    if (dbminit(db_name) < 0)
	ret = errno;
#endif
    if (ret == 0) {
	fd = open (okname, O_CREAT|O_RDWR|O_TRUNC, 0600);
	if (fd < 0)
	    ret = errno;
	close(fd);
    }
    return ret;
}

/*
 * "Atomically" rename the database in a way that locks out read
 * access in the middle of the rename.
 *
 * Not perfect; if we crash in the middle of an update, we don't
 * necessarily know to complete the transaction the rename, but...
 */

int
kerb_db_rename(char *from, char *to)
{
#ifdef HAVE_NEW_DB
    char *fromdb = gen_dbsuffix (from, ".db");
    char *todb = gen_dbsuffix (to, ".db");
#else
    char *fromdir = gen_dbsuffix (from, ".dir");
    char *todir = gen_dbsuffix (to, ".dir");
    char *frompag = gen_dbsuffix (from , ".pag");
    char *topag = gen_dbsuffix (to, ".pag");
#endif
    char *fromok = gen_dbsuffix(from, ".ok");
    long trans = kerb_start_update(to);
    int ok = 0;
    
#ifdef HAVE_NEW_DB
    if (rename (fromdb, todb) == 0) {
	unlink (fromok);
	ok = 1;
    }
    free (fromdb);
    free (todb);
#else
    if ((rename (fromdir, todir) == 0)
	&& (rename (frompag, topag) == 0)) {
	unlink (fromok);
	ok = 1;
    }
    free (fromdir);
    free (todir);
    free (frompag);
    free (topag);
#endif
    free (fromok);
    if (ok)
	return kerb_end_update(to, trans);
    else
	return -1;
}

int
kerb_db_delete_principal (char *name, char *inst)
{
    DBM *db;
    int try;
    int done = 0;
    int code;
    datum key;
    
    if(!init)
	kerb_db_init();
    
    for(try = 0; try < KERB_DB_MAX_RETRY; try++){
	if((code = kerb_dbl_lock(KERB_DBL_EXCLUSIVE)) != 0)
	    return -1;
	
	db = dbm_open(current_db_name, O_RDWR, 0600);
	if(db == NULL)
	    return -1;
	encode_princ_key(&key, name, inst);
	if(dbm_delete(db, key) == 0)
	    done = 1;
	
	dbm_close(db);
	kerb_dbl_unlock();
	if(done)
	    break;
	if(!non_blocking)
	    sleep(1);
    }
    if(!done)
	return -1;
    return 0;
}


/*
 * look up a principal in the data base returns number of principals
 * found , and whether there were more than requested. 
 */

int
kerb_db_get_principal (char *name, char *inst, Principal *principal, 
		       unsigned int max, int *more)
{
    int     found = 0, code;
    int     wildp, wildi;
    datum   key, contents;
    char    testname[ANAME_SZ], testinst[INST_SZ];
    u_long trans;
    int try;
    DBM    *db;

    if (!init)
	kerb_db_init();		/* initialize database routines */

    for (try = 0; try < KERB_DB_MAX_RETRY; try++) {
	trans = kerb_start_read();

	if ((code = kerb_dbl_lock(KERB_DBL_SHARED)) != 0)
	    return -1;

	db = dbm_open(current_db_name, O_RDONLY, 0600);
	if (db == NULL)
	    return -1;

	*more = 0;

#ifdef DEBUG
	if (kerb_debug & 2)
	    fprintf(stderr,
		    "%s: db_get_principal for %s %s max = %d",
		    progname, name, inst, max);
#endif

	wildp = !strcmp(name, "*");
	wildi = !strcmp(inst, "*");

	if (!wildi && !wildp) {	/* nothing's wild */
	    encode_princ_key(&key, name, inst);
	    contents = dbm_fetch(db, key);
	    if (contents.dptr == NULL) {
		found = 0;
		goto done;
	    }
	    decode_princ_contents(&contents, principal);
#ifdef DEBUG
	    if (kerb_debug & 1) {
		fprintf(stderr, "\t found %s %s p_n length %d t_n length %d\n",
			principal->name, principal->instance,
			strlen(principal->name),
			strlen(principal->instance));
	    }
#endif
	    found = 1;
	    goto done;
	}
	/* process wild cards by looping through entire database */

	for (key = dbm_firstkey(db); key.dptr != NULL;
	     key = dbm_next(db, key)) {
	    decode_princ_key(&key, testname, testinst);
	    if ((wildp || !strcmp(testname, name)) &&
		(wildi || !strcmp(testinst, inst))) { /* have a match */
		if (found >= max) {
		    *more = 1;
		    goto done;
		} else {
		    found++;
		    contents = dbm_fetch(db, key);
		    decode_princ_contents(&contents, principal);
#ifdef DEBUG
		    if (kerb_debug & 1) {
			fprintf(stderr,
				"\tfound %s %s p_n length %d t_n length %d\n",
				principal->name, principal->instance,
				strlen(principal->name),
				strlen(principal->instance));
		    }
#endif
		    principal++; /* point to next */
		}
	    }
	}

    done:
	kerb_dbl_unlock();	/* unlock read lock */
	dbm_close(db);
	if (kerb_end_read(trans) == 0)
	    break;
	found = -1;
	if (!non_blocking)
	    sleep(1);
    }
    return (found);
}

/* Use long * rather than DBM * so that the database structure is private */

long *
kerb_db_begin_update(void)
{
    int code;

    gettimeofday(&timestamp, NULL);

    if (!init)
	kerb_db_init();

    if ((code = kerb_dbl_lock(KERB_DBL_EXCLUSIVE)) != 0)
	return 0;

    return (long *) dbm_open(current_db_name, O_RDWR, 0600);
}

void
kerb_db_end_update(long *db)
{
    dbm_close((DBM *)db);
    kerb_dbl_unlock();		/* unlock database */
}

int
kerb_db_update(long *db, Principal *principal, unsigned int max)
{
    int     found = 0;
    u_long  i;
    datum   key, contents;

#ifdef DEBUG
    if (kerb_debug & 2)
	fprintf(stderr, "%s: kerb_db_put_principal  max = %d",
	    progname, max);
#endif

    /* for each one, stuff temps, and do replace/append */
    for (i = 0; i < max; i++) {
	encode_princ_contents(&contents, principal);
	encode_princ_key(&key, principal->name, principal->instance);
	if(dbm_store((DBM *)db, key, contents, DBM_REPLACE) < 0)
	    return found; /* XXX some better mechanism to report
			     failure should exist */
#ifdef DEBUG
	if (kerb_debug & 1) {
	    fprintf(stderr, "\n put %s %s\n",
		principal->name, principal->instance);
	}
#endif
	found++;
	principal++;		/* bump to next struct			   */
    }
    return found;
}

/*
 * Update a name in the data base.  Returns number of names
 * successfully updated.
 */

int
kerb_db_put_principal(Principal *principal,
		      unsigned max)

{
    int found;
    long    *db;

    db = kerb_db_begin_update();
    if (db == 0)
	return -1;

    found = kerb_db_update(db, principal, max);

    kerb_db_end_update(db);
    return (found);
}

void
kerb_db_get_stat(DB_stat *s)
{
    gettimeofday(&timestamp, NULL);

    s->cpu = 0;
    s->elapsed = 0;
    s->dio = 0;
    s->pfault = 0;
    s->t_stamp = timestamp.tv_sec;
    s->n_retrieve = 0;
    s->n_replace = 0;
    s->n_append = 0;
    s->n_get_stat = 0;
    s->n_put_stat = 0;
    /* update local copy too */
}

void
kerb_db_put_stat(DB_stat *s)
{
}

void
delta_stat(DB_stat *a, DB_stat *b, DB_stat *c)
{
    /* c = a - b then b = a for the next time */

    c->cpu = a->cpu - b->cpu;
    c->elapsed = a->elapsed - b->elapsed;
    c->dio = a->dio - b->dio;
    c->pfault = a->pfault - b->pfault;
    c->t_stamp = a->t_stamp - b->t_stamp;
    c->n_retrieve = a->n_retrieve - b->n_retrieve;
    c->n_replace = a->n_replace - b->n_replace;
    c->n_append = a->n_append - b->n_append;
    c->n_get_stat = a->n_get_stat - b->n_get_stat;
    c->n_put_stat = a->n_put_stat - b->n_put_stat;

    memcpy(b, a, sizeof(DB_stat));
}

/*
 * look up a dba in the data base returns number of dbas found , and
 * whether there were more than requested. 
 */

int
kerb_db_get_dba(char *dba_name,	/* could have wild card */
		char *dba_inst,	/* could have wild card */
		Dba *dba,
		unsigned max,	/* max number of name structs to return */
		int *more)	/* where there more than 'max' tuples? */
{
    *more = 0;
    return (0);
}

int
kerb_db_iterate (k_iter_proc_t func, void *arg)
{
    datum key, contents;
    Principal *principal;
    int code;
    DBM *db;
    
    kerb_db_init();		/* initialize and open the database */
    if ((code = kerb_dbl_lock(KERB_DBL_SHARED)) != 0)
	return code;

    db = dbm_open(current_db_name, O_RDONLY, 0600);
    if (db == NULL)
	return errno;

    for (key = dbm_firstkey (db); key.dptr != NULL; key = dbm_next(db, key)) {
	contents = dbm_fetch (db, key);
	/* XXX may not be properly aligned */
	principal = (Principal *) contents.dptr;
	if ((code = (*func)(arg, principal)) != 0)
	    return code;
    }
    dbm_close(db);
    kerb_dbl_unlock();
    return 0;
}
