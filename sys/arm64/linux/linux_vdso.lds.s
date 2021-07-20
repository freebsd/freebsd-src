/*
 * Linker script for 64-bit vDSO.
 * Copied from Linux kernel arch/x86/vdso/vdso-layout.lds.S
 *
 * $FreeBSD$
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

	.note		: { *(.note.*) }		:text	:note

	.eh_frame_hdr	: { *(.eh_frame_hdr) }		:text	:eh_frame_hdr
	.eh_frame	: { KEEP (*(.eh_frame)) }	:text

	.dynamic	: { *(.dynamic) }		:text	:dynamic

	.rodata		: { *(.rodata*) }		:text
	.data		: {
	      *(.data*)
	      *(.sdata*)
	      *(.got.plt) *(.got)
	      *(.gnu.linkonce.d.*)
	      *(.bss*)
	      *(.dynbss*)
	      *(.gnu.linkonce.b.*)
	}

	.altinstructions	: { *(.altinstructions) }
	.altinstr_replacement	: { *(.altinstr_replacement) }

	. = ALIGN(0x100);
	.text		: { *(.test .text*) }			:text	=0x90909090
}

PHDRS
{
	text		PT_LOAD		FLAGS(5) FILEHDR PHDRS; /* PF_R|PF_X */
	dynamic		PT_DYNAMIC	FLAGS(4);		/* PF_R */
	note		PT_NOTE		FLAGS(4);		/* PF_R */
	eh_frame_hdr	PT_GNU_EH_FRAME;
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
	local: *;
	};
}
