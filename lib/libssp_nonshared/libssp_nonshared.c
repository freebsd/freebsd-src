/*
 * Written by Alexander Kabaev <kan@FreeBSD.org>
 * The file is in public domain.
 */

#include <sys/cdefs.h>
void __stack_chk_fail(void);
void __stack_chk_fail_local(void);

void __hidden
__stack_chk_fail_local(void)
{

	__stack_chk_fail();
}
