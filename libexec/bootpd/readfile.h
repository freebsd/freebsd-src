/* readfile.h */
/* $FreeBSD: src/libexec/bootpd/readfile.h,v 1.2.36.1.2.1 2009/10/25 01:10:29 kensmith Exp $ */

#include "bptypes.h"
#include "hash.h"

extern boolean hwlookcmp(hash_datum *, hash_datum *);
extern boolean iplookcmp(hash_datum *, hash_datum *);
extern boolean nmcmp(hash_datum *, hash_datum *);
extern void readtab(int);
extern void rdtab_init(void);
