/* readfile.h */
/* $FreeBSD: src/libexec/bootpd/readfile.h,v 1.2.32.1 2008/11/25 02:59:29 kensmith Exp $ */

#include "bptypes.h"
#include "hash.h"

extern boolean hwlookcmp(hash_datum *, hash_datum *);
extern boolean iplookcmp(hash_datum *, hash_datum *);
extern boolean nmcmp(hash_datum *, hash_datum *);
extern void readtab(int);
extern void rdtab_init(void);
