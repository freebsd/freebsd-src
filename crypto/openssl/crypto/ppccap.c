/* $OpenBSD: ppccap.c,v 1.5 2014/06/12 15:49:27 deraadt Exp $ */

#include <sys/types.h>
#include <sys/sysctl.h>
#include <machine/cpu.h>
#include <unistd.h>

#include <crypto.h>
#include <openssl/bn.h>

#ifdef unused
#define PPC_FPU64	(1<<0)
#define PPC_ALTIVEC	(1<<1)

static int OPENSSL_ppccap_P = 0;
#endif

#ifdef OPENSSL_BN_ASM_MONT
extern int bn_mul_mont_int(BN_ULONG *, const BN_ULONG *, const BN_ULONG *,
	    const BN_ULONG *, const BN_ULONG *, int);
int
bn_mul_mont(BN_ULONG *rp, const BN_ULONG *ap, const BN_ULONG *bp,
    const BN_ULONG *np, const BN_ULONG *n0, int num)
{
	return bn_mul_mont_int(rp, ap, bp, np, n0, num);
}
#endif

#ifdef unused
void OPENSSL_cpuid_setup(void) __attribute__((constructor));

void
OPENSSL_cpuid_setup(void)
{
	static const int mib[2] = { CTL_MACHDEP, CPU_ALTIVEC };
	static int trigger = 0;
	int altivec = 0;
	size_t size;

	if (trigger)
		return;
	trigger = 1;

	size = sizeof altivec;
	if (sysctl(mib, 2, &altivec, &size, NULL, 0) != -1) {
		if (altivec != 0)
			OPENSSL_ppccap_P |= PPC_ALTIVEC;
	}
}
#endif
