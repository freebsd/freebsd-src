/*
 * Kernel-internal structure used to hold important bits of boot
 * information.  NOT to be used by boot blocks.
 *
 * Note that not all of the fields from the bootinfo struct(s)
 * passed by the boot blocks aren't here (because they're not currently
 * used by the kernel!).  Fields here which aren't supplied by the
 * bootinfo structure passed by the boot blocks are supposed to be
 * filled in at startup with sane contents.
 */
struct bootinfo_kernel {
	u_long	ssym;			/* start of syms */
	u_long	esym;			/* end of syms */
	u_long	hwrpb_phys;		/* hwrpb physical address */
	u_long	hwrpb_size;		/* size of hwrpb data */
	char	boot_flags[64];		/* boot flags */
	char	booted_kernel[64];	/* name of booted kernel */
	char	booted_dev[64];		/* name of booted device */
};

extern struct bootinfo_kernel bootinfo;
