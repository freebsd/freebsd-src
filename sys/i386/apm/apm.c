/*
 * APM (Advanced Power Management) BIOS Device Driver
 *
 * Copyright (c) 1994 UKAI, Fumitoshi.
 * Copyright (c) 1994-1995 by HOSOKAWA, Tatsumi <hosokawa@jp.FreeBSD.org>
 * Copyright (c) 1996 Nate Williams <nate@FreeBSD.org>
 * Copyright (c) 1997 Poul-Henning Kamp <phk@FreeBSD.org>
 *
 * This software may be used, modified, copied, and distributed, in
 * both source and binary form provided that the above copyright and
 * these terms are retained. Under no circumstances is the author
 * responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with its
 * use.
 *
 * Sep, 1994	Implemented on FreeBSD 1.1.5.1R (Toshiba AVS001WD)
 *
 * $FreeBSD$
 */

#include "opt_devfs.h"
#include "opt_vm86.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/
#include <sys/time.h>
#include <sys/reboot.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <sys/fcntl.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/signalvar.h>
#include <sys/sysctl.h>
#include <i386/isa/isa_device.h>
#include <machine/apm_bios.h>
#include <machine/segments.h>
#include <machine/clock.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <sys/syslog.h>
#include <i386/apm/apm_setup.h>

#ifdef VM86
#include <machine/psl.h>
#include <machine/vm86.h>
#endif

static int apm_display __P((int newstate));
static int apm_int __P((u_long *eax, u_long *ebx, u_long *ecx, u_long *edx));
static void apm_resume __P((void));
static int apm_check_function_supported __P((u_int version, u_int func));

#define APM_NEVENTS 16
#define APM_NPMEV   13

int	apm_evindex;

/* static data */
struct apm_softc {
	int	initialized, active, bios_busy;
	int	always_halt_cpu, slow_idle_cpu;
	int	disabled, disengaged;
 	int	standby_countdown, suspend_countdown;
	u_int	minorversion, majorversion;
	u_int	cs32_base, cs16_base, ds_base;
	u_int	cs16_limit, cs32_limit, ds_limit;
	u_int	cs_entry;
	u_int	intversion;
 	u_int	standbys, suspends;
	struct apmhook sc_suspend;
	struct apmhook sc_resume;
	struct selinfo sc_rsel;
	int	sc_flags;
	int	event_count;
	int	event_ptr;
	struct	apm_event_info event_list[APM_NEVENTS];
	u_char	event_filter[APM_NPMEV];
#ifdef DEVFS
	void 	*sc_devfs_token;
#endif
};
#define	SCFLAG_ONORMAL	0x0000001
#define	SCFLAG_OCTL	0x0000002
#define	SCFLAG_OPEN	(SCFLAG_ONORMAL|SCFLAG_OCTL)

#define APMDEV(dev)	(minor(dev)&0x0f)
#define APMDEV_NORMAL	0
#define APMDEV_CTL	8

static struct apm_softc apm_softc;
static struct apmhook	*hook[NAPM_HOOK];		/* XXX */

#define is_enabled(foo) ((foo) ? "enabled" : "disabled")

/* Map version number to integer (keeps ordering of version numbers) */
#define INTVERSION(major, minor)	((major)*100 + (minor))

static struct callout_handle apm_timeout_ch = 
    CALLOUT_HANDLE_INITIALIZER(&apm_timeout_ch);

static timeout_t apm_timeout;
static d_open_t apmopen;
static d_close_t apmclose;
static d_write_t apmwrite;
static d_ioctl_t apmioctl;
static d_poll_t apmpoll;

#define CDEV_MAJOR 39
static struct cdevsw apm_cdevsw = 
	{ apmopen,	apmclose,	noread,		apmwrite,	/*39*/
	  apmioctl,	nostop,		nullreset,	nodevtotty,/* APM */
	  apmpoll,	nommap,		NULL ,	"apm"	,NULL,	-1};

/* setup APM GDT discriptors */
static void
setup_apm_gdt(u_int code32_base, u_int code16_base, u_int data_base, u_int code32_limit, u_int code16_limit, u_int data_limit)
{
	/* setup 32bit code segment */
	gdt_segs[GAPMCODE32_SEL].ssd_base  = code32_base;
	gdt_segs[GAPMCODE32_SEL].ssd_limit = code32_limit;

	/* setup 16bit code segment */
	gdt_segs[GAPMCODE16_SEL].ssd_base  = code16_base;
	gdt_segs[GAPMCODE16_SEL].ssd_limit = code16_limit;

	/* setup data segment */
	gdt_segs[GAPMDATA_SEL  ].ssd_base  = data_base;
	gdt_segs[GAPMDATA_SEL  ].ssd_limit = data_limit;

	/* reflect these changes on physical GDT */
	ssdtosd(gdt_segs + GAPMCODE32_SEL, &gdt[GAPMCODE32_SEL].sd);
	ssdtosd(gdt_segs + GAPMCODE16_SEL, &gdt[GAPMCODE16_SEL].sd);
	ssdtosd(gdt_segs + GAPMDATA_SEL  , &gdt[GAPMDATA_SEL  ].sd);
}

/* 48bit far pointer. Do not staticize - used from apm_setup.s */
struct addr48 {
	u_long		offset;
	u_short		segment;
} apm_addr;

static int apm_errno;

static int apm_suspend_delay = 1;
static int apm_standby_delay = 1;

SYSCTL_INT(_machdep, OID_AUTO, apm_suspend_delay, CTLFLAG_RW, &apm_suspend_delay, 1, "");
SYSCTL_INT(_machdep, OID_AUTO, apm_standby_delay, CTLFLAG_RW, &apm_standby_delay, 1, "");

/*
 * return  0 if the function successfull,
 * return  1 if the function unsuccessfull,
 * return -1 if the function unsupported.
 */
static int
apm_do_int(struct apm_bios_arg *apap)
{
	struct apm_softc *sc = &apm_softc;
	int errno = 0;
	u_long apm_func = apap->eax & 0x00ff;

	if (!apm_check_function_supported(sc->intversion, apm_func)) {
#ifdef APM_DEBUG
		printf("apm_bioscall: function 0x%x is not supported in v%d.%d\n
",
			apm_func, sc->majorversion, sc->minorversion);

#endif
		return (-1);
	}

	sc->bios_busy = 1;
	errno = apm_bios_call(apap);
	sc->bios_busy = 0;
	return errno;
}

static int
apm_int(u_long *eax, u_long *ebx, u_long *ecx, u_long *edx)
{
	struct apm_bios_arg apa;
	int cf;

	apa.eax = *eax;
	apa.ebx = *ebx;
	apa.ecx = *ecx;
	apa.edx = *edx;
	apa.esi = 0;	/* clear register */
	apa.edi = 0;	/* clear register */
	cf = apm_do_int(&apa);
	*eax = apa.eax;
	*ebx = apa.ebx;
	*ecx = apa.ecx;
	*edx = apa.edx;
	apm_errno = ((*eax) >> 8) & 0xff;
	return cf;
}


/* check whether APM function is supported (1)  or not (0). */
static int
apm_check_function_supported(u_int version, u_int func)
{
	/* except driver version */
	if (func == APM_DRVVERSION) {
		return (1);
	}

	switch (version) {
	case INTVERSION(1, 0):
		if (func > APM_GETPMEVENT) {
			return (0); /* not supported */
		}
		break;
	case INTVERSION(1, 1):
		if (func > APM_ENGAGEDISENGAGEPM &&
		    func < APM_OEMFUNC) {
			return (0); /* not supported */
		}
		break;
	case INTVERSION(1, 2):
		break;
	}

	return (1); /* supported */
}

/* enable/disable power management */
static int
apm_enable_disable_pm(int enable)
{
	struct apm_softc *sc = &apm_softc;

	u_long eax, ebx, ecx, edx;

	eax = (APM_BIOS << 8) | APM_ENABLEDISABLEPM;

	if (sc->intversion >= INTVERSION(1, 1))
		ebx  = PMDV_ALLDEV;
	else
		ebx  = 0xffff;	/* APM version 1.0 only */
	ecx  = enable;
	edx = 0;
	return apm_int(&eax, &ebx, &ecx, &edx);
}

/* register driver version (APM 1.1 or later) */ 
static void
apm_driver_version(int version)
{
	u_long eax, ebx, ecx, edx;

	/* First try APM 1.2 */
	eax = (APM_BIOS << 8) | APM_DRVVERSION;
	ebx  = 0x0;
	ecx  = version;
	edx = 0;
	if (!apm_int(&eax, &ebx, &ecx, &edx)) {
		/*
		 * Some old BIOSes don't return
		 * the connection version in %ax.
		 */
		if (eax == ((APM_BIOS << 8) | APM_DRVVERSION)) {
			apm_version = version;
		} else {
			apm_version = eax & 0xffff;
		}
	}
}

/* engage/disengage power management (APM 1.1 or later) */
static int
apm_engage_disengage_pm(int engage)
{
	u_long eax, ebx, ecx, edx;

	eax = (APM_BIOS << 8) | APM_ENGAGEDISENGAGEPM;
	ebx = PMDV_ALLDEV;
	ecx = engage;
	edx = 0;
	return(apm_int(&eax, &ebx, &ecx, &edx));
}

/* get PM event */
static u_int
apm_getevent(void)
{
	u_long eax, ebx, ecx, edx;

	eax = (APM_BIOS << 8) | APM_GETPMEVENT;

	ebx = 0;
	ecx = 0;
	edx = 0;
	if (apm_int(&eax, &ebx, &ecx, &edx))
		return PMEV_NOEVENT;

	return ebx & 0xffff;
}

/* suspend entire system */
static int
apm_suspend_system(int state)
{
	u_long eax, ebx, ecx, edx;

	eax = (APM_BIOS << 8) | APM_SETPWSTATE;
	ebx = PMDV_ALLDEV;
	ecx = state;
	edx = 0;

	if (apm_int(&eax, &ebx, &ecx, &edx)) {
		printf("Entire system suspend failure: errcode = %ld\n",
			0xff & (eax >> 8));
		return 1;
	}
	return 0;
}

/* Display control */
/*
 * Experimental implementation: My laptop machine can't handle this function
 * If your laptop can control the display via APM, please inform me.
 *                            HOSOKAWA, Tatsumi <hosokawa@jp.FreeBSD.org>
 */
static int
apm_display(int newstate)
{
	u_long eax, ebx, ecx, edx;

	eax = (APM_BIOS << 8) | APM_SETPWSTATE;
	ebx = PMDV_DISP0;
	ecx = newstate ? PMST_APMENABLED:PMST_SUSPEND;
	edx = 0;
	if (apm_int(&eax, &ebx, &ecx, &edx)) {
		printf("Display off failure: errcode = %ld\n",
			0xff & (eax >> 8));
		return 1;
	}
	return 0;
}

/*
 * Turn off the entire system.
 */
static void
apm_power_off(int howto, void *junk)
{
	u_long eax, ebx, ecx, edx;

	/* Not halting powering off, or not active */
	if (!(howto & RB_POWEROFF) || !apm_softc.active)
		return;
	eax = (APM_BIOS << 8) | APM_SETPWSTATE;
	ebx = PMDV_ALLDEV;
	ecx = PMST_OFF;
	edx = 0;
	apm_int(&eax, &ebx, &ecx, &edx);
}

/* APM Battery low handler */
static void
apm_battery_low(void)
{
	printf("\007\007 * * * BATTERY IS LOW * * * \007\007");
}

/* APM hook manager */
static struct apmhook *
apm_add_hook(struct apmhook **list, struct apmhook *ah)
{
	int s;
	struct apmhook *p, *prev;

#ifdef APM_DEBUG
	printf("Add hook \"%s\"\n", ah->ah_name);
#endif

	s = splhigh();
	if (ah == NULL)
		panic("illegal apm_hook!");
	prev = NULL;
	for (p = *list; p != NULL; prev = p, p = p->ah_next)
		if (p->ah_order > ah->ah_order)
			break;

	if (prev == NULL) {
		ah->ah_next = *list;
		*list = ah;
	} else {
		ah->ah_next = prev->ah_next;
		prev->ah_next = ah;
	}
	splx(s);
	return ah;
}

static void
apm_del_hook(struct apmhook **list, struct apmhook *ah)
{
	int s;
	struct apmhook *p, *prev;

	s = splhigh();
	prev = NULL;
	for (p = *list; p != NULL; prev = p, p = p->ah_next)
		if (p == ah)
			goto deleteit;
	panic("Tried to delete unregistered apm_hook.");
	goto nosuchnode;
deleteit:
	if (prev != NULL)
		prev->ah_next = p->ah_next;
	else
		*list = p->ah_next;
nosuchnode:
	splx(s);
}


/* APM driver calls some functions automatically */
static void
apm_execute_hook(struct apmhook *list)
{
	struct apmhook *p;

	for (p = list; p != NULL; p = p->ah_next) {
#ifdef APM_DEBUG
		printf("Execute APM hook \"%s.\"\n", p->ah_name);
#endif
		if ((*(p->ah_fun))(p->ah_arg))
			printf("Warning: APM hook \"%s\" failed", p->ah_name);
	}
}


/* establish an apm hook */
struct apmhook *
apm_hook_establish(int apmh, struct apmhook *ah)
{
	if (apmh < 0 || apmh >= NAPM_HOOK)
		return NULL;

	return apm_add_hook(&hook[apmh], ah);
}

/* disestablish an apm hook */
void
apm_hook_disestablish(int apmh, struct apmhook *ah)
{
	if (apmh < 0 || apmh >= NAPM_HOOK)
		return;

	apm_del_hook(&hook[apmh], ah);
}


static struct timeval suspend_time;
static struct timeval diff_time;

static int
apm_default_resume(void *arg)
{
	int pl;
	u_int second, minute, hour;
	struct timeval resume_time, tmp_time;

	/* modified for adjkerntz */
	pl = splsoftclock();
	inittodr(0);			/* adjust time to RTC */
	microtime(&resume_time);
	getmicrotime(&tmp_time);
	timevaladd(&tmp_time, &diff_time);

#ifdef FIXME
	/* XXX THIS DOESN'T WORK!!! */
	time = tmp_time;
#endif

#ifdef APM_FIXUP_CALLTODO
	/* Calculate the delta time suspended */
	timevalsub(&resume_time, &suspend_time);
	/* Fixup the calltodo list with the delta time. */
	adjust_timeout_calltodo(&resume_time);
#endif /* APM_FIXUP_CALLTODOK */
	splx(pl);
#ifndef APM_FIXUP_CALLTODO
	second = resume_time.tv_sec - suspend_time.tv_sec; 
#else /* APM_FIXUP_CALLTODO */
	/* 
	 * We've already calculated resume_time to be the delta between 
	 * the suspend and the resume. 
	 */
	second = resume_time.tv_sec; 
#endif /* APM_FIXUP_CALLTODO */
	hour = second / 3600;
	second %= 3600;
	minute = second / 60;
	second %= 60;
	log(LOG_NOTICE, "resumed from suspended mode (slept %02d:%02d:%02d)\n",
		hour, minute, second);
	return 0;
}

static int
apm_default_suspend(void *arg)
{
	int	pl;

	pl = splsoftclock();
	microtime(&diff_time);
	inittodr(0);
	microtime(&suspend_time);
	timevalsub(&diff_time, &suspend_time);
	splx(pl);
	return 0;
}

static int apm_record_event __P((struct apm_softc *, u_int));
static void apm_processevent(void);

static u_int apm_op_inprog = 0;

static void
apm_do_suspend(void)
{
	struct apm_softc *sc = &apm_softc;

	if (!sc)
		return;

	apm_op_inprog = 0;
	sc->suspends = sc->suspend_countdown = 0;

	if (sc->initialized) {
		apm_execute_hook(hook[APM_HOOK_SUSPEND]);
		if (apm_suspend_system(PMST_SUSPEND) == 0)
			apm_processevent();
		else
			/* Failure, 'resume' the system again */
			apm_execute_hook(hook[APM_HOOK_RESUME]);
	}
}

static void
apm_do_standby(void)
{
	struct apm_softc *sc = &apm_softc;

	if (!sc)
		return;

	apm_op_inprog = 0;
	sc->standbys = sc->standby_countdown = 0;

	if (sc->initialized) {
		/*
		 * As far as standby, we don't need to execute 
		 * all of suspend hooks.
		 */
		apm_default_suspend(&apm_softc);
		if (apm_suspend_system(PMST_STANDBY) == 0)
			apm_processevent();
	}
}

static void
apm_lastreq_notify(void)
{
	u_long eax, ebx, ecx, edx;

	eax = (APM_BIOS << 8) | APM_SETPWSTATE;
	ebx = PMDV_ALLDEV;
	ecx = PMST_LASTREQNOTIFY;
	edx = 0;

	apm_int(&eax, &ebx, &ecx, &edx);
}

static int
apm_lastreq_rejected(void)
{
	u_long eax, ebx, ecx, edx;

	if (apm_op_inprog == 0) {
		return 1;	/* no operation in progress */
	}

	eax = (APM_BIOS << 8) | APM_SETPWSTATE;
	ebx = PMDV_ALLDEV;
	ecx = PMST_LASTREQREJECT;
	edx = 0;

	if (apm_int(&eax, &ebx, &ecx, &edx)) {
#ifdef APM_DEBUG
		printf("apm_lastreq_rejected: failed\n");
#endif
		return 1;
	}

	apm_op_inprog = 0;

	return 0;
}

/*
 * Public interface to the suspend/resume:
 *
 * Execute suspend and resume hook before and after sleep, respectively.
 *
 */

void
apm_suspend(int state)
{
	struct apm_softc *sc = &apm_softc;

	switch (state) {
	case PMST_SUSPEND:
		if (sc->suspends)
			return;
		sc->suspends++;
		sc->suspend_countdown = apm_suspend_delay;
		break;
	case PMST_STANDBY:
		if (sc->standbys)
			return;
		sc->standbys++;
		sc->standby_countdown = apm_standby_delay;
		break;
	default:
		printf("apm_suspend: Unknown Suspend state 0x%x\n", state);
		return;
	}

	apm_op_inprog++;
	apm_lastreq_notify();
}

void
apm_resume(void)
{
	struct apm_softc *sc = &apm_softc;

	if (!sc)
		return;

	if (sc->initialized)
		apm_execute_hook(hook[APM_HOOK_RESUME]);
}


/* get APM information */
static int
apm_get_info(apm_info_t aip)
{
	struct apm_softc *sc = &apm_softc;
	u_long eax, ebx, ecx, edx;

	eax = (APM_BIOS << 8) | APM_GETPWSTATUS;
	ebx = PMDV_ALLDEV;
	ecx = 0;
	edx = 0xffff;			/* default to unknown battery time */

	if (apm_int(&eax, &ebx, &ecx, &edx))
		return 1;

	aip->ai_infoversion = 1;
	aip->ai_acline      = (ebx >> 8) & 0xff;
	aip->ai_batt_stat   = ebx & 0xff;
	aip->ai_batt_life   = ecx & 0xff;
	aip->ai_major       = (u_int)sc->majorversion;
	aip->ai_minor       = (u_int)sc->minorversion;
	aip->ai_status      = (u_int)sc->active;
	edx &= 0xffff;
	if (edx == 0xffff)	/* Time is unknown */
		aip->ai_batt_time = -1;
	else if (edx & 0x8000)	/* Time is in minutes */
		aip->ai_batt_time = (edx & 0x7fff) * 60;
	else			/* Time is in seconds */
		aip->ai_batt_time = edx;

	eax = (APM_BIOS << 8) | APM_GETCAPABILITIES;
	ebx = 0;
	ecx = 0;
	edx = 0;
	if (apm_int(&eax, &ebx, &ecx, &edx)) {
		aip->ai_batteries = -1;	/* Unknown */
		aip->ai_capabilities = 0xff00; /* Unknown, with no bits set */
	} else {
		aip->ai_batteries = ebx & 0xff;
		aip->ai_capabilities = ecx & 0xf;
	}

	bzero(aip->ai_spare, sizeof aip->ai_spare);

	return 0;
}


/* inform APM BIOS that CPU is idle */
void
apm_cpu_idle(void)
{
	struct apm_softc *sc = &apm_softc;

	if (sc->active) {
		u_long eax, ebx, ecx, edx;

		eax = (APM_BIOS <<8) | APM_CPUIDLE;
		edx = ecx = ebx = 0;
		apm_int(&eax, &ebx, &ecx, &edx);
	}
	/*
	 * Some APM implementation halts CPU in BIOS, whenever
	 * "CPU-idle" function are invoked, but swtch() of
	 * FreeBSD halts CPU, therefore, CPU is halted twice
	 * in the sched loop. It makes the interrupt latency
	 * terribly long and be able to cause a serious problem
	 * in interrupt processing. We prevent it by removing
	 * "hlt" operation from swtch() and managed it under
	 * APM driver.
	 */
	if (!sc->active || sc->always_halt_cpu)
		__asm("hlt");	/* wait for interrupt */
}

/* inform APM BIOS that CPU is busy */
void
apm_cpu_busy(void)
{
	struct apm_softc *sc = &apm_softc;

	/*
	 * The APM specification says this is only necessary if your BIOS
	 * slows down the processor in the idle task, otherwise it's not
	 * necessary.
	 */
	if (sc->slow_idle_cpu && sc->active) {
		u_long eax, ebx, ecx, edx;

		eax = (APM_BIOS <<8) | APM_CPUBUSY;
		edx = ecx = ebx = 0;
		apm_int(&eax, &ebx, &ecx, &edx);
	}
}


/*
 * APM timeout routine:
 *
 * This routine is automatically called by timer once per second.
 */

static void
apm_timeout(void *dummy)
{
	struct apm_softc *sc = &apm_softc;

	if (apm_op_inprog)
		apm_lastreq_notify();

	if (sc->standbys && sc->standby_countdown-- <= 0)
		apm_do_standby();

	if (sc->suspends && sc->suspend_countdown-- <= 0)
		apm_do_suspend();

	if (!sc->bios_busy)
		apm_processevent();
  
	if (sc->active == 1)
		/* Run slightly more oftan than 1 Hz */
		apm_timeout_ch = timeout(apm_timeout, NULL, hz - 1 );
}

/* enable APM BIOS */
static void
apm_event_enable(void)
{
	struct apm_softc *sc = &apm_softc;

#ifdef APM_DEBUG
	printf("called apm_event_enable()\n");
#endif
	if (sc->initialized) {
		sc->active = 1;
		apm_timeout(sc);
	}
}

/* disable APM BIOS */
static void
apm_event_disable(void)
{
	struct apm_softc *sc = &apm_softc;

#ifdef APM_DEBUG
	printf("called apm_event_disable()\n");
#endif
	if (sc->initialized) {
		untimeout(apm_timeout, NULL, apm_timeout_ch);
		sc->active = 0;
	}
}

/* halt CPU in scheduling loop */
static void
apm_halt_cpu(void)
{
	struct apm_softc *sc = &apm_softc;

	if (sc->initialized)
		sc->always_halt_cpu = 1;
}

/* don't halt CPU in scheduling loop */
static void
apm_not_halt_cpu(void)
{
	struct apm_softc *sc = &apm_softc;

	if (sc->initialized)
		sc->always_halt_cpu = 0;
}

/* device driver definitions */
static int apmprobe (struct isa_device *);
static int apmattach(struct isa_device *);
struct isa_driver apmdriver = { apmprobe, apmattach, "apm" };

/*
 * probe APM (dummy):
 *
 * APM probing routine is placed on locore.s and apm_init.S because
 * this process forces the CPU to turn to real mode or V86 mode.
 * Current version uses real mode, but in a future version, we want
 * to use V86 mode in APM initialization.
 * 
 * XXX If VM86 is defined, we do.
 */

static int
apmprobe(struct isa_device *dvp)
{
#ifdef VM86
	struct vm86frame	vmf;
	int			i;
#endif

	if ( dvp->id_unit > 0 ) {
		printf("apm: Only one APM driver supported.\n");
		return 0;
	}

#ifdef VM86
	bzero(&vmf, sizeof(struct vm86frame));		/* safety */
	vmf.vmf_ax = (APM_BIOS << 8) | APM_INSTCHECK;
	vmf.vmf_bx = 0;
	if (((i = vm86_intcall(SYSTEM_BIOS, &vmf)) == 0) &&
	    !(vmf.vmf_eflags & PSL_C) && 
	    (vmf.vmf_bx == 0x504d)) {

		apm_version   = vmf.vmf_ax;
		apm_flags     = vmf.vmf_cx;

		vmf.vmf_ax = (APM_BIOS << 8) | APM_PROT32CONNECT;
		vmf.vmf_bx = 0;
		if (((i = vm86_intcall(SYSTEM_BIOS, &vmf)) == 0) &&
		    !(vmf.vmf_eflags & PSL_C)) {

			apm_cs32_base = vmf.vmf_ax;
			apm_cs_entry  = vmf.vmf_ebx;
			apm_cs16_base = vmf.vmf_cx;
			apm_ds_base   = vmf.vmf_dx;
			apm_cs32_limit  = vmf.vmf_si;
			if (apm_version >= 0x0102)
				apm_cs16_limit = (vmf.esi.r_ex >> 16);
			apm_ds_limit  = vmf.vmf_di;
#ifdef APM_DEBUG
			printf("apm: BIOS probe/32-bit connect successful\n");
#endif
		} else {
			/* XXX constant typo! */
			if (vmf.vmf_ah == APME_PROT32NOTDUPPORTED) {
				apm_version = APMINI_NOT32BIT;
			} else {
				apm_version = APMINI_CONNECTERR;
			}
#ifdef APM_DEBUG
			printf("apm: BIOS 32-bit connect failed: error 0x%x  carry %d  ah 0x%x\n",
			       i, (vmf.vmf_eflags & PSL_C) ? 1 : 0, vmf.vmf_ah);
#endif
		}
	} else {
		apm_version = APMINI_CANTFIND;
#ifdef APM_DEBUG
		printf("apm: BIOS probe failed: error 0x%x  carry %d  bx 0x%x\n",
		       i, (vmf.vmf_eflags & PSL_C) ? 1 : 0, vmf.vmf_bx);
#endif
	}
#endif

	bzero(&apm_softc, sizeof(apm_softc));

	switch (apm_version) {
	case APMINI_CANTFIND:
		/* silent */
		return ENXIO;
	case APMINI_NOT32BIT:
		printf("apm: 32bit connection is not supported.\n");
		return ENXIO;
	case APMINI_CONNECTERR:
		printf("apm: 32-bit connection error.\n");
		return ENXIO;
	}
	if (dvp->id_flags & 0x20)
		statclock_disable = 1;
	return -1;
}

/*
 * return 0 if the user will notice and handle the event,
 * return 1 if the kernel driver should do so.
 */
static int
apm_record_event(struct apm_softc *sc, u_int event_type)
{
	struct apm_event_info *evp;

	if ((sc->sc_flags & SCFLAG_OPEN) == 0)
		return 1;		/* no user waiting */
	if (sc->event_count == APM_NEVENTS)
		return 1;			/* overflow */
	if (sc->event_filter[event_type] == 0)
		return 1;		/* not registered */
	evp = &sc->event_list[sc->event_ptr];
	sc->event_count++;
	sc->event_ptr++;
	sc->event_ptr %= APM_NEVENTS;
	evp->type = event_type;
	evp->index = ++apm_evindex;
	selwakeup(&sc->sc_rsel);
	return (sc->sc_flags & SCFLAG_OCTL) ? 0 : 1; /* user may handle */
}

/* Process APM event */
static void
apm_processevent(void)
{
	int apm_event;
	struct apm_softc *sc = &apm_softc;

#ifdef APM_DEBUG
#  define OPMEV_DEBUGMESSAGE(symbol) case symbol: \
	printf("Received APM Event: " #symbol "\n");
#else
#  define OPMEV_DEBUGMESSAGE(symbol) case symbol:
#endif
	do {
		apm_event = apm_getevent();
		switch (apm_event) {
		    OPMEV_DEBUGMESSAGE(PMEV_STANDBYREQ);
			if (apm_op_inprog == 0) {
			    apm_op_inprog++;
			    if (apm_record_event(sc, apm_event)) {
				apm_suspend(PMST_STANDBY);
			    }
			}
			break;
		    OPMEV_DEBUGMESSAGE(PMEV_USERSTANDBYREQ);
			if (apm_op_inprog == 0) {
			    apm_op_inprog++;
			    if (apm_record_event(sc, apm_event)) {
				apm_suspend(PMST_STANDBY);
			    }
			}
			break;
		    OPMEV_DEBUGMESSAGE(PMEV_SUSPENDREQ);
 			apm_lastreq_notify();
			if (apm_op_inprog == 0) {
			    apm_op_inprog++;
			    if (apm_record_event(sc, apm_event)) {
				apm_do_suspend();
			    }
			}
			return; /* XXX skip the rest */
		    OPMEV_DEBUGMESSAGE(PMEV_USERSUSPENDREQ);
 			apm_lastreq_notify();
			if (apm_op_inprog == 0) {
			    apm_op_inprog++;
			    if (apm_record_event(sc, apm_event)) {
				apm_do_suspend();
			    }
			}
			return; /* XXX skip the rest */
		    OPMEV_DEBUGMESSAGE(PMEV_CRITSUSPEND);
			apm_do_suspend();
			break;
		    OPMEV_DEBUGMESSAGE(PMEV_NORMRESUME);
			apm_record_event(sc, apm_event);
			apm_resume();
			break;
		    OPMEV_DEBUGMESSAGE(PMEV_CRITRESUME);
			apm_record_event(sc, apm_event);
			apm_resume();
			break;
		    OPMEV_DEBUGMESSAGE(PMEV_STANDBYRESUME);
			apm_record_event(sc, apm_event);
			apm_resume();
			break;
		    OPMEV_DEBUGMESSAGE(PMEV_BATTERYLOW);
			if (apm_record_event(sc, apm_event)) {
			    apm_battery_low();
			    apm_suspend(PMST_SUSPEND);
			}
			break;
		    OPMEV_DEBUGMESSAGE(PMEV_POWERSTATECHANGE);
			apm_record_event(sc, apm_event);
			break;
		    OPMEV_DEBUGMESSAGE(PMEV_UPDATETIME);
			apm_record_event(sc, apm_event);
			inittodr(0);	/* adjust time to RTC */
			break;
		    OPMEV_DEBUGMESSAGE(PMEV_CAPABILITIESCHANGE);
			apm_record_event(sc, apm_event);
			break;
		    case PMEV_NOEVENT:
			break;
		    default:
			printf("Unknown Original APM Event 0x%x\n", apm_event);
			    break;
		}
	} while (apm_event != PMEV_NOEVENT);
}

/*
 * Attach APM:
 *
 * Initialize APM driver (APM BIOS itself has been initialized in locore.s)
 */

static int
apmattach(struct isa_device *dvp)
{
#define APM_KERNBASE	KERNBASE
	struct apm_softc	*sc = &apm_softc;

	sc->initialized = 0;

	/* Must be externally enabled */
	sc->active = 0;

	/* setup APM parameters */
	sc->cs16_base = (apm_cs16_base << 4) + APM_KERNBASE;
	sc->cs32_base = (apm_cs32_base << 4) + APM_KERNBASE;
	sc->ds_base = (apm_ds_base << 4) + APM_KERNBASE;
	sc->cs32_limit = apm_cs32_limit - 1;
	if (apm_cs16_limit == 0)
	    apm_cs16_limit = apm_cs32_limit;
	sc->cs16_limit = apm_cs16_limit - 1;
	sc->ds_limit = apm_ds_limit - 1;
	sc->cs_entry = apm_cs_entry;

	if (!(dvp->id_flags & 0x40)) {
		/* Don't trust the segment limits that the BIOS reports. */
		sc->cs32_limit = 0xffff;
		sc->cs16_limit = 0xffff;
		sc->ds_limit   = 0xffff;
	}

	/* Always call HLT in idle loop */
	sc->always_halt_cpu = 1;

	sc->slow_idle_cpu = ((apm_flags & APM_CPUIDLE_SLOW) != 0);
	sc->disabled = ((apm_flags & APM_DISABLED) != 0);
	sc->disengaged = ((apm_flags & APM_DISENGAGED) != 0);

	/* print bootstrap messages */
#ifdef APM_DEBUG
	printf("apm: APM BIOS version %04x\n",  apm_version);
	printf("apm: Code32 0x%08x, Code16 0x%08x, Data 0x%08x\n",
		sc->cs32_base, sc->cs16_base, sc->ds_base);
	printf("apm: Code entry 0x%08x, Idling CPU %s, Management %s\n",
		sc->cs_entry, is_enabled(sc->slow_idle_cpu),
		is_enabled(!sc->disabled));
	printf("apm: CS32_limit=0x%x, CS16_limit=0x%x, DS_limit=0x%x\n",
		(u_short)sc->cs32_limit, (u_short)sc->cs16_limit, (u_short)sc->ds_limit);
#endif /* APM_DEBUG */

#if 0
	/* Workaround for some buggy APM BIOS implementations */
	sc->cs_limit = 0xffff;
	sc->ds_limit = 0xffff;
#endif

	/* setup GDT */
	setup_apm_gdt(sc->cs32_base, sc->cs16_base, sc->ds_base,
			sc->cs32_limit, sc->cs16_limit, sc->ds_limit);

	/* setup entry point 48bit pointer */
	apm_addr.segment = GSEL(GAPMCODE32_SEL, SEL_KPL);
	apm_addr.offset  = sc->cs_entry;

	if ((dvp->id_flags & 0x10)) {
		if ((dvp->id_flags & 0xf) >= 0x2) {
			apm_driver_version(0x102);
		} 
		if (!apm_version && (dvp->id_flags & 0xf) >= 0x1) {
			apm_driver_version(0x101);
		}
	} else {
		apm_driver_version(0x102);
		if (!apm_version)
			apm_driver_version(0x101);
	} 
	if (!apm_version)
		apm_version = 0x100;

	sc->minorversion = ((apm_version & 0x00f0) >>  4) * 10 +
			((apm_version & 0x000f) >> 0);
	sc->majorversion = ((apm_version & 0xf000) >> 12) * 10 +
			((apm_version & 0x0f00) >> 8);

	sc->intversion = INTVERSION(sc->majorversion, sc->minorversion);

#ifdef APM_DEBUG
	if (sc->intversion >= INTVERSION(1, 1))
		printf("apm: Engaged control %s\n", is_enabled(!sc->disengaged));
#endif

	printf("apm: found APM BIOS version %d.%d\n",
		sc->majorversion, sc->minorversion);

#ifdef APM_DEBUG
	printf("apm: Slow Idling CPU %s\n", is_enabled(sc->slow_idle_cpu));
#endif

	/* enable power management */
	if (sc->disabled) {
		if (apm_enable_disable_pm(1)) {
#ifdef APM_DEBUG
			printf("apm: *Warning* enable function failed! [%x]\n",
				apm_errno);
#endif
		}
	}

	/* engage power managment (APM 1.1 or later) */
	if (sc->intversion >= INTVERSION(1, 1) && sc->disengaged) {
		if (apm_engage_disengage_pm(1)) {
#ifdef APM_DEBUG
			printf("apm: *Warning* engage function failed err=[%x]",
				apm_errno);
			printf(" (Docked or using external power?).\n");
#endif
		}
	}

        /* default suspend hook */
        sc->sc_suspend.ah_fun = apm_default_suspend;
        sc->sc_suspend.ah_arg = sc;
        sc->sc_suspend.ah_name = "default suspend";
        sc->sc_suspend.ah_order = APM_MAX_ORDER;

        /* default resume hook */
        sc->sc_resume.ah_fun = apm_default_resume;
        sc->sc_resume.ah_arg = sc;
        sc->sc_resume.ah_name = "default resume";
        sc->sc_resume.ah_order = APM_MIN_ORDER;

        apm_hook_establish(APM_HOOK_SUSPEND, &sc->sc_suspend);
        apm_hook_establish(APM_HOOK_RESUME , &sc->sc_resume);

	/* Power the system off using APM */
	at_shutdown_pri(apm_power_off, NULL, SHUTDOWN_FINAL, SHUTDOWN_PRI_LAST);

	sc->initialized = 1;

#ifdef DEVFS
	sc->sc_devfs_token = 
		devfs_add_devswf(&apm_cdevsw, 0, DV_CHR, 0, 0, 0600, "apm");
#endif
	return 0;
}

static int
apmopen(dev_t dev, int flag, int fmt, struct proc *p)
{
	struct apm_softc *sc = &apm_softc;
	int ctl = APMDEV(dev);

	if (!sc->initialized)
		return (ENXIO);

	switch (ctl) {
	case APMDEV_CTL:
		if (!(flag & FWRITE))
			return EINVAL;
		if (sc->sc_flags & SCFLAG_OCTL)
			return EBUSY;
		sc->sc_flags |= SCFLAG_OCTL;
		bzero(sc->event_filter, sizeof sc->event_filter);
		break;
	case APMDEV_NORMAL:
		sc->sc_flags |= SCFLAG_ONORMAL;
		break;
	default:
		return ENXIO;
		break;
	}
	return 0;
}

static int
apmclose(dev_t dev, int flag, int fmt, struct proc *p)
{
	struct apm_softc *sc = &apm_softc;
	int ctl = APMDEV(dev);

	switch (ctl) {
	case APMDEV_CTL:
		apm_lastreq_rejected();
		sc->sc_flags &= ~SCFLAG_OCTL;
		bzero(sc->event_filter, sizeof sc->event_filter);
		break;
	case APMDEV_NORMAL:
		sc->sc_flags &= ~SCFLAG_ONORMAL;
		break;
	}
	if ((sc->sc_flags & SCFLAG_OPEN) == 0) {
		sc->event_count = 0;
		sc->event_ptr = 0;
	}
	return 0;
}

static int
apmioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc *p)
{
	struct apm_softc *sc = &apm_softc;
	int error = 0;
	int ret;
	int newstate;

	if (!sc->initialized)
		return (ENXIO);
#ifdef APM_DEBUG
	printf("APM ioctl: cmd = 0x%x\n", cmd);
#endif
	switch (cmd) {
	case APMIO_SUSPEND:
		if (sc->active)
			apm_suspend(PMST_SUSPEND);
		else
			error = EINVAL;
		break;

	case APMIO_STANDBY:
		if (sc->active)
			apm_suspend(PMST_STANDBY);
		else
			error = EINVAL;
		break;

	case APMIO_GETINFO_OLD:
		{
			struct apm_info info;
			apm_info_old_t aiop;

			if (apm_get_info(&info))
				error = ENXIO;
			aiop = (apm_info_old_t)addr;
			aiop->ai_major = info.ai_major;
			aiop->ai_minor = info.ai_minor;
			aiop->ai_acline = info.ai_acline;
			aiop->ai_batt_stat = info.ai_batt_stat;
			aiop->ai_batt_life = info.ai_batt_life;
			aiop->ai_status = info.ai_status;
		}
		break;
	case APMIO_GETINFO:
		if (apm_get_info((apm_info_t)addr))
			error = ENXIO;
		break;
	case APMIO_ENABLE:
		apm_event_enable();
		break;
	case APMIO_DISABLE:
		apm_event_disable();
		break;
	case APMIO_HALTCPU:
		apm_halt_cpu();
		break;
	case APMIO_NOTHALTCPU:
		apm_not_halt_cpu();
		break;
	case APMIO_DISPLAY:
		newstate = *(int *)addr;
		if (apm_display(newstate))
			error = ENXIO;
		break;
	case APMIO_BIOS:
		if ((ret = apm_do_int((struct apm_bios_arg*)addr))) {
			/*
			 * Return code 1 means bios call was unsuccessful.
			 * Error code is stored in %ah.
			 * Return code -1 means bios call was unsupported
			 * in the APM BIOS version.
			 */
			if (ret == -1) {
				error = EINVAL;
			}
		} else {
			/*
			 * Return code 0 means bios call was successful.
			 * We need only %al and can discard %ah.
			 */
			((struct apm_bios_arg*)addr)->eax &= 0xff;
		}
		break;
	default:
		error = EINVAL;
		break;
	}

	/* for /dev/apmctl */
	if (APMDEV(dev) == APMDEV_CTL) {
		struct apm_event_info *evp;
		int i;

		error = 0;
		switch (cmd) {
		case APMIO_NEXTEVENT:
			if (!sc->event_count) {
				error = EAGAIN;
			} else {
				evp = (struct apm_event_info *)addr;
				i = sc->event_ptr + APM_NEVENTS - sc->event_count;
				i %= APM_NEVENTS;
				*evp = sc->event_list[i];
				sc->event_count--;
			}
			break;
		case APMIO_REJECTLASTREQ:
			if (apm_lastreq_rejected()) {
				error = EINVAL;
			}
			break;
		default:
			error = EINVAL;
			break;
		}
	}

	return error;
}

static int
apmwrite(dev_t dev, struct uio *uio, int ioflag)
{
	struct apm_softc *sc = &apm_softc;
	u_int event_type;
	int error;
	u_char enabled;

	if (APMDEV(dev) != APMDEV_CTL)
		return(ENODEV);
	if (uio->uio_resid != sizeof(u_int))
		return(E2BIG);

	if ((error = uiomove((caddr_t)&event_type, sizeof(u_int), uio)))
		return(error);

	if (event_type < 0 || event_type >= APM_NPMEV)
		return(EINVAL);

	if (sc->event_filter[event_type] == 0) {
		enabled = 1;
	} else {
		enabled = 0;
	}
	sc->event_filter[event_type] = enabled;
#ifdef APM_DEBUG
	printf("apmwrite: event 0x%x %s\n", event_type, is_enabled(enabled));
#endif

	return uio->uio_resid;
}

static int
apmpoll(dev_t dev, int events, struct proc *p)
{
	struct apm_softc *sc = &apm_softc;
	int revents = 0;

	if (events & (POLLIN | POLLRDNORM)) {
		if (sc->event_count) {
			revents |= events & (POLLIN | POLLRDNORM);
		} else {
			selrecord(p, &sc->sc_rsel);
		}
	}

	return (revents);
}


static apm_devsw_installed = 0;

static void
apm_drvinit(void *unused)
{
	dev_t dev;

	if( ! apm_devsw_installed ) {
		dev = makedev(CDEV_MAJOR,0);
		cdevsw_add(&dev,&apm_cdevsw,NULL);
		apm_devsw_installed = 1;
    	}
}

SYSINIT(apmdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,apm_drvinit,NULL)
