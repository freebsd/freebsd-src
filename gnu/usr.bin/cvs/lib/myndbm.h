/* @(#)myndbm.h 1.3 92/02/29	 */

#ifdef MY_NDBM

#define	DBLKSIZ	4096

typedef struct
{
    List *dbm_list;			/* cached database */
    Node *dbm_next;			/* next key to return for nextkey() */
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

#if __STDC__
DBM *mydbm_open (char *file, int flags, int mode);
void mydbm_close (DBM * db);
datum mydbm_fetch (DBM * db, datum key);
datum mydbm_firstkey (DBM * db);
datum mydbm_nextkey (DBM * db);
#else
DBM *mydbm_open ();
void mydbm_close ();
datum mydbm_fetch ();
datum mydbm_firstkey ();
datum mydbm_nextkey ();
#endif				/* __STDC__ */

#endif				/* MY_NDBM */
