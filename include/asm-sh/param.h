#ifndef __ASM_SH_PARAM_H
#define __ASM_SH_PARAM_H

#ifndef HZ
#define HZ 100
#endif

#define EXEC_PAGESIZE	4096

#ifndef NGROUPS
#define NGROUPS		32
#endif

#ifndef NOGROUP
#define NOGROUP		(-1)
#endif

#define MAXHOSTNAMELEN	64	/* max length of hostname */

#ifdef __KERNEL__
#define CLOCKS_PER_SEC	HZ	/* frequency at which times() counts */
#endif

#endif /* __ASM_SH_PARAM_H */
