#define APM_DEBUG 1
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
 *
 *	$Id: apm.c,v 1.8 1994/12/16 06:16:30 phk Exp $
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
#include <machine/clock.h>
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

#define is_enabled(foo) ((foo) ? "enabled" : "disabled")

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
	ssdtosd(gdt_segs + GAPMCODE32_SEL, &gdt[GAPMCODE32_SEL].sd);
	ssdtosd(gdt_segs + GAPMCODE16_SEL, &gdt[GAPMCODE16_SEL].sd);
	ssdtosd(gdt_segs + GAPMDATA_SEL  , &gdt[GAPMDATA_SEL  ].sd);
}

/* 48bit far pointer */
struct addr48 {
	u_long		offset;
	u_short		segment;
} apm_addr;

int apm_errno;

inline
int
apm_int(u_long *eax,u_long *ebx,u_long *ecx)
{
	u_long cf;
	__asm ("pushl	%%ebp
		pushl	%%edx
		pushl	%%esi
		pushl	%%edi
		xorl	%3,%3
		movl	%3,%%esi
		lcall	_apm_addr
		jnc	1f
		incl	%3
	1:	
		popl	%%edi
		popl	%%esi
		popl	%%edx
		popl	%%ebp"
		: "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=D" (cf)
		: "0" (*eax),  "1" (*ebx),  "2" (*ecx)
		);		
	apm_errno = ((*eax) >> 8) & 0xff;
	return cf;
}


/* enable/disable power management */
static int 
apm_enable_disable_pm(int enable)
{
	u_long eax,ebx,ecx;

	eax = (APM_BIOS<<8) | APM_ENABLEDISABLEPM;

	if (intversion >= INTVERSION(1, 1)) {
		ebx  = PMDV_ALLDEV;
	} else {
		ebx  = 0xffff;	/* APM version 1.0 only */
	}
	ecx  = enable;
	return apm_int(&eax,&ebx,&ecx);
}

/* Tell APM-BIOS that WE will do 1.1 and see what they say... */
static void
apm_driver_version()
{
	u_long eax,ebx,ecx,i;

#ifdef APM_DEBUG
	eax = (APM_BIOS<<8) | APM_INSTCHECK;
	ebx  = 0x0;
	ecx  = 0x0101;
	i = apm_int(&eax,&ebx,&ecx);
	printf("[%04lx %04lx %04lx %ld %02x]\n",
		eax,ebx,ecx,i,apm_errno);
#endif

	eax = (APM_BIOS<<8) | APM_DRVVERSION;
	ebx  = 0x0;
	ecx  = 0x0101;
	if(!apm_int(&eax,&ebx,&ecx)) 
		apm_version = eax & 0xffff;

#ifdef APM_DEBUG
	eax = (APM_BIOS<<8) | APM_INSTCHECK;
	ebx  = 0x0;
	ecx  = 0x0101;
	i = apm_int(&eax,&ebx,&ecx);
	printf("[%04lx %04lx %04lx %ld %02x]\n",
		eax,ebx,ecx,i,apm_errno);
#endif
}

/* engage/disengage power management (APM 1.1 or later) */
static int 
apm_engage_disengage_pm(int engage)
{
	u_long eax,ebx,ecx,i;

	eax = (APM_BIOS<<8) | APM_ENGAGEDISENGAGEPM;
	ebx = PMDV_ALLDEV;
	ecx = engage;
	i = apm_int(&eax,&ebx,&ecx);
	return i;
}

/* get PM event */
static u_int 
apm_getevent(void)
{
	u_long eax,ebx,ecx;

	eax = (APM_BIOS<<8) | APM_GETPMEVENT;
	
	ebx = 0;
	ecx = 0;
	if (apm_int(&eax,&ebx,&ecx))
		return PMEV_NOEVENT;

	return ebx & 0xffff;
}

/* suspend entire system */
static int 
apm_suspend_system(void)
{
	u_long eax,ebx,ecx;

	eax = (APM_BIOS<<8) | APM_SETPWSTATE;
	ebx = PMDV_ALLDEV;
	ecx = PMST_SUSPEND;

	if (apm_int(&eax,&ebx,&ecx)) {
		printf("Entire system suspend failure: errcode = %ld\n", 
			0xff & (eax >> 8));
		return 1;
	}
	return 0;
}

/* APM Battery low handler */
static void 
apm_battery_low(void)
{
	printf("\007\007 * * * BATTERY IS LOW * * * \007\007");
}

static struct timeval suspend_time;

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
	log(LOG_NOTICE, "resumed from suspended mode (slept %02d:%02d:%02d)\n",
		hour, minute, second);
	return 0;
}

static int 
apm_default_suspend(void)
{
	int	pl;
	microtime(&suspend_time);
	apm_suspend_system();
	return 0;
}

/* get APM information */
static int
apm_get_info(apm_info_t aip)
{
	u_long eax,ebx,ecx;

	eax = (APM_BIOS<<8)|APM_GETPWSTATUS;
	ebx = PMDV_ALLDEV;
	ecx = 0;

	if (apm_int(&eax,&ebx,&ecx))
		return 1;

	aip->ai_acline    = (ebx >> 8) & 0xff;
	aip->ai_batt_stat = ebx & 0xff;
	aip->ai_batt_life = ecx & 0xff;
	aip->ai_major     = (u_int)majorversion;
	aip->ai_minor     = (u_int)minorversion;
	return 0;
}


static void apm_processevent(void);

/* inform APM BIOS that CPU is idle */
void 
apm_cpu_idle(void)
{
	if (idle_cpu) {
		if (active) {
			__asm ("movw $0x5305, %ax; lcall _apm_addr");
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
		__asm("sti ; hlt");	/* wait for interrupt */
	}
}

/* inform APM BIOS that CPU is busy */
void 
apm_cpu_busy(void)
{
	if (idle_cpu && active) {
		__asm("movw $0x5306, %ax; lcall _apm_addr");
	}
}


/*
 * APM timeout routine:
 *
 * This routine is automatically called by timer once per second.
 */

static void 
apm_timeout(void *arg1)
{
	apm_processevent();
	timeout(apm_timeout, NULL, hz - 1 );  /* More than 1 Hz */
}

/* enable APM BIOS */
static void 
apm_event_enable(void)
{
#ifdef APM_DEBUG
	printf("called apm_event_enable()\n");
#endif
	if (apm_initialized) {
		active = 1;
		apm_timeout(0);
	}
}

/* disable APM BIOS */
static void 
apm_event_disable(void)
{
#ifdef APM_DEBUG
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
		printf("apm%d: 32bit connection is not supported.\n", 
			dvp->id_unit);
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


/* Process APM event */
static void
apm_processevent(void)
{
	int apm_event;

#ifdef APM_DEBUG
#  define OPMEV_DEBUGMESSAGE(symbol) case symbol: \
	printf("Original APM Event: " #symbol "\n");
#else
#  define OPMEV_DEBUGMESSAGE(symbol) case symbol:
#endif

	while (1) {
		apm_event = apm_getevent();
		if (apm_event == PMEV_NOEVENT) 
			break;
		switch (apm_event) {
		    OPMEV_DEBUGMESSAGE(PMEV_STANDBYREQ);
			apm_default_suspend();
			break;
		    OPMEV_DEBUGMESSAGE(PMEV_SUSPENDREQ);
			apm_default_suspend();
			break;
		    OPMEV_DEBUGMESSAGE(PMEV_USERSUSPENDREQ);
			apm_default_suspend();
			break;

		    OPMEV_DEBUGMESSAGE(PMEV_CRITSUSPEND);
			apm_default_suspend();
			break;

		    OPMEV_DEBUGMESSAGE(PMEV_NORMRESUME);
			apm_default_resume();
			break;
		    OPMEV_DEBUGMESSAGE(PMEV_CRITRESUME);
			apm_default_resume();
			break;
		    OPMEV_DEBUGMESSAGE(PMEV_STANDBYRESUME);
			apm_default_resume();
			break;

		    OPMEV_DEBUGMESSAGE(PMEV_BATTERYLOW);
			apm_battery_low();
			apm_default_suspend();
			break;

		    OPMEV_DEBUGMESSAGE(PMEV_POWERSTATECHANGE);
			break;

		    OPMEV_DEBUGMESSAGE(PMEV_UPDATETIME);
			inittodr(0);	/* adjust time to RTC */
			break;

		    default:
			printf("Unknown Original APM Event 0x%x\n", apm_event);
			    break;
		}
	}
}

/*
 * Attach APM:
 *
 * Initialize APM driver (APM BIOS itself has been initialized in locore.s)
 *
 * Now, unless I'm mad, (not quite ruled out yet), the APM-1.1 spec is bogus:
 * 
 * Appendix C says under the header "APM 1.0/APM 1.1 Modal BIOS Behavior"
 * that "When an APM Driver connects with an APM 1.1 BIOS, the APM 1.1 BIOS
 * will default to an APM 1.0 connection.  After an APM Driver calls the APM
 * Driver Version function, specifying that it supports APM 1.1, and [sic!]
 * APM BIOS will change its behavior to an APM 1.1 connection.  If the APM
 * BIOS is an APM 1.0 BIOS, the APM Driver Version function call will fail,
 * and the connection will remain an APM 1.0 connection."
 *
 * OK so I can establish a 1.0 connection, and then tell that I'm a 1.1
 * and maybe then the BIOS will tell that it too is a 1.1.
 * Fine.
 * Now how will I ever get the segment-limits for instance ?  There is no 
 * way I can see that I can get a 1.1 response back from an "APM Protected 
 * Mode 32-bit Interface Connect" function ???
 * 
 * Who made this,  Intel and Microsoft ?  -- How did you guess !
 *
 * /phk
 */

int 
apmattach(struct isa_device *dvp)
{

	/* setup APM parameters */
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
#ifdef APM_DEBUG
	printf(" found APM BIOS version %04x\n",  apm_version);
	printf("apm%d: Code32 0x%08x, Code16 0x%08x, Data 0x%08x\n",
		dvp->id_unit, cs32_base, cs16_base, ds_base);
	printf("apm%d: Code entry 0x%08x, Idling CPU %s, Management %s\n",
		dvp->id_unit, cs_entry, is_enabled(idle_cpu), 
		is_enabled(!disabled));
	printf("apm%d: CS_limit=%x, DS_limit=%x\n",
		dvp->id_unit, cs_limit,ds_limit);

#endif /* APM_DEBUG */

	cs_limit = 0xffff;
	ds_limit = 0xffff;

	/* setup GDT */
	setup_apm_gdt(cs32_base, cs16_base, ds_base, cs_limit, ds_limit);

	/* setup entry point 48bit pointer */
	apm_addr.segment = GSEL(GAPMCODE32_SEL, SEL_KPL);
	apm_addr.offset  = cs_entry;

	/* Try to kick bios into 1.1 mode */
	apm_driver_version();

	minorversion = ((apm_version & 0x00f0) >>  4) * 10 +
			((apm_version & 0x000f) >> 0);
	majorversion = ((apm_version & 0xf000) >> 12) * 10 + 
			((apm_version & 0x0f00) >> 8);

	intversion = INTVERSION(majorversion, minorversion);

	if (intversion >= INTVERSION(1, 1)) {
		printf("apm%d: Engaged control %s\n",
			dvp->id_unit, is_enabled(!disengaged));
	}

	printf(" found APM BIOS version %d.%d\n", majorversion, minorversion);
	printf("apm%d: Idling CPU %s\n", dvp->id_unit, is_enabled(idle_cpu));

	/* enable power management */
	if (disabled) {
		if (apm_enable_disable_pm(1)) {
			printf("Warning: APM enable function failed! [%x]\n", 
				apm_errno);
		}
	}

	/* engage power managment (APM 1.1 or later) */
	if (intversion >= INTVERSION(1, 1) && disengaged) {
		if (apm_engage_disengage_pm(1)) {
			printf("Warning: APM engage function failed [%x]\n", 
				apm_errno);
		}
	}

	apm_initialized = 1;

	apm_event_enable();

	return 0;
}

int 
apmopen(dev_t dev, int flag, int fmt, struct proc *p) 
{
	if (!apm_initialized) {
		return ENXIO;
	}
	if (minor(dev))
		return (ENXIO);
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

#ifdef APM_DEBUG
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
		apm_default_suspend();
		break;
	case APMIO_GETINFO:
		if (apm_get_info((apm_info_t)addr)) {
			error = ENXIO;
		}
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
