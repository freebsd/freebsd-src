/*
 * sdbm - ndbm work-alike hashed database library
 * tuning and portability constructs [not nearly enough]
 * author: oz@nexus.yorku.ca
 */

#define BYTESIZ		8

/*
 * important tuning parms (hah)
 */

#define SEEDUPS			/* always detect duplicates */
#define BADMESS			/* generate a message for worst case:
				   cannot make room after SPLTMAX splits */
/*
 * misc
 */
#ifdef DEBUG
#define debug(x)	printf x
#else
#define debug(x)
#endif
