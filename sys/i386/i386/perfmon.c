/*
 * Copyright 1996 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/ioccom.h>

#include <machine/cpu.h>
#include <machine/cputypes.h>
#include <machine/clock.h>
#include <machine/perfmon.h>

static int perfmon_inuse;
static int perfmon_cpuok;
static int msr_ctl[NPMC];
static int msr_pmc[NPMC];

/*
 * Must be called after cpu_class is set up.
 */
void
perfmon_init(void)
{
	switch(cpu_class) {
	case CPUCLASS_586:	/* assume it's the same for now */
	case CPUCLASS_686:
		perfmon_cpuok = 1;
		msr_ctl[0] = 0x186;
		msr_ctl[1] = 0x187;
		msr_pmc[0] = 0xc1;
		msr_pmc[1] = 0xc2;
		break;

	default:
		perfmon_cpuok = 0;
		break;
	}
}

int
perfmon_avail(void)
{
	return perfmon_cpuok;
}

int
perfmon_setup(int pmc, unsigned int control)
{
	if (pmc < 0 || pmc >= NPMC)
		return EINVAL;

	perfmon_inuse |= (1 << pmc);
	control &= ~(PMCF_SYS_FLAGS << 16);
	wrmsr(msr_ctl[pmc], control);
	wrmsr(msr_pmc[pmc], 0);
	return 0;
}

int
perfmon_get(int pmc, unsigned int *control)
{
	if (pmc < 0 || pmc >= NPMC)
		return EINVAL;

	if (perfmon_inuse & (1 << pmc)) {
		*control = rdmsr(msr_ctl[pmc]);
		return 0;
	}
	return EBUSY;		/* XXX reversed sense */
}

int
perfmon_fini(int pmc)
{
	if (pmc < 0 || pmc >= NPMC)
		return EINVAL;

	if (perfmon_inuse & (1 << pmc)) {
		perfmon_stop(pmc);
		perfmon_inuse &= ~(1 << pmc);
		return 0;
	}
	return EBUSY;		/* XXX reversed sense */
}

int
perfmon_start(int pmc)
{
	/*
	 * XXX - Current Intel design does not allow counters to be enabled
	 * independently.
	 */
	if (pmc != PMC_ALL)
		return EINVAL;

#if 0
	if (perfmon_inuse & (1 << pmc)) {
		wrmsr(msr_ctl[pmc], rdmsr(msr_ctl[pmc]) | (PMCF_EN << 16));
		return 0;
	}
#else
	if (perfmon_inuse) {
		wrmsr(msr_ctl[0], rdmsr(msr_ctl[0]) | (PMCF_EN << 16));
		return 0;
	}
#endif
	return EBUSY;
}

int
perfmon_stop(int pmc)
{
	/*
	 * XXX - Current Intel design does not allow counters to be enabled
	 * independently.
	 */
	if (pmc != PMC_ALL)
		return EINVAL;

#if 0
	if (perfmon_inuse & (1 << pmc)) {
		wrmsr(msr_ctl[pmc], rdmsr(msr_ctl[pmc]) & ~(PMCF_EN << 16));
		return 0;
	}
#else
	if (perfmon_inuse) {
		wrmsr(msr_ctl[0], rdmsr(msr_ctl[0]) & ~(PMCF_EN << 16));
		return 0;
	}
#endif
	return EBUSY;
}

int
perfmon_read(int pmc, quad_t *val)
{
	if (pmc < 0 || pmc >= NPMC)
		return EINVAL;

	if (perfmon_inuse & (1 << pmc)) {
		*val = rdmsr(msr_pmc[pmc]);
		return 0;
	}

	return EBUSY;
}

int
perfmon_reset(int pmc)
{
	if (pmc < 0 || pmc >= NPMC)
		return EINVAL;

	if (perfmon_inuse & (1 << pmc)) {
		wrmsr(msr_pmc[pmc], 0);
		return 0;
	}
	return EBUSY;
}

/*
 * Now the user-mode interface, called from a subdevice of mem.c.
 */
static int writer;
static int writerpmc;

int
perfmon_open(dev_t dev, int flags, int fmt, struct proc *p)
{
	if (!perfmon_cpuok)
		return ENXIO;

	if (flags & FWRITE) {
		if (writer) {
			return EBUSY;
		} else {
			writer = 1;
			writerpmc = 0;
		}
	}
	return 0;
}

int
perfmon_close(dev_t dev, int flags, int fmt, struct proc *p)
{
	if (flags & FWRITE) {
		int i;

		for (i = 0; i < NPMC; i++) {
			if (writerpmc & (1 << i))
				perfmon_fini(i);
		}
		writer = 0;
	}
	return 0;
}

int
perfmon_ioctl(dev_t dev, int cmd, caddr_t param, int flags, struct proc *p)
{
	struct pmc *pmc;
	struct pmc_data *pmcd;
	struct pmc_tstamp *pmct;
	int *ip;
	int rv;

	switch(cmd) {
	case PMIOSETUP:
		if (!(flags & FWRITE))
			return EPERM;
		pmc = (struct pmc *)param;

		rv = perfmon_setup(pmc->pmc_num, pmc->pmc_val);
		if (!rv) {
			writerpmc |= (1 << pmc->pmc_num);
		}
		break;

	case PMIOGET:
		pmc = (struct pmc *)param;
		rv = perfmon_get(pmc->pmc_num, &pmc->pmc_val);
		break;

	case PMIOSTART:
		if (!(flags & FWRITE))
			return EPERM;

		ip = (int *)param;
		rv = perfmon_start(*ip);
		break;

	case PMIOSTOP:
		if (!(flags & FWRITE))
			return EPERM;

		ip = (int *)param;
		rv = perfmon_stop(*ip);
		break;

	case PMIORESET:
		if (!(flags & FWRITE))
			return EPERM;

		ip = (int *)param;
		rv = perfmon_reset(*ip);
		break;

	case PMIOREAD:
		pmcd = (struct pmc_data *)param;
		rv = perfmon_read(pmcd->pmcd_num, &pmcd->pmcd_value);
		break;

	case PMIOTSTAMP:
		pmct = (struct pmc_tstamp *)param;
		pmct->pmct_rate = i586_ctr_rate >> I586_CTR_RATE_SHIFT;
		pmct->pmct_value = rdtsc();
		rv = 0;
		break;

	default:
		rv = ENOTTY;
	}

	return rv;
}
