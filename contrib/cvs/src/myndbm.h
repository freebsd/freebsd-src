#ifdef MY_NDBM

#define	DBLKSIZ	4096

typedef struct
{
    List *dbm_list;			/* cached database */
    Node *dbm_next;			/* next key to return for nextkey() */

    /* Name of the file to write to if modified is set.  malloc'd.  */
    char *name;

    /* Nonzero if the database has been modified and dbm_close needs to
       write it out to disk.  */
    int modified;
} DBM;

typedef struct
{
    char *dptr;
    int dsize;
} datum;

/*
 * So as not to conflict with other dbm_open, etc., routines that may
 * be included by someone's libc, all of my emulation routines are prefixed
 * by "my" and we define the "standard" ones to be "my" ones here.
 */
#define	dbm_open	mydbm_open
#define	dbm_close	mydbm_close
#define	dbm_fetch	mydbm_fetch
#define	dbm_firstkey	mydbm_firstkey
#define	dbm_nextkey	mydbm_nextkey
#define dbm_store	mydbm_store
#define  DBM_INSERT  0
#define  DBM_REPLACE 1

DBM *mydbm_open PROTO((char *file, int flags, int mode));
void mydbm_close PROTO((DBM * db));
datum mydbm_fetch PROTO((DBM * db, datum key));
datum mydbm_firstkey PROTO((DBM * db));
datum mydbm_nextkey PROTO((DBM * db));
extern int mydbm_store PROTO ((DBM *, datum, datum, int));

#endif				/* MY_NDBM */
