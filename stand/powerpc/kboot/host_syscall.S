/*
 * 
 * $FreeBSD$
 */

#include <machine/asm.h>

ENTRY(host_read)
	li %r0, 3 # SYS_read
	sc
	bso 1f
	blr
1:
	li %r3, 0
	blr
END(host_read)

ENTRY(host_write)
	li %r0, 4 # SYS_write
	sc
	blr
END(host_write)

ENTRY(host_seek)
	mr %r4,%r5
	mr %r5,%r6
	mr %r6,%r7
	li %r0, 140 # SYS_llseek
	sc
	blr
END(host_seek)

ENTRY(host_llseek)
	li %r0, 140 # SYS_llseek
	sc
	blr
END(host_llseek)

ENTRY(host_open)
	li %r0, 5 # SYS_open
	sc
	bso 1f
	blr
1:
	li %r3, 0
	blr
END(host_open)

ENTRY(host_close)
	li %r0, 6 # SYS_close
	sc
	blr
END(host_close)

ENTRY(host_mmap)
	li %r0, 90 # SYS_mmap
	sc
	blr
END(host_mmap)

ENTRY(host_uname)
	li %r0, 122 # SYS_uname
	sc
	blr
END(host_uname)

ENTRY(host_gettimeofday)
	li %r0, 78 # SYS_gettimeofday
	sc
	blr
END(host_gettimeofday)

ENTRY(host_select)
	li %r0, 142 # SYS_select
	sc
	blr
END(host_select)

ENTRY(kexec_load)
	lis	%r6,21	# KEXEC_ARCH_PPC64
	li	%r0,268 # __NR_kexec_load
	sc
	blr
END(kexec_load)

ENTRY(host_reboot)
	li %r0,88 # SYS_reboot
	sc
	blr
END(host_reboot)

ENTRY(host_getdents)
	li %r0,141 # SYS_getdents
	sc
	blr
END(host_getdents)

