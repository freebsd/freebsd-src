/* readfile.h */
/* $FreeBSD: src/libexec/bootpd/readfile.h,v 1.2.30.1 2008/10/02 02:57:24 kensmith Exp $ */

#include "bptypes.h"
#include "hash.h"

extern boolean hwlookcmp(hash_datum *, hash_datum *);
extern boolean iplookcmp(hash_datum *, hash_datum *);
extern boolean nmcmp(hash_datum *, hash_datum *);
extern void readtab(int);
extern void rdtab_init(void);
