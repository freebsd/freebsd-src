/*======================================================================

    Device driver for the PCMCIA control functionality of StrongARM
    SA-1100 microprocessors.

    The contents of this file are subject to the Mozilla Public
    License Version 1.1 (the "License"); you may not use this file
    except in compliance with the License. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

    Software distributed under the License is distributed on an "AS
    IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
    implied. See the License for the specific language governing
    rights and limitations under the License.

    The initial developer of the original code is John G. Dorsey
    <john+@cs.cmu.edu>.  Portions created by John G. Dorsey are
    Copyright (C) 1999 John G. Dorsey.  All Rights Reserved.

    Alternatively, the contents of this file may be used under the
    terms of the GNU Public License version 2 (the "GPL"), in which
    case the provisions of the GPL are applicable instead of the
    above.  If you wish to allow the use of your version of this file
    only under the terms of the GPL and not to allow others to use
    your version of this file under the MPL, indicate your decision
    by deleting the provisions above and replace them with the notice
    and other provisions required by the GPL.  If you do not delete
    the provisions above, a recipient may use your version of this
    file under either the MPL or the GPL.
    
======================================================================*/
/*
 * Please see linux/Documentation/arm/SA1100/PCMCIA for more information
 * on the low-level kernel interface.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/config.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/tqueue.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/notifier.h>
#include <linux/proc_fs.h>
#include <linux/version.h>

#include <pcmcia/version.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/cs.h>
#include <pcmcia/ss.h>
#include <pcmcia/bus_ops.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/arch/assabet.h>

#include "sa1100.h"

#ifdef PCMCIA_DEBUG
static int pc_debug;
#endif

/* This structure maintains housekeeping state for each socket, such
 * as the last known values of the card detect pins, or the Card Services
 * callback value associated with the socket:
 */
static int sa1100_pcmcia_socket_count;
static struct sa1100_pcmcia_socket sa1100_pcmcia_socket[SA1100_PCMCIA_MAX_SOCK];

#define PCMCIA_SOCKET(x)	(sa1100_pcmcia_socket + (x))

/* Returned by the low-level PCMCIA interface: */
static struct pcmcia_low_level *pcmcia_low_level;

static struct timer_list poll_timer;
static struct tq_struct sa1100_pcmcia_task;

/*
 * sa1100_pcmcia_state_to_config
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 *
 * Convert PCMCIA socket state to our socket configure structure.
 */
static struct pcmcia_configure
sa1100_pcmcia_state_to_config(unsigned int sock, socket_state_t *state)
{
  struct pcmcia_configure conf;

  conf.sock    = sock;
  conf.vcc     = state->Vcc;
  conf.vpp     = state->Vpp;
  conf.output  = state->flags & SS_OUTPUT_ENA ? 1 : 0;
  conf.speaker = state->flags & SS_SPKR_ENA ? 1 : 0;
  conf.reset   = state->flags & SS_RESET ? 1 : 0;
  conf.irq     = state->io_irq != 0;

  return conf;
}

/* sa1100_pcmcia_init()
 * ^^^^^^^^^^^^^^^^^^^^
 *
 * (Re-)Initialise the socket, turning on status interrupts
 * and PCMCIA bus.  This must wait for power to stabilise
 * so that the card status signals report correctly.
 *
 * Returns: 0
 */
static int sa1100_pcmcia_init(unsigned int sock)
{
  struct sa1100_pcmcia_socket *skt = PCMCIA_SOCKET(sock);
  struct pcmcia_configure conf;

  DEBUG(2, "%s(): initializing socket %u\n", __FUNCTION__, sock);

  skt->cs_state = dead_socket;

  conf = sa1100_pcmcia_state_to_config(sock, &dead_socket);

  pcmcia_low_level->configure_socket(&conf);

  return pcmcia_low_level->socket_init(sock);
}


/*
 * sa1100_pcmcia_suspend()
 * ^^^^^^^^^^^^^^^^^^^^^^^
 *
 * Remove power on the socket, disable IRQs from the card.
 * Turn off status interrupts, and disable the PCMCIA bus.
 *
 * Returns: 0
 */
static int sa1100_pcmcia_suspend(unsigned int sock)
{
  struct sa1100_pcmcia_socket *skt = PCMCIA_SOCKET(sock);
  struct pcmcia_configure conf;
  int ret;

  DEBUG(2, "%s(): suspending socket %u\n", __FUNCTION__, sock);

  conf = sa1100_pcmcia_state_to_config(sock, &dead_socket);

  ret = pcmcia_low_level->configure_socket(&conf);

  if (ret == 0) {
    skt->cs_state = dead_socket;
    ret = pcmcia_low_level->socket_suspend(sock);
  }

  return ret;
}


/* sa1100_pcmcia_events()
 * ^^^^^^^^^^^^^^^^^^^^^^
 * Helper routine to generate a Card Services event mask based on
 * state information obtained from the kernel low-level PCMCIA layer
 * in a recent (and previous) sampling. Updates `prev_state'.
 *
 * Returns: an event mask for the given socket state.
 */
static inline unsigned int
sa1100_pcmcia_events(struct pcmcia_state *state,
		     struct pcmcia_state *prev_state,
		     unsigned int mask, unsigned int flags)
{
  unsigned int events = 0;

  if (state->detect != prev_state->detect) {
    DEBUG(3, "%s(): card detect value %u\n", __FUNCTION__, state->detect);

    events |= SS_DETECT;
  }

  if (state->ready != prev_state->ready) {
    DEBUG(3, "%s(): card ready value %u\n", __FUNCTION__, state->ready);

    events |= flags & SS_IOCARD ? 0 : SS_READY;
  }

  if (state->bvd1 != prev_state->bvd1) {
    DEBUG(3, "%s(): card BVD1 value %u\n", __FUNCTION__, state->bvd1);

    events |= flags & SS_IOCARD ? SS_STSCHG : SS_BATDEAD;
  }

  if (state->bvd2 != prev_state->bvd2) {
    DEBUG(3, "%s(): card BVD2 value %u\n", __FUNCTION__, state->bvd2);

    events |= flags & SS_IOCARD ? 0 : SS_BATWARN;
  }

  *prev_state = *state;

  events &= mask;

  DEBUG(2, "events: %s%s%s%s%s%s\n",
	events == 0         ? "<NONE>"   : "",
	events & SS_DETECT  ? "DETECT "  : "",
	events & SS_READY   ? "READY "   : "",
	events & SS_BATDEAD ? "BATDEAD " : "",
	events & SS_BATWARN ? "BATWARN " : "",
	events & SS_STSCHG  ? "STSCHG "  : "");

  return events;
}  /* sa1100_pcmcia_events() */


/* sa1100_pcmcia_task_handler()
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 * Processes serviceable socket events using the "eventd" thread context.
 *
 * Event processing (specifically, the invocation of the Card Services event
 * callback) occurs in this thread rather than in the actual interrupt
 * handler due to the use of scheduling operations in the PCMCIA core.
 */
static void sa1100_pcmcia_task_handler(void *data)
{
  struct pcmcia_state state[SA1100_PCMCIA_MAX_SOCK];
  struct pcmcia_state_array state_array;
  unsigned int all_events;

  DEBUG(4, "%s(): entering PCMCIA monitoring thread\n", __FUNCTION__);

  state_array.size = sa1100_pcmcia_socket_count;
  state_array.state = state;

  do {
    unsigned int events;
    int ret, i;

    memset(state, 0, sizeof(state));

    DEBUG(4, "%s(): interrogating low-level PCMCIA service\n", __FUNCTION__);

    ret = pcmcia_low_level->socket_state(&state_array);
    if (ret < 0) {
      printk(KERN_ERR "sa1100_pcmcia: unable to read socket status\n");
      break;
    }

    all_events = 0;

    for (i = 0; i < state_array.size; i++, all_events |= events) {
      struct sa1100_pcmcia_socket *skt = PCMCIA_SOCKET(i);

      events = sa1100_pcmcia_events(&state[i], &skt->k_state,
				    skt->cs_state.csc_mask,
				    skt->cs_state.flags);

      if (events && sa1100_pcmcia_socket[i].handler != NULL)
	skt->handler(skt->handler_info, events);
    }
  } while(all_events);
}  /* sa1100_pcmcia_task_handler() */

static struct tq_struct sa1100_pcmcia_task = {
	routine: sa1100_pcmcia_task_handler
};


/* sa1100_pcmcia_poll_event()
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^
 * Let's poll for events in addition to IRQs since IRQ only is unreliable...
 */
static void sa1100_pcmcia_poll_event(unsigned long dummy)
{
  DEBUG(4, "%s(): polling for events\n", __FUNCTION__);
  poll_timer.function = sa1100_pcmcia_poll_event;
  poll_timer.expires = jiffies + SA1100_PCMCIA_POLL_PERIOD;
  add_timer(&poll_timer);
  schedule_task(&sa1100_pcmcia_task);
}


/* sa1100_pcmcia_interrupt()
 * ^^^^^^^^^^^^^^^^^^^^^^^^^
 * Service routine for socket driver interrupts (requested by the
 * low-level PCMCIA init() operation via sa1100_pcmcia_thread()).
 * The actual interrupt-servicing work is performed by
 * sa1100_pcmcia_thread(), largely because the Card Services event-
 * handling code performs scheduling operations which cannot be
 * executed from within an interrupt context.
 */
static void sa1100_pcmcia_interrupt(int irq, void *dev, struct pt_regs *regs)
{
  DEBUG(3, "%s(): servicing IRQ %d\n", __FUNCTION__, irq);
  schedule_task(&sa1100_pcmcia_task);
}


/* sa1100_pcmcia_register_callback()
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 * Implements the register_callback() operation for the in-kernel
 * PCMCIA service (formerly SS_RegisterCallback in Card Services). If 
 * the function pointer `handler' is not NULL, remember the callback 
 * location in the state for `sock', and increment the usage counter 
 * for the driver module. (The callback is invoked from the interrupt
 * service routine, sa1100_pcmcia_interrupt(), to notify Card Services
 * of interesting events.) Otherwise, clear the callback pointer in the
 * socket state and decrement the module usage count.
 *
 * Returns: 0
 */
static int
sa1100_pcmcia_register_callback(unsigned int sock,
				void (*handler)(void *, unsigned int),
				void *info)
{
  struct sa1100_pcmcia_socket *skt = PCMCIA_SOCKET(sock);

  if (handler == NULL) {
    skt->handler = NULL;
    MOD_DEC_USE_COUNT;
  } else {
    MOD_INC_USE_COUNT;
    skt->handler_info = info;
    skt->handler = handler;
  }

  return 0;
}


/* sa1100_pcmcia_inquire_socket()
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 * Implements the inquire_socket() operation for the in-kernel PCMCIA
 * service (formerly SS_InquireSocket in Card Services). Of note is
 * the setting of the SS_CAP_PAGE_REGS bit in the `features' field of
 * `cap' to "trick" Card Services into tolerating large "I/O memory" 
 * addresses. Also set is SS_CAP_STATIC_MAP, which disables the memory
 * resource database check. (Mapped memory is set up within the socket
 * driver itself.)
 *
 * In conjunction with the STATIC_MAP capability is a new field,
 * `io_offset', recommended by David Hinds. Rather than go through
 * the SetIOMap interface (which is not quite suited for communicating
 * window locations up from the socket driver), we just pass up
 * an offset which is applied to client-requested base I/O addresses
 * in alloc_io_space().
 *
 * SS_CAP_PAGE_REGS: used by setup_cis_mem() in cistpl.c to set the
 *   force_low argument to validate_mem() in rsrc_mgr.c -- since in
 *   general, the mapped * addresses of the PCMCIA memory regions
 *   will not be within 0xffff, setting force_low would be
 *   undesirable.
 *
 * SS_CAP_STATIC_MAP: don't bother with the (user-configured) memory
 *   resource database; we instead pass up physical address ranges
 *   and allow other parts of Card Services to deal with remapping.
 *
 * SS_CAP_PCCARD: we can deal with 16-bit PCMCIA & CF cards, but
 *   not 32-bit CardBus devices.
 *
 * Return value is irrelevant; the pcmcia subsystem ignores it.
 */
static int
sa1100_pcmcia_inquire_socket(unsigned int sock, socket_cap_t *cap)
{
  struct sa1100_pcmcia_socket *skt = PCMCIA_SOCKET(sock);
  int ret = -1;

  DEBUG(2, "%s() for sock %u\n", __FUNCTION__, sock);

  if (sock < sa1100_pcmcia_socket_count) {
    cap->features  = SS_CAP_PAGE_REGS | SS_CAP_STATIC_MAP | SS_CAP_PCCARD;
    cap->irq_mask  = 0;
    cap->map_size  = PAGE_SIZE;
    cap->pci_irq   = skt->irq;
    cap->io_offset = (unsigned long)skt->virt_io;

    ret = 0;
  }

  return ret;
}


/* sa1100_pcmcia_get_status()
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^
 * Implements the get_status() operation for the in-kernel PCMCIA
 * service (formerly SS_GetStatus in Card Services). Essentially just
 * fills in bits in `status' according to internal driver state or
 * the value of the voltage detect chipselect register.
 *
 * As a debugging note, during card startup, the PCMCIA core issues
 * three set_socket() commands in a row the first with RESET deasserted,
 * the second with RESET asserted, and the last with RESET deasserted
 * again. Following the third set_socket(), a get_status() command will
 * be issued. The kernel is looking for the SS_READY flag (see
 * setup_socket(), reset_socket(), and unreset_socket() in cs.c).
 *
 * Returns: 0
 */
static int
sa1100_pcmcia_get_status(unsigned int sock, unsigned int *status)
{
  struct sa1100_pcmcia_socket *skt = PCMCIA_SOCKET(sock);
  struct pcmcia_state state[SA1100_PCMCIA_MAX_SOCK];
  struct pcmcia_state_array state_array;
  unsigned int stat;

  DEBUG(2, "%s() for sock %u\n", __FUNCTION__, sock);

  state_array.size = sa1100_pcmcia_socket_count;
  state_array.state = state;

  memset(state, 0, sizeof(state));

  if ((pcmcia_low_level->socket_state(&state_array)) < 0) {
    printk(KERN_ERR "sa1100_pcmcia: unable to get socket status\n");
    return -1;
  }

  skt->k_state = state[sock];

  stat = state[sock].detect ? SS_DETECT : 0;
  stat |= state[sock].ready ? SS_READY  : 0;
  stat |= state[sock].vs_3v ? SS_3VCARD : 0;
  stat |= state[sock].vs_Xv ? SS_XVCARD : 0;

  /* The power status of individual sockets is not available
   * explicitly from the hardware, so we just remember the state
   * and regurgitate it upon request:
   */
  stat |= skt->cs_state.Vcc ? SS_POWERON : 0;

  if (skt->cs_state.flags & SS_IOCARD)
    stat |= state[sock].bvd1 ? SS_STSCHG : 0;
  else {
    if (state[sock].bvd1 == 0)
      stat |= SS_BATDEAD;
    else if (state[sock].bvd2 == 0)
      stat |= SS_BATWARN;
  }

  DEBUG(3, "\tstatus: %s%s%s%s%s%s%s%s\n",
	stat & SS_DETECT  ? "DETECT "  : "",
	stat & SS_READY   ? "READY "   : "", 
	stat & SS_BATDEAD ? "BATDEAD " : "",
	stat & SS_BATWARN ? "BATWARN " : "",
	stat & SS_POWERON ? "POWERON " : "",
	stat & SS_STSCHG  ? "STSCHG "  : "",
	stat & SS_3VCARD  ? "3VCARD "  : "",
	stat & SS_XVCARD  ? "XVCARD "  : "");

  *status = stat;

  return 0;
}  /* sa1100_pcmcia_get_status() */


/* sa1100_pcmcia_get_socket()
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^
 * Implements the get_socket() operation for the in-kernel PCMCIA
 * service (formerly SS_GetSocket in Card Services). Not a very 
 * exciting routine.
 *
 * Returns: 0
 */
static int
sa1100_pcmcia_get_socket(unsigned int sock, socket_state_t *state)
{
  struct sa1100_pcmcia_socket *skt = PCMCIA_SOCKET(sock);

  DEBUG(2, "%s() for sock %u\n", __FUNCTION__, sock);

  *state = skt->cs_state;

  return 0;
}

/* sa1100_pcmcia_set_socket()
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^
 * Implements the set_socket() operation for the in-kernel PCMCIA
 * service (formerly SS_SetSocket in Card Services). We more or
 * less punt all of this work and let the kernel handle the details
 * of power configuration, reset, &c. We also record the value of
 * `state' in order to regurgitate it to the PCMCIA core later.
 *
 * Returns: 0
 */
static int
sa1100_pcmcia_set_socket(unsigned int sock, socket_state_t *state)
{
  struct sa1100_pcmcia_socket *skt = PCMCIA_SOCKET(sock);
  struct pcmcia_configure conf;

  DEBUG(2, "%s() for sock %u\n", __FUNCTION__, sock);

  DEBUG(3, "\tmask:  %s%s%s%s%s%s\n\tflags: %s%s%s%s%s%s\n",
	(state->csc_mask==0)?"<NONE>":"",
	(state->csc_mask&SS_DETECT)?"DETECT ":"",
	(state->csc_mask&SS_READY)?"READY ":"",
	(state->csc_mask&SS_BATDEAD)?"BATDEAD ":"",
	(state->csc_mask&SS_BATWARN)?"BATWARN ":"",
	(state->csc_mask&SS_STSCHG)?"STSCHG ":"",
	(state->flags==0)?"<NONE>":"",
	(state->flags&SS_PWR_AUTO)?"PWR_AUTO ":"",
	(state->flags&SS_IOCARD)?"IOCARD ":"",
	(state->flags&SS_RESET)?"RESET ":"",
	(state->flags&SS_SPKR_ENA)?"SPKR_ENA ":"",
	(state->flags&SS_OUTPUT_ENA)?"OUTPUT_ENA ":"");
  DEBUG(3, "\tVcc %d  Vpp %d  irq %d\n",
	state->Vcc, state->Vpp, state->io_irq);

  conf = sa1100_pcmcia_state_to_config(sock, state);

  if (pcmcia_low_level->configure_socket(&conf) < 0) {
    printk(KERN_ERR "sa1100_pcmcia: unable to configure socket %d\n", sock);
    return -1;
  }

  skt->cs_state = *state;
  
  return 0;
}  /* sa1100_pcmcia_set_socket() */


/* sa1100_pcmcia_get_io_map()
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^
 * Implements the get_io_map() operation for the in-kernel PCMCIA
 * service (formerly SS_GetIOMap in Card Services). Just returns an
 * I/O map descriptor which was assigned earlier by a set_io_map().
 *
 * Returns: 0 on success, -1 if the map index was out of range
 */
static int
sa1100_pcmcia_get_io_map(unsigned int sock, struct pccard_io_map *map)
{
  struct sa1100_pcmcia_socket *skt = PCMCIA_SOCKET(sock);
  int ret = -1;

  DEBUG(2, "%s() for sock %u\n", __FUNCTION__, sock);

  if (map->map < MAX_IO_WIN) {
    *map = skt->io_map[map->map];
    ret = 0;
  }

  return ret;
}


/* sa1100_pcmcia_set_io_map()
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^
 * Implements the set_io_map() operation for the in-kernel PCMCIA
 * service (formerly SS_SetIOMap in Card Services). We configure
 * the map speed as requested, but override the address ranges
 * supplied by Card Services.
 *
 * Returns: 0 on success, -1 on error
 */
static int
sa1100_pcmcia_set_io_map(unsigned int sock, struct pccard_io_map *map)
{
  struct sa1100_pcmcia_socket *skt = PCMCIA_SOCKET(sock);

  DEBUG(2, "%s() for sock %u\n", __FUNCTION__, sock);

  DEBUG(3, "\tmap %u  speed %u\n\tstart 0x%08x  stop 0x%08x\n",
	map->map, map->speed, map->start, map->stop);
  DEBUG(3, "\tflags: %s%s%s%s%s%s%s%s\n",
	(map->flags==0)?"<NONE>":"",
	(map->flags&MAP_ACTIVE)?"ACTIVE ":"",
	(map->flags&MAP_16BIT)?"16BIT ":"",
	(map->flags&MAP_AUTOSZ)?"AUTOSZ ":"",
	(map->flags&MAP_0WS)?"0WS ":"",
	(map->flags&MAP_WRPROT)?"WRPROT ":"",
	(map->flags&MAP_USE_WAIT)?"USE_WAIT ":"",
	(map->flags&MAP_PREFETCH)?"PREFETCH ":"");

  if (map->map >= MAX_IO_WIN) {
    printk(KERN_ERR "%s(): map (%d) out of range\n", __FUNCTION__,
	   map->map);
    return -1;
  }

  if (map->flags & MAP_ACTIVE) {
    unsigned int clock, speed = map->speed;
    unsigned long mecr;

    if (speed == 0)
      speed = SA1100_PCMCIA_IO_ACCESS;

    clock = cpufreq_get(0);

    mecr = MECR;

    MECR_BSIO_SET(mecr, sock, sa1100_pcmcia_mecr_bs(speed, clock));

    skt->speed_io = speed;

    DEBUG(4, "%s(): FAST%u %lx  BSM%u %lx  BSA%u %lx  BSIO%u %lx\n",
	  __FUNCTION__, sock, MECR_FAST_GET(mecr, sock), sock,
	  MECR_BSM_GET(mecr, sock), sock, MECR_BSA_GET(mecr, sock), 
	  sock, MECR_BSIO_GET(mecr, sock));

    MECR = mecr;
  }

  if (map->stop == 1)
    map->stop = PAGE_SIZE-1;

  map->stop -= map->start;
  map->stop += (unsigned long)skt->virt_io;
  map->start = (unsigned long)skt->virt_io;

  skt->io_map[map->map] = *map;

  return 0;
}  /* sa1100_pcmcia_set_io_map() */


/* sa1100_pcmcia_get_mem_map()
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^^
 * Implements the get_mem_map() operation for the in-kernel PCMCIA
 * service (formerly SS_GetMemMap in Card Services). Just returns a
 *  memory map descriptor which was assigned earlier by a
 *  set_mem_map() request.
 *
 * Returns: 0 on success, -1 if the map index was out of range
 */
static int
sa1100_pcmcia_get_mem_map(unsigned int sock, struct pccard_mem_map *map)
{
  struct sa1100_pcmcia_socket *skt = PCMCIA_SOCKET(sock);
  int ret = -1;

  DEBUG(2, "%s() for sock %u\n", __FUNCTION__, sock);

  if (map->map < MAX_WIN) {
    *map = skt->mem_map[map->map];
    ret = 0;
  }

  return ret;
}


/* sa1100_pcmcia_set_mem_map()
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^^
 * Implements the set_mem_map() operation for the in-kernel PCMCIA
 * service (formerly SS_SetMemMap in Card Services). We configure
 * the map speed as requested, but override the address ranges
 * supplied by Card Services.
 *
 * Returns: 0 on success, -1 on error
 */
static int
sa1100_pcmcia_set_mem_map(unsigned int sock, struct pccard_mem_map *map)
{
  struct sa1100_pcmcia_socket *skt = PCMCIA_SOCKET(sock);
  unsigned long start;

  DEBUG(2, "%s() for sock %u\n", __FUNCTION__, sock);

  DEBUG(3, "\tmap %u speed %u sys_start %08lx sys_stop %08lx card_start %08x\n",
	map->map, map->speed, map->sys_start, map->sys_stop, map->card_start);
  DEBUG(3, "\tflags: %s%s%s%s%s%s%s%s\n",
	(map->flags==0)?"<NONE>":"",
	(map->flags&MAP_ACTIVE)?"ACTIVE ":"",
	(map->flags&MAP_16BIT)?"16BIT ":"",
	(map->flags&MAP_AUTOSZ)?"AUTOSZ ":"",
	(map->flags&MAP_0WS)?"0WS ":"",
	(map->flags&MAP_WRPROT)?"WRPROT ":"",
	(map->flags&MAP_ATTRIB)?"ATTRIB ":"",
	(map->flags&MAP_USE_WAIT)?"USE_WAIT ":"");

  if (map->map >= MAX_WIN){
    printk(KERN_ERR "%s(): map (%d) out of range\n", __FUNCTION__,
	   map->map);
    return -1;
  }

  if (map->flags & MAP_ACTIVE) {
    unsigned int clock, speed = map->speed;
    unsigned long mecr;

    /*
     * When clients issue RequestMap, the access speed is not always
     * properly configured.  Choose some sensible defaults.
     */
    if (speed == 0) {
      if (skt->cs_state.Vcc == 33)
	speed = SA1100_PCMCIA_3V_MEM_ACCESS;
      else
	speed = SA1100_PCMCIA_5V_MEM_ACCESS;
    }

    clock = cpufreq_get(0);

    /* Fixme: MECR is not pre-empt safe. */
    mecr = MECR;

    if (map->flags & MAP_ATTRIB) {
      MECR_BSA_SET(mecr, sock, sa1100_pcmcia_mecr_bs(speed, clock));
      skt->speed_attr = speed;
    } else {
      MECR_BSM_SET(mecr, sock, sa1100_pcmcia_mecr_bs(speed, clock));
      skt->speed_mem = speed;
    }

    DEBUG(4, "%s(): FAST%u %lx  BSM%u %lx  BSA%u %lx  BSIO%u %lx\n",
	  __FUNCTION__, sock, MECR_FAST_GET(mecr, sock), sock,
	  MECR_BSM_GET(mecr, sock), sock, MECR_BSA_GET(mecr, sock), 
	  sock, MECR_BSIO_GET(mecr, sock));

    MECR = mecr;
  }

  start = (map->flags & MAP_ATTRIB) ? skt->phys_attr : skt->phys_mem;

  if (map->sys_stop == 0)
    map->sys_stop = PAGE_SIZE-1;

  map->sys_stop -= map->sys_start;
  map->sys_stop += start;
  map->sys_start = start;

  skt->mem_map[map->map] = *map;

  return 0;
}  /* sa1100_pcmcia_set_mem_map() */


#if defined(CONFIG_PROC_FS)

/* sa1100_pcmcia_proc_status()
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^^
 * Implements the /proc/bus/pccard/??/status file.
 *
 * Returns: the number of characters added to the buffer
 */
static int
sa1100_pcmcia_proc_status(char *buf, char **start, off_t pos,
			  int count, int *eof, void *data)
{
  unsigned int sock = (unsigned int)data;
  unsigned int clock = cpufreq_get(0);
  struct sa1100_pcmcia_socket *skt = PCMCIA_SOCKET(sock);
  unsigned long mecr = MECR;
  char *p = buf;

  p+=sprintf(p, "k_state  : %s%s%s%s%s%s%s\n", 
	     skt->k_state.detect ? "detect " : "",
	     skt->k_state.ready  ? "ready "  : "",
	     skt->k_state.bvd1   ? "bvd1 "   : "",
	     skt->k_state.bvd2   ? "bvd2 "   : "",
	     skt->k_state.wrprot ? "wrprot " : "",
	     skt->k_state.vs_3v  ? "vs_3v "  : "",
	     skt->k_state.vs_Xv  ? "vs_Xv "  : "");

  p+=sprintf(p, "status   : %s%s%s%s%s%s%s%s%s\n",
	     skt->k_state.detect ? "SS_DETECT " : "",
	     skt->k_state.ready  ? "SS_READY " : "",
	     skt->cs_state.Vcc   ? "SS_POWERON " : "",
	     skt->cs_state.flags & SS_IOCARD ? "SS_IOCARD " : "",
	     (skt->cs_state.flags & SS_IOCARD &&
	      skt->k_state.bvd1) ? "SS_STSCHG " : "",
	     ((skt->cs_state.flags & SS_IOCARD)==0 &&
	      (skt->k_state.bvd1==0)) ? "SS_BATDEAD " : "",
	     ((skt->cs_state.flags & SS_IOCARD)==0 &&
	      (skt->k_state.bvd2==0)) ? "SS_BATWARN " : "",
	     skt->k_state.vs_3v  ? "SS_3VCARD " : "",
	     skt->k_state.vs_Xv  ? "SS_XVCARD " : "");

  p+=sprintf(p, "mask     : %s%s%s%s%s\n",
	     skt->cs_state.csc_mask & SS_DETECT  ? "SS_DETECT "  : "",
	     skt->cs_state.csc_mask & SS_READY   ? "SS_READY "   : "",
	     skt->cs_state.csc_mask & SS_BATDEAD ? "SS_BATDEAD " : "",
	     skt->cs_state.csc_mask & SS_BATWARN ? "SS_BATWARN " : "",
	     skt->cs_state.csc_mask & SS_STSCHG  ? "SS_STSCHG "  : "");

  p+=sprintf(p, "cs_flags : %s%s%s%s%s\n",
	     skt->cs_state.flags & SS_PWR_AUTO   ? "SS_PWR_AUTO "   : "",
	     skt->cs_state.flags & SS_IOCARD     ? "SS_IOCARD "     : "",
	     skt->cs_state.flags & SS_RESET      ? "SS_RESET "      : "",
	     skt->cs_state.flags & SS_SPKR_ENA   ? "SS_SPKR_ENA "   : "",
	     skt->cs_state.flags & SS_OUTPUT_ENA ? "SS_OUTPUT_ENA " : "");

  p+=sprintf(p, "Vcc      : %d\n", skt->cs_state.Vcc);
  p+=sprintf(p, "Vpp      : %d\n", skt->cs_state.Vpp);
  p+=sprintf(p, "IRQ      : %d\n", skt->cs_state.io_irq);

  p+=sprintf(p, "I/O      : %u (%u)\n", skt->speed_io,
	     sa1100_pcmcia_cmd_time(clock, MECR_BSIO_GET(mecr, sock)));

  p+=sprintf(p, "attribute: %u (%u)\n", skt->speed_attr,
	     sa1100_pcmcia_cmd_time(clock, MECR_BSA_GET(mecr, sock)));

  p+=sprintf(p, "common   : %u (%u)\n", skt->speed_mem,
	     sa1100_pcmcia_cmd_time(clock, MECR_BSM_GET(mecr, sock)));

  return p-buf;
}

/* sa1100_pcmcia_proc_setup()
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^
 * Implements the proc_setup() operation for the in-kernel PCMCIA
 * service (formerly SS_ProcSetup in Card Services).
 *
 * Returns: 0 on success, -1 on error
 */
static void
sa1100_pcmcia_proc_setup(unsigned int sock, struct proc_dir_entry *base)
{
  struct proc_dir_entry *entry;

  DEBUG(4, "%s() for sock %u\n", __FUNCTION__, sock);

  if ((entry = create_proc_entry("status", 0, base)) == NULL){
    printk(KERN_ERR "unable to install \"status\" procfs entry\n");
    return;
  }

  entry->read_proc = sa1100_pcmcia_proc_status;
  entry->data = (void *)sock;
}

#endif  /* defined(CONFIG_PROC_FS) */

static struct pccard_operations sa1100_pcmcia_operations = {
  init:			sa1100_pcmcia_init,
  suspend:		sa1100_pcmcia_suspend,
  register_callback:	sa1100_pcmcia_register_callback,
  inquire_socket:	sa1100_pcmcia_inquire_socket,
  get_status:		sa1100_pcmcia_get_status,
  get_socket:		sa1100_pcmcia_get_socket,
  set_socket:		sa1100_pcmcia_set_socket,
  get_io_map:		sa1100_pcmcia_get_io_map,
  set_io_map:		sa1100_pcmcia_set_io_map,
  get_mem_map:		sa1100_pcmcia_get_mem_map,
  set_mem_map:		sa1100_pcmcia_set_mem_map,
#ifdef CONFIG_PROC_FS
  proc_setup:		sa1100_pcmcia_proc_setup
#endif
};

#ifdef CONFIG_CPU_FREQ

/* sa1100_pcmcia_update_mecr()
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^^
 * When sa1100_pcmcia_notifier() decides that a MECR adjustment (due
 * to a core clock frequency change) is needed, this routine establishes
 * new BS_xx values consistent with the clock speed `clock'.
 */
static void sa1100_pcmcia_update_mecr(unsigned int clock)
{
  unsigned int sock;
  unsigned long mecr = MECR;

  for(sock = 0; sock < SA1100_PCMCIA_MAX_SOCK; ++sock){

    MECR_BSIO_SET(mecr, sock,
		  sa1100_pcmcia_mecr_bs(sa1100_pcmcia_socket[sock].speed_io,
					clock));
    MECR_BSA_SET(mecr, sock,
		 sa1100_pcmcia_mecr_bs(sa1100_pcmcia_socket[sock].speed_attr,
				       clock));
    MECR_BSM_SET(mecr, sock,
		 sa1100_pcmcia_mecr_bs(sa1100_pcmcia_socket[sock].speed_mem,
				       clock));
  }

  MECR = mecr;

}

/* sa1100_pcmcia_notifier()
 * ^^^^^^^^^^^^^^^^^^^^^^^^
 * When changing the processor core clock frequency, it is necessary
 * to adjust the MECR timings accordingly. We've recorded the timings
 * requested by Card Services, so this is just a matter of finding
 * out what our current speed is, and then recomputing the new MECR
 * values.
 *
 * Returns: 0 on success, -1 on error
 */
static int
sa1100_pcmcia_notifier(struct notifier_block *nb, unsigned long val,
		       void *data)
{
  struct cpufreq_info *ci = data;

  switch (val) {
  case CPUFREQ_PRECHANGE:
    if (ci->new_freq > ci->old_freq) {
      DEBUG(2, "%s(): new frequency %u.%uMHz > %u.%uMHz, pre-updating\n",
	    __FUNCTION__,
	    ci->new_freq / 1000, (ci->new_freq / 100) % 10,
	    ci->old_freq / 1000, (ci->old_freq / 100) % 10);
      sa1100_pcmcia_update_mecr(ci->new_freq);
    }
    break;

  case CPUFREQ_POSTCHANGE:
    if (ci->new_freq < ci->old_freq) {
      DEBUG(2, "%s(): new frequency %u.%uMHz < %u.%uMHz, post-updating\n",
	    __FUNCTION__,
	    ci->new_freq / 1000, (ci->new_freq / 100) % 10,
	    ci->old_freq / 1000, (ci->old_freq / 100) % 10);
      sa1100_pcmcia_update_mecr(ci->new_freq);
    }
    break;
  }

  return 0;
}

static struct notifier_block sa1100_pcmcia_notifier_block = {
  notifier_call: sa1100_pcmcia_notifier
};
#endif

static int __init sa1100_pcmcia_machine_init(void)
{
#ifdef CONFIG_SA1100_ASSABET
  if(machine_is_assabet()) {
    if(machine_has_neponset()) {
#ifdef CONFIG_ASSABET_NEPONSET
      pcmcia_low_level = &neponset_pcmcia_ops;
#else
      printk(KERN_ERR "Card Services disabled: missing Neponset support\n");
      return -1;
#endif
    } else
      pcmcia_low_level = &assabet_pcmcia_ops;
  }
#endif
#ifdef CONFIG_SA1100_BADGE4
  if (machine_is_badge4())
    pcmcia_low_level = &badge4_pcmcia_ops;
#endif
#ifdef CONFIG_SA1100_FREEBIRD
  if (machine_is_freebird())
    pcmcia_low_level = &freebird_pcmcia_ops;
#endif
#ifdef CONFIG_SA1100_H3600
  if (machine_is_h3600())
    pcmcia_low_level = &h3600_pcmcia_ops;
#endif
#ifdef CONFIG_SA1100_CERF
  if (machine_is_cerf())
    pcmcia_low_level = &cerf_pcmcia_ops;
#endif
#ifdef CONFIG_SA1100_GRAPHICSCLIENT
  if (machine_is_graphicsclient())
    pcmcia_low_level = &gcplus_pcmcia_ops;
#endif
#ifdef CONFIG_SA1100_XP860
  if (machine_is_xp860())
    pcmcia_low_level = &xp860_pcmcia_ops;
#endif
#ifdef CONFIG_SA1100_YOPY
  if (machine_is_yopy())
    pcmcia_low_level = &yopy_pcmcia_ops;
#endif
#ifdef CONFIG_SA1100_SHANNON
  if (machine_is_shannon())
    pcmcia_low_level = &shannon_pcmcia_ops;
#endif
#ifdef CONFIG_SA1100_PANGOLIN
  if (machine_is_pangolin())
    pcmcia_low_level = &pangolin_pcmcia_ops;
#endif
#ifdef CONFIG_SA1100_JORNADA720
  if (machine_is_jornada720())
    pcmcia_low_level = &jornada720_pcmcia_ops;
#endif
#ifdef CONFIG_SA1100_PFS168
  if(machine_is_pfs168())
    pcmcia_low_level = &pfs168_pcmcia_ops;
#endif
#ifdef CONFIG_SA1100_FLEXANET
  if(machine_is_flexanet())
    pcmcia_low_level = &flexanet_pcmcia_ops;
#endif
#ifdef CONFIG_SA1100_SIMPAD
  if(machine_is_simpad())
    pcmcia_low_level = &simpad_pcmcia_ops;
#endif
#ifdef CONFIG_SA1100_GRAPHICSMASTER
  if(machine_is_graphicsmaster())
    pcmcia_low_level = &graphicsmaster_pcmcia_ops;
#endif
#ifdef CONFIG_SA1100_ADSBITSY
  if(machine_is_adsbitsy())
    pcmcia_low_level = &adsbitsy_pcmcia_ops;
#endif
#ifdef CONFIG_SA1100_STORK
  if(machine_is_stork())
    pcmcia_low_level = &stork_pcmcia_ops;
#endif

  if (!pcmcia_low_level) {
    printk(KERN_ERR "This hardware is not supported by the SA1100 Card Service driver\n");
    return -ENODEV;
  }

  return 0;
}

/* sa1100_pcmcia_driver_init()
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^^
 *
 * This routine performs a basic sanity check to ensure that this
 * kernel has been built with the appropriate board-specific low-level
 * PCMCIA support, performs low-level PCMCIA initialization, registers
 * this socket driver with Card Services, and then spawns the daemon
 * thread which is the real workhorse of the socket driver.
 *
 * Returns: 0 on success, -1 on error
 */
static int __init sa1100_pcmcia_driver_init(void)
{
  servinfo_t info;
  struct pcmcia_init pcmcia_init;
  struct pcmcia_state state[SA1100_PCMCIA_MAX_SOCK];
  struct pcmcia_state_array state_array;
  unsigned int i, clock;
  unsigned long mecr;
  int ret;

  printk(KERN_INFO "SA-1100 PCMCIA (CS release %s)\n", CS_RELEASE);

  CardServices(GetCardServicesInfo, &info);

  if (info.Revision != CS_RELEASE_CODE) {
    printk(KERN_ERR "Card Services release codes do not match\n");
    return -EINVAL;
  }

  ret = sa1100_pcmcia_machine_init();
  if (ret)
    return ret;

  pcmcia_init.handler = sa1100_pcmcia_interrupt;

  ret = pcmcia_low_level->init(&pcmcia_init);
  if (ret < 0) {
    printk(KERN_ERR "Unable to initialize kernel PCMCIA service (%d).\n", ret);
    return ret == -1 ? -EIO : ret;
  }

  sa1100_pcmcia_socket_count = ret;
  state_array.size  = sa1100_pcmcia_socket_count;
  state_array.state = state;

  memset(state, 0, sizeof(state));

  if (pcmcia_low_level->socket_state(&state_array) < 0) {
    pcmcia_low_level->shutdown();
    printk(KERN_ERR "Unable to get PCMCIA status from kernel.\n");
    return -EIO;
  }

  /*
   * We initialize the MECR to default values here, because we are
   * not guaranteed to see a SetIOMap operation at runtime.
   */
  mecr = 0;

  clock = cpufreq_get(0);

  for (i = 0; i < sa1100_pcmcia_socket_count; i++) {
    struct sa1100_pcmcia_socket *skt = PCMCIA_SOCKET(i);
    struct pcmcia_irq_info irq_info;

    if (!request_mem_region(_PCMCIA(i), PCMCIASp, "PCMCIA")) {
      ret = -EBUSY;
      goto out_err;
    }

    irq_info.sock = i;
    irq_info.irq  = -1;
    ret = pcmcia_low_level->get_irq_info(&irq_info);
    if (ret < 0)
      printk(KERN_ERR "Unable to get IRQ for socket %u (%d)\n", i, ret);

    skt->irq        = irq_info.irq;
    skt->k_state    = state[i];
    skt->speed_io   = SA1100_PCMCIA_IO_ACCESS;
    skt->speed_attr = SA1100_PCMCIA_5V_MEM_ACCESS;
    skt->speed_mem  = SA1100_PCMCIA_5V_MEM_ACCESS;
    skt->phys_attr  = _PCMCIAAttr(i);
    skt->phys_mem   = _PCMCIAMem(i);
    skt->virt_io    = ioremap(_PCMCIAIO(i), 0x10000);

    if (skt->virt_io == NULL) {
      ret = -ENOMEM;
      goto out_err;
    }

    MECR_FAST_SET(mecr, i, 0);
    MECR_BSIO_SET(mecr, i, sa1100_pcmcia_mecr_bs(skt->speed_io, clock));
    MECR_BSA_SET(mecr, i, sa1100_pcmcia_mecr_bs(skt->speed_attr, clock));
    MECR_BSM_SET(mecr, i, sa1100_pcmcia_mecr_bs(skt->speed_mem, clock));
  }

  MECR = mecr;

#ifdef CONFIG_CPU_FREQ
  ret = cpufreq_register_notifier(&sa1100_pcmcia_notifier_block);
  if (ret < 0) {
    printk(KERN_ERR "Unable to register CPU frequency change notifier (%d)\n", ret);
    goto out_err;
  }
#endif

  /* Only advertise as many sockets as we can detect */
  ret = register_ss_entry(sa1100_pcmcia_socket_count,
			  &sa1100_pcmcia_operations);
  if (ret < 0) {
    printk(KERN_ERR "Unable to register sockets\n");
    goto out_err;
  }

  /*
   * Start the event poll timer.  It will reschedule by itself afterwards.
   */
  sa1100_pcmcia_poll_event(0);

  return 0;

 out_err:
  for (i = 0; i < sa1100_pcmcia_socket_count; i++) {
    iounmap(sa1100_pcmcia_socket[i].virt_io);
    release_mem_region(_PCMCIA(i), PCMCIASp);
  }

  pcmcia_low_level->shutdown();

  return ret;
}  /* sa1100_pcmcia_driver_init() */

/* sa1100_pcmcia_driver_shutdown()
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 * Invokes the low-level kernel service to free IRQs associated with this
 * socket controller and reset GPIO edge detection.
 */
static void __exit sa1100_pcmcia_driver_shutdown(void)
{
  int i;

  del_timer_sync(&poll_timer);

  unregister_ss_entry(&sa1100_pcmcia_operations);

#ifdef CONFIG_CPU_FREQ
  cpufreq_unregister_notifier(&sa1100_pcmcia_notifier_block);
#endif

  for (i = 0; i < sa1100_pcmcia_socket_count; i++) {
    iounmap(sa1100_pcmcia_socket[i].virt_io);
    release_mem_region(_PCMCIA(i), PCMCIASp);
  }

  pcmcia_low_level->shutdown();

  flush_scheduled_tasks();
}

MODULE_AUTHOR("John Dorsey <john+@cs.cmu.edu>");
MODULE_DESCRIPTION("Linux PCMCIA Card Services: SA-1100 Socket Controller");
MODULE_LICENSE("Dual MPL/GPL");

module_init(sa1100_pcmcia_driver_init);
module_exit(sa1100_pcmcia_driver_shutdown);
