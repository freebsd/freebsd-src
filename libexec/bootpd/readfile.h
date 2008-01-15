/* readfile.h */
/* $FreeBSD: src/libexec/bootpd/readfile.h,v 1.2 2002/05/28 18:31:41 alfred Exp $ */

#include "bptypes.h"
#include "hash.h"

extern boolean hwlookcmp(hash_datum *, hash_datum *);
extern boolean iplookcmp(hash_datum *, hash_datum *);
extern boolean nmcmp(hash_datum *, hash_datum *);
extern void readtab(int);
extern void rdtab_init(void);
