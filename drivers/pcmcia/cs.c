/*======================================================================

    Kernel Card Services -- core services

    cs.c 1.271 2000/10/02 20:27:49
    
    The contents of this file are subject to the Mozilla Public
    License Version 1.1 (the "License"); you may not use this file
    except in compliance with the License. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

    Software distributed under the License is distributed on an "AS
    IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
    implied. See the License for the specific language governing
    rights and limitations under the License.

    The initial developer of the original code is David A. Hinds
    <dahinds@users.sourceforge.net>.  Portions created by David A. Hinds
    are Copyright (C) 1999 David A. Hinds.  All Rights Reserved.

    Alternatively, the contents of this file may be used under the
    terms of the GNU General Public License version 2 (the "GPL"), in which
    case the provisions of the GPL are applicable instead of the
    above.  If you wish to allow the use of your version of this file
    only under the terms of the GPL and not to allow others to use
    your version of this file under the MPL, indicate your decision
    by deleting the provisions above and replace them with the notice
    and other provisions required by the GPL.  If you do not delete
    the provisions above, a recipient may use your version of this
    file under either the MPL or the GPL.
    
======================================================================*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/config.h>
#include <linux/string.h>
#include <linux/major.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/pm.h>
#include <linux/pci.h>
#include <asm/system.h>
#include <asm/irq.h>

#define IN_CARD_SERVICES
#include <pcmcia/version.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/ss.h>
#include <pcmcia/cs.h>
#include <pcmcia/bulkmem.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/cisreg.h>
#include <pcmcia/bus_ops.h>
#include "cs_internal.h"

#ifdef CONFIG_PCI
#define PCI_OPT " [pci]"
#else
#define PCI_OPT ""
#endif
#ifdef CONFIG_CARDBUS
#define CB_OPT " [cardbus]"
#else
#define CB_OPT ""
#endif
#ifdef CONFIG_PM
#define PM_OPT " [pm]"
#else
#define PM_OPT ""
#endif
#if !defined(CONFIG_CARDBUS) && !defined(CONFIG_PCI) && !defined(CONFIG_PM)
#define OPTIONS " none"
#else
#define OPTIONS PCI_OPT CB_OPT PM_OPT
#endif

static const char *release = "Linux Kernel Card Services " CS_RELEASE;
static const char *options = "options: " OPTIONS;

/*====================================================================*/

/* Module parameters */

MODULE_AUTHOR("David Hinds <dahinds@users.sourceforge.net>");
MODULE_DESCRIPTION("Linux Kernel Card Services " CS_RELEASE
		   "\n  options:" OPTIONS);
MODULE_LICENSE("Dual MPL/GPL");	  

#define INT_MODULE_PARM(n, v) static int n = v; MODULE_PARM(n, "i")

INT_MODULE_PARM(setup_delay,	10);		/* centiseconds */
INT_MODULE_PARM(resume_delay,	20);		/* centiseconds */
INT_MODULE_PARM(shutdown_delay,	3);		/* centiseconds */
INT_MODULE_PARM(vcc_settle,	40);		/* centiseconds */
INT_MODULE_PARM(reset_time,	10);		/* usecs */
INT_MODULE_PARM(unreset_delay,	10);		/* centiseconds */
INT_MODULE_PARM(unreset_check,	10);		/* centiseconds */
INT_MODULE_PARM(unreset_limit,	30);		/* unreset_check's */

/* Access speed for attribute memory windows */
INT_MODULE_PARM(cis_speed,	300);		/* ns */

/* Access speed for IO windows */
INT_MODULE_PARM(io_speed,	0);		/* ns */

/* Optional features */
#ifdef CONFIG_PM
INT_MODULE_PARM(do_apm,		1);
#else
INT_MODULE_PARM(do_apm,		0);
#endif

#ifdef PCMCIA_DEBUG
INT_MODULE_PARM(pc_debug, PCMCIA_DEBUG);
static const char *version =
"cs.c 1.279 2001/10/13 00:08:28 (David Hinds)";
#endif
 
/*====================================================================*/

socket_state_t dead_socket = {
    0, SS_DETECT, 0, 0, 0
};

/* Table of sockets */
socket_t sockets = 0;
socket_info_t *socket_table[MAX_SOCK];

#ifdef CONFIG_PROC_FS
struct proc_dir_entry *proc_pccard = NULL;
#endif

/*====================================================================*/

/* String tables for error messages */

typedef struct lookup_t {
    int key;
    char *msg;
} lookup_t;

static const lookup_t error_table[] = {
    { CS_SUCCESS,		"Operation succeeded" },
    { CS_BAD_ADAPTER,		"Bad adapter" },
    { CS_BAD_ATTRIBUTE, 	"Bad attribute", },
    { CS_BAD_BASE,		"Bad base address" },
    { CS_BAD_EDC,		"Bad EDC" },
    { CS_BAD_IRQ,		"Bad IRQ" },
    { CS_BAD_OFFSET,		"Bad offset" },
    { CS_BAD_PAGE,		"Bad page number" },
    { CS_READ_FAILURE,		"Read failure" },
    { CS_BAD_SIZE,		"Bad size" },
    { CS_BAD_SOCKET,		"Bad socket" },
    { CS_BAD_TYPE,		"Bad type" },
    { CS_BAD_VCC,		"Bad Vcc" },
    { CS_BAD_VPP,		"Bad Vpp" },
    { CS_BAD_WINDOW,		"Bad window" },
    { CS_WRITE_FAILURE,		"Write failure" },
    { CS_NO_CARD,		"No card present" },
    { CS_UNSUPPORTED_FUNCTION,	"Usupported function" },
    { CS_UNSUPPORTED_MODE,	"Unsupported mode" },
    { CS_BAD_SPEED,		"Bad speed" },
    { CS_BUSY,			"Resource busy" },
    { CS_GENERAL_FAILURE,	"General failure" },
    { CS_WRITE_PROTECTED,	"Write protected" },
    { CS_BAD_ARG_LENGTH,	"Bad argument length" },
    { CS_BAD_ARGS,		"Bad arguments" },
    { CS_CONFIGURATION_LOCKED,	"Configuration locked" },
    { CS_IN_USE,		"Resource in use" },
    { CS_NO_MORE_ITEMS,		"No more items" },
    { CS_OUT_OF_RESOURCE,	"Out of resource" },
    { CS_BAD_HANDLE,		"Bad handle" },
    { CS_BAD_TUPLE,		"Bad CIS tuple" }
};
#define ERROR_COUNT (sizeof(error_table)/sizeof(lookup_t))

static const lookup_t service_table[] = {
    { AccessConfigurationRegister,	"AccessConfigurationRegister" },
    { AddSocketServices,		"AddSocketServices" },
    { AdjustResourceInfo,		"AdjustResourceInfo" },
    { CheckEraseQueue,			"CheckEraseQueue" },
    { CloseMemory,			"CloseMemory" },
    { DeregisterClient,			"DeregisterClient" },
    { DeregisterEraseQueue,		"DeregisterEraseQueue" },
    { GetCardServicesInfo,		"GetCardServicesInfo" },
    { GetClientInfo,			"GetClientInfo" },
    { GetConfigurationInfo,		"GetConfigurationInfo" },
    { GetEventMask,			"GetEventMask" },
    { GetFirstClient,			"GetFirstClient" },
    { GetFirstRegion,			"GetFirstRegion" },
    { GetFirstTuple,			"GetFirstTuple" },
    { GetNextClient,			"GetNextClient" },
    { GetNextRegion,			"GetNextRegion" },
    { GetNextTuple,			"GetNextTuple" },
    { GetStatus,			"GetStatus" },
    { GetTupleData,			"GetTupleData" },
    { MapMemPage,			"MapMemPage" },
    { ModifyConfiguration,		"ModifyConfiguration" },
    { ModifyWindow,			"ModifyWindow" },
    { OpenMemory,			"OpenMemory" },
    { ParseTuple,			"ParseTuple" },
    { ReadMemory,			"ReadMemory" },
    { RegisterClient,			"RegisterClient" },
    { RegisterEraseQueue,		"RegisterEraseQueue" },
    { RegisterMTD,			"RegisterMTD" },
    { ReleaseConfiguration,		"ReleaseConfiguration" },
    { ReleaseIO,			"ReleaseIO" },
    { ReleaseIRQ,			"ReleaseIRQ" },
    { ReleaseWindow,			"ReleaseWindow" },
    { RequestConfiguration,		"RequestConfiguration" },
    { RequestIO,			"RequestIO" },
    { RequestIRQ,			"RequestIRQ" },
    { RequestSocketMask,		"RequestSocketMask" },
    { RequestWindow,			"RequestWindow" },
    { ResetCard,			"ResetCard" },
    { SetEventMask,			"SetEventMask" },
    { ValidateCIS,			"ValidateCIS" },
    { WriteMemory,			"WriteMemory" },
    { BindDevice,			"BindDevice" },
    { BindMTD,				"BindMTD" },
    { ReportError,			"ReportError" },
    { SuspendCard,			"SuspendCard" },
    { ResumeCard,			"ResumeCard" },
    { EjectCard,			"EjectCard" },
    { InsertCard,			"InsertCard" },
    { ReplaceCIS,			"ReplaceCIS" }
};
#define SERVICE_COUNT (sizeof(service_table)/sizeof(lookup_t))

/*======================================================================

 These functions are just shorthand for the actual low-level drivers

======================================================================*/

static int register_callback(socket_info_t *s, void (*handler)(void *, unsigned int), void * info)
{
	return s->ss_entry->register_callback(s->sock, handler, info);
}

static int get_socket_status(socket_info_t *s, int *val)
{
	return s->ss_entry->get_status(s->sock, val);
}

static int set_socket(socket_info_t *s, socket_state_t *state)
{
	return s->ss_entry->set_socket(s->sock, state);
}

static int set_io_map(socket_info_t *s, struct pccard_io_map *io)
{
	return s->ss_entry->set_io_map(s->sock, io);
}

static int set_mem_map(socket_info_t *s, struct pccard_mem_map *mem)
{
	return s->ss_entry->set_mem_map(s->sock, mem);
}

static int suspend_socket(socket_info_t *s)
{
	s->socket = dead_socket;
	return s->ss_entry->suspend(s->sock);
}

static int init_socket(socket_info_t *s)
{
	s->socket = dead_socket;
	return s->ss_entry->init(s->sock);
}

/*====================================================================*/

#if defined(CONFIG_PROC_FS) && defined(PCMCIA_DEBUG)
static int proc_read_clients(char *buf, char **start, off_t pos,
			     int count, int *eof, void *data)
{
    socket_info_t *s = data;
    client_handle_t c;
    char *p = buf;

    for (c = s->clients; c; c = c->next)
	p += sprintf(p, "fn %x: '%s' [attr 0x%04x] [state 0x%04x]\n",
		     c->Function, c->dev_info, c->Attributes, c->state);
    return (p - buf);
}
#endif

/*======================================================================

    Low-level PC Card interface drivers need to register with Card
    Services using these calls.
    
======================================================================*/

static int setup_socket(socket_info_t *);
static void shutdown_socket(socket_info_t *);
static void reset_socket(socket_info_t *);
static void unreset_socket(socket_info_t *);
static void parse_events(void *info, u_int events);

socket_info_t *pcmcia_register_socket (int slot,
	struct pccard_operations * ss_entry,
	int use_bus_pm)
{
    socket_info_t *s;
    int i;

    DEBUG(0, "cs: pcmcia_register_socket(0x%p)\n", ss_entry);

    s = kmalloc(sizeof(struct socket_info_t), GFP_KERNEL);
    if (!s)
    	return NULL;
    memset(s, 0, sizeof(socket_info_t));

    s->ss_entry = ss_entry;
    s->sock = slot;

    /* base address = 0, map = 0 */
    s->cis_mem.flags = 0;
    s->cis_mem.speed = cis_speed;
    s->use_bus_pm = use_bus_pm;
    s->erase_busy.next = s->erase_busy.prev = &s->erase_busy;
    spin_lock_init(&s->lock);
    
    for (i = 0; i < sockets; i++)
	if (socket_table[i] == NULL) break;
    socket_table[i] = s;
    if (i == sockets) sockets++;

    init_socket(s);
    ss_entry->inquire_socket(slot, &s->cap);
#ifdef CONFIG_PROC_FS
    if (proc_pccard) {
	char name[3];
	sprintf(name, "%02d", i);
	s->proc = proc_mkdir(name, proc_pccard);
	if (s->proc)
	    ss_entry->proc_setup(slot, s->proc);
#ifdef PCMCIA_DEBUG
	if (s->proc)
	    create_proc_read_entry("clients", 0, s->proc,
				   proc_read_clients, s);
#endif
    }
#endif
    return s;
} /* pcmcia_register_socket */

int register_ss_entry(int nsock, struct pccard_operations * ss_entry)
{
    int ns;

    DEBUG(0, "cs: register_ss_entry(%d, 0x%p)\n", nsock, ss_entry);

    for (ns = 0; ns < nsock; ns++) {
	pcmcia_register_socket (ns, ss_entry, 0);
    }
    
    return 0;
} /* register_ss_entry */

/*====================================================================*/

void pcmcia_unregister_socket(socket_info_t *s)
{
    int j, socket = -1;
    client_t *client;

    for (j = 0; j < MAX_SOCK; j++)
	if (socket_table [j] == s) {
	    socket = j;
	    break;
	}
    if (socket < 0)
	return;

#ifdef CONFIG_PROC_FS
    if (proc_pccard) {
	char name[3];
	sprintf(name, "%02d", socket);
#ifdef PCMCIA_DEBUG
	remove_proc_entry("clients", s->proc);
#endif
	remove_proc_entry(name, proc_pccard);
    }
#endif

    shutdown_socket(s);
    release_cis_mem(s);
    while (s->clients) {
	client = s->clients;
	s->clients = s->clients->next;
	kfree(client);
    }
    s->ss_entry = NULL;
    kfree(s);

    socket_table[socket] = NULL;
    for (j = socket; j < sockets-1; j++)
	socket_table[j] = socket_table[j+1];
    sockets--;
} /* pcmcia_unregister_socket */

void unregister_ss_entry(struct pccard_operations * ss_entry)
{
    int i;

    for (i = sockets-1; i >= 0; i-- ) {
	socket_info_t *socket = socket_table[i];
	if (socket->ss_entry == ss_entry)
		pcmcia_unregister_socket (socket);
    }
} /* unregister_ss_entry */

/*======================================================================

    Shutdown_Socket() and setup_socket() are scheduled using add_timer
    calls by the main event handler when card insertion and removal
    events are received.  Shutdown_Socket() unconfigures a socket and
    turns off socket power.  Setup_socket() turns on socket power
    and resets the socket, in two stages.

======================================================================*/

static void free_regions(memory_handle_t *list)
{
    memory_handle_t tmp;
    while (*list != NULL) {
	tmp = *list;
	*list = tmp->info.next;
	tmp->region_magic = 0;
	kfree(tmp);
    }
}

static int send_event(socket_info_t *s, event_t event, int priority);

/*
 * Sleep for n_cs centiseconds (1 cs = 1/100th of a second)
 */
static void cs_sleep(unsigned int n_cs)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule_timeout( (n_cs * HZ + 99) / 100);
}

static void shutdown_socket(socket_info_t *s)
{
    client_t **c;
    
    DEBUG(1, "cs: shutdown_socket(%p)\n", s);

    /* Blank out the socket state */
    s->state &= SOCKET_PRESENT|SOCKET_SETUP_PENDING;
    init_socket(s);
    s->irq.AssignedIRQ = s->irq.Config = 0;
    s->lock_count = 0;
    s->cis_used = 0;
    if (s->fake_cis) {
	kfree(s->fake_cis);
	s->fake_cis = NULL;
    }
    /* Should not the socket be forced quiet as well?  e.g. turn off Vcc */
    /* Without these changes, the socket is left hot, even though card-services */
    /* realizes that no card is in place. */
    s->socket.flags &= ~SS_OUTPUT_ENA;
    s->socket.Vpp = 0;
    s->socket.Vcc = 0;
    s->socket.io_irq = 0;
    set_socket(s, &s->socket);
    /* */
#ifdef CONFIG_CARDBUS
    cb_release_cis_mem(s);
    cb_free(s);
#endif
    s->functions = 0;
    if (s->config) {
	kfree(s->config);
	s->config = NULL;
    }
    for (c = &s->clients; *c; ) {
	if ((*c)->state & CLIENT_UNBOUND) {
	    client_t *d = *c;
	    *c = (*c)->next;
	    kfree(d);
	} else {
	    c = &((*c)->next);
	}
    }
    free_regions(&s->a_region);
    free_regions(&s->c_region);
} /* shutdown_socket */

/*
 * Return zero if we think the card isn't actually present
 */
static int setup_socket(socket_info_t *s)
{
	int val, ret;
	int setup_timeout = 100;

	/* Wait for "not pending" */
	for (;;) {
		get_socket_status(s, &val);
		if (!(val & SS_PENDING))
			break;
		if (--setup_timeout) {
			cs_sleep(10);
			continue;
		}
		printk(KERN_NOTICE "cs: socket %p voltage interrogation"
			" timed out\n", s);
		ret = 0;
		goto out;
	}

	if (val & SS_DETECT) {
		DEBUG(1, "cs: setup_socket(%p): applying power\n", s);
		s->state |= SOCKET_PRESENT;
		s->socket.flags &= SS_DEBOUNCED;
		if (val & SS_3VCARD)
		    s->socket.Vcc = s->socket.Vpp = 33;
		else if (!(val & SS_XVCARD))
		    s->socket.Vcc = s->socket.Vpp = 50;
		else {
		    printk(KERN_NOTICE "cs: socket %p: unsupported "
			   "voltage key\n", s);
		    s->socket.Vcc = 0;
		}
		if (val & SS_CARDBUS) {
		    s->state |= SOCKET_CARDBUS;
#ifndef CONFIG_CARDBUS
		    printk(KERN_NOTICE "cs: unsupported card type detected!\n");
#endif
		}
		set_socket(s, &s->socket);
		cs_sleep(vcc_settle);
		reset_socket(s);
		ret = 1;
	} else {
		DEBUG(0, "cs: setup_socket(%p): no card!\n", s);
		ret = 0;
	}
out:
	return ret;
} /* setup_socket */

/*======================================================================

    Reset_socket() and unreset_socket() handle hard resets.  Resets
    have several causes: card insertion, a call to reset_socket, or
    recovery from a suspend/resume cycle.  Unreset_socket() sends
    a CS event that matches the cause of the reset.
    
======================================================================*/

static void reset_socket(socket_info_t *s)
{
    DEBUG(1, "cs: resetting socket %p\n", s);
    s->socket.flags |= SS_OUTPUT_ENA | SS_RESET;
    set_socket(s, &s->socket);
    udelay((long)reset_time);
    s->socket.flags &= ~SS_RESET;
    set_socket(s, &s->socket);
    cs_sleep(unreset_delay);
    unreset_socket(s);
} /* reset_socket */

#define EVENT_MASK \
(SOCKET_SETUP_PENDING|SOCKET_SUSPEND|SOCKET_RESET_PENDING)

static void unreset_socket(socket_info_t *s)
{
	int setup_timeout = unreset_limit;
	int val;

	/* Wait for "ready" */
	for (;;) {
		get_socket_status(s, &val);
		if (val & SS_READY)
			break;
		DEBUG(2, "cs: socket %d not ready yet\n", s->sock);
		if (--setup_timeout) {
			cs_sleep(unreset_check);
			continue;
		}
		printk(KERN_NOTICE "cs: socket %p timed out during"
			" reset.  Try increasing setup_delay.\n", s);
		s->state &= ~EVENT_MASK;
		return;
	}

	DEBUG(1, "cs: reset done on socket %p\n", s);
	if (s->state & SOCKET_SUSPEND) {
	    s->state &= ~EVENT_MASK;
	    if (verify_cis_cache(s) != 0)
		parse_events(s, SS_DETECT);
	    else
		send_event(s, CS_EVENT_PM_RESUME, CS_EVENT_PRI_LOW);
	} else if (s->state & SOCKET_SETUP_PENDING) {
#ifdef CONFIG_CARDBUS
	    if (s->state & SOCKET_CARDBUS)
		cb_alloc(s);
#endif
	    send_event(s, CS_EVENT_CARD_INSERTION, CS_EVENT_PRI_LOW);
	    s->state &= ~SOCKET_SETUP_PENDING;
	} else {
	    send_event(s, CS_EVENT_CARD_RESET, CS_EVENT_PRI_LOW);
	    if (s->reset_handle) { 
		    s->reset_handle->event_callback_args.info = NULL;
		    EVENT(s->reset_handle, CS_EVENT_RESET_COMPLETE,
			  CS_EVENT_PRI_LOW);
	    }
	    s->state &= ~EVENT_MASK;
	}
} /* unreset_socket */

/*======================================================================

    The central event handler.  Send_event() sends an event to all
    valid clients.  Parse_events() interprets the event bits from
    a card status change report.  Do_shutdown() handles the high
    priority stuff associated with a card removal.
    
======================================================================*/

static int send_event(socket_info_t *s, event_t event, int priority)
{
    client_t *client = s->clients;
    int ret;
    DEBUG(1, "cs: send_event(sock %d, event %d, pri %d)\n",
	  s->sock, event, priority);
    ret = 0;
    for (; client; client = client->next) { 
	if (client->state & (CLIENT_UNBOUND|CLIENT_STALE))
	    continue;
	if (client->EventMask & event) {
	    ret = EVENT(client, event, priority);
	    if (ret != 0)
		return ret;
	}
    }
    return ret;
} /* send_event */

static void do_shutdown(socket_info_t *s)
{
    client_t *client;
    if (s->state & SOCKET_SHUTDOWN_PENDING)
	return;
    s->state |= SOCKET_SHUTDOWN_PENDING;
    send_event(s, CS_EVENT_CARD_REMOVAL, CS_EVENT_PRI_HIGH);
    for (client = s->clients; client; client = client->next)
	if (!(client->Attributes & INFO_MASTER_CLIENT))
	    client->state |= CLIENT_STALE;
    if (s->state & (SOCKET_SETUP_PENDING|SOCKET_RESET_PENDING)) {
	DEBUG(0, "cs: flushing pending setup\n");
	s->state &= ~EVENT_MASK;
    }
    cs_sleep(shutdown_delay);
    s->state &= ~SOCKET_PRESENT;
    shutdown_socket(s);
}

static void parse_events(void *info, u_int events)
{
    socket_info_t *s = info;
    if (events & SS_DETECT) {
	int status;

	get_socket_status(s, &status);
	if ((s->state & SOCKET_PRESENT) &&
	    (!(s->state & SOCKET_SUSPEND) ||
	     !(status & SS_DETECT)))
	    do_shutdown(s);
	if (status & SS_DETECT) {
	    if (s->state & SOCKET_SETUP_PENDING) {
		DEBUG(1, "cs: delaying pending setup\n");
		return;
	    }
	    s->state |= SOCKET_SETUP_PENDING;
	    if (s->state & SOCKET_SUSPEND)
		cs_sleep(resume_delay);
	    else
		cs_sleep(setup_delay);
	    s->socket.flags |= SS_DEBOUNCED;
	    if (setup_socket(s) == 0)
		s->state &= ~SOCKET_SETUP_PENDING;
	    s->socket.flags &= ~SS_DEBOUNCED;
	}
    }
    if (events & SS_BATDEAD)
	send_event(s, CS_EVENT_BATTERY_DEAD, CS_EVENT_PRI_LOW);
    if (events & SS_BATWARN)
	send_event(s, CS_EVENT_BATTERY_LOW, CS_EVENT_PRI_LOW);
    if (events & SS_READY) {
	if (!(s->state & SOCKET_RESET_PENDING))
	    send_event(s, CS_EVENT_READY_CHANGE, CS_EVENT_PRI_LOW);
	else DEBUG(1, "cs: ready change during reset\n");
    }
} /* parse_events */

/*======================================================================

    Another event handler, for power management events.

    This does not comply with the latest PC Card spec for handling
    power management events.
    
======================================================================*/

void pcmcia_suspend_socket (socket_info_t *s)
{
    if ((s->state & SOCKET_PRESENT) && !(s->state & SOCKET_SUSPEND)) {
	send_event(s, CS_EVENT_PM_SUSPEND, CS_EVENT_PRI_LOW);
	suspend_socket(s);
	s->state |= SOCKET_SUSPEND;
    }
}

void pcmcia_resume_socket (socket_info_t *s)
{
    int	stat;

    /* Do this just to reinitialize the socket */
    init_socket(s);
    get_socket_status(s, &stat);

    /* If there was or is a card here, we need to do something
    about it... but parse_events will sort it all out. */
    if ((s->state & SOCKET_PRESENT) || (stat & SS_DETECT))
	parse_events(s, SS_DETECT);
}

static int handle_pm_event(struct pm_dev *dev, pm_request_t rqst, void *data)
{
    int i;
    socket_info_t *s;

    /* only for busses that don't suspend/resume slots directly */

    switch (rqst) {
    case PM_SUSPEND:
	DEBUG(1, "cs: received suspend notification\n");
	for (i = 0; i < sockets; i++) {
	    s = socket_table [i];
	    if (!s->use_bus_pm)
		pcmcia_suspend_socket (socket_table [i]);
	}
	break;
    case PM_RESUME:
	DEBUG(1, "cs: received resume notification\n");
	for (i = 0; i < sockets; i++) {
	    s = socket_table [i];
	    if (!s->use_bus_pm)
		pcmcia_resume_socket (socket_table [i]);
	}
	break;
    }
    return 0;
} /* handle_pm_event */

/*======================================================================

    Special stuff for managing IO windows, because they are scarce.
    
======================================================================*/

static int alloc_io_space(socket_info_t *s, u_int attr, ioaddr_t *base,
			  ioaddr_t num, u_int lines, char *name)
{
    int i;
    ioaddr_t try, align;

    align = (*base) ? (lines ? 1<<lines : 0) : 1;
    if (align && (align < num)) {
	if (*base) {
	    DEBUG(0, "odd IO request: num %04x align %04x\n",
		  num, align);
	    align = 0;
	} else
	    while (align && (align < num)) align <<= 1;
    }
    if (*base & ~(align-1)) {
	DEBUG(0, "odd IO request: base %04x align %04x\n",
	      *base, align);
	align = 0;
    }
    if ((s->cap.features & SS_CAP_STATIC_MAP) && s->cap.io_offset) {
	*base = s->cap.io_offset | (*base & 0x0fff);
	return 0;
    }
    /* Check for an already-allocated window that must conflict with
       what was asked for.  It is a hack because it does not catch all
       potential conflicts, just the most obvious ones. */
    for (i = 0; i < MAX_IO_WIN; i++)
	if ((s->io[i].NumPorts != 0) &&
	    ((s->io[i].BasePort & (align-1)) == *base))
	    return 1;
    for (i = 0; i < MAX_IO_WIN; i++) {
	if (s->io[i].NumPorts == 0) {
	    if (find_io_region(base, num, align, name, s) == 0) {
		s->io[i].Attributes = attr;
		s->io[i].BasePort = *base;
		s->io[i].NumPorts = s->io[i].InUse = num;
		break;
	    } else
		return 1;
	} else if (s->io[i].Attributes != attr)
	    continue;
	/* Try to extend top of window */
	try = s->io[i].BasePort + s->io[i].NumPorts;
	if ((*base == 0) || (*base == try))
	    if (find_io_region(&try, num, 0, name, s) == 0) {
		*base = try;
		s->io[i].NumPorts += num;
		s->io[i].InUse += num;
		break;
	    }
	/* Try to extend bottom of window */
	try = s->io[i].BasePort - num;
	if ((*base == 0) || (*base == try))
	    if (find_io_region(&try, num, 0, name, s) == 0) {
		s->io[i].BasePort = *base = try;
		s->io[i].NumPorts += num;
		s->io[i].InUse += num;
		break;
	    }
    }
    return (i == MAX_IO_WIN);
} /* alloc_io_space */

static void release_io_space(socket_info_t *s, ioaddr_t base,
			     ioaddr_t num)
{
    int i;
    if(!(s->cap.features & SS_CAP_STATIC_MAP))
	release_region(base, num);
    for (i = 0; i < MAX_IO_WIN; i++) {
	if ((s->io[i].BasePort <= base) &&
	    (s->io[i].BasePort+s->io[i].NumPorts >= base+num)) {
	    s->io[i].InUse -= num;
	    /* Free the window if no one else is using it */
	    if (s->io[i].InUse == 0)
		s->io[i].NumPorts = 0;
	}
    }
}

/*======================================================================

    Access_configuration_register() reads and writes configuration
    registers in attribute memory.  Memory window 0 is reserved for
    this and the tuple reading services.
    
======================================================================*/

int pcmcia_access_configuration_register(client_handle_t handle,
					 conf_reg_t *reg)
{
    socket_info_t *s;
    config_t *c;
    int addr;
    u_char val;
    
    if (CHECK_HANDLE(handle))
	return CS_BAD_HANDLE;
    s = SOCKET(handle);
    if (handle->Function == BIND_FN_ALL) {
	if (reg->Function >= s->functions)
	    return CS_BAD_ARGS;
	c = &s->config[reg->Function];
    } else
	c = CONFIG(handle);

    if (c == NULL)
	return CS_NO_CARD;

    if (!(c->state & CONFIG_LOCKED))
	return CS_CONFIGURATION_LOCKED;

    addr = (c->ConfigBase + reg->Offset) >> 1;
    
    switch (reg->Action) {
    case CS_READ:
	read_cis_mem(s, 1, addr, 1, &val);
	reg->Value = val;
	break;
    case CS_WRITE:
	val = reg->Value;
	write_cis_mem(s, 1, addr, 1, &val);
	break;
    default:
	return CS_BAD_ARGS;
	break;
    }
    return CS_SUCCESS;
} /* access_configuration_register */

/*======================================================================

    Bind_device() associates a device driver with a particular socket.
    It is normally called by Driver Services after it has identified
    a newly inserted card.  An instance of that driver will then be
    eligible to register as a client of this socket.
    
======================================================================*/

int pcmcia_bind_device(bind_req_t *req)
{
    client_t *client;
    socket_info_t *s;

    if (CHECK_SOCKET(req->Socket))
	return CS_BAD_SOCKET;
    s = SOCKET(req);

    client = (client_t *)kmalloc(sizeof(client_t), GFP_KERNEL);
    if (!client) return CS_OUT_OF_RESOURCE;
    memset(client, '\0', sizeof(client_t));
    client->client_magic = CLIENT_MAGIC;
    strncpy(client->dev_info, (char *)req->dev_info, DEV_NAME_LEN);
    client->Socket = req->Socket;
    client->Function = req->Function;
    client->state = CLIENT_UNBOUND;
    client->erase_busy.next = &client->erase_busy;
    client->erase_busy.prev = &client->erase_busy;
    init_waitqueue_head(&client->mtd_req);
    client->next = s->clients;
    s->clients = client;
    DEBUG(1, "cs: bind_device(): client 0x%p, sock %d, dev %s\n",
	  client, client->Socket, client->dev_info);
    return CS_SUCCESS;
} /* bind_device */

/*======================================================================

    Bind_mtd() associates a device driver with a particular memory
    region.  It is normally called by Driver Services after it has
    identified a memory device type.  An instance of the corresponding
    driver will then be able to register to control this region.
    
======================================================================*/

int pcmcia_bind_mtd(mtd_bind_t *req)
{
    socket_info_t *s;
    memory_handle_t region;
    
    if (CHECK_SOCKET(req->Socket))
	return CS_BAD_SOCKET;
    s = SOCKET(req);
    
    if (req->Attributes & REGION_TYPE_AM)
	region = s->a_region;
    else
	region = s->c_region;
    
    while (region) {
	if (region->info.CardOffset == req->CardOffset) break;
	region = region->info.next;
    }
    if (!region || (region->mtd != NULL))
	return CS_BAD_OFFSET;
    strncpy(region->dev_info, (char *)req->dev_info, DEV_NAME_LEN);
    
    DEBUG(1, "cs: bind_mtd(): attr 0x%x, offset 0x%x, dev %s\n",
	  req->Attributes, req->CardOffset, (char *)req->dev_info);
    return CS_SUCCESS;
} /* bind_mtd */

/*====================================================================*/

int pcmcia_deregister_client(client_handle_t handle)
{
    client_t **client;
    socket_info_t *s;
    memory_handle_t region;
    u_long flags;
    int i, sn;
    
    DEBUG(1, "cs: deregister_client(%p)\n", handle);
    if (CHECK_HANDLE(handle))
	return CS_BAD_HANDLE;
    if (handle->state &
	(CLIENT_IRQ_REQ|CLIENT_IO_REQ|CLIENT_CONFIG_LOCKED))
	return CS_IN_USE;
    for (i = 0; i < MAX_WIN; i++)
	if (handle->state & CLIENT_WIN_REQ(i))
	    return CS_IN_USE;

    /* Disconnect all MTD links */
    s = SOCKET(handle);
    if (handle->mtd_count) {
	for (region = s->a_region; region; region = region->info.next)
	    if (region->mtd == handle) region->mtd = NULL;
	for (region = s->c_region; region; region = region->info.next)
	    if (region->mtd == handle) region->mtd = NULL;
    }
    
    sn = handle->Socket; s = socket_table[sn];

    if ((handle->state & CLIENT_STALE) ||
	(handle->Attributes & INFO_MASTER_CLIENT)) {
	spin_lock_irqsave(&s->lock, flags);
	client = &s->clients;
	while ((*client) && ((*client) != handle))
	    client = &(*client)->next;
	if (*client == NULL) {
	    spin_unlock_irqrestore(&s->lock, flags);
	    return CS_BAD_HANDLE;
	}
	*client = handle->next;
	handle->client_magic = 0;
	kfree(handle);
	spin_unlock_irqrestore(&s->lock, flags);
    } else {
	handle->state = CLIENT_UNBOUND;
	handle->mtd_count = 0;
	handle->event_handler = NULL;
    }

    if (--s->real_clients == 0)
        register_callback(s, NULL, NULL);
    
    return CS_SUCCESS;
} /* deregister_client */

/*====================================================================*/

int pcmcia_get_configuration_info(client_handle_t handle,
				  config_info_t *config)
{
    socket_info_t *s;
    config_t *c;
    
    if (CHECK_HANDLE(handle))
	return CS_BAD_HANDLE;
    s = SOCKET(handle);
    if (!(s->state & SOCKET_PRESENT))
	return CS_NO_CARD;

    if (handle->Function == BIND_FN_ALL) {
	if (config->Function && (config->Function >= s->functions))
	    return CS_BAD_ARGS;
    } else
	config->Function = handle->Function;
    
#ifdef CONFIG_CARDBUS
    if (s->state & SOCKET_CARDBUS) {
	u_char fn = config->Function;
	memset(config, 0, sizeof(config_info_t));
	config->Function = fn;
	config->Vcc = s->socket.Vcc;
	config->Vpp1 = config->Vpp2 = s->socket.Vpp;
	config->Option = s->cap.cb_dev->subordinate->number;
	if (s->cb_config) {
	    config->Attributes = CONF_VALID_CLIENT;
	    config->IntType = INT_CARDBUS;
	    config->AssignedIRQ = s->irq.AssignedIRQ;
	    if (config->AssignedIRQ)
		config->Attributes |= CONF_ENABLE_IRQ;
	    config->BasePort1 = s->io[0].BasePort;
	    config->NumPorts1 = s->io[0].NumPorts;
	}
	return CS_SUCCESS;
    }
#endif
    
    c = (s->config != NULL) ? &s->config[config->Function] : NULL;
    
    if ((c == NULL) || !(c->state & CONFIG_LOCKED)) {
	config->Attributes = 0;
	config->Vcc = s->socket.Vcc;
	config->Vpp1 = config->Vpp2 = s->socket.Vpp;
	return CS_SUCCESS;
    }
    
    /* !!! This is a hack !!! */
    memcpy(&config->Attributes, &c->Attributes, sizeof(config_t));
    config->Attributes |= CONF_VALID_CLIENT;
    config->CardValues = c->CardValues;
    config->IRQAttributes = c->irq.Attributes;
    config->AssignedIRQ = s->irq.AssignedIRQ;
    config->BasePort1 = c->io.BasePort1;
    config->NumPorts1 = c->io.NumPorts1;
    config->Attributes1 = c->io.Attributes1;
    config->BasePort2 = c->io.BasePort2;
    config->NumPorts2 = c->io.NumPorts2;
    config->Attributes2 = c->io.Attributes2;
    config->IOAddrLines = c->io.IOAddrLines;
    
    return CS_SUCCESS;
} /* get_configuration_info */

/*======================================================================

    Return information about this version of Card Services.
    
======================================================================*/

int pcmcia_get_card_services_info(servinfo_t *info)
{
    info->Signature[0] = 'C';
    info->Signature[1] = 'S';
    info->Count = sockets;
    info->Revision = CS_RELEASE_CODE;
    info->CSLevel = 0x0210;
    info->VendorString = (char *)release;
    return CS_SUCCESS;
} /* get_card_services_info */

/*======================================================================

    Note that get_first_client() *does* recognize the Socket field
    in the request structure.
    
======================================================================*/

int pcmcia_get_first_client(client_handle_t *handle, client_req_t *req)
{
    socket_t s;
    if (req->Attributes & CLIENT_THIS_SOCKET)
	s = req->Socket;
    else
	s = 0;
    if (CHECK_SOCKET(req->Socket))
	return CS_BAD_SOCKET;
    if (socket_table[s]->clients == NULL)
	return CS_NO_MORE_ITEMS;
    *handle = socket_table[s]->clients;
    return CS_SUCCESS;
} /* get_first_client */

/*====================================================================*/

int pcmcia_get_next_client(client_handle_t *handle, client_req_t *req)
{
    socket_info_t *s;
    if ((handle == NULL) || CHECK_HANDLE(*handle))
	return CS_BAD_HANDLE;
    if ((*handle)->next == NULL) {
	if (req->Attributes & CLIENT_THIS_SOCKET)
	    return CS_NO_MORE_ITEMS;
	s = SOCKET(*handle);
	if (s->clients == NULL)
	    return CS_NO_MORE_ITEMS;
	*handle = s->clients;
    } else
	*handle = (*handle)->next;
    return CS_SUCCESS;
} /* get_next_client */

/*====================================================================*/

int pcmcia_get_window(window_handle_t *handle, int idx, win_req_t *req)
{
    socket_info_t *s;
    window_t *win;
    int w;

    if (idx == 0)
	s = SOCKET((client_handle_t)*handle);
    else
	s = (*handle)->sock;
    if (!(s->state & SOCKET_PRESENT))
	return CS_NO_CARD;
    for (w = idx; w < MAX_WIN; w++)
	if (s->state & SOCKET_WIN_REQ(w)) break;
    if (w == MAX_WIN)
	return CS_NO_MORE_ITEMS;
    win = &s->win[w];
    req->Base = win->ctl.sys_start;
    req->Size = win->ctl.sys_stop - win->ctl.sys_start + 1;
    req->AccessSpeed = win->ctl.speed;
    req->Attributes = 0;
    if (win->ctl.flags & MAP_ATTRIB)
	req->Attributes |= WIN_MEMORY_TYPE_AM;
    if (win->ctl.flags & MAP_ACTIVE)
	req->Attributes |= WIN_ENABLE;
    if (win->ctl.flags & MAP_16BIT)
	req->Attributes |= WIN_DATA_WIDTH_16;
    if (win->ctl.flags & MAP_USE_WAIT)
	req->Attributes |= WIN_USE_WAIT;
    *handle = win;
    return CS_SUCCESS;
} /* get_window */

int pcmcia_get_first_window(window_handle_t *win, win_req_t *req)
{
    if ((win == NULL) || ((*win)->magic != WINDOW_MAGIC))
	return CS_BAD_HANDLE;
    return pcmcia_get_window(win, 0, req);
}

int pcmcia_get_next_window(window_handle_t *win, win_req_t *req)
{
    if ((win == NULL) || ((*win)->magic != WINDOW_MAGIC))
	return CS_BAD_HANDLE;
    return pcmcia_get_window(win, (*win)->index+1, req);
}

/*=====================================================================

    Return the PCI device associated with a card..

======================================================================*/

#ifdef CONFIG_CARDBUS

struct pci_bus *pcmcia_lookup_bus(client_handle_t handle)
{
	socket_info_t *s;

	if (CHECK_HANDLE(handle))
		return NULL;
	s = SOCKET(handle);
	if (!(s->state & SOCKET_CARDBUS))
		return NULL;

	return s->cap.cb_dev->subordinate;
}

EXPORT_SYMBOL(pcmcia_lookup_bus);

#endif

/*======================================================================

    Get the current socket state bits.  We don't support the latched
    SocketState yet: I haven't seen any point for it.
    
======================================================================*/

int pcmcia_get_status(client_handle_t handle, cs_status_t *status)
{
    socket_info_t *s;
    config_t *c;
    int val;
    
    if (CHECK_HANDLE(handle))
	return CS_BAD_HANDLE;
    s = SOCKET(handle);
    get_socket_status(s, &val);
    status->CardState = status->SocketState = 0;
    status->CardState |= (val & SS_DETECT) ? CS_EVENT_CARD_DETECT : 0;
    status->CardState |= (val & SS_CARDBUS) ? CS_EVENT_CB_DETECT : 0;
    status->CardState |= (val & SS_3VCARD) ? CS_EVENT_3VCARD : 0;
    status->CardState |= (val & SS_XVCARD) ? CS_EVENT_XVCARD : 0;
    if (s->state & SOCKET_SUSPEND)
	status->CardState |= CS_EVENT_PM_SUSPEND;
    if (!(s->state & SOCKET_PRESENT))
	return CS_NO_CARD;
    if (s->state & SOCKET_SETUP_PENDING)
	status->CardState |= CS_EVENT_CARD_INSERTION;
    
    /* Get info from the PRR, if necessary */
    if (handle->Function == BIND_FN_ALL) {
	if (status->Function && (status->Function >= s->functions))
	    return CS_BAD_ARGS;
	c = (s->config != NULL) ? &s->config[status->Function] : NULL;
    } else
	c = CONFIG(handle);
    if ((c != NULL) && (c->state & CONFIG_LOCKED) &&
	(c->IntType & (INT_MEMORY_AND_IO|INT_ZOOMED_VIDEO))) {
	u_char reg;
	if (c->Present & PRESENT_PIN_REPLACE) {
	    read_cis_mem(s, 1, (c->ConfigBase+CISREG_PRR)>>1, 1, &reg);
	    status->CardState |=
		(reg & PRR_WP_STATUS) ? CS_EVENT_WRITE_PROTECT : 0;
	    status->CardState |=
		(reg & PRR_READY_STATUS) ? CS_EVENT_READY_CHANGE : 0;
	    status->CardState |=
		(reg & PRR_BVD2_STATUS) ? CS_EVENT_BATTERY_LOW : 0;
	    status->CardState |=
		(reg & PRR_BVD1_STATUS) ? CS_EVENT_BATTERY_DEAD : 0;
	} else {
	    /* No PRR?  Then assume we're always ready */
	    status->CardState |= CS_EVENT_READY_CHANGE;
	}
	if (c->Present & PRESENT_EXT_STATUS) {
	    read_cis_mem(s, 1, (c->ConfigBase+CISREG_ESR)>>1, 1, &reg);
	    status->CardState |=
		(reg & ESR_REQ_ATTN) ? CS_EVENT_REQUEST_ATTENTION : 0;
	}
	return CS_SUCCESS;
    }
    status->CardState |=
	(val & SS_WRPROT) ? CS_EVENT_WRITE_PROTECT : 0;
    status->CardState |=
	(val & SS_BATDEAD) ? CS_EVENT_BATTERY_DEAD : 0;
    status->CardState |=
	(val & SS_BATWARN) ? CS_EVENT_BATTERY_LOW : 0;
    status->CardState |=
	(val & SS_READY) ? CS_EVENT_READY_CHANGE : 0;
    return CS_SUCCESS;
} /* get_status */

/*======================================================================

    Change the card address of an already open memory window.
    
======================================================================*/

int pcmcia_get_mem_page(window_handle_t win, memreq_t *req)
{
    if ((win == NULL) || (win->magic != WINDOW_MAGIC))
	return CS_BAD_HANDLE;
    req->Page = 0;
    req->CardOffset = win->ctl.card_start;
    return CS_SUCCESS;
} /* get_mem_page */

int pcmcia_map_mem_page(window_handle_t win, memreq_t *req)
{
    socket_info_t *s;
    if ((win == NULL) || (win->magic != WINDOW_MAGIC))
	return CS_BAD_HANDLE;
    if (req->Page != 0)
	return CS_BAD_PAGE;
    s = win->sock;
    win->ctl.card_start = req->CardOffset;
    if (set_mem_map(s, &win->ctl) != 0)
	return CS_BAD_OFFSET;
    return CS_SUCCESS;
} /* map_mem_page */

/*======================================================================

    Modify a locked socket configuration
    
======================================================================*/

int pcmcia_modify_configuration(client_handle_t handle,
				modconf_t *mod)
{
    socket_info_t *s;
    config_t *c;
    
    if (CHECK_HANDLE(handle))
	return CS_BAD_HANDLE;
    s = SOCKET(handle); c = CONFIG(handle);
    if (!(s->state & SOCKET_PRESENT))
	return CS_NO_CARD;
    if (!(c->state & CONFIG_LOCKED))
	return CS_CONFIGURATION_LOCKED;
    
    if (mod->Attributes & CONF_IRQ_CHANGE_VALID) {
	if (mod->Attributes & CONF_ENABLE_IRQ) {
	    c->Attributes |= CONF_ENABLE_IRQ;
	    s->socket.io_irq = s->irq.AssignedIRQ;
	} else {
	    c->Attributes &= ~CONF_ENABLE_IRQ;
	    s->socket.io_irq = 0;
	}
	set_socket(s, &s->socket);
    }

    if (mod->Attributes & CONF_VCC_CHANGE_VALID)
	return CS_BAD_VCC;

    /* We only allow changing Vpp1 and Vpp2 to the same value */
    if ((mod->Attributes & CONF_VPP1_CHANGE_VALID) &&
	(mod->Attributes & CONF_VPP2_CHANGE_VALID)) {
	if (mod->Vpp1 != mod->Vpp2)
	    return CS_BAD_VPP;
	c->Vpp1 = c->Vpp2 = s->socket.Vpp = mod->Vpp1;
	if (set_socket(s, &s->socket))
	    return CS_BAD_VPP;
    } else if ((mod->Attributes & CONF_VPP1_CHANGE_VALID) ||
	       (mod->Attributes & CONF_VPP2_CHANGE_VALID))
	return CS_BAD_VPP;

    return CS_SUCCESS;
} /* modify_configuration */

/*======================================================================

    Modify the attributes of a window returned by RequestWindow.

======================================================================*/

int pcmcia_modify_window(window_handle_t win, modwin_t *req)
{
    if ((win == NULL) || (win->magic != WINDOW_MAGIC))
	return CS_BAD_HANDLE;

    win->ctl.flags &= ~(MAP_ATTRIB|MAP_ACTIVE);
    if (req->Attributes & WIN_MEMORY_TYPE)
	win->ctl.flags |= MAP_ATTRIB;
    if (req->Attributes & WIN_ENABLE)
	win->ctl.flags |= MAP_ACTIVE;
    if (req->Attributes & WIN_DATA_WIDTH_16)
	win->ctl.flags |= MAP_16BIT;
    if (req->Attributes & WIN_USE_WAIT)
	win->ctl.flags |= MAP_USE_WAIT;
    win->ctl.speed = req->AccessSpeed;
    set_mem_map(win->sock, &win->ctl);
    
    return CS_SUCCESS;
} /* modify_window */

/*======================================================================

    Register_client() uses the dev_info_t handle to match the
    caller with a socket.  The driver must have already been bound
    to a socket with bind_device() -- in fact, bind_device()
    allocates the client structure that will be used.
    
======================================================================*/

int pcmcia_register_client(client_handle_t *handle, client_reg_t *req)
{
    client_t *client;
    socket_info_t *s;
    socket_t ns;
    
    /* Look for unbound client with matching dev_info */
    client = NULL;
    for (ns = 0; ns < sockets; ns++) {
	client = socket_table[ns]->clients;
	while (client != NULL) {
	    if ((strcmp(client->dev_info, (char *)req->dev_info) == 0)
		&& (client->state & CLIENT_UNBOUND)) break;
	    client = client->next;
	}
	if (client != NULL) break;
    }
    if (client == NULL)
	return CS_OUT_OF_RESOURCE;

    s = socket_table[ns];
    if (++s->real_clients == 1) {
	int status;
	register_callback(s, &parse_events, s);
	get_socket_status(s, &status);
	if ((status & SS_DETECT) &&
	    !(s->state & SOCKET_SETUP_PENDING)) {
	    s->state |= SOCKET_SETUP_PENDING;
	    if (setup_socket(s) == 0)
		    s->state &= ~SOCKET_SETUP_PENDING;
	}
    }

    *handle = client;
    client->state &= ~CLIENT_UNBOUND;
    client->Socket = ns;
    client->Attributes = req->Attributes;
    client->EventMask = req->EventMask;
    client->event_handler = req->event_handler;
    client->event_callback_args = req->event_callback_args;
    client->event_callback_args.client_handle = client;
    client->event_callback_args.bus = s->cap.bus;

    if (s->state & SOCKET_CARDBUS)
	client->state |= CLIENT_CARDBUS;
    
    if ((!(s->state & SOCKET_CARDBUS)) && (s->functions == 0) &&
	(client->Function != BIND_FN_ALL)) {
	cistpl_longlink_mfc_t mfc;
	if (read_tuple(client, CISTPL_LONGLINK_MFC, &mfc)
	    == CS_SUCCESS)
	    s->functions = mfc.nfn;
	else
	    s->functions = 1;
	s->config = kmalloc(sizeof(config_t) * s->functions,
			    GFP_KERNEL);
	if (!s->config)
		return CS_OUT_OF_RESOURCE;
	memset(s->config, 0, sizeof(config_t) * s->functions);
    }
    
    DEBUG(1, "cs: register_client(): client 0x%p, sock %d, dev %s\n",
	  client, client->Socket, client->dev_info);
    if (client->EventMask & CS_EVENT_REGISTRATION_COMPLETE)
	EVENT(client, CS_EVENT_REGISTRATION_COMPLETE, CS_EVENT_PRI_LOW);
    if ((socket_table[ns]->state & SOCKET_PRESENT) &&
	!(socket_table[ns]->state & SOCKET_SETUP_PENDING)) {
	if (client->EventMask & CS_EVENT_CARD_INSERTION)
	    EVENT(client, CS_EVENT_CARD_INSERTION, CS_EVENT_PRI_LOW);
	else
	    client->PendingEvents |= CS_EVENT_CARD_INSERTION;
    }
    return CS_SUCCESS;
} /* register_client */

/*====================================================================*/

int pcmcia_release_configuration(client_handle_t handle)
{
    pccard_io_map io = { 0, 0, 0, 0, 1 };
    socket_info_t *s;
    int i;
    
    if (CHECK_HANDLE(handle) ||
	!(handle->state & CLIENT_CONFIG_LOCKED))
	return CS_BAD_HANDLE;
    handle->state &= ~CLIENT_CONFIG_LOCKED;
    s = SOCKET(handle);
    
#ifdef CONFIG_CARDBUS
    if (handle->state & CLIENT_CARDBUS) {
	cb_disable(s);
	s->lock_count = 0;
	return CS_SUCCESS;
    }
#endif
    
    if (!(handle->state & CLIENT_STALE)) {
	config_t *c = CONFIG(handle);
	if (--(s->lock_count) == 0) {
	    s->socket.flags = SS_OUTPUT_ENA;   /* Is this correct? */
	    s->socket.Vpp = 0;
	    s->socket.io_irq = 0;
	    set_socket(s, &s->socket);
	}
	if (c->state & CONFIG_IO_REQ)
	    for (i = 0; i < MAX_IO_WIN; i++) {
		if (s->io[i].NumPorts == 0)
		    continue;
		s->io[i].Config--;
		if (s->io[i].Config != 0)
		    continue;
		io.map = i;
		set_io_map(s, &io);
	    }
	c->state &= ~CONFIG_LOCKED;
    }
    
    return CS_SUCCESS;
} /* release_configuration */

/*======================================================================

    Release_io() releases the I/O ranges allocated by a client.  This
    may be invoked some time after a card ejection has already dumped
    the actual socket configuration, so if the client is "stale", we
    don't bother checking the port ranges against the current socket
    values.
    
======================================================================*/

int pcmcia_release_io(client_handle_t handle, io_req_t *req)
{
    socket_info_t *s;
    
    if (CHECK_HANDLE(handle) || !(handle->state & CLIENT_IO_REQ))
	return CS_BAD_HANDLE;
    handle->state &= ~CLIENT_IO_REQ;
    s = SOCKET(handle);
    
#ifdef CONFIG_CARDBUS
    if (handle->state & CLIENT_CARDBUS) {
	cb_release(s);
	return CS_SUCCESS;
    }
#endif
    
    if (!(handle->state & CLIENT_STALE)) {
	config_t *c = CONFIG(handle);
	if (c->state & CONFIG_LOCKED)
	    return CS_CONFIGURATION_LOCKED;
	if ((c->io.BasePort1 != req->BasePort1) ||
	    (c->io.NumPorts1 != req->NumPorts1) ||
	    (c->io.BasePort2 != req->BasePort2) ||
	    (c->io.NumPorts2 != req->NumPorts2))
	    return CS_BAD_ARGS;
	c->state &= ~CONFIG_IO_REQ;
    }

    release_io_space(s, req->BasePort1, req->NumPorts1);
    if (req->NumPorts2)
	release_io_space(s, req->BasePort2, req->NumPorts2);
    
    return CS_SUCCESS;
} /* release_io */

/*====================================================================*/

int pcmcia_release_irq(client_handle_t handle, irq_req_t *req)
{
    socket_info_t *s;
    if (CHECK_HANDLE(handle) || !(handle->state & CLIENT_IRQ_REQ))
	return CS_BAD_HANDLE;
    handle->state &= ~CLIENT_IRQ_REQ;
    s = SOCKET(handle);
    
    if (!(handle->state & CLIENT_STALE)) {
	config_t *c = CONFIG(handle);
	if (c->state & CONFIG_LOCKED)
	    return CS_CONFIGURATION_LOCKED;
	if (c->irq.Attributes != req->Attributes)
	    return CS_BAD_ATTRIBUTE;
	if (s->irq.AssignedIRQ != req->AssignedIRQ)
	    return CS_BAD_IRQ;
	if (--s->irq.Config == 0) {
	    c->state &= ~CONFIG_IRQ_REQ;
	    s->irq.AssignedIRQ = 0;
	}
    }
    
    if (req->Attributes & IRQ_HANDLE_PRESENT) {
	bus_free_irq(s->cap.bus, req->AssignedIRQ, req->Instance);
    }

#ifdef CONFIG_ISA
    if (req->AssignedIRQ != s->cap.pci_irq)
	undo_irq(req->Attributes, req->AssignedIRQ);
#endif
    
    return CS_SUCCESS;
} /* cs_release_irq */

/*====================================================================*/

int pcmcia_release_window(window_handle_t win)
{
    socket_info_t *s;
    
    if ((win == NULL) || (win->magic != WINDOW_MAGIC))
	return CS_BAD_HANDLE;
    s = win->sock;
    if (!(win->handle->state & CLIENT_WIN_REQ(win->index)))
	return CS_BAD_HANDLE;

    /* Shut down memory window */
    win->ctl.flags &= ~MAP_ACTIVE;
    set_mem_map(s, &win->ctl);
    s->state &= ~SOCKET_WIN_REQ(win->index);

    /* Release system memory */
    if(!(s->cap.features & SS_CAP_STATIC_MAP))
	release_mem_region(win->base, win->size);
    win->handle->state &= ~CLIENT_WIN_REQ(win->index);

    win->magic = 0;
    
    return CS_SUCCESS;
} /* release_window */

/*====================================================================*/

int pcmcia_request_configuration(client_handle_t handle,
				 config_req_t *req)
{
    int i;
    u_int base;
    socket_info_t *s;
    config_t *c;
    pccard_io_map iomap;
    
    if (CHECK_HANDLE(handle))
	return CS_BAD_HANDLE;
    i = handle->Socket; s = socket_table[i];
    if (!(s->state & SOCKET_PRESENT))
	return CS_NO_CARD;
    
#ifdef CONFIG_CARDBUS
    if (handle->state & CLIENT_CARDBUS) {
	if (!(req->IntType & INT_CARDBUS))
	    return CS_UNSUPPORTED_MODE;
	if (s->lock_count != 0)
	    return CS_CONFIGURATION_LOCKED;
	cb_enable(s);
	handle->state |= CLIENT_CONFIG_LOCKED;
	s->lock_count++;
	return CS_SUCCESS;
    }
#endif
    
    if (req->IntType & INT_CARDBUS)
	return CS_UNSUPPORTED_MODE;
    c = CONFIG(handle);
    if (c->state & CONFIG_LOCKED)
	return CS_CONFIGURATION_LOCKED;

    /* Do power control.  We don't allow changes in Vcc. */
    if (s->socket.Vcc != req->Vcc)
	return CS_BAD_VCC;
    if (req->Vpp1 != req->Vpp2)
	return CS_BAD_VPP;
    s->socket.Vpp = req->Vpp1;
    if (set_socket(s, &s->socket))
	return CS_BAD_VPP;
    
    c->Vcc = req->Vcc; c->Vpp1 = c->Vpp2 = req->Vpp1;
    
    /* Pick memory or I/O card, DMA mode, interrupt */
    c->IntType = req->IntType;
    c->Attributes = req->Attributes;
    if (req->IntType & INT_MEMORY_AND_IO)
	s->socket.flags |= SS_IOCARD;
    if (req->IntType & INT_ZOOMED_VIDEO)
	s->socket.flags |= SS_ZVCARD|SS_IOCARD;
    if (req->Attributes & CONF_ENABLE_DMA)
	s->socket.flags |= SS_DMA_MODE;
    if (req->Attributes & CONF_ENABLE_SPKR)
	s->socket.flags |= SS_SPKR_ENA;
    if (req->Attributes & CONF_ENABLE_IRQ)
	s->socket.io_irq = s->irq.AssignedIRQ;
    else
	s->socket.io_irq = 0;
    set_socket(s, &s->socket);
    s->lock_count++;
    
    /* Set up CIS configuration registers */
    base = c->ConfigBase = req->ConfigBase;
    c->Present = c->CardValues = req->Present;
    if (req->Present & PRESENT_COPY) {
	c->Copy = req->Copy;
	write_cis_mem(s, 1, (base + CISREG_SCR)>>1, 1, &c->Copy);
    }
    if (req->Present & PRESENT_OPTION) {
	if (s->functions == 1) {
	    c->Option = req->ConfigIndex & COR_CONFIG_MASK;
	} else {
	    c->Option = req->ConfigIndex & COR_MFC_CONFIG_MASK;
	    c->Option |= COR_FUNC_ENA|COR_IREQ_ENA;
	    if (req->Present & PRESENT_IOBASE_0)
		c->Option |= COR_ADDR_DECODE;
	}
	if (c->state & CONFIG_IRQ_REQ)
	    if (!(c->irq.Attributes & IRQ_FORCED_PULSE))
		c->Option |= COR_LEVEL_REQ;
	write_cis_mem(s, 1, (base + CISREG_COR)>>1, 1, &c->Option);
	mdelay(40);
    }
    if (req->Present & PRESENT_STATUS) {
	c->Status = req->Status;
	write_cis_mem(s, 1, (base + CISREG_CCSR)>>1, 1, &c->Status);
    }
    if (req->Present & PRESENT_PIN_REPLACE) {
	c->Pin = req->Pin;
	write_cis_mem(s, 1, (base + CISREG_PRR)>>1, 1, &c->Pin);
    }
    if (req->Present & PRESENT_EXT_STATUS) {
	c->ExtStatus = req->ExtStatus;
	write_cis_mem(s, 1, (base + CISREG_ESR)>>1, 1, &c->ExtStatus);
    }
    if (req->Present & PRESENT_IOBASE_0) {
	u_char b = c->io.BasePort1 & 0xff;
	write_cis_mem(s, 1, (base + CISREG_IOBASE_0)>>1, 1, &b);
	b = (c->io.BasePort1 >> 8) & 0xff;
	write_cis_mem(s, 1, (base + CISREG_IOBASE_1)>>1, 1, &b);
    }
    if (req->Present & PRESENT_IOSIZE) {
	u_char b = c->io.NumPorts1 + c->io.NumPorts2 - 1;
	write_cis_mem(s, 1, (base + CISREG_IOSIZE)>>1, 1, &b);
    }
    
    /* Configure I/O windows */
    if (c->state & CONFIG_IO_REQ) {
	iomap.speed = io_speed;
	for (i = 0; i < MAX_IO_WIN; i++)
	    if (s->io[i].NumPorts != 0) {
		iomap.map = i;
		iomap.flags = MAP_ACTIVE;
		switch (s->io[i].Attributes & IO_DATA_PATH_WIDTH) {
		case IO_DATA_PATH_WIDTH_16:
		    iomap.flags |= MAP_16BIT; break;
		case IO_DATA_PATH_WIDTH_AUTO:
		    iomap.flags |= MAP_AUTOSZ; break;
		default:
		    break;
		}
		iomap.start = s->io[i].BasePort;
		iomap.stop = iomap.start + s->io[i].NumPorts - 1;
		set_io_map(s, &iomap);
		s->io[i].Config++;
	    }
    }
    
    c->state |= CONFIG_LOCKED;
    handle->state |= CLIENT_CONFIG_LOCKED;
    return CS_SUCCESS;
} /* request_configuration */

/*======================================================================
  
    Request_io() reserves ranges of port addresses for a socket.
    I have not implemented range sharing or alias addressing.
    
======================================================================*/

int pcmcia_request_io(client_handle_t handle, io_req_t *req)
{
    socket_info_t *s;
    config_t *c;
    
    if (CHECK_HANDLE(handle))
	return CS_BAD_HANDLE;
    s = SOCKET(handle);
    if (!(s->state & SOCKET_PRESENT))
	return CS_NO_CARD;

    if (handle->state & CLIENT_CARDBUS) {
#ifdef CONFIG_CARDBUS
	int ret = cb_config(s);
	if (ret == CS_SUCCESS)
	    handle->state |= CLIENT_IO_REQ;
	return ret;
#else
	return CS_UNSUPPORTED_FUNCTION;
#endif
    }

    if (!req)
	return CS_UNSUPPORTED_MODE;
    c = CONFIG(handle);
    if (c->state & CONFIG_LOCKED)
	return CS_CONFIGURATION_LOCKED;
    if (c->state & CONFIG_IO_REQ)
	return CS_IN_USE;
    if (req->Attributes1 & (IO_SHARED | IO_FORCE_ALIAS_ACCESS))
	return CS_BAD_ATTRIBUTE;
    if ((req->NumPorts2 > 0) &&
	(req->Attributes2 & (IO_SHARED | IO_FORCE_ALIAS_ACCESS)))
	return CS_BAD_ATTRIBUTE;

    if (alloc_io_space(s, req->Attributes1, &req->BasePort1,
		       req->NumPorts1, req->IOAddrLines,
		       handle->dev_info))
	return CS_IN_USE;

    if (req->NumPorts2) {
	if (alloc_io_space(s, req->Attributes2, &req->BasePort2,
			   req->NumPorts2, req->IOAddrLines,
			   handle->dev_info)) {
	    release_io_space(s, req->BasePort1, req->NumPorts1);
	    return CS_IN_USE;
	}
    }

    c->io = *req;
    c->state |= CONFIG_IO_REQ;
    handle->state |= CLIENT_IO_REQ;
    return CS_SUCCESS;
} /* request_io */

/*======================================================================

    Request_irq() reserves an irq for this client.

    Also, since Linux only reserves irq's when they are actually
    hooked, we don't guarantee that an irq will still be available
    when the configuration is locked.  Now that I think about it,
    there might be a way to fix this using a dummy handler.
    
======================================================================*/

int pcmcia_request_irq(client_handle_t handle, irq_req_t *req)
{
    socket_info_t *s;
    config_t *c;
    int ret = CS_IN_USE, irq = 0;
    
    if (CHECK_HANDLE(handle))
	return CS_BAD_HANDLE;
    s = SOCKET(handle);
    if (!(s->state & SOCKET_PRESENT))
	return CS_NO_CARD;
    c = CONFIG(handle);
    if (c->state & CONFIG_LOCKED)
	return CS_CONFIGURATION_LOCKED;
    if (c->state & CONFIG_IRQ_REQ)
	return CS_IN_USE;

#ifdef CONFIG_ISA
    if (s->irq.AssignedIRQ != 0) {
	/* If the interrupt is already assigned, it must match */
	irq = s->irq.AssignedIRQ;
	if (req->IRQInfo1 & IRQ_INFO2_VALID) {
	    u_int mask = req->IRQInfo2 & s->cap.irq_mask;
	    ret = ((mask >> irq) & 1) ? 0 : CS_BAD_ARGS;
	} else
	    ret = ((req->IRQInfo1&IRQ_MASK) == irq) ? 0 : CS_BAD_ARGS;
    } else {
	if (req->IRQInfo1 & IRQ_INFO2_VALID) {
	    u_int try, mask = req->IRQInfo2 & s->cap.irq_mask;
	    for (try = 0; try < 2; try++) {
		for (irq = 0; irq < 32; irq++)
		    if ((mask >> irq) & 1) {
			ret = try_irq(req->Attributes, irq, try);
			if (ret == 0) break;
		    }
		if (ret == 0) break;
	    }
	} else {
	    irq = req->IRQInfo1 & IRQ_MASK;
	    ret = try_irq(req->Attributes, irq, 1);
	}
    }
#endif
    if (ret != 0) {
	if (!s->cap.pci_irq)
	    return ret;
	irq = s->cap.pci_irq;
    }

    if (req->Attributes & IRQ_HANDLE_PRESENT) {
	if (bus_request_irq(s->cap.bus, irq, req->Handler,
			    ((req->Attributes & IRQ_TYPE_DYNAMIC_SHARING) || 
			     (s->functions > 1) ||
			     (irq == s->cap.pci_irq)) ? SA_SHIRQ : 0,
			    handle->dev_info, req->Instance))
	    return CS_IN_USE;
    }

    c->irq.Attributes = req->Attributes;
    s->irq.AssignedIRQ = req->AssignedIRQ = irq;
    s->irq.Config++;
    
    c->state |= CONFIG_IRQ_REQ;
    handle->state |= CLIENT_IRQ_REQ;
    return CS_SUCCESS;
} /* pcmcia_request_irq */

/*======================================================================

    Request_window() establishes a mapping between card memory space
    and system memory space.

======================================================================*/

int pcmcia_request_window(client_handle_t *handle, win_req_t *req, window_handle_t *wh)
{
    socket_info_t *s;
    window_t *win;
    u_long align;
    int w;
    
    if (CHECK_HANDLE(*handle))
	return CS_BAD_HANDLE;
    s = SOCKET(*handle);
    if (!(s->state & SOCKET_PRESENT))
	return CS_NO_CARD;
    if (req->Attributes & (WIN_PAGED | WIN_SHARED))
	return CS_BAD_ATTRIBUTE;

    /* Window size defaults to smallest available */
    if (req->Size == 0)
	req->Size = s->cap.map_size;
    align = (((s->cap.features & SS_CAP_MEM_ALIGN) ||
	      (req->Attributes & WIN_STRICT_ALIGN)) ?
	     req->Size : s->cap.map_size);
    if (req->Size & (s->cap.map_size-1))
	return CS_BAD_SIZE;
    if ((req->Base && (s->cap.features & SS_CAP_STATIC_MAP)) ||
	(req->Base & (align-1)))
	return CS_BAD_BASE;
    if (req->Base)
	align = 0;

    /* Allocate system memory window */
    for (w = 0; w < MAX_WIN; w++)
	if (!(s->state & SOCKET_WIN_REQ(w))) break;
    if (w == MAX_WIN)
	return CS_OUT_OF_RESOURCE;

    win = &s->win[w];
    win->magic = WINDOW_MAGIC;
    win->index = w;
    win->handle = *handle;
    win->sock = s;
    win->base = req->Base;
    win->size = req->Size;

    if (!(s->cap.features & SS_CAP_STATIC_MAP) &&
	find_mem_region(&win->base, win->size, align,
			(req->Attributes & WIN_MAP_BELOW_1MB) ||
			!(s->cap.features & SS_CAP_PAGE_REGS),
			(*handle)->dev_info, s))
	return CS_IN_USE;
    (*handle)->state |= CLIENT_WIN_REQ(w);

    /* Configure the socket controller */
    win->ctl.map = w+1;
    win->ctl.flags = 0;
    win->ctl.speed = req->AccessSpeed;
    if (req->Attributes & WIN_MEMORY_TYPE)
	win->ctl.flags |= MAP_ATTRIB;
    if (req->Attributes & WIN_ENABLE)
	win->ctl.flags |= MAP_ACTIVE;
    if (req->Attributes & WIN_DATA_WIDTH_16)
	win->ctl.flags |= MAP_16BIT;
    if (req->Attributes & WIN_USE_WAIT)
	win->ctl.flags |= MAP_USE_WAIT;
    win->ctl.sys_start = win->base;
    win->ctl.sys_stop = win->base + win->size-1;
    win->ctl.card_start = 0;
    if (set_mem_map(s, &win->ctl) != 0)
	return CS_BAD_ARGS;
    s->state |= SOCKET_WIN_REQ(w);

    /* Return window handle */
    req->Base = win->ctl.sys_start;
    *wh = win;
    
    return CS_SUCCESS;
} /* request_window */

/*======================================================================

    I'm not sure which "reset" function this is supposed to use,
    but for now, it uses the low-level interface's reset, not the
    CIS register.
    
======================================================================*/

int pcmcia_reset_card(client_handle_t handle, client_req_t *req)
{
    int i, ret;
    socket_info_t *s;
    
    if (CHECK_HANDLE(handle))
	return CS_BAD_HANDLE;
    i = handle->Socket; s = socket_table[i];
    if (!(s->state & SOCKET_PRESENT))
	return CS_NO_CARD;
    if (s->state & SOCKET_RESET_PENDING)
	return CS_IN_USE;
    s->state |= SOCKET_RESET_PENDING;

    ret = send_event(s, CS_EVENT_RESET_REQUEST, CS_EVENT_PRI_LOW);
    if (ret != 0) {
	s->state &= ~SOCKET_RESET_PENDING;
	handle->event_callback_args.info = (void *)(u_long)ret;
	EVENT(handle, CS_EVENT_RESET_COMPLETE, CS_EVENT_PRI_LOW);
    } else {
	DEBUG(1, "cs: resetting socket %d\n", i);
	send_event(s, CS_EVENT_RESET_PHYSICAL, CS_EVENT_PRI_LOW);
	s->reset_handle = handle;
	reset_socket(s);
    }
    return CS_SUCCESS;
} /* reset_card */

/*======================================================================

    These shut down or wake up a socket.  They are sort of user
    initiated versions of the APM suspend and resume actions.
    
======================================================================*/

int pcmcia_suspend_card(client_handle_t handle, client_req_t *req)
{
    int i;
    socket_info_t *s;
    
    if (CHECK_HANDLE(handle))
	return CS_BAD_HANDLE;
    i = handle->Socket; s = socket_table[i];
    if (!(s->state & SOCKET_PRESENT))
	return CS_NO_CARD;
    if (s->state & SOCKET_SUSPEND)
	return CS_IN_USE;

    DEBUG(1, "cs: suspending socket %d\n", i);
    send_event(s, CS_EVENT_PM_SUSPEND, CS_EVENT_PRI_LOW);
    suspend_socket(s);
    s->state |= SOCKET_SUSPEND;

    return CS_SUCCESS;
} /* suspend_card */

int pcmcia_resume_card(client_handle_t handle, client_req_t *req)
{
    int i;
    socket_info_t *s;
    
    if (CHECK_HANDLE(handle))
	return CS_BAD_HANDLE;
    i = handle->Socket; s = socket_table[i];
    if (!(s->state & SOCKET_PRESENT))
	return CS_NO_CARD;
    if (!(s->state & SOCKET_SUSPEND))
	return CS_IN_USE;

    DEBUG(1, "cs: waking up socket %d\n", i);
    setup_socket(s);

    return CS_SUCCESS;
} /* resume_card */

/*======================================================================

    These handle user requests to eject or insert a card.
    
======================================================================*/

int pcmcia_eject_card(client_handle_t handle, client_req_t *req)
{
    int i, ret;
    socket_info_t *s;
    u_long flags;
    
    if (CHECK_HANDLE(handle))
	return CS_BAD_HANDLE;
    i = handle->Socket; s = socket_table[i];
    if (!(s->state & SOCKET_PRESENT))
	return CS_NO_CARD;

    DEBUG(1, "cs: user eject request on socket %d\n", i);

    ret = send_event(s, CS_EVENT_EJECTION_REQUEST, CS_EVENT_PRI_LOW);
    if (ret != 0)
	return ret;

    spin_lock_irqsave(&s->lock, flags);
    do_shutdown(s);
    spin_unlock_irqrestore(&s->lock, flags);
    
    return CS_SUCCESS;
    
} /* eject_card */

int pcmcia_insert_card(client_handle_t handle, client_req_t *req)
{
    int i, status;
    socket_info_t *s;
    u_long flags;
    
    if (CHECK_HANDLE(handle))
	return CS_BAD_HANDLE;
    i = handle->Socket; s = socket_table[i];
    if (s->state & SOCKET_PRESENT)
	return CS_IN_USE;

    DEBUG(1, "cs: user insert request on socket %d\n", i);

    spin_lock_irqsave(&s->lock, flags);
    if (!(s->state & SOCKET_SETUP_PENDING)) {
	s->state |= SOCKET_SETUP_PENDING;
	spin_unlock_irqrestore(&s->lock, flags);
	get_socket_status(s, &status);
	if ((status & SS_DETECT) == 0 || (setup_socket(s) == 0)) {
	    s->state &= ~SOCKET_SETUP_PENDING;
	    return CS_NO_CARD;
	}
    } else
	spin_unlock_irqrestore(&s->lock, flags);

    return CS_SUCCESS;
} /* insert_card */

/*======================================================================

    Maybe this should send a CS_EVENT_CARD_INSERTION event if we
    haven't sent one to this client yet?
    
======================================================================*/

int pcmcia_set_event_mask(client_handle_t handle, eventmask_t *mask)
{
    u_int events, bit;
    if (CHECK_HANDLE(handle))
	return CS_BAD_HANDLE;
    if (handle->Attributes & CONF_EVENT_MASK_VALID)
	return CS_BAD_SOCKET;
    handle->EventMask = mask->EventMask;
    events = handle->PendingEvents & handle->EventMask;
    handle->PendingEvents -= events;
    while (events != 0) {
	bit = ((events ^ (events-1)) + 1) >> 1;
	EVENT(handle, bit, CS_EVENT_PRI_LOW);
	events -= bit;
    }
    return CS_SUCCESS;
} /* set_event_mask */

/*====================================================================*/

int pcmcia_report_error(client_handle_t handle, error_info_t *err)
{
    int i;
    char *serv;

    if (CHECK_HANDLE(handle))
	printk(KERN_NOTICE);
    else
	printk(KERN_NOTICE "%s: ", handle->dev_info);
    
    for (i = 0; i < SERVICE_COUNT; i++)
	if (service_table[i].key == err->func) break;
    if (i < SERVICE_COUNT)
	serv = service_table[i].msg;
    else
	serv = "Unknown service number";

    for (i = 0; i < ERROR_COUNT; i++)
	if (error_table[i].key == err->retcode) break;
    if (i < ERROR_COUNT)
	printk("%s: %s\n", serv, error_table[i].msg);
    else
	printk("%s: Unknown error code %#x\n", serv, err->retcode);

    return CS_SUCCESS;
} /* report_error */

/*====================================================================*/

int CardServices(int func, void *a1, void *a2, void *a3)
{

#ifdef PCMCIA_DEBUG
    if (pc_debug > 2) {
	int i;
	for (i = 0; i < SERVICE_COUNT; i++)
	    if (service_table[i].key == func) break;
	if (i < SERVICE_COUNT)
	    printk(KERN_DEBUG "cs: CardServices(%s, 0x%p, 0x%p)\n",
		   service_table[i].msg, a1, a2);
	else
	    printk(KERN_DEBUG "cs: CardServices(Unknown func %d, "
		   "0x%p, 0x%p)\n", func, a1, a2);
    }
#endif
    switch (func) {
    case AccessConfigurationRegister:
	return pcmcia_access_configuration_register(a1, a2); break;
    case AdjustResourceInfo:
	return pcmcia_adjust_resource_info(a1, a2); break;
    case CheckEraseQueue:
	return pcmcia_check_erase_queue(a1); break;
    case CloseMemory:
	return pcmcia_close_memory(a1); break;
    case CopyMemory:
	return pcmcia_copy_memory(a1, a2); break;
    case DeregisterClient:
	return pcmcia_deregister_client(a1); break;
    case DeregisterEraseQueue:
	return pcmcia_deregister_erase_queue(a1); break;
    case GetFirstClient:
	return pcmcia_get_first_client(a1, a2); break;
    case GetCardServicesInfo:
	return pcmcia_get_card_services_info(a1); break;
    case GetConfigurationInfo:
	return pcmcia_get_configuration_info(a1, a2); break;
    case GetNextClient:
	return pcmcia_get_next_client(a1, a2); break;
    case GetFirstRegion:
	return pcmcia_get_first_region(a1, a2); break;
    case GetFirstTuple:
	return pcmcia_get_first_tuple(a1, a2); break;
    case GetNextRegion:
	return pcmcia_get_next_region(a1, a2); break;
    case GetNextTuple:
	return pcmcia_get_next_tuple(a1, a2); break;
    case GetStatus:
	return pcmcia_get_status(a1, a2); break;
    case GetTupleData:
	return pcmcia_get_tuple_data(a1, a2); break;
    case MapMemPage:
	return pcmcia_map_mem_page(a1, a2); break;
    case ModifyConfiguration:
	return pcmcia_modify_configuration(a1, a2); break;
    case ModifyWindow:
	return pcmcia_modify_window(a1, a2); break;
    case OpenMemory:
/*	return pcmcia_open_memory(a1, a2); */
    {
	memory_handle_t m;
        int ret = pcmcia_open_memory(a1, a2, &m);
        *(memory_handle_t *)a1 = m;
	return  ret;
    }
        break;
    case ParseTuple:
	return pcmcia_parse_tuple(a1, a2, a3); break;
    case ReadMemory:
	return pcmcia_read_memory(a1, a2, a3); break;
    case RegisterClient:
	return pcmcia_register_client(a1, a2); break;
    case RegisterEraseQueue:
    {
	eraseq_handle_t w;
        int ret = pcmcia_register_erase_queue(a1, a2, &w);
        *(eraseq_handle_t *)a1 = w;
	return  ret;
    }
        break;
/*	return pcmcia_register_erase_queue(a1, a2); break; */

	return pcmcia_register_mtd(a1, a2); break;
    case ReleaseConfiguration:
	return pcmcia_release_configuration(a1); break;
    case ReleaseIO:
	return pcmcia_release_io(a1, a2); break;
    case ReleaseIRQ:
	return pcmcia_release_irq(a1, a2); break;
    case ReleaseWindow:
	return pcmcia_release_window(a1); break;
    case RequestConfiguration:
	return pcmcia_request_configuration(a1, a2); break;
    case RequestIO:
	return pcmcia_request_io(a1, a2); break;
    case RequestIRQ:
	return pcmcia_request_irq(a1, a2); break;
    case RequestWindow:
    {
	window_handle_t w;
        int ret = pcmcia_request_window(a1, a2, &w);
        *(window_handle_t *)a1 = w;
	return  ret;
    }
        break;
    case ResetCard:
	return pcmcia_reset_card(a1, a2); break;
    case SetEventMask:
	return pcmcia_set_event_mask(a1, a2); break;
    case ValidateCIS:
	return pcmcia_validate_cis(a1, a2); break;
    case WriteMemory:
	return pcmcia_write_memory(a1, a2, a3); break;
    case BindDevice:
	return pcmcia_bind_device(a1); break;
    case BindMTD:
	return pcmcia_bind_mtd(a1); break;
    case ReportError:
	return pcmcia_report_error(a1, a2); break;
    case SuspendCard:
	return pcmcia_suspend_card(a1, a2); break;
    case ResumeCard:
	return pcmcia_resume_card(a1, a2); break;
    case EjectCard:
	return pcmcia_eject_card(a1, a2); break;
    case InsertCard:
	return pcmcia_insert_card(a1, a2); break;
    case ReplaceCIS:
	return pcmcia_replace_cis(a1, a2); break;
    case GetFirstWindow:
	return pcmcia_get_first_window(a1, a2); break;
    case GetNextWindow:
	return pcmcia_get_next_window(a1, a2); break;
    case GetMemPage:
	return pcmcia_get_mem_page(a1, a2); break;
    default:
	return CS_UNSUPPORTED_FUNCTION; break;
    }
    
} /* CardServices */

/*======================================================================

    OS-specific module glue goes here
    
======================================================================*/
/* in alpha order */
EXPORT_SYMBOL(pcmcia_access_configuration_register);
EXPORT_SYMBOL(pcmcia_adjust_resource_info);
EXPORT_SYMBOL(pcmcia_bind_device);
EXPORT_SYMBOL(pcmcia_bind_mtd);
EXPORT_SYMBOL(pcmcia_check_erase_queue);
EXPORT_SYMBOL(pcmcia_close_memory);
EXPORT_SYMBOL(pcmcia_copy_memory);
EXPORT_SYMBOL(pcmcia_deregister_client);
EXPORT_SYMBOL(pcmcia_deregister_erase_queue);
EXPORT_SYMBOL(pcmcia_eject_card);
EXPORT_SYMBOL(pcmcia_get_first_client);
EXPORT_SYMBOL(pcmcia_get_card_services_info);
EXPORT_SYMBOL(pcmcia_get_configuration_info);
EXPORT_SYMBOL(pcmcia_get_mem_page);
EXPORT_SYMBOL(pcmcia_get_next_client);
EXPORT_SYMBOL(pcmcia_get_first_region);
EXPORT_SYMBOL(pcmcia_get_first_tuple);
EXPORT_SYMBOL(pcmcia_get_first_window);
EXPORT_SYMBOL(pcmcia_get_next_region);
EXPORT_SYMBOL(pcmcia_get_next_tuple);
EXPORT_SYMBOL(pcmcia_get_next_window);
EXPORT_SYMBOL(pcmcia_get_status);
EXPORT_SYMBOL(pcmcia_get_tuple_data);
EXPORT_SYMBOL(pcmcia_insert_card);
EXPORT_SYMBOL(pcmcia_map_mem_page);
EXPORT_SYMBOL(pcmcia_modify_configuration);
EXPORT_SYMBOL(pcmcia_modify_window);
EXPORT_SYMBOL(pcmcia_open_memory);
EXPORT_SYMBOL(pcmcia_parse_tuple);
EXPORT_SYMBOL(pcmcia_read_memory);
EXPORT_SYMBOL(pcmcia_register_client);
EXPORT_SYMBOL(pcmcia_register_erase_queue);
EXPORT_SYMBOL(pcmcia_register_mtd);
EXPORT_SYMBOL(pcmcia_release_configuration);
EXPORT_SYMBOL(pcmcia_release_io);
EXPORT_SYMBOL(pcmcia_release_irq);
EXPORT_SYMBOL(pcmcia_release_window);
EXPORT_SYMBOL(pcmcia_replace_cis);
EXPORT_SYMBOL(pcmcia_report_error);
EXPORT_SYMBOL(pcmcia_request_configuration);
EXPORT_SYMBOL(pcmcia_request_io);
EXPORT_SYMBOL(pcmcia_request_irq);
EXPORT_SYMBOL(pcmcia_request_window);
EXPORT_SYMBOL(pcmcia_reset_card);
EXPORT_SYMBOL(pcmcia_resume_card);
EXPORT_SYMBOL(pcmcia_set_event_mask);
EXPORT_SYMBOL(pcmcia_suspend_card);
EXPORT_SYMBOL(pcmcia_validate_cis);
EXPORT_SYMBOL(pcmcia_write_memory);

EXPORT_SYMBOL(dead_socket);
EXPORT_SYMBOL(register_ss_entry);
EXPORT_SYMBOL(unregister_ss_entry);
EXPORT_SYMBOL(CardServices);
EXPORT_SYMBOL(MTDHelperEntry);
#ifdef CONFIG_PROC_FS
EXPORT_SYMBOL(proc_pccard);
#endif

EXPORT_SYMBOL(pcmcia_register_socket);
EXPORT_SYMBOL(pcmcia_unregister_socket);
EXPORT_SYMBOL(pcmcia_suspend_socket);
EXPORT_SYMBOL(pcmcia_resume_socket);

static int __init init_pcmcia_cs(void)
{
    printk(KERN_INFO "%s\n", release);
    printk(KERN_INFO "  %s\n", options);
    DEBUG(0, "%s\n", version);
    if (do_apm)
	pm_register(PM_SYS_DEV, PM_SYS_PCMCIA, handle_pm_event);
#ifdef CONFIG_PROC_FS
    proc_pccard = proc_mkdir("pccard", proc_bus);
#endif
    return 0;
}

static void __exit exit_pcmcia_cs(void)
{
    printk(KERN_INFO "unloading Kernel Card Services\n");
#ifdef CONFIG_PROC_FS
    if (proc_pccard) {
	remove_proc_entry("pccard", proc_bus);
    }
#endif
    if (do_apm)
	pm_unregister_all(handle_pm_event);
    release_resource_db();
}

module_init(init_pcmcia_cs);
module_exit(exit_pcmcia_cs);

/*====================================================================*/

