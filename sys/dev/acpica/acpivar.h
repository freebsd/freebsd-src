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

#include <machine/bus.h>
#include <machine/resource.h>

extern devclass_t	acpi_devclass;

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

    int			acpi_s4bios;

    int			acpi_verbose;

    bus_dma_tag_t	acpi_waketag;
    bus_dmamap_t	acpi_wakemap;
    vm_offset_t		acpi_wakeaddr;
    vm_offset_t		acpi_wakephys;

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

/*
 * The ACPI subsystem lives under a single mutex.  You *must*
 * acquire this mutex before calling any of the acpi_ or Acpi* functions.
 *
 * XXX the ACPI_MSLEEP macro should go away once locking is resolved
 */
extern struct mtx	acpi_mutex;
#if 0
# define ACPI_LOCK			mtx_lock(&acpi_mutex)
# define ACPI_UNLOCK			mtx_unlock(&acpi_mutex)
# define ACPI_ASSERTLOCK		mtx_assert(&acpi_mutex, MA_OWNED)
# define ACPI_MSLEEP(a, b, c, d, e)	msleep(a, b, c, d, e)
#else
# define ACPI_LOCK
# define ACPI_UNLOCK
# define ACPI_ASSERTLOCK
# define ACPI_MSLEEP(a, b, c, d, e)	tsleep(a, c, d, e)
#endif

/*
 * This is a cheap and nasty way to get around the horrid counted list
 * argument format that AcpiEvalateObject uses.
 */
#define ACPI_OBJECTLIST_MAX	16
struct acpi_object_list {
    UINT32	count;
    ACPI_OBJECT	*pointer[ACPI_OBJECTLIST_MAX];
    ACPI_OBJECT	object[ACPI_OBJECTLIST_MAX];
};

static __inline struct acpi_object_list *
acpi_AllocObjectList(int nobj)
{
    struct acpi_object_list	*l;
    int				i;

    if (nobj > ACPI_OBJECTLIST_MAX)
	return(NULL);
    if ((l = AcpiOsAllocate(sizeof(*l))) == NULL)
	return(NULL);
    bzero(l, sizeof(*l));
    for (i = 0; i < ACPI_OBJECTLIST_MAX; i++)
	l->pointer[i] = &l->object[i];
    l->count = nobj;
    return(l);
}

/*
 * Note that the low ivar values are reserved to provide
 * interface compatibility with ISA drivers which can also
 * attach to ACPI.
 */
#define ACPI_IVAR_HANDLE	0x100
#define ACPI_IVAR_MAGIC		0x101
#define ACPI_IVAR_PRIVATE	0x102

static __inline ACPI_HANDLE
acpi_get_handle(device_t dev)
{
    uintptr_t up;
    ACPI_HANDLE	h;

    if (BUS_READ_IVAR(device_get_parent(dev), dev, ACPI_IVAR_HANDLE, &up))
	return(NULL);
    h = (ACPI_HANDLE)up;
    return(h);
}
	    
static __inline int
acpi_set_handle(device_t dev, ACPI_HANDLE h)
{
    uintptr_t up;

    up = (uintptr_t)h;
    return(BUS_WRITE_IVAR(device_get_parent(dev), dev, ACPI_IVAR_HANDLE, up));
}
	    
static __inline int
acpi_get_magic(device_t dev)
{
    uintptr_t up;
    int	m;

    if (BUS_READ_IVAR(device_get_parent(dev), dev, ACPI_IVAR_MAGIC, &up))
	return(0);
    m = (int)up;
    return(m);
}

static __inline int
acpi_set_magic(device_t dev, int m)
{
    uintptr_t up;

    up = (uintptr_t)m;
    return(BUS_WRITE_IVAR(device_get_parent(dev), dev, ACPI_IVAR_MAGIC, up));
}

static __inline void *
acpi_get_private(device_t dev)
{
    uintptr_t up;
    void *p;

    if (BUS_READ_IVAR(device_get_parent(dev), dev, ACPI_IVAR_PRIVATE, &up))
	return(NULL);
    p = (void *)up;
    return(p);
}

static __inline int
acpi_set_private(device_t dev, void *p)
{
    uintptr_t up;

    up = (uintptr_t)p;
    return(BUS_WRITE_IVAR(device_get_parent(dev), dev, ACPI_IVAR_PRIVATE, up));
}

static __inline ACPI_OBJECT_TYPE
acpi_get_type(device_t dev)
{
    ACPI_HANDLE		h;
    ACPI_OBJECT_TYPE	t;

    if ((h = acpi_get_handle(dev)) == NULL)
	return(ACPI_TYPE_NOT_FOUND);
    if (AcpiGetType(h, &t) != AE_OK)
	return(ACPI_TYPE_NOT_FOUND);
    return(t);
}

#ifdef ENABLE_DEBUGGER
extern void		acpi_EnterDebugger(void);
#endif

#ifdef ACPI_DEBUG
#include <sys/cons.h>
#define STEP(x)		do {printf x, printf("\n"); cngetc();} while (0)
#else
#define STEP(x)
#endif

#define ACPI_VPRINT(dev, acpi_sc, x...) do {				\
	if (acpi_get_verbose(acpi_sc))					\
		device_printf(dev, x);					\
} while (0)

extern BOOLEAN		acpi_DeviceIsPresent(device_t dev);
extern BOOLEAN		acpi_MatchHid(device_t dev, char *hid);
extern ACPI_STATUS	acpi_GetHandleInScope(ACPI_HANDLE parent, char *path, ACPI_HANDLE *result);
extern ACPI_BUFFER	*acpi_AllocBuffer(int size);
extern ACPI_STATUS	acpi_GetIntoBuffer(ACPI_HANDLE handle, 
					   ACPI_STATUS (*func)(ACPI_HANDLE, ACPI_BUFFER *), 
					   ACPI_BUFFER *buf);
extern ACPI_STATUS	acpi_GetTableIntoBuffer(ACPI_TABLE_TYPE table, UINT32 instance, ACPI_BUFFER *buf);
extern ACPI_STATUS	acpi_EvaluateIntoBuffer(ACPI_HANDLE object, ACPI_STRING pathname,
						ACPI_OBJECT_LIST *params, ACPI_BUFFER *buf);
extern ACPI_STATUS	acpi_EvaluateInteger(ACPI_HANDLE handle, char *path, int *number);
extern ACPI_STATUS	acpi_ConvertBufferToInteger(ACPI_BUFFER *bufp, int *number);
extern ACPI_STATUS	acpi_ForeachPackageObject(ACPI_OBJECT *obj, 
						  void (* func)(ACPI_OBJECT *comp, void *arg),
						  void *arg);
extern ACPI_STATUS	acpi_FindIndexedResource(ACPI_BUFFER *buf, int index, ACPI_RESOURCE **resp);
extern ACPI_STATUS	acpi_AppendBufferResource(ACPI_BUFFER *buf, ACPI_RESOURCE *res);

extern ACPI_STATUS	acpi_SetSleepState(struct acpi_softc *sc, int state);
extern ACPI_STATUS	acpi_Enable(struct acpi_softc *sc);
extern ACPI_STATUS	acpi_Disable(struct acpi_softc *sc);

struct acpi_parse_resource_set {
    void	(* set_init)(device_t dev, void **context);
    void	(* set_done)(device_t dev, void *context);
    void	(* set_ioport)(device_t dev, void *context, u_int32_t base, u_int32_t length);
    void	(* set_iorange)(device_t dev, void *context, u_int32_t low, u_int32_t high, 
				u_int32_t length, u_int32_t align);
    void	(* set_memory)(device_t dev, void *context, u_int32_t base, u_int32_t length);
    void	(* set_memoryrange)(device_t dev, void *context, u_int32_t low, u_int32_t high, 
				    u_int32_t length, u_int32_t align);
    void	(* set_irq)(device_t dev, void *context, u_int32_t irq);
    void	(* set_drq)(device_t dev, void *context, u_int32_t drq);
    void	(* set_start_dependant)(device_t dev, void *context, int preference);
    void	(* set_end_dependant)(device_t dev, void *context);
};

extern struct acpi_parse_resource_set	acpi_res_parse_set;
extern ACPI_STATUS	acpi_parse_resources(device_t dev, ACPI_HANDLE handle,
					     struct acpi_parse_resource_set *set);
/* XXX until Intel fix this in their headers, based on NEXT_RESOURCE */
#define ACPI_RESOURCE_NEXT(Res)      (ACPI_RESOURCE *)((UINT8 *) Res + Res->Length)

/* 
 * ACPI event handling
 */
extern UINT32		acpi_eventhandler_power_button_for_sleep(void *context);
extern UINT32		acpi_eventhandler_power_button_for_wakeup(void *context);
extern UINT32		acpi_eventhandler_sleep_button_for_sleep(void *context);
extern UINT32		acpi_eventhandler_sleep_button_for_wakeup(void *context);

#define ACPI_EVENT_PRI_FIRST      0
#define ACPI_EVENT_PRI_DEFAULT    10000
#define ACPI_EVENT_PRI_LAST       20000

typedef void (*acpi_event_handler_t) __P((void *, int));

EVENTHANDLER_DECLARE(acpi_sleep_event, acpi_event_handler_t);
EVENTHANDLER_DECLARE(acpi_wakeup_event, acpi_event_handler_t);

/*
 * Device power control.
 */
extern ACPI_STATUS	acpi_pwr_switch_consumer(ACPI_HANDLE consumer, int state);

/* 
 * Misc. 
 */
static __inline struct acpi_softc *
acpi_device_get_parent_softc(device_t child)
{
    device_t	parent;

    parent = device_get_parent(child);
    if (parent == NULL) {
	return(NULL);
    }
    return(device_get_softc(parent));
}

static __inline int
acpi_get_verbose(struct acpi_softc *sc)
{
    if (sc)
	return(sc->acpi_verbose);
    return(0);
}

extern char	*acpi_name(ACPI_HANDLE handle);
extern int	acpi_avoid(ACPI_HANDLE handle);
extern int	acpi_disabled(char *subsys);

extern int	acpi_machdep_init(device_t dev);
extern void	acpi_install_wakeup_handler(struct acpi_softc *sc);
extern int	acpi_sleep_machdep(struct acpi_softc *sc, int state);

/*
 * Battery Abstruction.
 */
struct acpi_battinfo;
struct acpi_battdesc;

extern int	acpi_battery_register(int, int);
extern int	acpi_battery_get_battinfo(int, struct acpi_battinfo *);
extern int	acpi_battery_get_units(void);
extern int	acpi_battery_get_info_expire(void);
extern int	acpi_battery_get_battdesc(int, struct acpi_battdesc *);

extern int	acpi_cmbat_get_battinfo(int, struct acpi_battinfo *);

/*
 * AC adapter interface.
 */

extern int	acpi_acad_get_acline(int *);

/*
 * System power API.
 *
 * XXX should this be further generalised?
 *
 */
#define POWERPROFILE_PERFORMANCE	0
#define POWERPROFILE_ECONOMY		1

extern int	powerprofile_get_state(void);
extern void	powerprofile_set_state(int state);

typedef void (*powerprofile_change_hook)(void *);
EVENTHANDLER_DECLARE(powerprofile_change, powerprofile_change_hook);

#if defined(ACPI_MAX_THREADS) && ACPI_MAX_THREADS > 0
/*
 * ACPI task kernel thread initialization.
 */
extern int	acpi_task_thread_init(void);
#endif

