#ifndef _ASM_PPC64_PARAM_H
#define _ASM_PPC64_PARAM_H

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef HZ
#define HZ 100
#ifdef __KERNEL__
#if HZ == 100
/* ppc (like X86) is defined to provide userspace with a world where HZ=100
   We have to do this, (x*const)/const2 isnt optimised out because its not
   a null operation as it might overflow.. */
#define hz_to_std(a) (a)
#else
#define hz_to_std(a) ((a)*(100/HZ)+((a)*(100%HZ))/HZ)
#endif
#endif
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
# define CLOCKS_PER_SEC	HZ	/* frequency at which times() counts */
#endif

#endif /* _ASM_PPC64_PARAM_H */
