/*
 * ELF definitions for 26-bit CPUs
 */

#define ELF_EXEC_PAGESIZE	32768

#ifdef __KERNEL__

/* We can only execute 26-bit code. */
#define ELF_PROC_OK(x)		\
	((x)->e_flags & EF_ARM_APCS26)

#define SET_PERSONALITY(ex,ibcs2) set_personality(PER_LINUX)

#endif
