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
 *	$FreeBSD$
 */

#include "opt_acpi.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/ctype.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#include <dev/acpi/aml/aml_env.h>
#include <dev/acpi/aml/aml_evalobj.h>

/* 
 * ACPI events
 */
static void acpi_process_event(acpi_softc_t *sc, u_int32_t status_e,
			       u_int32_t status_0, u_int32_t status_1);

/*
 * ACPI events
 */

static void
acpi_process_event(acpi_softc_t *sc, u_int32_t status_e,
		   u_int32_t status_0, u_int32_t status_1)
{
    int i;
	
    if (status_e & ACPI_PM1_PWRBTN_EN) {
	if (sc->ignore_events & ACPI_PM1_PWRBTN_EN) {
	    ACPI_DEBUGPRINT("PWRBTN event ingnored\n");
	} else {
#if 1
	    acpi_set_sleeping_state(sc, ACPI_S_STATE_S5);
#else
	    /*
	     * If there is ACPI userland daemon,
	     * this event should be passed to it
	     * so that the user can determine power policy.
	     */
	    acpi_queue_event(sc, ACPI_EVENT_TYPE_FIXEDREG, 0);
#endif
	}
    }

    if (status_e & ACPI_PM1_SLPBTN_EN) {
	if (sc->ignore_events & ACPI_PM1_SLPBTN_EN) {
	    ACPI_DEBUGPRINT("SLPBTN event ingnored\n");
	} else {
#if 1
	    acpi_set_sleeping_state(sc, ACPI_S_STATE_S1);
#else
	    acpi_queue_event(sc, ACPI_EVENT_TYPE_FIXEDREG, 1);
#endif
	}
    }
    for(i = 0; i < sc->facp_body->gpe0_len * 4; i++)
	if((status_0 & (1 << i)) && (sc->gpe0_mask & (1 << i)))
	    acpi_queue_event(sc, ACPI_EVENT_TYPE_GPEREG, i);
    for(i = 0; i < sc->facp_body->gpe1_len * 4 ; i++)
	if((status_1 & (1 << i)) && (sc->gpe1_mask & (1 << i)))
	    acpi_queue_event(sc, ACPI_EVENT_TYPE_GPEREG,
			     i + sc->facp_body->gpe1_base);
}

void
acpi_intr(void *data)
{
    u_int32_t	enable;
    u_int32_t	status_e, status_0, status_1;
    u_int32_t	val;
    int		debug;
    acpi_softc_t	*sc;

    sc = (acpi_softc_t *)data;
    debug = acpi_debug;	/* Save debug level */
    acpi_debug = 0;		/* Shut up */

    /* 
     * Power Management 1 Status Registers 
     */
    status_e = enable = 0;
    acpi_io_pm1_status(sc, ACPI_REGISTER_INPUT, &status_e);

    /* 
     * Get current interrupt mask
     */
    acpi_io_pm1_enable(sc, ACPI_REGISTER_INPUT, &enable);

    /* 
     * Disable events and re-enable again
     */
    if ((status_e & enable) != 0) {
	acpi_debug = debug;	/* OK, you can speak */

	ACPI_DEBUGPRINT("pm1_status intr CALLED\n");

	/* Disable all interrupt generation */
	val = enable & (~ACPI_PM1_ALL_ENABLE_BITS);
	acpi_io_pm1_enable(sc, ACPI_REGISTER_OUTPUT, &val);

	/* Clear interrupt status */
	val = enable & ACPI_PM1_ALL_ENABLE_BITS;
	acpi_io_pm1_status(sc, ACPI_REGISTER_OUTPUT, &val);

	/* Re-enable interrupt */
	acpi_io_pm1_enable(sc, ACPI_REGISTER_OUTPUT, &enable);

	acpi_debug = 0;		/* Shut up again */
    }

    /* 
     * General-Purpose Events 0 Status Registers
     */
    status_0 = enable = 0;
    acpi_io_gpe0_status(sc, ACPI_REGISTER_INPUT, &status_0);

    /* 
     * Get current interrupt mask 
     */
    acpi_io_gpe0_enable(sc, ACPI_REGISTER_INPUT, &enable);

    /* 
     * Disable events and re-enable again 
     */
    if ((status_0 & enable) != 0) {
	acpi_debug = debug;	/* OK, you can speak */

	ACPI_DEBUGPRINT("gpe0_status intr CALLED\n");

	/* Disable all interrupt generation */
	val = enable & ~status_0;
#if 0
	/* or should we disable all? */
	val = 0x0;
#endif
	acpi_io_gpe0_enable(sc, ACPI_REGISTER_OUTPUT, &val);
#if 0
	/* Clear interrupt status */
	val = enable;	/* XXX */
	acpi_io_gpe0_status(sc, ACPI_REGISTER_OUTPUT, &val);

	/* Re-enable interrupt */
	acpi_io_gpe0_enable(sc, ACPI_REGISTER_OUTPUT, &enable);

	acpi_debug = 0;		/* Shut up again */
#endif
    }

    /*
     * General-Purpose Events 1 Status Registers
     */
    status_1 = enable = 0;
    acpi_io_gpe1_status(sc, ACPI_REGISTER_INPUT, &status_1);

    /*
      Get current interrupt mask
    */
    acpi_io_gpe1_enable(sc, ACPI_REGISTER_INPUT, &enable);

    /*
     * Disable events and re-enable again
     */
    if ((status_1 & enable) != 0) {
	acpi_debug = debug;	/* OK, you can speak */

	ACPI_DEBUGPRINT("gpe1_status intr CALLED\n");

	/* Disable all interrupt generation */
	val = enable & ~status_1;
#if 0
	/* or should we disable all? */
	val = 0x0;
#endif
	acpi_io_gpe1_enable(sc, ACPI_REGISTER_OUTPUT, &val);

	/* Clear interrupt status */
	val = enable;	/* XXX */
	acpi_io_gpe1_status(sc, ACPI_REGISTER_OUTPUT, &val);

	/* Re-enable interrupt */
	acpi_io_gpe1_enable(sc, ACPI_REGISTER_OUTPUT, &enable);

	acpi_debug = 0;		/* Shut up again */
    }

    acpi_debug = debug;	/* Restore debug level */

    /* do something to handle the events... */
    acpi_process_event(sc, status_e, status_0, status_1);
}

static int
acpi_set_gpe_bits(struct aml_name *name, va_list ap)
{
    struct acpi_softc	*sc = va_arg(ap, struct acpi_softc *);
    int			*gpemask0 = va_arg(ap, int *);
    int			*gpemask1 = va_arg(ap, int *);
    int			gpenum;

#define XDIGITTONUM(c) ((isdigit(c)) ? ((c) - '0') : ('A' <= (c)&& (c) <= 'F') ? ((c) - 'A' + 10) : 0)

    if (isxdigit(name->name[2]) && isxdigit(name->name[3])) {
	gpenum = XDIGITTONUM(name->name[2]) * 16 + 
	    XDIGITTONUM(name->name[3]);
	ACPI_DEBUGPRINT("GPENUM %d %d \n", gpenum, sc->facp_body->gpe0_len * 4);
	if (gpenum < (sc->facp_body->gpe0_len * 4)) {
	    *gpemask0 |= (1 << gpenum);
	} else {
	    *gpemask1 |= (1 << (gpenum - sc->facp_body->gpe1_base));
	}
    }
    ACPI_DEBUGPRINT("GPEMASK %x %x\n", *gpemask0, *gpemask1);
    return 0;
}

void
acpi_enable_events(acpi_softc_t *sc)
{
    u_int32_t	status;
    u_int32_t	mask0, mask1;
    u_int32_t	flags;

    /*
     * Setup PM1 Enable Registers Fixed Feature Enable Bits (4.7.3.1.2)
     * based on flags field of Fixed ACPI Description Table (5.2.5).
     */
    acpi_io_pm1_enable(sc, ACPI_REGISTER_INPUT, &status);
    flags = sc->facp_body->flags;
    if ((flags & ACPI_FACP_FLAG_PWR_BUTTON) == 0) {
	status |= ACPI_PM1_PWRBTN_EN;
    }
    if ((flags & ACPI_FACP_FLAG_SLP_BUTTON) == 0) {
	status |= ACPI_PM1_SLPBTN_EN;
    }
    acpi_io_pm1_enable(sc, ACPI_REGISTER_OUTPUT, &status);

#if 1
    /*
     * XXX
     * This should be done based on level event handlers in
     * \_GPE scope (4.7.2.2.1.2).
     */

    mask0 = mask1 = 0;
    aml_apply_foreach_found_objects(NULL, "\\_GPE._L", acpi_set_gpe_bits,
				    sc, &mask0, &mask1);	/* XXX correct? */
    sc->gpe0_mask = mask0;
    sc->gpe1_mask = mask1;

    acpi_io_gpe0_enable(sc, ACPI_REGISTER_OUTPUT, &mask0);
    acpi_io_gpe1_enable(sc, ACPI_REGISTER_OUTPUT, &mask1);
#endif

    /* print all event status for debugging */
    acpi_io_pm1_status(sc, ACPI_REGISTER_INPUT, &status);
    acpi_io_pm1_enable(sc, ACPI_REGISTER_INPUT,  &status);
    acpi_io_gpe0_status(sc, ACPI_REGISTER_INPUT, &status);
    acpi_io_gpe0_enable(sc, ACPI_REGISTER_INPUT, &status);
    acpi_io_gpe1_status(sc, ACPI_REGISTER_INPUT, &status);
    acpi_io_gpe1_enable(sc, ACPI_REGISTER_INPUT, &status);
    acpi_io_pm1_control(sc, ACPI_REGISTER_INPUT,  &mask0, &mask1);
    acpi_io_pm2_control(sc, ACPI_REGISTER_INPUT,  &status);
    acpi_io_pm_timer(sc, ACPI_REGISTER_INPUT,  &status);
}

void
acpi_disable_events(acpi_softc_t *sc)
{
    u_int32_t	zero;

    if (sc->enabled) {
	zero = 0;
	acpi_io_pm1_enable(sc, ACPI_REGISTER_OUTPUT, &zero);
	acpi_io_gpe0_enable(sc, ACPI_REGISTER_OUTPUT, &zero);
	acpi_io_gpe1_enable(sc, ACPI_REGISTER_OUTPUT, &zero);
    }
}

void
acpi_clear_ignore_events(void *arg)
{
    ((acpi_softc_t *)arg)->ignore_events = 0;
    ACPI_DEBUGPRINT("ignore events cleared\n");
}

/*
 * Transition the rest of the system through state changes.
 */
int
acpi_send_pm_event(acpi_softc_t *sc, u_int8_t state)
{
    int	error;

    error = 0;
    switch (state) {
    case ACPI_S_STATE_S0:
	if (sc->system_state != ACPI_S_STATE_S0) {
	    DEVICE_RESUME(root_bus);
	}
	break;
    case ACPI_S_STATE_S1:
    case ACPI_S_STATE_S2:
    case ACPI_S_STATE_S3:
    case ACPI_S_STATE_S4:
	error = DEVICE_SUSPEND(root_bus);
	break;
    default:
	break;
    }

    return (error);
}

/*
 * Event-handler thread.
 */
void 
acpi_queue_event(acpi_softc_t *sc, int type, int arg)
{
    struct acpi_event	*ae;
    int			s;

    ae = malloc(sizeof(*ae), M_TEMP, M_NOWAIT);
    if(ae == NULL)
	panic("acpi_queue_event: can't allocate event");
	
    ae->ae_type = type;
    ae->ae_arg = arg;
    s = splhigh();
    STAILQ_INSERT_TAIL(&sc->event, ae, ae_q);
    splx(s);
    wakeup(&sc->event);
}

void 
acpi_event_thread(void *arg)
{
    acpi_softc_t	*sc = arg;
    int			s , gpe1_base = sc->facp_body->gpe1_base;
    u_int32_t		status,bit;
    struct acpi_event	*ae;
    const char		numconv[] = {'0','1','2','3','4','5','6','7',
				     '8','9','A','B','C','D','E','F',-1};
    char		gpemethod[] = "\\_GPE._LXX";
    union aml_object	argv;	/* Dummy*/

    while(1) {
	s = splhigh();
	if ((ae = STAILQ_FIRST(&sc->event)) == NULL) {
	    splx(s);
	    tsleep(&sc->event, PWAIT, "acpiev", 0);
	    continue;
	} else {
	    splx(s);
	}
	s = splhigh();
	STAILQ_REMOVE_HEAD_UNTIL(&sc->event, ae, ae_q);
	splx(s);
	switch(ae->ae_type) {
	case ACPI_EVENT_TYPE_GPEREG:
	    sprintf(gpemethod, "\\_GPE._L%c%c",
		    numconv[(ae->ae_arg / 0x10) & 0xf],
		    numconv[ae->ae_arg & 0xf]);
	    aml_invoke_method_by_name(gpemethod, 0, &argv);
	    sprintf(gpemethod, "\\_GPE._E%c%c",
		    numconv[(ae->ae_arg / 0x10) & 0xf],
		    numconv[ae->ae_arg & 0xf]);
	    aml_invoke_method_by_name(gpemethod, 0, &argv);
	    s=splhigh();
	    if((ae->ae_arg < gpe1_base) || (gpe1_base == 0)){
		bit = 1 << ae->ae_arg;
		ACPI_DEBUGPRINT("GPE0%x\n", bit);
		acpi_io_gpe0_status(sc, ACPI_REGISTER_OUTPUT,
				    &bit);
		acpi_io_gpe0_enable(sc, ACPI_REGISTER_INPUT,
				    &status);
		ACPI_DEBUGPRINT("GPE0%x\n", status);
		status |= bit;
		acpi_io_gpe0_enable(sc, ACPI_REGISTER_OUTPUT,
				    &status);
	    } else {
		bit = 1 << (ae->ae_arg - sc->facp_body->gpe1_base);
		acpi_io_gpe1_status(sc, ACPI_REGISTER_OUTPUT,
				    &bit);
		acpi_io_gpe1_enable(sc, ACPI_REGISTER_INPUT,
				    &status);
		status |= bit;
		acpi_io_gpe1_enable(sc, ACPI_REGISTER_OUTPUT,
				    &status);
	    }
	    splx(s);
	    break;
	}
	free(ae, M_TEMP);
    }
    ACPI_DEVPRINTF("????\n");
}
