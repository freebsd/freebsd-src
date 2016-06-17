#ifndef _PPC64_INIT_H
#define _PPC64_INIT_H

#include <linux/init.h>

#if __GNUC__ > 2 || __GNUC_MINOR__ >= 90 /* egcs */
/* DRENG add back in when we get section attribute support */
#define __chrp __attribute__ ((__section__ (".text.chrp")))
#define __chrpdata __attribute__ ((__section__ (".data.chrp")))
#define __chrpfunc(__argchrp) \
	__argchrp __chrp; \
	__argchrp

/* this is actually just common chrp/pmac code, not OF code -- Cort */
#define __openfirmware __attribute__ ((__section__ (".text.openfirmware")))
#define __openfirmwaredata __attribute__ ((__section__ (".data.openfirmware")))
#define __openfirmwarefunc(__argopenfirmware) \
	__argopenfirmware __openfirmware; \
	__argopenfirmware
	
#else /* not egcs */

#define __openfirmware
#define __openfirmwaredata
#define __openfirmwarefunc(x) x

#endif /* egcs */

#endif /* _PPC64_INIT_H */
