/*-
 * Copyright (c) 2000 Mitsuru IWASAKI <iwasaki@jp.freebsd.org>
 * Copyright (c) 2000 Michael Smith <msmith@freebsd.org>
 * Copyright (c) 2000 BSDi
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

#include "bus_if.h"
#include <sys/eventhandler.h>
#include <sys/sysctl.h>
#if __FreeBSD_version >= 500000
#include <sys/lock.h>
#include <sys/mutex.h>
#endif

#include <machine/bus.h>
#include <machine/resource.h>

#if __FreeBSD_version < 500000
typedef vm_offset_t vm_paddr_t;
#endif

struct acpi_softc {
    device_t		acpi_dev;
    dev_t		acpi_dev_t;

    struct resource	*acpi_irq;
    int			acpi_irq_rid;
    void		*acpi_irq_handle;

    int			acpi_enabled;
    int			acpi_sstate;
    int			acpi_sleep_disabled;

    struct sysctl_ctx_list acpi_sysctl_ctx;
    struct sysctl_oid	*acpi_sysctl_tree;
#define ACPI_POWER_BUTTON_DEFAULT_SX	ACPI_STATE_S5;
#define ACPI_SLEEP_BUTTON_DEFAULT_SX	ACPI_STATE_S1;
#define ACPI_LID_SWITCH_DEFAULT_SX	ACPI_STATE_S1;
    int			acpi_power_button_sx;
    int			acpi_sleep_button_sx;
    int			acpi_lid_switch_sx;

    int			acpi_standby_sx;
    int			acpi_suspend_sx;

    int			acpi_sleep_delay;
    int			acpi_s4bios;
    int			acpi_disable_on_poweroff;

    int			acpi_verbose;

    bus_dma_tag_t	acpi_waketag;
    bus_dmamap_t	acpi_wakemap;
    vm_offset_t		acpi_wakeaddr;
    vm_paddr_t		acpi_wakephys;

    struct sysctl_ctx_list	 acpi_battery_sysctl_ctx;
    struct sysctl_oid		*acpi_battery_sysctl_tree;
};

struct acpi_device {
    /* ACPI ivars */
    ACPI_HANDLE			ad_handle;
    int				ad_magic;
    void			*ad_private;

    /* resources */
    struct resource_list	ad_rl;

};

#if __FreeBSD_version < 500000
/*
 * In 4.x, ACPI is protected by splhigh().
 */
# define ACPI_LOCK			s = splhigh()
# define ACPI_UNLOCK			splx(s)
# define ACPI_ASSERTLOCK
# define ACPI_MSLEEP(a, b, c, d, e)	tsleep(a, c, d, e)
# define ACPI_LOCK_DECL			int s
# define kthread_create(a, b, c, d, e, f)	kthread_create(a, b, c, f)
# define tc_init(a)			init_timecounter(a)
#else
# define ACPI_LOCK
# define ACPI_UNLOCK
# define ACPI_ASSERTLOCK
# define ACPI_LOCK_DECL
#endif

/*
 * ACPI CA does not define layers for non-ACPI CA drivers.
 * We define some here within the range provided.
 */
#define	ACPI_BUS		0x00010000
#define	ACPI_SYSTEM		0x00020000
#define	ACPI_POWER		0x00040000
#define	ACPI_EC			0x00080000
#define	ACPI_AC_ADAPTER		0x00100000
#define	ACPI_BATTERY		0x00110000
#define	ACPI_BUTTON		0x00120000
#define	ACPI_PROCESSOR		0x00140000
#define	ACPI_THERMAL		0x00180000
#define	ACPI_FAN		0x00200000

/*
 * Constants for different interrupt models used with acpi_SetIntrModel().
 */
#define	ACPI_INTR_PIC		0
#define	ACPI_INTR_APIC		1
#define	ACPI_INTR_SAPIC		2

/*
 * Note that the low ivar values are reserved to provide
 * interface compatibility with ISA drivers which can also
 * attach to ACPI.
 */
#define ACPI_IVAR_HANDLE	0x100
#define ACPI_IVAR_MAGIC		0x101
#define ACPI_IVAR_PRIVATE	0x102

extern ACPI_HANDLE	acpi_get_handle(device_t dev);
extern int		acpi_set_handle(device_t dev, ACPI_HANDLE h);
extern int		acpi_get_magic(device_t dev);
extern int		acpi_set_magic(device_t dev, int m);
extern void *		acpi_get_private(device_t dev);
extern int		acpi_set_private(device_t dev, void *p);
extern ACPI_OBJECT_TYPE	acpi_get_type(device_t dev);
struct resource *	acpi_bus_alloc_gas(device_t dev, int *rid,
					   ACPI_GENERIC_ADDRESS *gas);

#ifdef ACPI_DEBUGGER
extern void		acpi_EnterDebugger(void);
#endif

#ifdef ACPI_DEBUG
#include <sys/cons.h>
#define STEP(x)		do {printf x, printf("\n"); cngetc();} while (0)
#else
#define STEP(x)
#endif

#define ACPI_VPRINT(dev, acpi_sc, x...) do {			\
    if (acpi_get_verbose(acpi_sc))				\
	device_printf(dev, x);					\
} while (0)

#define ACPI_DEVINFO_PRESENT(x)	(((x) & 0x9) == 9)
extern BOOLEAN		acpi_DeviceIsPresent(device_t dev);
extern BOOLEAN		acpi_BatteryIsPresent(device_t dev);
extern BOOLEAN		acpi_MatchHid(device_t dev, char *hid);
extern ACPI_STATUS	acpi_GetHandleInScope(ACPI_HANDLE parent, char *path,
					      ACPI_HANDLE *result);
extern ACPI_BUFFER	*acpi_AllocBuffer(int size);
extern ACPI_STATUS	acpi_EvaluateInteger(ACPI_HANDLE handle, char *path,
					     int *number);
extern ACPI_STATUS	acpi_ConvertBufferToInteger(ACPI_BUFFER *bufp,
						    int *number);
extern ACPI_STATUS	acpi_ForeachPackageObject(ACPI_OBJECT *obj, 
				void (*func)(ACPI_OBJECT *comp, void *arg),
				void *arg);
extern ACPI_STATUS	acpi_FindIndexedResource(ACPI_BUFFER *buf, int index,
						 ACPI_RESOURCE **resp);
extern ACPI_STATUS	acpi_AppendBufferResource(ACPI_BUFFER *buf,
						  ACPI_RESOURCE *res);
extern ACPI_STATUS	acpi_OverrideInterruptLevel(UINT32 InterruptNumber);
extern ACPI_STATUS	acpi_SetIntrModel(int model);
extern ACPI_STATUS	acpi_SetSleepState(struct acpi_softc *sc, int state);
extern ACPI_STATUS	acpi_Enable(struct acpi_softc *sc);
extern ACPI_STATUS	acpi_Disable(struct acpi_softc *sc);
extern void		acpi_UserNotify(const char *subsystem, ACPI_HANDLE h,
					uint8_t notify);

struct acpi_parse_resource_set {
    void	(*set_init)(device_t dev, void **context);
    void	(*set_done)(device_t dev, void *context);
    void	(*set_ioport)(device_t dev, void *context, u_int32_t base,
			      u_int32_t length);
    void	(*set_iorange)(device_t dev, void *context,
			       u_int32_t low, u_int32_t high, 
			       u_int32_t length, u_int32_t align);
    void	(*set_memory)(device_t dev, void *context, u_int32_t base,
			      u_int32_t length);
    void	(*set_memoryrange)(device_t dev, void *context, u_int32_t low,
				   u_int32_t high, u_int32_t length,
				   u_int32_t align);
    void	(*set_irq)(device_t dev, void *context, u_int32_t *irq,
			   int count, int trig, int pol);
    void	(*set_drq)(device_t dev, void *context, u_int32_t *drq,
			   int count);
    void	(*set_start_dependant)(device_t dev, void *context,
				       int preference);
    void	(*set_end_dependant)(device_t dev, void *context);
};

extern struct acpi_parse_resource_set	acpi_res_parse_set;
extern ACPI_STATUS	acpi_parse_resources(device_t dev, ACPI_HANDLE handle,
			    struct acpi_parse_resource_set *set);
/* XXX until Intel fix this in their headers, based on NEXT_RESOURCE */
#define ACPI_RESOURCE_NEXT(Res) (ACPI_RESOURCE *)((UINT8 *)Res + Res->Length)

/* ACPI event handling */
extern UINT32	acpi_eventhandler_power_button_for_sleep(void *context);
extern UINT32	acpi_eventhandler_power_button_for_wakeup(void *context);
extern UINT32	acpi_eventhandler_sleep_button_for_sleep(void *context);
extern UINT32	acpi_eventhandler_sleep_button_for_wakeup(void *context);

#define ACPI_EVENT_PRI_FIRST      0
#define ACPI_EVENT_PRI_DEFAULT    10000
#define ACPI_EVENT_PRI_LAST       20000

typedef void (*acpi_event_handler_t)(void *, int);

EVENTHANDLER_DECLARE(acpi_sleep_event, acpi_event_handler_t);
EVENTHANDLER_DECLARE(acpi_wakeup_event, acpi_event_handler_t);

/* Device power control. */
extern ACPI_STATUS	acpi_pwr_switch_consumer(ACPI_HANDLE consumer,
						 int state);

/* Misc. */
static __inline struct acpi_softc *
acpi_device_get_parent_softc(device_t child)
{
    device_t	parent;

    parent = device_get_parent(child);
    if (parent == NULL)
	return (NULL);
    return (device_get_softc(parent));
}

static __inline int
acpi_get_verbose(struct acpi_softc *sc)
{
    if (sc)
	return (sc->acpi_verbose);
    return (0);
}

extern char	*acpi_name(ACPI_HANDLE handle);
extern int	acpi_avoid(ACPI_HANDLE handle);
extern int	acpi_disabled(char *subsys);
extern void	acpi_device_enable_wake_capability(ACPI_HANDLE h, int enable);
extern void	acpi_device_enable_wake_event(ACPI_HANDLE h);
extern int	acpi_machdep_init(device_t dev);
extern void	acpi_install_wakeup_handler(struct acpi_softc *sc);
extern int	acpi_sleep_machdep(struct acpi_softc *sc, int state);

/* Battery Abstraction. */
struct acpi_battinfo;
struct acpi_battdesc;

extern int	acpi_battery_register(int, int);
extern int	acpi_battery_get_battinfo(int, struct acpi_battinfo *);
extern int	acpi_battery_get_units(void);
extern int	acpi_battery_get_info_expire(void);
extern int	acpi_battery_get_battdesc(int, struct acpi_battdesc *);

extern int	acpi_cmbat_get_battinfo(int, struct acpi_battinfo *);

/* Embedded controller. */
extern void	acpi_ec_ecdt_probe(device_t);

/* AC adapter interface. */
extern int	acpi_acad_get_acline(int *);

/* Package manipulation convenience functions. */
#define ACPI_PKG_VALID(pkg, size)				\
    ((pkg) != NULL && (pkg)->Type == ACPI_TYPE_PACKAGE &&	\
     (pkg)->Package.Count >= (size))
int		acpi_PkgInt(device_t dev, ACPI_OBJECT *res, int idx,
			    ACPI_INTEGER *dst);
int		acpi_PkgInt32(device_t dev, ACPI_OBJECT *res, int idx,
			      uint32_t *dst);
int		acpi_PkgStr(device_t dev, ACPI_OBJECT *res, int idx, void *dst,
			    size_t size);
int		acpi_PkgGas(device_t dev, ACPI_OBJECT *res, int idx, int *rid,
			    struct resource **dst);

#if __FreeBSD_version >= 500000
#ifndef ACPI_MAX_THREADS
#define ACPI_MAX_THREADS	3
#endif
#if ACPI_MAX_THREADS > 0
#define ACPI_USE_THREADS
#endif
#endif

#ifdef ACPI_USE_THREADS
/* ACPI task kernel thread initialization. */
extern int	acpi_task_thread_init(void);
#endif
