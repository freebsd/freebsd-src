/*-
 * Copyright (c) 1999 Takanori Watanabe <takawata@shidahara1.planet.sci.kobe-u.ac.jp>
 * Copyright (c) 1999, 2000 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: acpi.c,v 1.26 2000/08/15 14:43:43 iwasaki Exp $
 *	$FreeBSD$
 */

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/ioccom.h>
#include <sys/reboot.h>
#include <sys/sysctl.h>

#include <machine/clock.h>
#include <sys/kthread.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpiio.h>

#include <dev/acpi/aml/aml_common.h>
#include <dev/acpi/aml/aml_env.h>
#include <dev/acpi/aml/aml_evalobj.h>
#include <dev/acpi/aml/aml_parse.h>

/*
 * These items cannot be in acpi_softc because they are initialized
 * by MD code before the softc is allocated.
 */
struct		ACPIaddr acpi_addr;
struct		ACPIrsdp *acpi_rsdp;

/*
 * Character device 
 */

static d_open_t		acpiopen;
static d_close_t	acpiclose;
static d_ioctl_t	acpiioctl;
static d_mmap_t		acpimmap;

#define CDEV_MAJOR 152
static struct cdevsw acpi_cdevsw = {
	acpiopen,
	acpiclose,
	noread,
	nowrite,
	acpiioctl,
	nopoll,
	acpimmap,
	nostrategy,
	"acpi",
	CDEV_MAJOR,
	nodump,
	nopsize,
	0,
	-1
};

/* 
 * Miscellaneous utility functions 
 */
static void acpi_handle_dsdt(acpi_softc_t *sc);
static void acpi_handle_facp(acpi_softc_t *sc, struct ACPIsdt *facp);
static int  acpi_handle_rsdt(acpi_softc_t *sc);

/* 
 * System sleeping state
 */
static void acpi_trans_sleeping_state(acpi_softc_t *sc, u_int8_t state);
static void acpi_soft_off(void *data, int howto);
static void acpi_execute_pts(acpi_softc_t *sc, u_int8_t state);
static void acpi_execute_wak(acpi_softc_t *sc, u_int8_t state);

/*
 * Bus interface 
 */
static void acpi_identify(driver_t *driver, device_t parent);
static int  acpi_probe(device_t dev);
static int  acpi_attach(device_t dev);
static void acpi_free(struct acpi_softc *sc);

/* for debugging */
#ifdef ACPI_DEBUG
int	acpi_debug = 1;
#else	/* !ACPI_DEBUG */
int	acpi_debug = 0;
#endif	/* ACPI_DEBUG */

SYSCTL_INT(_debug, OID_AUTO, acpi_debug, CTLFLAG_RW, &acpi_debug, 1, "");

/*
 * ACPI pmap subsystem
 */
void
acpi_init_addr_range(void)
{
	acpi_addr.entries = 0;
}

void
acpi_register_addr_range(u_int64_t base, u_int64_t size, u_int32_t type)
{
	int		i;
	u_int32_t	ext_size;
	vm_offset_t	pa_base, pa_next_base;

	if (acpi_addr.entries == ACPI_SMAP_MAX_SIZE) {
		return;		/* no room */
	}

	for (i = 0; i < acpi_addr.entries; i++) {
		if (type != acpi_addr.t[i].type) {
			continue;
		}

		pa_base = acpi_addr.t[i].pa_base;
		pa_next_base = pa_base + acpi_addr.t[i].size;

		/* continuous or overlap? */
		if (base > pa_base && base <= pa_next_base) {
			ext_size = size - (pa_next_base - base);
			acpi_addr.t[i].size += ext_size;
			return;
		}
	}

	i = acpi_addr.entries;
	acpi_addr.t[i].pa_base = base;
	acpi_addr.t[i].size = size;
	acpi_addr.t[i].type = type;
	acpi_addr.entries++;
}

vm_offset_t
acpi_pmap_ptv(vm_offset_t pa)
{
	int		i;
	vm_offset_t	va;

	va = 0;
	for (i = 0; i < acpi_addr.entries; i++) {
		if (pa >= acpi_addr.t[i].pa_base &&
		    pa < acpi_addr.t[i].pa_base + acpi_addr.t[i].size) {
			va = acpi_addr.t[i].va_base + pa - acpi_addr.t[i].pa_base;
			return (va);
		}
	}

	return (va);
}

vm_offset_t
acpi_pmap_vtp(vm_offset_t va)
{
	int		i;
	vm_offset_t	pa;

	pa = 0;
	for (i = 0; i < acpi_addr.entries; i++) {
		if (va >= acpi_addr.t[i].va_base &&
		    va < acpi_addr.t[i].va_base + acpi_addr.t[i].size) {
			pa = acpi_addr.t[i].pa_base + va - acpi_addr.t[i].va_base;
			return (pa);
		}
	}

	return (pa);
}

/*
 * Miscellaneous utility functions
 */
int
acpi_sdt_checksum(struct ACPIsdt *sdt)
{
	u_char	cksm, *ckbf;
	int	i;

	cksm = 0;
	ckbf = (u_char *) sdt;

	for (i = 0; i < sdt->len; i++)
		cksm += ckbf[i];

	return ((cksm == 0) ? 0 : EINVAL);
}

/*
 * Handle the DSDT
 */
static void
acpi_handle_dsdt(acpi_softc_t *sc)
{
	int					i;
	int					debug;
	struct aml_name				*newname, *sname;
	union aml_object			*spkg;
	struct aml_name_group			*newgrp;
	struct ACPIsdt				*dsdp;
	char					namestr[5];
	struct aml_environ			env;
	struct acpi_system_state_package	ssp;

	/*
	 * Some systems (eg. IBM laptops) expect "Microsoft Windows*" as
	 * \_OS_ string, so we create it anyway.
	 */
	aml_new_name_group(AML_NAME_GROUP_OS_DEFINED);
	env.curname = aml_get_rootname();
	newname = aml_create_name(&env, "\\_OS_");
	newname->property = aml_alloc_object(aml_t_string, NULL);
	newname->property->str.needfree = 0;
	newname->property->str.string = "Microsoft Windows NG";

	/* 
	 * Create namespace. 
	 */
	dsdp = sc->dsdt;
	newgrp = aml_new_name_group((int)dsdp->body);

	bzero(&env, sizeof(env));
	env.dp = (u_int8_t *)dsdp->body;
	env.end = (u_int8_t *)dsdp + dsdp->len;
	env.curname = aml_get_rootname();

	/* 
	 * Suppress debugging during AML parsing.
	 */
	debug = aml_debug;
	aml_debug = 0;

	aml_local_stack_push(aml_local_stack_create());
	aml_parse_objectlist(&env, 0);
	aml_local_stack_delete(aml_local_stack_pop());

	aml_debug = debug;

	if (aml_debug) {
		aml_showtree(aml_get_rootname(), 0);
	}

	/* 
	 * Get sleeping type values from ACPI namespace.
	 */
	sc->system_state = ACPI_S_STATE_S0;
	sc->system_state_initialized = 1;
	for (i = ACPI_S_STATE_S0; i <= ACPI_S_STATE_S5; i++) {
		ssp.mode[i].slp_typ_a = ACPI_UNSUPPORTSLPTYP;
		ssp.mode[i].slp_typ_b = ACPI_UNSUPPORTSLPTYP;
		sprintf(namestr, "_S%d_", i);
		sname = aml_find_from_namespace(NULL, namestr);
		if (sname == NULL) {
			continue;
		}
		spkg = aml_eval_name(&env, sname);
		if (spkg == NULL || spkg->type != aml_t_package) {
			continue;
		}
		if (spkg->package.elements < 2) {
			continue;
		}
		if (spkg->package.objects[0] == NULL ||
		    spkg->package.objects[0]->type != aml_t_num) {
			continue;
		}
		ssp.mode[i].slp_typ_a = spkg->package.objects[0]->num.number;
		ssp.mode[i].slp_typ_b = spkg->package.objects[1]->num.number;
		ACPI_DEBUGPRINT("%s : [%d, %d]\n", namestr,
				ssp.mode[i].slp_typ_a, ssp.mode[i].slp_typ_b);
	}
	sc->system_state_package = ssp;

#if 0
	while (name_group_list->id != AML_NAME_GROUP_ROOT) {
		aml_delete_name_group(name_group_list);
	}
	memman_statistics(aml_memman);
	memman_freeall(aml_memman);
#endif
}

/*
 * Handle the FACP.
 */
static void
acpi_handle_facp(acpi_softc_t *sc, struct ACPIsdt *facp)
{
	struct ACPIsdt	*dsdt;
	struct FACPbody	*body;
	struct FACS	*facs;

	ACPI_DEBUGPRINT("	FACP found\n");
	body = (struct FACPbody *)facp->body;
	sc->facp = facp;
	sc->facp_body = body;
	sc->dsdt = NULL;
	sc->facs = NULL;
	dsdt = (struct ACPIsdt *) acpi_pmap_ptv(body->dsdt_ptr);
	if (dsdt == NULL) {
		return;
	}

	if (strncmp(dsdt->signature, "DSDT", 4) == 0 &&
	    acpi_sdt_checksum(dsdt) == 0) {
		ACPI_DEBUGPRINT("	DSDT found Size=%d bytes\n", dsdt->len);
		sc->dsdt = dsdt;
		acpi_handle_dsdt(sc);
	}

	facs = (struct FACS *) acpi_pmap_ptv(body->facs_ptr);
	if (facs == NULL) {
		return;
	}

	/*
	 * FACS has no checksum (modified by both OS and BIOS) and in many
	 * cases, it is in nonvolatile storage.
	 */
	if (strncmp(facs->signature, "FACS", 4) == 0) {
		sc->facs = facs;
		ACPI_DEBUGPRINT("	FACS found Size=%d bytes\n", facs->len);
	}
}

/*
 * Handle the RSDT.
 */
static int
acpi_handle_rsdt(acpi_softc_t *sc)
{
	u_int32_t	*ptrs;
	int		entries;
	int		i;
	struct ACPIsdt	*rsdt, *sdt;
	char		sigstring[5];

	rsdt = (struct ACPIsdt *) acpi_pmap_ptv(acpi_rsdp->addr);
	if (rsdt == 0) {
		ACPI_DEVPRINTF("cannot map physical memory\n");
		return (-1);
	}
	if ((strncmp(rsdt->signature, "RSDT", 4) != 0) ||
	    (acpi_sdt_checksum(rsdt) != 0)) {
		ACPI_DEVPRINTF("RSDT is broken\n");
		return (-1);
	}
	sc->rsdt = rsdt;
	entries = (rsdt->len - SIZEOF_SDT_HDR) / sizeof(u_int32_t);
	ACPI_DEBUGPRINT("RSDT have %d entries\n", entries);
	ptrs = (u_int32_t *) & rsdt->body;

	for (i = 0; i < entries; i++) {
		sdt = (struct ACPIsdt *) acpi_pmap_ptv((vm_offset_t) ptrs[i]);
		bzero(sigstring, sizeof(sigstring));
		strncpy(sigstring, sdt->signature, sizeof(sdt->signature));
		ACPI_DEBUGPRINT("RSDT entry%d %s\n", i, sigstring);

		if (strncmp(sdt->signature, "FACP", 4) == 0 &&
		    acpi_sdt_checksum(sdt) == 0) {
			acpi_handle_facp(sc, sdt);
		}
	}
	return (0);
}

/*
 * System sleeping state.
 */
static void
acpi_trans_sleeping_state(acpi_softc_t *sc, u_int8_t state)
{
	u_int8_t	slp_typx;
	u_int32_t	val_a, val_b;
	int		debug, count;
	u_long		ef;

	/* XXX should be MI */
	/* XXX should always be called with interrupts enabled! */
	ef = read_eflags();
	disable_intr();

	if (state > ACPI_S_STATE_S0) {
		/* clear WAK_STS bit by writing a one */
		acpi_io_pm1_status(sc, ACPI_REGISTER_INPUT, &val_a);
		if (val_a & ACPI_PM1_WAK_STS) {
			sc->broken_wakeuplogic = 0;
		} else {
			ACPI_DEVPRINTF("wake-up logic seems broken, "
				       "this may cause troubles on wakeup\n");
			sc->broken_wakeuplogic = 1;
		}
		val_a = ACPI_PM1_WAK_STS;
		acpi_io_pm1_status(sc, ACPI_REGISTER_OUTPUT, &val_a);

		/* ignore power button and sleep button events for 5 sec. */
		sc->ignore_events = ACPI_PM1_PWRBTN_EN | ACPI_PM1_SLPBTN_EN;
		timeout(acpi_clear_ignore_events, (caddr_t)sc, hz * 5);
	}

	acpi_io_pm1_control(sc, ACPI_REGISTER_INPUT, &val_a, &val_b);
	val_a  &= ~(ACPI_CNT_SLP_TYPX);
	val_b  &= ~(ACPI_CNT_SLP_TYPX);
	slp_typx = sc->system_state_package.mode[state].slp_typ_a;
	val_a  |= ACPI_CNT_SET_SLP_TYP(slp_typx) | ACPI_CNT_SLP_EN;
	slp_typx = sc->system_state_package.mode[state].slp_typ_b;
	val_b  |= ACPI_CNT_SET_SLP_TYP(slp_typx) | ACPI_CNT_SLP_EN;
	acpi_io_pm1_control(sc, ACPI_REGISTER_OUTPUT, &val_a, &val_b);

	if (state == ACPI_S_STATE_S0) {
		goto sleep_done;
	}

	/*
	 * wait for WAK_STS bit
	 */
	debug = acpi_debug;	/* Save debug level */
	acpi_debug = 0;		/* Shut up */

	count = 0;
	for (;;) {
		acpi_io_pm1_status(sc, ACPI_REGISTER_INPUT, &val_a);
		if (val_a & ACPI_PM1_WAK_STS) {
			break;
		}
		/* XXX
		 * some BIOSes doesn't set WAK_STS at all,
		 * give up waiting for wakeup if timeout...
		 */
		if (sc->broken_wakeuplogic) {
			if (count++ >= 100) {
				break;		/* giving up */
			}
		}
		DELAY(10*1000);			/* 0.01 sec */
	}
	acpi_debug = debug;	/* Restore debug level */

sleep_done:
	/* XXX should be MI */
	write_eflags(ef);
}

static void
acpi_soft_off(void *data, int howto)
{
	acpi_softc_t	*sc;
	u_int32_t	vala = 0;

	if (!(howto & RB_POWEROFF)) {
		return;
	}
	sc = (acpi_softc_t *) data;
	acpi_execute_pts(sc, ACPI_S_STATE_S5);

	/* XXX Disable GPE intrrupt,or power on again in some machine */
	acpi_io_gpe0_enable(sc, ACPI_REGISTER_OUTPUT, &vala);
	acpi_io_gpe1_enable(sc, ACPI_REGISTER_OUTPUT, &vala);

	acpi_trans_sleeping_state(sc, ACPI_S_STATE_S5);
}

void
acpi_set_sleeping_state(acpi_softc_t *sc, u_int8_t state)
{
	u_int8_t	slp_typ_a, slp_typ_b;

	if (!sc->system_state_initialized) {
		return;
	}

	slp_typ_a = sc->system_state_package.mode[state].slp_typ_a;
	slp_typ_b = sc->system_state_package.mode[state].slp_typ_b;

	if (slp_typ_a == ACPI_UNSUPPORTSLPTYP ||
	    slp_typ_b == ACPI_UNSUPPORTSLPTYP) {
		return;	/* unsupported sleeping type */
	}

	if (state < ACPI_S_STATE_S5) {
		/* inform all devices that we are going to sleep. */
		if (acpi_send_pm_event(sc, state) != 0) {
			/* if failure, 'wakeup' the system again */
			acpi_send_pm_event(sc, ACPI_S_STATE_S0);
			return;
		}

		/* Prepare to sleep */
		acpi_execute_pts(sc, state);

		/* PowerResource manipulation */
		acpi_powerres_set_sleeping_state(sc, state);
		if (acpi_debug) {
			acpi_powerres_debug(sc);
		}

		sc->system_state = state;
	}

	/*
	 * XXX currently support S1 and S5 only.
	 */
	switch (state) {
	case ACPI_S_STATE_S0:
	case ACPI_S_STATE_S1:
		acpi_trans_sleeping_state(sc, state);
		break;
	case ACPI_S_STATE_S5:
		/* Power the system off using ACPI */
		shutdown_nice(RB_POWEROFF);
		break;
	default:
		break;
	}

	if (state < ACPI_S_STATE_S5) {
		acpi_powerres_set_sleeping_state(sc, 0);
		if (acpi_debug) {
			acpi_powerres_debug(sc);
		}
		acpi_execute_wak(sc, state);
		acpi_send_pm_event(sc, ACPI_S_STATE_S0);

		sc->system_state = ACPI_S_STATE_S0;
	}
}

static void
acpi_execute_pts(acpi_softc_t *sc, u_int8_t state)
{
	union aml_object	argv[1], *retval;

	argv[0].type = aml_t_num;
	argv[0].num.number = state;
	aml_local_stack_push(aml_local_stack_create());
	retval = aml_invoke_method_by_name("_PTS", 1, argv);
	aml_local_stack_delete(aml_local_stack_pop());
}

static void
acpi_execute_wak(acpi_softc_t *sc, u_int8_t state)
{
	union aml_object	argv[1], *retval;

	argv[0].type = aml_t_num;
	argv[0].num.number = state;
	aml_local_stack_push(aml_local_stack_create());
	retval = aml_invoke_method_by_name("_WAK", 1, argv);
	aml_local_stack_delete(aml_local_stack_pop());

	/* 
	 * XXX These shouldn't be here, but tentatively implemented.
	 * Sample application of aml_apply_foreach_found_objects().
	 * We try to find and evaluate all of objects which have specified
	 * string. As the result, Battery Information, Battery Status and
	 * Power source will be reported.
	 */
	aml_apply_foreach_found_objects(NULL, "_BIF", aml_eval_name_simple);
	aml_apply_foreach_found_objects(NULL, "_BST", aml_eval_name_simple);
	aml_apply_foreach_found_objects(NULL, "_PSR", aml_eval_name_simple);
}

/*
 * Character device
 */

static int
acpiopen(dev_t dev, int flag, int fmt, struct proc * p)
{
	return (0);
}

static int
acpiclose(dev_t dev, int flag, int fmt, struct proc * p)
{
	return (0);
}

static int
acpiioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc * p)
{
	int		error, state;
	acpi_softc_t	*sc = (struct acpi_softc *)dev->si_drv1;

	error = 0;

	if (sc == NULL) {
		return (EINVAL);
	}

	switch (cmd) {
	case ACPIIO_ENABLE:
		acpi_enable_disable(sc, 1);
		acpi_enable_events(sc);
		break;

	case ACPIIO_DISABLE:
		acpi_enable_disable(sc, 0);
		break;

	case ACPIIO_SETSLPSTATE:
		state = *(int *)addr;
		if (state >= ACPI_S_STATE_S0 && state <= ACPI_S_STATE_S5) {
			acpi_set_sleeping_state(sc, state);
		} else {
			error = EINVAL;
		}
		ACPI_DEBUGPRINT("ACPIIO_SETSLPSTATE = %d\n", state);
		break;

	default:
		error = EINVAL;
		break;
	}

	return (error);
}

static int
acpimmap(dev_t dev, vm_offset_t offset, int nprot)
{
	acpi_softc_t	*sc = (struct acpi_softc *)dev->si_drv1;

	if (sc == NULL) {
		return (EINVAL);
	}
	/* XXX should be MI */
	return (i386_btop(acpi_pmap_vtp((vm_offset_t)(sc->dsdt + offset))));
}

/*
 * Bus interface
 */
static devclass_t acpi_devclass;

static void
acpi_identify(driver_t *driver, device_t parent)
{
    device_t		child;
    struct ACPIrsdp	*rsdp;

    /*
     * If we've already got ACPI attached somehow, don't try again.
     */
    if (device_find_child(parent, "acpi", 0)) {
	printf("ACPI: already attached\n");
	return;
    }

    /*
     * Ask the MD code to find the ACPI RSDP
     */
    if ((rsdp = acpi_find_rsdp()) == NULL)
	return;
    acpi_rsdp = rsdp;

    /*
     * Call the MD code to map memory claimed by ACPI
     */
    acpi_mapmem();

    /*
     * Attach the actual ACPI device.
     */
    if ((child = BUS_ADD_CHILD(parent, 101, "acpi", 0)) == NULL) {
	    device_printf(parent, "ACPI: could not attach\n");
	    return;
    }
}

static int
acpi_probe(device_t dev)
{
	int	debug;
	char	oemstring[7];

	/* get debug variables specified in loader. */
	if (getenv_int("debug.acpi_debug", &debug)) {
		acpi_debug = debug;
	}
	if (getenv_int("debug.aml_debug", &debug)) {
		aml_debug = debug;
	}

	bzero(oemstring, sizeof(oemstring));
	strncpy(oemstring, acpi_rsdp->oem, sizeof(acpi_rsdp->oem));

	ACPI_DEBUGPRINT("Found ACPI BIOS data at %p (<%s>, RSDT@%x)\n",
			acpi_rsdp, oemstring, acpi_rsdp->addr);

	device_set_desc_copy(dev, oemstring);
	return (0);
}

static int
acpi_attach(device_t dev)
{
	acpi_softc_t	*sc;
	int		rid;

	/*
	 * Set up the softc and parse the ACPI data completely.
	 */
	sc = device_get_softc(dev);
	sc->dev = dev;
	if (acpi_handle_rsdt(sc) != 0) {
		acpi_free(sc);
		return (ENXIO);
	}

	/*
	 * SMI command register
	 */
	rid = ACPI_RES_SMI_CMD;
	acpi_attach_resource(sc, SYS_RES_IOPORT, &rid,
			     sc->facp_body->smi_cmd, 1);

	/*
	 * PM1 event registers
	 */
	rid = ACPI_RES_PM1A_EVT;
	acpi_attach_resource(sc, SYS_RES_IOPORT, &rid,
			     sc->facp_body->pm1a_evt_blk, sc->facp_body->pm1_evt_len);
	if (sc->facp_body->pm1b_evt_blk != 0) {
		rid = ACPI_RES_PM1B_EVT;
		acpi_attach_resource(sc, SYS_RES_IOPORT, &rid,
				     sc->facp_body->pm1b_evt_blk, sc->facp_body->pm1_evt_len);
	}

	/*
	 * PM1 control registers
	 */
	rid = ACPI_RES_PM1A_CNT;
	acpi_attach_resource(sc, SYS_RES_IOPORT, &rid,
			     sc->facp_body->pm1a_cnt_blk, sc->facp_body->pm1_cnt_len);
	if (sc->facp_body->pm1b_cnt_blk != 0) {
		rid = ACPI_RES_PM1B_CNT;
		acpi_attach_resource(sc, SYS_RES_IOPORT, &rid,
				     sc->facp_body->pm1b_cnt_blk, sc->facp_body->pm1_cnt_len);
	}

	/*
	 * PM2 control register
	 */
	rid = ACPI_RES_PM2_CNT;
	acpi_attach_resource(sc, SYS_RES_IOPORT, &rid,
			     sc->facp_body->pm2_cnt_blk, sc->facp_body->pm2_cnt_len);

	/*
	 * PM timer register
	 */
	rid = ACPI_RES_PM_TMR;
	acpi_attach_resource(sc, SYS_RES_IOPORT, &rid,
			     sc->facp_body->pm_tmr_blk, 4);

	/*
	 * General purpose event registers
	 */
	if (sc->facp_body->gpe0_blk != 0) {
		rid = ACPI_RES_GPE0;
		acpi_attach_resource(sc, SYS_RES_IOPORT, &rid,
				     sc->facp_body->gpe0_blk, sc->facp_body->gpe0_len);
	}
	if (sc->facp_body->gpe1_blk != 0) {
		rid = ACPI_RES_GPE1;
		acpi_attach_resource(sc, SYS_RES_IOPORT, &rid,
				     sc->facp_body->gpe1_blk, sc->facp_body->gpe1_len);
	}

	/*
	 * Notification interrupt
	 */
	if (sc->facp_body->sci_int != 0)
		bus_set_resource(sc->dev, SYS_RES_IRQ, 0, sc->facp_body->sci_int, 1);
	sc->irq_rid = 0;
	if ((sc->irq = bus_alloc_resource(sc->dev, SYS_RES_IRQ, &sc->irq_rid, 
					  0, ~0, 1, RF_SHAREABLE | RF_ACTIVE)) == NULL) {
		ACPI_DEVPRINTF("could not allocate interrupt\n");
		acpi_free(sc);
		return(ENOMEM);
	}
	if (bus_setup_intr(sc->dev, sc->irq, INTR_TYPE_MISC, acpi_intr, sc, &sc->irq_handle)) {
		ACPI_DEVPRINTF("could not set up irq\n");
		acpi_free(sc);
		return(ENXIO);
	}

	/* initialise the event queue */
	STAILQ_INIT(&sc->event);

#ifndef ACPI_NO_ENABLE_ON_BOOT
	acpi_enable_disable(sc, 1);
	acpi_enable_events(sc);
	acpi_intr((void *)sc);
#endif


	acpi_powerres_init(sc);
	if (acpi_debug) {
		acpi_powerres_debug(sc);
	}

	EVENTHANDLER_REGISTER(shutdown_pre_sync, acpi_disable_events,
			      sc, SHUTDOWN_PRI_LAST);
	EVENTHANDLER_REGISTER(shutdown_final, acpi_soft_off,
			      sc, SHUTDOWN_PRI_LAST);

	sc->dev_t = make_dev(&acpi_cdevsw, 0, 0, 5, 0660, "acpi");
	sc->dev_t->si_drv1 = sc;
	
	/*
	 * Probe/attach children now that the AML has been parsed.
	 */
	bus_generic_probe(dev);
	bus_generic_attach(dev);

	/*
	 * Start the eventhandler thread
	 */
	if (kthread_create(acpi_event_thread, sc, &sc->acpi_thread, 0, "acpi")) {
		ACPI_DEVPRINTF("CANNOT CREATE THREAD\n");
	}
	return (0);
}

static int
acpi_detach(device_t dev)
{
	acpi_softc_t	*sc = device_get_softc(dev);
	
	/* acpi_disable_events(sc); */
	acpi_enable_disable(sc, 0);
	acpi_free(sc);

	return(0);
}

static void
acpi_free(struct acpi_softc *sc)
{
	int	i;

	for (i = 0; i < ACPI_RES_MAX; i++) {
		if (sc->iores[i].rsc != NULL) {
			bus_release_resource(sc->dev, 
					     SYS_RES_IOPORT, 
					     sc->iores[i].rid,
					     sc->iores[1].rsc);
		}
	}
	if (sc->irq_handle != NULL)
		bus_teardown_intr(sc->dev, sc->irq, sc->irq_handle);
	if (sc->irq != NULL)
		bus_release_resource(sc->dev, SYS_RES_IRQ, sc->irq_rid, sc->irq);
}

static int
acpi_resume(device_t dev)
{
	acpi_softc_t	*sc;

	sc = device_get_softc(dev);
	if (sc->enabled) {
		/* re-enable on wakeup */
		acpi_enable_disable(sc, 1);
		acpi_enable_events(sc);
	}
	return (0);
}

static device_method_t acpi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	acpi_identify),
	DEVMETHOD(device_probe,		acpi_probe),
	DEVMETHOD(device_attach,	acpi_attach),
	DEVMETHOD(device_resume,	acpi_resume),
	DEVMETHOD(device_detach,	acpi_detach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	{0, 0}
};

static driver_t acpi_driver = {
	"acpi",
	acpi_methods,
	sizeof(acpi_softc_t),
};

DRIVER_MODULE(acpi, nexus, acpi_driver, acpi_devclass, 0, 0);

int
acpi_attach_resource(acpi_softc_t *sc, int type, int *wantidx, u_long start, u_long size)
{
	int		i, idx;

	/*
	 * The caller is unaware of the softc, so find it.
	 */
	if (sc == NULL)
		sc = devclass_get_softc(acpi_devclass, 0);
	if (sc == NULL)
		return(ENXIO);

	/* 
	 * The caller wants an automatic index
	 */
	idx = *wantidx;
	if (idx == ACPI_RES_AUTO) {
		for (i = ACPI_RES_FIRSTFREE; i < ACPI_RES_MAX; i++) {
			if (sc->iores[i].rsc == NULL)
				break;
		}
		if (i == ACPI_RES_MAX)
			return(ENOMEM);
		idx = i;
	}

	/*
	 * Connect the resource to ourselves.
	 */
	bus_set_resource(sc->dev, type, idx, start, size);
	sc->iores[idx].rid = idx;
	sc->iores[idx].size = size;
	sc->iores[idx].rsc = bus_alloc_resource(sc->dev, type, &sc->iores[idx].rid, 0, ~0, 1, RF_ACTIVE);
	if (sc->iores[idx].rsc != NULL) {
		sc->iores[idx].bhandle = rman_get_bushandle(sc->iores[idx].rsc);
		sc->iores[idx].btag = rman_get_bustag(sc->iores[idx].rsc);
		*wantidx = idx;
		return(0);
	} else {
		return(ENXIO);
	}
}

/*
 * System service interface
 */

#include <sys/proc.h>

int
acpi_sleep(u_int32_t micro)
{
	static u_int8_t	count = 0;
	int		x, error;
	u_int32_t	timo;

	x = error = 0;

	if (micro == 0) {
		return (1);
	}

	if (curproc == NULL) {
		return (2);
	}

	timo = ((hz * micro) / 1000000L) ? ((hz * micro) / 1000000L) : 1;
	error = tsleep((caddr_t)acpi_sleep + count, PWAIT, "acpislp", timo);
	if (error != 0 && error != EWOULDBLOCK) {
		return (2);
	}
	x = splhigh();
	count++;
	splx(x);

	return (0);
}

