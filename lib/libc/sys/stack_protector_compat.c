/*
 * Written by Alexander Kabaev <kan@FreeBSD.org>
 * The file is in public domain.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/sys/stack_protector_compat.c,v 1.1.2.2.2.1 2010/12/21 17:09:25 kensmith Exp $");

void __stack_chk_fail(void);

#ifdef PIC
void
__stack_chk_fail_local_hidden(void)
{

	__stack_chk_fail();
}

__sym_compat(__stack_chk_fail_local, __stack_chk_fail_local_hidden, FBSD_1.0);
#endif
