/*
 * Linker script for 64-bit vDSO.
 * Copied from Linux kernel arch/arm64/kernel/vdso/vdso.lds.S
 */

SECTIONS
{
	. = . + SIZEOF_HEADERS;

	.hash		: { *(.hash) }			:text
	.gnu.hash	: { *(.gnu.hash) }
	.dynsym		: { *(.dynsym) }
	.dynstr		: { *(.dynstr) }
	.gnu.version	: { *(.gnu.version) }
	.gnu.version_d	: { *(.gnu.version_d) }
	.gnu.version_r	: { *(.gnu.version_r) }

	/DISCARD/	: {
		*(.note.GNU-stack .note.gnu.property)
	}

	.note		: { *(.note.*) }		:text	:note

	. = ALIGN(0x100);

	.text		: { *(.text*) }			:text	=0x90909090
	PROVIDE (__etext = .);
	PROVIDE (_etext = .);
	PROVIDE (etext = .);

	.dynamic	: { *(.dynamic) }		:text	:dynamic

	.rodata		: { *(.rodata*) }		:text
	.data		: {
		*(.data*)
	}

	_end = .;
	PROVIDE(end = .);

	/DISCARD/	: {
		*(.eh_frame .eh_frame_hdr)
	}
}

PHDRS
{
	text		PT_LOAD		FLAGS(5) FILEHDR PHDRS; /* PF_R|PF_X */
	dynamic		PT_DYNAMIC	FLAGS(4);		/* PF_R */
	note		PT_NOTE		FLAGS(4);		/* PF_R */
}

/*
 * This controls what symbols we export from the DSO.
 */
VERSION
{
	LINUX_2.6.39 {
	global:
		__kernel_rt_sigreturn;
		__kernel_gettimeofday;
		__kernel_clock_gettime;
		__kernel_clock_getres;
	local: *;
	};

	LINUX_0.0 {
	global:
		linux_platform;
		kern_timekeep_base;
		__user_rt_sigreturn;
	local: *;
	};
}
