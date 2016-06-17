/*
 * ELF definitions for 32-bit CPUs
 */

#define ELF_EXEC_PAGESIZE	4096

#ifdef __KERNEL__

/*
 * 32-bit code is always OK.  Some cpus can do 26-bit, some can't.
 */
#define ELF_PROC_OK(x) 							 \
	(( (elf_hwcap & HWCAP_THUMB) && ((x)->e_entry & 1) == 1)      || \
	 (!(elf_hwcap & HWCAP_THUMB) && ((x)->e_entry & 3) == 0)      || \
	 ( (elf_hwcap & HWCAP_26BIT) && (x)->e_flags & EF_ARM_APCS26) || \
	 ((x)->e_flags & EF_ARM_APCS26) == 0)

/* Old NetWinder binaries were compiled in such a way that the iBCS
   heuristic always trips on them.  Until these binaries become uncommon
   enough not to care, don't trust the `ibcs' flag here.  In any case
   there is no other ELF system currently supported by iBCS.
   @@ Could print a warning message to encourage users to upgrade.  */
#define SET_PERSONALITY(ex,ibcs2) \
	set_personality(((ex).e_flags&EF_ARM_APCS26 ?PER_LINUX :PER_LINUX_32BIT))

#endif
