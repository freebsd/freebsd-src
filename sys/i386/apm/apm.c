/*
 * LP (Laptop Package)
 * 
 * Copyright (c) 1994 by HOSOKAWA, Tatsumi <hosokawa@mt.cs.keio.ac.jp>
 *
 * This software may be used, modified, copied, and distributed, in
 * both source and binary form provided that the above copyright and
 * these terms are retained. Under no circumstances is the author 
 * responsible for the proper functioning of this software, nor does 
 * the author assume any responsibility for damages incurred with its 
 * use.
 *
 * Sep, 1994	Implemented on FreeBSD 1.1.5.1R (Toshiba AVS001WD)
 */

#include "apm.h"

#if NAPM > 0

#include <sys/param.h>
#include "conf.h"
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include "i386/isa/isa.h"
#include "i386/isa/isa_device.h"
#include <machine/apm_bios.h>
#include <machine/segments.h>
#include <vm/vm.h>
#include <sys/syslog.h>
#include "apm_setup.h"

/* static data */
static int	apm_initialized = 0, active = 0, halt_cpu = 1;
static u_int	minorversion, majorversion;
static u_int	cs32_base, cs16_base, ds_base;
static u_int	cs_limit, ds_limit;
static u_int	cs_entry;
static u_int	intversion;
static int	idle_cpu, disabled, disengaged;

/* Map version number to integer (keeps ordering of version numbers) */
#define INTVERSION(major, minor)	((major)*100 + (minor))

static timeout_t apm_timeout;

/* setup APM GDT discriptors */
static void 
setup_apm_gdt(u_int code32_base, u_int code16_base, u_int data_base, u_int code_limit, u_int data_limit) 
{
	/* setup 32bit code segment */
	gdt_segs[GAPMCODE32_SEL].ssd_base  = code32_base;
	gdt_segs[GAPMCODE32_SEL].ssd_limit = code_limit;

	/* setup 16bit code segment */
	gdt_segs[GAPMCODE16_SEL].ssd_base  = code16_base;
	gdt_segs[GAPMCODE16_SEL].ssd_limit = code_limit;

	/* setup data segment */
	gdt_segs[GAPMDATA_SEL  ].ssd_base  = data_base;
	gdt_segs[GAPMDATA_SEL  ].ssd_limit = data_limit;

	/* reflect these changes on physical GDT */
	ssdtosd(gdt_segs + GAPMCODE32_SEL, gdt + GAPMCODE32_SEL);
	ssdtosd(gdt_segs + GAPMCODE16_SEL, gdt + GAPMCODE16_SEL);
	ssdtosd(gdt_segs + GAPMDATA_SEL  , gdt + GAPMDATA_SEL  );
}

/* 48bit far pointer */
struct addr48 {
	u_long		offset;
	u_short		segment;
} apm_addr;


/* register structure for BIOS call */
union real_regs {
	struct xregs {
		u_short	ax;
		u_short	bx __attribute__ ((packed));
		u_short	cx __attribute__ ((packed));
		u_short	dx __attribute__ ((packed));
		u_short	si __attribute__ ((packed));
		u_short	di __attribute__ ((packed));
		u_short	cf __attribute__ ((packed));	/* carry */
	} x;
	struct hlregs {
		u_char	al;
		u_char	ah __attribute__ ((packed));
		u_char	bl __attribute__ ((packed));
		u_char	bh __attribute__ ((packed));
		u_char	cl __attribute__ ((packed));
		u_char	ch __attribute__ ((packed));
		u_char	dl __attribute__ ((packed));
		u_char	dh __attribute__ ((packed));
		u_short	si __attribute__ ((packed));
		u_short	di __attribute__ ((packed));
		u_short	cf __attribute__ ((packed));	/* carry */
	} hl;
};


/* call APM BIOS */
extern void call_apm(union real_regs* regs);

extern u_char apm_errno;

/* enable/disable power management */
static int 
apm_enable_disable_pm(int enable)
{
	union real_regs regs;

	regs.hl.ah = APM_BIOS;
	regs.hl.al = APM_ENABLEDISABLEPM;
	if (intversion >= INTVERSION(1, 1)) {
		regs.x.bx  = PMDV_ALLDEV;
	}
	else {
		regs.x.bx  = 0xffff;	/* APM version 1.0 only */
	}
	regs.x.cx  = enable;
	call_apm(&regs);
	return regs.x.cf;
}

/* engage/disengage power management (APM 1.1 or later) */
static int 
apm_engage_disengage_pm(int engage)
{
	union real_regs regs;

	regs.hl.ah = APM_BIOS;
	regs.hl.al = APM_ENGAGEDISENGAGEPM;
	regs.x.bx = PMDV_ALLDEV;
	regs.x.cx = engage;
	call_apm(&regs);
	return regs.x.cf;
}

/* get PM event */
static u_int 
apm_getevent(void)
{
	union real_regs regs;

	regs.hl.ah = APM_BIOS;
	regs.hl.al = APM_GETPMEVENT;
	call_apm(&regs);
	if (regs.x.cf) {
#if 0
		printf("No event: errcode = %d\n", apm_errno);
#endif
		return PMEV_NOEVENT;
	}
	return (u_int)regs.x.bx;
}

/*
 * In many cases, the first event that occured after resume, needs
 * special treatment. This binary flag make this process possible.
 * Initial value of this variable is 1, because the bootstrap
 * condition is equivalent to resumed condition for the power
 * manager.
 */
static int	resumed_event = 1;

/* suspend entire system */
static int 
apm_suspend_system(void)
{
	union real_regs regs;

	regs.hl.ah = APM_BIOS;
	regs.hl.al = APM_SETPWSTATE;
	regs.x.bx = PMDV_ALLDEV;
	regs.x.cx = PMST_SUSPEND;
	call_apm(&regs);
	if (regs.x.cf) {
		printf("Entire system suspend failure: errcode = %d\n", apm_errno);
		return 1;
	}
	resumed_event = 1;

	return 0;
}

/* APM Battery low handler */
static void 
apm_battery_low(void)
{
	/* Currently, this routine has not been implemented. Sorry... */
}


/* APM driver calls some functions automatically when the system wakes up */
static void 
apm_execute_hook(apm_hook_func_t list)
{
	apm_hook_func_t p;

	for (p = list; p != NULL; p = p->next) {
		if ((*(p->func))()) {
			printf("Warning: APM hook of %s failed", p->name);
		}
	}
}


/* APM hook manager */
static apm_hook_func_t 
apm_hook_init(apm_hook_func_t *list, int (*func)(void), char *name, int order)
{
	int pl;
	apm_hook_func_t p, prev, new_node;
	
	pl = splhigh();
	new_node = malloc(sizeof(*new_node), M_DEVBUF, M_NOWAIT);
	if (new_node == NULL) {
		panic("Can't allocate device buffer for apm_resume_hook.");
	}
	new_node->func = func;
	new_node->name = name;
#if 0
	new_node->next = *list;
	*list = new_node;
#else
	prev = NULL;
	for (p = *list; p != NULL; prev = p, p = p->next) {
		if (p->order > order) {
			break;
		}
	}

	if (prev == NULL) {
		new_node->next = *list;
		*list = new_node;
	}
	else {
		new_node->next = prev->next;
		prev->next = new_node;
	}
#endif
	splx(pl);
	return new_node;
}

void 
apm_hook_delete(apm_hook_func_t *list, apm_hook_func_t delete_node)
{
	int pl;
	apm_hook_func_t p, prev;

	pl = splhigh();
	prev = NULL;
	for (p = *list; p != NULL; prev = p, p = p->next) {
		if (p == delete_node) {
			goto deleteit;
		}
	}
	panic("Tried to delete unregistered apm_resume_hook.");
	goto nosuchnode;
deleteit:
	if (prev != NULL) {
		prev->next = p->next;
	}
	else {
		*list = p->next;
	}
	free(delete_node, M_DEVBUF);
nosuchnode:
	splx(pl);
}

static struct timeval suspend_time;

/* default APM hook functions */
static int 
apm_default_resume(void)
{
	u_int second, minute, hour;
	struct timeval resume_time;

	inittodr(0);	/* adjust time to RTC */
	microtime(&resume_time);
	second = resume_time.tv_sec - suspend_time.tv_sec;
	hour = second / 3600;
	second %= 3600;
	minute = second / 60;
	second %= 60;
	log(LOG_NOTICE, "resumed from suspended mode (slept %02d:%02d:%02d)\n", hour, minute, second);
	return 0;
}

static int 
apm_default_suspend(void)
{
	int	pl;
#if 0
	pl = splhigh();
	sync(curproc, NULL, NULL);
	splx(pl);
#endif
	microtime(&suspend_time);
	return 0;
}

/* list structure for hook */
static apm_hook_func_t	apm_resume_hook  = NULL;
static apm_hook_func_t	apm_suspend_hook = NULL;

/* execute resume hook */
static void 
apm_execute_resume_hook(void)
{
	apm_execute_hook(apm_resume_hook);
}

/* add a node on resume hook */
apm_hook_func_t
apm_resume_hook_init(int (*func)(void), char *name, int order)
{
	return apm_hook_init(&apm_resume_hook, func, name, order);
}

/* delete a node from resume hook */
void
apm_resume_hook_delete(apm_hook_func_t delete_node)
{
	apm_hook_delete(&apm_resume_hook, delete_node);
}

/* execute suspend hook */
static void 
apm_execute_suspend_hook(void)
{
	apm_execute_hook(apm_suspend_hook);
}

/* add a node on resume hook */
apm_hook_func_t
apm_suspend_hook_init(int (*func)(void), char *name, int order)
{
	return apm_hook_init(&apm_suspend_hook, func, name, order);
}

/* delete a node from resume hook */
void
apm_suspend_hook_delete(apm_hook_func_t delete_node)
{
	apm_hook_delete(&apm_suspend_hook, delete_node);
}

/* get APM information */
static int
apm_get_info(apm_info_t aip)
{
	union real_regs regs;

	regs.hl.ah = APM_BIOS;
	regs.hl.al = APM_GETPWSTATUS;
	regs.x.bx = PMDV_ALLDEV;
	call_apm(&regs);
	if (regs.x.cf) {
		printf("Get APM info failure: errcode = %d\n", apm_errno);
		return 1;
	}
	aip->ai_major = (u_int)majorversion;
	aip->ai_minor = (u_int)minorversion;
	aip->ai_acline = (u_int)regs.hl.bh;
	aip->ai_batt_stat = (u_int)regs.hl.bl;
	aip->ai_batt_life = (u_int)regs.hl.cl;
	return 0;
}


/* Define equivalent event sets */

static int equiv_event_num = 0;
static struct apm_eqv_event equiv_events[APM_MAX_EQUIV_EVENTS];

static int
apm_def_eqv(apm_eqv_event_t aee)
{
	if (equiv_event_num == APM_MAX_EQUIV_EVENTS) {
		return 1;
	}
	memcpy(&equiv_events[equiv_event_num], aee, sizeof(struct apm_eqv_event));
	equiv_event_num++;
	return 0;
}

static void
apm_flush_eqv(void)
{
	equiv_event_num = 0;
}

static void apm_processevent(void);

/*
 * Public interface to the suspend/resume:
 *
 * Execute suspend and resume hook before and after sleep, respectively.
 */

void 
apm_suspend_resume(void)
{
	int	pl;
#if 0
	printf("Called apm_suspend_resume();\n");
#endif
	if (apm_initialized) {
		apm_execute_suspend_hook();
		apm_suspend_system();
		apm_execute_resume_hook();
		apm_processevent();
	}
}

/* inform APM BIOS that CPU is idle */
void 
apm_cpu_idle(void)
{
	if (idle_cpu) {
		if (active) {
			asm("movw $0x5305, %ax; lcall _apm_addr");
		}
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
	if (!active || halt_cpu) {
		asm("sti ; hlt");	/* wait for interrupt */
	}
}

/* inform APM BIOS that CPU is busy */
void 
apm_cpu_busy(void)
{
	if (idle_cpu && active) {
		asm("movw $0x5306, %ax; lcall _apm_addr");
	}
}


/*
 * APM timeout routine:
 *
 * This routine is automatically called by timer two times within one 
 * seconed.
 */

static void 
apm_timeout(void *arg1)
{
#if 0
	printf("Called apm_timeout\n");
#endif
	apm_processevent();
	timeout(apm_timeout, NULL, hz / 2); /* 2 Hz */
	/* APM driver must polls APM event a time per second */
}

/* enable APM BIOS */
static void 
apm_event_enable(void)
{
#if 0
	printf("called apm_event_enable()\n");
#endif
	if (apm_initialized) {
		active = 1;
		timeout(apm_timeout, NULL, 2 * hz);
	}
}

/* disable APM BIOS */
static void 
apm_event_disable(void)
{
#if 0
	printf("called apm_event_disable()\n");
#endif
	if (apm_initialized) {
		untimeout(apm_timeout, NULL);
		active = 0;
	}
}

/* halt CPU in scheduling loop */
static void apm_halt_cpu(void)
{
	if (apm_initialized) {
		halt_cpu = 1;
	}
}

/* don't halt CPU in scheduling loop */
static void apm_not_halt_cpu(void)
{
	if (apm_initialized) {
		halt_cpu = 0;
	}
}

/* device driver definitions */
int apmprobe (struct isa_device *);
int apmattach(struct isa_device *);

struct isa_driver apmdriver = { apmprobe, apmattach, "apm" };


/*
 * probe APM (dummy):
 *
 * APM probing routine is placed on locore.s and apm_init.S because
 * this process forces the CPU to turn to real mode or V86 mode.
 * Current version uses real mode, but on future version, we want
 * to use V86 mode in APM initialization.
 */
int 
apmprobe(struct isa_device *dvp)
{
	switch (apm_version) {
	case APMINI_CANTFIND:
		/* silent */
		return 0;
	case APMINI_NOT32BIT:
		printf("apm%d: 32bit connection is not supported.\n", dvp->id_unit);
		return 0;
	case APMINI_CONNECTERR:
		printf("apm%d: 32-bit connection error.\n", dvp->id_unit);
		return 0;
	}
	if ((apm_version & 0xff00) != 0x0100) return 0;
	if ((apm_version & 0x00f0) >= 0x00a0) return 0;
	if ((apm_version & 0x000f) >= 0x000a) return 0;
	return -1;
}

static const char *
is_enabled(int enabled)
{
	if (enabled) {
		return "enabled";
	}
	return "disabled";
}

static const char *
apm_error(void)
{
	static char buffer[64];

	switch (apm_errno) {
	case 0:
		return "APM OK."; 
	default:
		sprintf(buffer, "Unknown Error 0x%x", (u_int)apm_errno);
		return buffer;
	}
}



/* Process APM event */
static void
apm_processevent(void)
{
	int i, apm_event;

getevent:
	while (1) {
		if ((apm_event = apm_getevent()) == PMEV_NOEVENT) {
			break;
		}
#if 0
#if 1
#define OPMEV_DEBUGMESSAGE(symbol) case symbol: break;
#else
#define OPMEV_DEBUGMESSAGE(symbol) case symbol: printf("Original APM Event: " #symbol "\n"); break
#endif
		switch (apm_event) {
			OPMEV_DEBUGMESSAGE(PMEV_NOEVENT);
			OPMEV_DEBUGMESSAGE(PMEV_STANDBYREQ);
			OPMEV_DEBUGMESSAGE(PMEV_SUSPENDREQ);
			OPMEV_DEBUGMESSAGE(PMEV_NORMRESUME);
			OPMEV_DEBUGMESSAGE(PMEV_CRITRESUME);
			OPMEV_DEBUGMESSAGE(PMEV_BATTERYLOW);
			OPMEV_DEBUGMESSAGE(PMEV_POWERSTATECHANGE);
			OPMEV_DEBUGMESSAGE(PMEV_UPDATETIME);
			OPMEV_DEBUGMESSAGE(PMEV_CRITSUSPEND);
			OPMEV_DEBUGMESSAGE(PMEV_USERSUSPENDREQ);
			OPMEV_DEBUGMESSAGE(PMEV_STANDBYRESUME);
			default:
				printf("Unknown Original APM Event 0x%x\n", apm_event);
				break;
		}
#endif
		for (i = 0; i < equiv_event_num; i++) {
			if (equiv_events[i].aee_event == apm_event) {
				u_int tmp = PMEV_DEFAULT;
				if (resumed_event) {
					tmp = equiv_events[i].aee_resume;
				}
				else {
					tmp = equiv_events[i].aee_equiv;
				}
				if (tmp != PMEV_DEFAULT) {
					apm_event = tmp;
					break;
				}
			}
		}
#if 1
#if 1
#define PMEV_DEBUGMESSAGE(symbol) case symbol: break;
#else
#define PMEV_DEBUGMESSAGE(symbol) case symbol: printf("APM Event: " #symbol "\n"); break
#endif
		switch (apm_event) {
			PMEV_DEBUGMESSAGE(PMEV_NOEVENT);
			PMEV_DEBUGMESSAGE(PMEV_STANDBYREQ);
			PMEV_DEBUGMESSAGE(PMEV_SUSPENDREQ);
			PMEV_DEBUGMESSAGE(PMEV_NORMRESUME);
			PMEV_DEBUGMESSAGE(PMEV_CRITRESUME);
			PMEV_DEBUGMESSAGE(PMEV_BATTERYLOW);
			PMEV_DEBUGMESSAGE(PMEV_POWERSTATECHANGE);
			PMEV_DEBUGMESSAGE(PMEV_UPDATETIME);
			PMEV_DEBUGMESSAGE(PMEV_CRITSUSPEND);
			PMEV_DEBUGMESSAGE(PMEV_USERSUSPENDREQ);
			PMEV_DEBUGMESSAGE(PMEV_STANDBYRESUME);
			default:
				printf("Unknown APM Event 0x%x\n", apm_event);
				break;
		}
#endif
		switch (apm_event) {
		case PMEV_NOEVENT:
		case PMEV_STANDBYREQ:
		case PMEV_POWERSTATECHANGE:
		case PMEV_CRITSUSPEND:
		case PMEV_USERSTANDBYREQ:
		case PMEV_USERSUSPENDREQ:
			break;
		case PMEV_BATTERYLOW:
			apm_battery_low();
			break;
		case PMEV_SUSPENDREQ:
			apm_suspend_resume();
			break;
		case PMEV_NORMRESUME:
		case PMEV_CRITRESUME:
		case PMEV_UPDATETIME:
		case PMEV_STANDBYRESUME:
			inittodr(0);	/* adjust time to RTC */
			break;
		}
	}
	resumed_event = 0;
}


/*
 * Attach APM:
 *
 * Initialize APM driver (APM BIOS itself has been initialized in locore.s)
 */

int 
apmattach(struct isa_device *dvp)
{

	/* setup APM parameters */
	minorversion = ((apm_version & 0x00f0) >>  4) * 10 + ((apm_version & 0x000f) >> 0);
	majorversion = ((apm_version & 0xf000) >> 12) * 10 + ((apm_version & 0x0f00) >> 8);
	intversion = INTVERSION(majorversion, minorversion);
	cs32_base = (apm_cs32_base << 4) + KERNBASE;
	cs16_base = (apm_cs16_base << 4) + KERNBASE;
	ds_base = (apm_ds_base << 4) + KERNBASE;
	cs_limit = apm_cs_limit;
	ds_limit = apm_ds_limit;
	cs_entry = apm_cs_entry;
	idle_cpu = ((apm_flags & APM_CPUIDLE_SLOW) != 0);
	disabled = ((apm_flags & APM_DISABLED) != 0);
	disengaged = ((apm_flags & APM_DISENGAGED) != 0);

	/* print bootstrap messages */
#ifdef DEBUG
	printf(" found APM BIOS version %d.%d\n", dvp->id_unit, majorversion, minorversion);
	printf("apm%d: Code32 0x%08x, Code16 0x%08x, Data 0x%08x\n", dvp->id_unit, cs32_base, cs16_base, ds_base);
	printf("apm%d: Code entry 0x%08x, Idling CPU %s, Management %s\n", dvp->id_unit, cs_entry, is_enabled(idle_cpu), is_enabled(!disabled));
#else
	printf(" found APM BIOS version %d.%d\n", majorversion, minorversion);
	printf("apm%d: Idling CPU %s\n", dvp->id_unit, is_enabled(idle_cpu));
#endif

	/*
	 * APM 1.0 does not have:
	 * 
	 * 	1. segment limit parameters
	 *
	 *	2. engage/disengage operations
	 */
	if (intversion >= INTVERSION(1, 1)) {
		printf("apm%d: Engaged control %s\n", dvp->id_unit, is_enabled(!disengaged));
	}
	else {
		cs_limit = 0xffff;
		ds_limit = 0xffff;
	}

	/* setup GDT */
	setup_apm_gdt(cs32_base, cs16_base, ds_base, cs_limit, ds_limit);

	/* setup entry point 48bit pointer */
	apm_addr.segment = GSEL(GAPMCODE32_SEL, SEL_KPL);
	apm_addr.offset  = cs_entry;

	/* enable power management */
	if (disabled) {
		if (apm_enable_disable_pm(1)) {
			printf("Warning: APM enable function failed! [%s]\n", apm_error());
		}
	}

	/* engage power managment (APM 1.1 or later) */
	if (intversion >= INTVERSION(1, 1) && disengaged) {
		if (apm_engage_disengage_pm(1)) {
			printf("Warning: APM engage function failed [%s]\n", apm_error());
		}
	}

	apm_suspend_hook_init(apm_default_suspend, "default suspend", APM_MAX_ORDER);
	apm_resume_hook_init (apm_default_resume , "default resume" , APM_MIN_ORDER);
	apm_initialized = 1;

	return 0;
}

int 
apmopen(dev_t dev, int flag, int fmt, struct proc *p) 
{
	if (!apm_initialized) {
		return ENXIO;
	}
	switch (minor(dev)) {
	case 0:	/* apm0 */
		break;
	defaults:
		return (ENXIO);
	}
	return 0;
}

int 
apmclose(dev_t dev, int flag, int fmt, struct proc *p)
{
	return 0;
}

int 
apmioctl(dev_t dev, int cmd, caddr_t addr, int flag, struct proc *p)
{
	int error = 0;
	int pl;

#if 0
	printf("APM ioctl: minor = %d, cmd = 0x%x\n", minor(dev), cmd);
#endif

	pl = splhigh();
	if (minor(dev) != 0) {
		return ENXIO;
	}
	if (!apm_initialized) {
		return ENXIO;
	}
	switch (cmd) {
	case APMIO_SUSPEND:
		apm_suspend_resume();
		break;
	case APMIO_GETINFO:
		if (apm_get_info((apm_info_t)addr)) {
			error = ENXIO;
		}
		break;
	case APMIO_DEFEQV:
		if (apm_def_eqv((apm_eqv_event_t)addr)) {
			error = ENOSPC;
		}
		break;
	case APMIO_FLUSHEQV:
		apm_flush_eqv();
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
	default:
		error = EINVAL;
		break;
	}
	splx(pl);
	return error;
}

#endif /* NAPM > 0 */
