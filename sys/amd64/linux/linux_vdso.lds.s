/*
 * Linker script for 64-bit vDSO.
 * Copied from Linux kernel arch/x86/vdso/vdso-layout.lds.S
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

VERSION
{
	LINUX_2.6 {
	global:
		__vdso_time;
		__vdso_gettimeofday;
		__vdso_getcpu;
		__vdso_clock_gettime;
		__vdso_clock_getres;
	local: *;
	};

	LINUX_0.0 {
	global:
		linux_rt_sigcode;
		linux_platform;
		kern_timekeep_base;
		kern_tsc_selector;
		kern_cpu_selector;
	local: *;
	};
}
