#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/random.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/libkern.h>

long __stack_chk_guard[8] = {};
void __stack_chk_fail(void);

/*
 * XXX This default is unsafe!!!  We intend to change it after resolving issues
 * with early entropy in the installer; some kinds of systems that do not use
 * loader(8), such as riscv, aarch64, and power; and perhaps others that I am
 * forgetting off the top of my head.
 */
static bool permit_nonrandom_cookies = true;

SYSCTL_NODE(_security, OID_AUTO, stack_protect, CTLFLAG_RW, 0,
    "-fstack-protect support");
SYSCTL_BOOL(_security_stack_protect, OID_AUTO, permit_nonrandom_cookies,
    CTLFLAG_RDTUN, &permit_nonrandom_cookies, 0,
    "Allow stack guard to be used without real random cookies");

void
__stack_chk_fail(void)
{

	panic("stack overflow detected; backtrace may be corrupted");
}

static void
__stack_chk_init(void *dummy __unused)
{
	size_t i;
	long guard[nitems(__stack_chk_guard)];

	if (is_random_seeded()) {
		arc4rand(guard, sizeof(guard), 0);
		for (i = 0; i < nitems(guard); i++)
			__stack_chk_guard[i] = guard[i];
		return;
	}

	if (permit_nonrandom_cookies) {
		printf("%s: WARNING: Initializing stack protection with "
		    "non-random cookies!\n", __func__);
		printf("%s: WARNING: This severely limits the benefit of "
		    "-fstack-protector!\n", __func__);

		/*
		 * The emperor is naked, but I rolled some dice and at least
		 * these values aren't zero.
		 */
		__stack_chk_guard[0] = (long)0xe7318d5959af899full;
		__stack_chk_guard[1] = (long)0x35a9481c089348bfull;
		__stack_chk_guard[2] = (long)0xde657fdc04117255ull;
		__stack_chk_guard[3] = (long)0x0dd44c61c22e4a6bull;
		__stack_chk_guard[4] = (long)0x0a5869a354edb0a5ull;
		__stack_chk_guard[5] = (long)0x05cebfed255b5232ull;
		__stack_chk_guard[6] = (long)0x270ffac137c4c72full;
		__stack_chk_guard[7] = (long)0xd8141a789bad478dull;
		_Static_assert(nitems(__stack_chk_guard) == 8,
		    "__stack_chk_guard doesn't have 8 items");
		return;
	}

	panic("%s: cannot initialize stack cookies because random device is "
	    "not yet seeded", __func__);
}
SYSINIT(stack_chk, SI_SUB_RANDOM, SI_ORDER_ANY, __stack_chk_init, NULL);
