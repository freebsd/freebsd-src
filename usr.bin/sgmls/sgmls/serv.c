#include "sgmlincl.h"         /* #INCLUDE statements for SGML parser. */
/* ETDDEF: Define an element type definition.
           Use an existing one if there is one; otherwise create one, which
           rmalloc initializes to zero which shows it is a virgin etd.
*/
PETD etddef(ename)
UNCH *ename;                  /* Element name (GI) with length byte. */
{
     PETD p;                  /* Pointer to an etd. */
     int hnum;                /* Hash number for ename. */

     if ((p = (PETD)hfind((THASH)etdtab,ename,hnum = hash(ename, ETDHASH)))==0){
          p = (PETD)hin((THASH)etdtab, ename, hnum, ETDSZ);
     }
     return p;
}
/* ETDSET: Store data in an element type definition.
           The etd must be valid and virgin (except for adl and etdmin).
           As an etd cannot be modified, there is no checking for existing
           pointers and no freeing of their storage.
*/
#ifdef USE_PROTOTYPES
PETD etdset(PETD p, UNCH fmin, struct thdr *cmod, PETD *mexgrp, PETD *pexgrp,
	    struct entity **srm)
#else
PETD etdset(p, fmin, cmod, mexgrp, pexgrp, srm)
PETD p;                       /* Pointer to an etd. */
UNCH fmin;                    /* Minimization bit flags. */
struct thdr *cmod;            /* Pointer to content model. */
PETD *mexgrp;                 /* Pointers to minus and plus exception lists. */
PETD *pexgrp;                 /* Pointers to minus and plus exception lists. */
struct entity **srm;          /* Short reference map. */
#endif
{
     p->etdmin |= fmin;
     p->etdmod = cmod;
     p->etdmex = mexgrp;
     p->etdpex = pexgrp;
     p->etdsrm = srm;
     return p;
}
/* ETDREF: Retrieve the pointer to an element type definition.
*/
PETD etdref(ename)
UNCH *ename;                  /* Element name (GI) with length byte.. */
{

     return (PETD)hfind((THASH)etdtab, ename, hash(ename, ETDHASH));
}
/* ETDCAN: Cancel an element definition.  The etd is freed and is removed
           from the hash table, but its model and other pointers are not freed.
*/
VOID etdcan(ename)
UNCH *ename;                  /* GI name (with length and EOS). */
{
     PETD p;

     if ((p = (PETD)hout((THASH)etdtab, ename, hash(ename, ETDHASH)))!=0)
          frem((UNIV)p);
}
/* SYMBOL TABLE FUNCTIONS: These functions manage hash tables that are used
   for entities, element type definitions, IDs, and other purposes.  The
   interface will be expanded in the future to include multiple environments,
   probably by creating arrays of the present hash tables with each table
   in the array corresponding to an environment level.
*/
/* HASH: Form hash value for a string.
         From the Dragon Book, p436.
*/
int hash(s, hashsize)
UNCH *s;                      /* String to be hashed. */
int hashsize;                 /* Size of hash table array. */
{
     unsigned long h = 0, g;
     
     while (*s != 0) {
	  h <<= 4;
	  h += *s++;
	  if ((g = h & 0xf0000000) != 0) {
	       h ^= g >> 24;
	       h ^= g;
	  }
     }
     return (int)(h % hashsize);
}
/* HFIND: Look for a name in a hash table.
*/
struct hash *hfind(htab, s, h)
struct hash *htab[];          /* Hash table. */
UNCH *s;                      /* Entity name. */
int h;                        /* Hash value for entity name. */
{
     struct hash *np;

     for (np = htab[h]; np != 0; np = np->enext)
          if (ustrcmp(s, np->ename) == 0) return np;    /* Found it. */
     return (struct hash *)0;                          /* Not found. */
}
/* HIN: Locates an entry in a hash table, or allocates a new one.
        Returns a pointer to a structure containing a name
        and a pointer to the next entry.  Other data in the
        structure must be maintained by the caller.
*/
struct hash *hin(htab, name, h, size)
struct hash *htab[];          /* Hash table. */
UNCH *name;                   /* Entity name. */
int h;                        /* Hash value for entity name. */
UNS size;                     /* Size of structures pointed to by table. */
{
     struct hash *np;

     if ((np = hfind(htab, name, h))!=0) return np;  /* Return if name found. */
     /* Allocate space for structure and name. */
     np = (struct hash *)rmalloc(size + name[0]);
     np->ename = (UNCH *)np + size;
     memcpy(np->ename, name, name[0]);            /* Store name in it. */
     np->enext = htab[h];                         /* 1st entry is now 2nd.*/
     htab[h] = np;                                /* New entry is now 1st.*/
     return np;                                   /* Return new entry ptr. */
}
/* HOUT: Remove an entry from a hash table and return its pointer.
         The caller must free any pointers in the entry and then
         free the entry itself if that is what is desired; this
         routine does not free any storage.
*/
struct hash *hout(htab, s, h)
struct hash *htab[];          /* Hash table. */
UNCH *s;                      /* Search argument entry name. */
int h;                        /* Hash value for search entry name. */
{
     struct hash **pp;

     for (pp = &htab[h]; *pp != 0; pp = &(*pp)->enext)
          if (ustrcmp(s, (*pp)->ename) == 0) {   /* Found it. */
	       struct hash *tem = *pp;
	       *pp = (*pp)->enext;
               return tem;
          }
     return 0;                /* NULL if not found; else ptr. */
}
/* SAVESTR: Save a null-terminated string
*/
UNCH *savestr(s)
UNCH *s;
{
     UNCH *rp;

     rp = (UNCH *)rmalloc(ustrlen(s) + 1);
     ustrcpy(rp, s);
     return rp;
}
/* SAVENM: Save a name (with length and EOS)
*/
UNCH *savenm(s)
UNCH *s;
{
     UNCH *p;
     p = (UNCH *)rmalloc(*s);
     memcpy(p, s, *s);
     return p;
}
/* REPLACE: Free the storage for the old string (p) and store the new (s).
            If the specified ptr is NULL, don't free it.
*/
UNCH *replace(p, s)
UNCH *p;
UNCH *s;
{
     if (p) frem((UNIV)p);               /* Free old storage (if any). */
     if (!s) return(s);            /* Return NULL if new string is NULL. */
     return savestr(s);
}
/* RMALLOC: Interface to memory allocation with error handling.
            If storage is not available, fatal error message is issued.
            Storage is initialized to zeros.
*/
UNIV rmalloc(size)
unsigned size;                /* Number of bytes of initialized storage. */
{
     UNIV p = (UNIV)calloc(size, 1);
     if (!p) exiterr(33, (struct parse *)0);
     return p;
}
UNIV rrealloc(p, n)
UNIV p;
UNS n;
{
     UNIV r = realloc(p, n);
     if (!r)
	  exiterr(33, (struct parse *)0);
     return r;
}

UNCH *pt;
/* FREM: Free specified memory area gotten with rmalloc().
*/
VOID frem(ptr)
UNIV ptr;                     /* Memory area to be freed. */
{
     free(ptr);
}
/* MAPSRCH: Find a string in a table and return its associated value.
            The last entry must be a dummy consisting of a NULL pointer for
            the string and whatever return code is desired if the
            string is not found in the table.
*/
int mapsrch(maptab, name)
struct map maptab[];
UNCH *name;
{
     int i = 0;

     do {
	  UNCH *mapnm, *nm;
          for (mapnm = maptab[i].mapnm, nm=name; *nm==*mapnm; mapnm++) {
               if (!*nm++) return maptab[i].mapdata;
          }
     } while (maptab[++i].mapnm);
     return maptab[i].mapdata;
}
/* IDDEF: Define an ID control block; return -1 if it already exists.
*/
int iddef(iname)
UNCH *iname;                  /* ID name (with length and EOS). */
{
     PID p;
     struct fwdref *r;

     p = (PID)hin((THASH)itab, iname, hash(iname, IDHASH), IDSZ);
     if (p->iddefed) return(-1);
     p->iddefed = 1;
     TRACEID("IDDEF", p);
     /* Delete any forward references. */
     r = p->idrl;
     p->idrl = 0;
     while (r) {
	  struct fwdref *tem = r->next;
	  if (r->msg)
	       msgsfree(r->msg);
	  frem((UNIV)r);
	  r = tem;
     }
     return(0);
}
/* IDREF: Store a reference to an ID and define the ID if it doesn't yet exist.
          Return 0 if already defined, otherwise pointer to a fwdref.
*/
struct fwdref *idref(iname)
UNCH *iname;                  /* ID name (with length and EOS). */
{
     PID p;
     int hnum;
     struct fwdref *rp;

     if ((p = (PID)hfind((THASH)itab, iname, (hnum = hash(iname, IDHASH))))==0)
          p = (PID)hin((THASH)itab, iname, hnum, IDSZ);
     if (p->iddefed)
	  return 0;
     rp = (struct fwdref *)rmalloc(FWDREFSZ);
     rp->next = p->idrl;
     p->idrl = rp;
     rp->msg = 0;
     TRACEID("IDREF", p);
     return rp;
}
/* IDRCK: Check idrefs.
*/
VOID idrck()
{
     int i;
     PID p;
     struct fwdref *r;

     for (i = 0; i < IDHASH; i++)
	  for (p = itab[i]; p; p = p->idnext)
	       if (!p->iddefed)
		    for (r = p->idrl; r; r = r->next)
			 svderr(r->msg);
}
/* NTOA: Converts a positive integer to an ASCII string (abuf)
         No leading zeros are generated.
*/
UNCH *ntoa(i)
int i;
{
     static UNCH buf[1 + 3*sizeof(int) + 1];
     sprintf((char *)buf, "%d", i);
     return buf;
}
/*
Local Variables:
c-indent-level: 5
c-continued-statement-offset: 5
c-brace-offset: -5
c-argdecl-indent: 0
c-label-offset: -5
comment-column: 30
End:
*/
