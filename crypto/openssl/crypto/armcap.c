/* $OpenBSD: armcap.c,v 1.5 2014/06/12 15:49:27 deraadt Exp $ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <signal.h>
#include <crypto.h>

#include "arm_arch.h"

unsigned int OPENSSL_armcap_P;

#if __ARM_ARCH__ >= 7
static sigset_t all_masked;

static sigjmp_buf ill_jmp;
	static void ill_handler (int sig) { siglongjmp(ill_jmp, sig);
}

/*
 * Following subroutines could have been inlined, but it's not all
 * ARM compilers support inline assembler...
 */
void _armv7_neon_probe(void);
#endif

#if defined(__GNUC__) && __GNUC__>=2
void OPENSSL_cpuid_setup(void) __attribute__((constructor));
#endif

void
OPENSSL_cpuid_setup(void)
{
#ifndef __OpenBSD__
	char *e;
#endif
#if __ARM_ARCH__ >= 7
	struct sigaction	ill_oact, ill_act;
	sigset_t		oset;
#endif
	static int trigger = 0;

	if (trigger)
		return;
	trigger = 1;

	OPENSSL_armcap_P = 0;

#if __ARM_ARCH__ >= 7
	sigfillset(&all_masked);
	sigdelset(&all_masked, SIGILL);
	sigdelset(&all_masked, SIGTRAP);
	sigdelset(&all_masked, SIGFPE);
	sigdelset(&all_masked, SIGBUS);
	sigdelset(&all_masked, SIGSEGV);

	memset(&ill_act, 0, sizeof(ill_act));
	ill_act.sa_handler = ill_handler;
	ill_act.sa_mask = all_masked;

	sigprocmask(SIG_SETMASK, &ill_act.sa_mask, &oset);
	sigaction(SIGILL, &ill_act, &ill_oact);

	if (sigsetjmp(ill_jmp, 1) == 0) {
		_armv7_neon_probe();
		OPENSSL_armcap_P |= ARMV7_NEON;
	}

	sigaction (SIGILL, &ill_oact, NULL);
	sigprocmask(SIG_SETMASK, &oset, NULL);
#endif
}
