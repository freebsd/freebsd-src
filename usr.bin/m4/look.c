/*  File   : look.c
    Author : Ozan Yigit
    Updated: 4 May 1992
    Purpose: Hash table for M4 
*/

#include "mdef.h"
#include "extr.h"

ndptr hashtab[HASHSIZE];


/*
 * hash - get a hash value for string s
 */
int
hash(name)
char *name;
{
        register unsigned long h = 0;

        while (*name)
                h = (h << 5) + h + *name++;

        return h % HASHSIZE;
}

/* 
 * lookup(name) - find name in the hash table
 */
ndptr lookup(name)
    char *name;
    {
	register ndptr p;

	for (p = hashtab[hash(name)]; p != nil; p = p->nxtptr)
	    if (strcmp(name, p->name) == 0)
		break;
	return p;
    }

/*  
 * addent(name) - hash and create an entry in the hash table.
 * The new entry is added at the front of a hash bucket.
 * BEWARE: the type and defn fields are UNDEFINED.
 */
ndptr addent(name)
    char *name;
    {
	register ndptr p, *h;

	p = (ndptr)malloc(sizeof *p);
	if (p == NULL) error("m4: no more memory.");
	h = &hashtab[hash(name)];
	p->name = strsave(name);
	p->defn = null;
	p->nxtptr = *h;
	*h = p;
	return p;
    }


/*  
 * addkywd(name, type) - stores a keyword in the hash table.
 */
void addkywd(name, type)
    char *name;
    int type;
    {
	register ndptr p = addent(name);
	p->type = type | STATIC;
    }


/* 
 * remhash(name, all)
 * remove one entry (all==0) or all entries (all!=0) for a given name
 * from the hash table.  All hash table entries must have been obtained
 * from malloc(), so it is safe to free the records themselves.
 * However, the ->name and ->defn fields might point to storage which
 * was obtained from strsave() -- in which case they may be freed -- or
 * to static storage -- in which case they must not be freed.  If the
 * STATIC bit is set, the fields are not to be freed.
 */
void remhash(name, all)
    char *name;
    int all;
    {
	register ndptr p, *h;
	/*  h always points to the pointer to p  */

	h = &hashtab[hash(name)];
	while ((p = *h) != nil) {
	    if (strcmp(p->name, name) == 0) {
		*h = p->nxtptr;			/* delink this record */		
		if (!(p->type & STATIC)) {	/* free the name and defn */
		    free(p->name);		/* if they came from strsave */
		    if (p->defn != null) free(p->defn);
		}				/* otherwise leave them */
		free(p);			/* free the record itself */
		if (!all) return;		/* first occurrence has gone */
	    } else {
		h = &(p->nxtptr);
	    }
	}
    }

