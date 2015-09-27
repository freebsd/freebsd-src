/* $OpenBSD: sparcv9cap.c,v 1.6 2014/06/12 15:49:27 deraadt Exp $ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/time.h>
#include <openssl/bn.h>

#define SPARCV9_PREFER_FPU	(1<<1)
#define SPARCV9_VIS1		(1<<2)
#define SPARCV9_VIS2		(1<<3)	/* reserved */
#define SPARCV9_FMADD		(1<<4)	/* reserved for SPARC64 V */

static int OPENSSL_sparcv9cap_P = 0;

int
bn_mul_mont(BN_ULONG *rp, const BN_ULONG *ap, const BN_ULONG *bp,
    const BN_ULONG *np, const BN_ULONG *n0, int num)
{
	int bn_mul_mont_fpu(BN_ULONG *rp, const BN_ULONG *ap, const BN_ULONG *bp, const BN_ULONG *np, const BN_ULONG *n0, int num);
	int bn_mul_mont_int(BN_ULONG *rp, const BN_ULONG *ap, const BN_ULONG *bp, const BN_ULONG *np, const BN_ULONG *n0, int num);

	if (num >= 8 && !(num & 1) &&
	    (OPENSSL_sparcv9cap_P & (SPARCV9_PREFER_FPU|SPARCV9_VIS1)) ==
	    (SPARCV9_PREFER_FPU|SPARCV9_VIS1))
		return bn_mul_mont_fpu(rp, ap, bp, np, n0, num);
	else
		return bn_mul_mont_int(rp, ap, bp, np, n0, num);
}

void		_sparcv9_vis1_probe(void);
unsigned long	_sparcv9_vis1_instrument(void);
void		_sparcv9_vis2_probe(void);
void		_sparcv9_fmadd_probe(void);

static sigjmp_buf common_jmp;
static void
common_handler(int sig)
{
	siglongjmp(common_jmp, sig);
}

void
OPENSSL_cpuid_setup(void)
{
	char *e;
	struct sigaction	common_act, ill_oact, bus_oact;
	sigset_t		all_masked, oset;
	static int trigger = 0;

	if (trigger)
		return;
	trigger = 1;

	/* Initial value, fits UltraSPARC-I&II... */
	OPENSSL_sparcv9cap_P = SPARCV9_PREFER_FPU;

	sigfillset(&all_masked);
	sigdelset(&all_masked, SIGILL);
	sigdelset(&all_masked, SIGTRAP);
#ifdef SIGEMT
	sigdelset(&all_masked, SIGEMT);
#endif
	sigdelset(&all_masked, SIGFPE);
	sigdelset(&all_masked, SIGBUS);
	sigdelset(&all_masked, SIGSEGV);
	sigprocmask(SIG_SETMASK, &all_masked, &oset);

	memset(&common_act, 0, sizeof(common_act));
	common_act.sa_handler = common_handler;
	common_act.sa_mask = all_masked;

	sigaction(SIGILL, &common_act, &ill_oact);
	sigaction(SIGBUS,&common_act,&bus_oact);/* T1 fails 16-bit ldda [on Linux] */

	if (sigsetjmp(common_jmp, 1) == 0) {
		_sparcv9_vis1_probe();
		OPENSSL_sparcv9cap_P |= SPARCV9_VIS1;
		/* detect UltraSPARC-Tx, see sparccpud.S for details... */
		if (_sparcv9_vis1_instrument() >= 12)
			OPENSSL_sparcv9cap_P &= ~(SPARCV9_VIS1|SPARCV9_PREFER_FPU);
		else {
			_sparcv9_vis2_probe();
			OPENSSL_sparcv9cap_P |= SPARCV9_VIS2;
		}
	}

	if (sigsetjmp(common_jmp, 1) == 0) {
		_sparcv9_fmadd_probe();
		OPENSSL_sparcv9cap_P |= SPARCV9_FMADD;
	}

	sigaction(SIGBUS, &bus_oact, NULL);
	sigaction(SIGILL, &ill_oact, NULL);

	sigprocmask(SIG_SETMASK, &oset, NULL);
}
