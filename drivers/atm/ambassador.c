/*
  Madge Ambassador ATM Adapter driver.
  Copyright (C) 1995-1999  Madge Networks Ltd.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  The GNU GPL is contained in /usr/doc/copyright/GPL on a Debian
  system and in the file COPYING in the Linux kernel source.
*/

/* * dedicated to the memory of Graham Gordon 1971-1998 * */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/atmdev.h>
#include <linux/delay.h>
#include <linux/interrupt.h>

#include <asm/atomic.h>
#include <asm/io.h>
#include <asm/byteorder.h>

#include "ambassador.h"

#define maintainer_string "Giuliano Procida at Madge Networks <gprocida@madge.com>"
#define description_string "Madge ATM Ambassador driver"
#define version_string "1.2.4"

static inline void __init show_version (void) {
  printk ("%s version %s\n", description_string, version_string);
}

/*
  
  Theory of Operation
  
  I Hardware, detection, initialisation and shutdown.
  
  1. Supported Hardware
  
  This driver is for the PCI ATMizer-based Ambassador card (except
  very early versions). It is not suitable for the similar EISA "TR7"
  card. Commercially, both cards are known as Collage Server ATM
  adapters.
  
  The loader supports image transfer to the card, image start and few
  other miscellaneous commands.
  
  Only AAL5 is supported with vpi = 0 and vci in the range 0 to 1023.
  
  The cards are big-endian.
  
  2. Detection
  
  Standard PCI stuff, the early cards are detected and rejected.
  
  3. Initialisation
  
  The cards are reset and the self-test results are checked. The
  microcode image is then transferred and started. This waits for a
  pointer to a descriptor containing details of the host-based queues
  and buffers and various parameters etc. Once they are processed
  normal operations may begin. The BIA is read using a microcode
  command.
  
  4. Shutdown
  
  This may be accomplished either by a card reset or via the microcode
  shutdown command. Further investigation required.
  
  5. Persistent state
  
  The card reset does not affect PCI configuration (good) or the
  contents of several other "shared run-time registers" (bad) which
  include doorbell and interrupt control as well as EEPROM and PCI
  control. The driver must be careful when modifying these registers
  not to touch bits it does not use and to undo any changes at exit.
  
  II Driver software
  
  0. Generalities
  
  The adapter is quite intelligent (fast) and has a simple interface
  (few features). VPI is always zero, 1024 VCIs are supported. There
  is limited cell rate support. UBR channels can be capped and ABR
  (explicit rate, but not EFCI) is supported. There is no CBR or VBR
  support.
  
  1. Driver <-> Adapter Communication
  
  Apart from the basic loader commands, the driver communicates
  through three entities: the command queue (CQ), the transmit queue
  pair (TXQ) and the receive queue pairs (RXQ). These three entities
  are set up by the host and passed to the microcode just after it has
  been started.
  
  All queues are host-based circular queues. They are contiguous and
  (due to hardware limitations) have some restrictions as to their
  locations in (bus) memory. They are of the "full means the same as
  empty so don't do that" variety since the adapter uses pointers
  internally.
  
  The queue pairs work as follows: one queue is for supply to the
  adapter, items in it are pending and are owned by the adapter; the
  other is the queue for return from the adapter, items in it have
  been dealt with by the adapter. The host adds items to the supply
  (TX descriptors and free RX buffer descriptors) and removes items
  from the return (TX and RX completions). The adapter deals with out
  of order completions.
  
  Interrupts (card to host) and the doorbell (host to card) are used
  for signalling.
  
  1. CQ
  
  This is to communicate "open VC", "close VC", "get stats" etc. to
  the adapter. At most one command is retired every millisecond by the
  card. There is no out of order completion or notification. The
  driver needs to check the return code of the command, waiting as
  appropriate.
  
  2. TXQ
  
  TX supply items are of variable length (scatter gather support) and
  so the queue items are (more or less) pointers to the real thing.
  Each TX supply item contains a unique, host-supplied handle (the skb
  bus address seems most sensible as this works for Alphas as well,
  there is no need to do any endian conversions on the handles).
  
  TX return items consist of just the handles above.
  
  3. RXQ (up to 4 of these with different lengths and buffer sizes)
  
  RX supply items consist of a unique, host-supplied handle (the skb
  bus address again) and a pointer to the buffer data area.
  
  RX return items consist of the handle above, the VC, length and a
  status word. This just screams "oh so easy" doesn't it?

  Note on RX pool sizes:
   
  Each pool should have enough buffers to handle a back-to-back stream
  of minimum sized frames on a single VC. For example:
  
    frame spacing = 3us (about right)
    
    delay = IRQ lat + RX handling + RX buffer replenish = 20 (us)  (a guess)
    
    min number of buffers for one VC = 1 + delay/spacing (buffers)

    delay/spacing = latency = (20+2)/3 = 7 (buffers)  (rounding up)
    
  The 20us delay assumes that there is no need to sleep; if we need to
  sleep to get buffers we are going to drop frames anyway.
  
  In fact, each pool should have enough buffers to support the
  simultaneous reassembly of a separate frame on each VC and cope with
  the case in which frames complete in round robin cell fashion on
  each VC.
  
  Only one frame can complete at each cell arrival, so if "n" VCs are
  open, the worst case is to have them all complete frames together
  followed by all starting new frames together.
  
    desired number of buffers = n + delay/spacing
    
  These are the extreme requirements, however, they are "n+k" for some
  "k" so we have only the constant to choose. This is the argument
  rx_lats which current defaults to 7.
  
  Actually, "n ? n+k : 0" is better and this is what is implemented,
  subject to the limit given by the pool size.
  
  4. Driver locking
  
  Simple spinlocks are used around the TX and RX queue mechanisms.
  Anyone with a faster, working method is welcome to implement it.
  
  The adapter command queue is protected with a spinlock. We always
  wait for commands to complete.
  
  A more complex form of locking is used around parts of the VC open
  and close functions. There are three reasons for a lock: 1. we need
  to do atomic rate reservation and release (not used yet), 2. Opening
  sometimes involves two adapter commands which must not be separated
  by another command on the same VC, 3. the changes to RX pool size
  must be atomic. The lock needs to work over context switches, so we
  use a semaphore.
  
  III Hardware Features and Microcode Bugs
  
  1. Byte Ordering
  
  *%^"$&%^$*&^"$(%^$#&^%$(&#%$*(&^#%!"!"!*!
  
  2. Memory access
  
  All structures that are not accessed using DMA must be 4-byte
  aligned (not a problem) and must not cross 4MB boundaries.
  
  There is a DMA memory hole at E0000000-E00000FF (groan).
  
  TX fragments (DMA read) must not cross 4MB boundaries (would be 16MB
  but for a hardware bug).
  
  RX buffers (DMA write) must not cross 16MB boundaries and must
  include spare trailing bytes up to the next 4-byte boundary; they
  will be written with rubbish.
  
  The PLX likes to prefetch; if reading up to 4 u32 past the end of
  each TX fragment is not a problem, then TX can be made to go a
  little faster by passing a flag at init that disables a prefetch
  workaround. We do not pass this flag. (new microcode only)
  
  Now we:
  . Note that alloc_skb rounds up size to a 16byte boundary.  
  . Ensure all areas do not traverse 4MB boundaries.
  . Ensure all areas do not start at a E00000xx bus address.
  (I cannot be certain, but this may always hold with Linux)
  . Make all failures cause a loud message.
  . Discard non-conforming SKBs (causes TX failure or RX fill delay).
  . Discard non-conforming TX fragment descriptors (the TX fails).
  In the future we could:
  . Allow RX areas that traverse 4MB (but not 16MB) boundaries.
  . Segment TX areas into some/more fragments, when necessary.
  . Relax checks for non-DMA items (ignore hole).
  . Give scatter-gather (iovec) requirements using ???. (?)
  
  3. VC close is broken (only for new microcode)
  
  The VC close adapter microcode command fails to do anything if any
  frames have been received on the VC but none have been transmitted.
  Frames continue to be reassembled and passed (with IRQ) to the
  driver.
  
  IV To Do List
  
  . Fix bugs!
  
  . Timer code may be broken.
  
  . Deal with buggy VC close (somehow) in microcode 12.
  
  . Handle interrupted and/or non-blocking writes - is this a job for
    the protocol layer?
  
  . Add code to break up TX fragments when they span 4MB boundaries.
  
  . Add SUNI phy layer (need to know where SUNI lives on card).
  
  . Implement a tx_alloc fn to (a) satisfy TX alignment etc. and (b)
    leave extra headroom space for Ambassador TX descriptors.
  
  . Understand these elements of struct atm_vcc: recvq (proto?),
    sleep, callback, listenq, backlog_quota, reply and user_back.
  
  . Adjust TX/RX skb allocation to favour IP with LANE/CLIP (configurable).
  
  . Impose a TX-pending limit (2?) on each VC, help avoid TX q overflow.
  
  . Decide whether RX buffer recycling is or can be made completely safe;
    turn it back on. It looks like Werner is going to axe this.
  
  . Implement QoS changes on open VCs (involves extracting parts of VC open
    and close into separate functions and using them to make changes).
  
  . Hack on command queue so that someone can issue multiple commands and wait
    on the last one (OR only "no-op" or "wait" commands are waited for).
  
  . Eliminate need for while-schedule around do_command.
  
*/

/********** microcode **********/

#ifdef AMB_NEW_MICROCODE
#define UCODE(x) UCODE2(atmsar12.x)
#else
#define UCODE(x) UCODE2(atmsar11.x)
#endif
#define UCODE2(x) #x

static u32 __initdata ucode_start = 
#include UCODE(start)
;

static region __initdata ucode_regions[] = {
#include UCODE(regions)
  { 0, 0 }
};

static u32 __initdata ucode_data[] = {
#include UCODE(data)
  0xdeadbeef
};

/********** globals **********/

static amb_dev * amb_devs = NULL;
static struct timer_list housekeeping;

static unsigned short debug = 0;
static unsigned int cmds = 8;
static unsigned int txs = 32;
static unsigned int rxs[NUM_RX_POOLS] = { 64, 64, 64, 64 };
static unsigned int rxs_bs[NUM_RX_POOLS] = { 4080, 12240, 36720, 65535 };
static unsigned int rx_lats = 7;
static unsigned char pci_lat = 0;

static const unsigned long onegigmask = -1 << 30;

/********** access to adapter **********/

static inline void wr_plain (const amb_dev * dev, size_t addr, u32 data) {
  PRINTD (DBG_FLOW|DBG_REGS, "wr: %08x <- %08x", addr, data);
#ifdef AMB_MMIO
  dev->membase[addr / sizeof(u32)] = data;
#else
  outl (data, dev->iobase + addr);
#endif
}

static inline u32 rd_plain (const amb_dev * dev, size_t addr) {
#ifdef AMB_MMIO
  u32 data = dev->membase[addr / sizeof(u32)];
#else
  u32 data = inl (dev->iobase + addr);
#endif
  PRINTD (DBG_FLOW|DBG_REGS, "rd: %08x -> %08x", addr, data);
  return data;
}

static inline void wr_mem (const amb_dev * dev, size_t addr, u32 data) {
  u32 be = cpu_to_be32 (data);
  PRINTD (DBG_FLOW|DBG_REGS, "wr: %08x <- %08x b[%08x]", addr, data, be);
#ifdef AMB_MMIO
  dev->membase[addr / sizeof(u32)] = be;
#else
  outl (be, dev->iobase + addr);
#endif
}

static inline u32 rd_mem (const amb_dev * dev, size_t addr) {
#ifdef AMB_MMIO
  u32 be = dev->membase[addr / sizeof(u32)];
#else
  u32 be = inl (dev->iobase + addr);
#endif
  u32 data = be32_to_cpu (be);
  PRINTD (DBG_FLOW|DBG_REGS, "rd: %08x -> %08x b[%08x]", addr, data, be);
  return data;
}

/********** dump routines **********/

static inline void dump_registers (const amb_dev * dev) {
#ifdef DEBUG_AMBASSADOR
  if (debug & DBG_REGS) {
    size_t i;
    PRINTD (DBG_REGS, "reading PLX control: ");
    for (i = 0x00; i < 0x30; i += sizeof(u32))
      rd_mem (dev, i);
    PRINTD (DBG_REGS, "reading mailboxes: ");
    for (i = 0x40; i < 0x60; i += sizeof(u32))
      rd_mem (dev, i);
    PRINTD (DBG_REGS, "reading doorb irqev irqen reset:");
    for (i = 0x60; i < 0x70; i += sizeof(u32))
      rd_mem (dev, i);
  }
#else
  (void) dev;
#endif
  return;
}

static inline void dump_loader_block (volatile loader_block * lb) {
#ifdef DEBUG_AMBASSADOR
  unsigned int i;
  PRINTDB (DBG_LOAD, "lb @ %p; res: %d, cmd: %d, pay:",
	   lb, be32_to_cpu (lb->result), be32_to_cpu (lb->command));
  for (i = 0; i < MAX_COMMAND_DATA; ++i)
    PRINTDM (DBG_LOAD, " %08x", be32_to_cpu (lb->payload.data[i]));
  PRINTDE (DBG_LOAD, ", vld: %08x", be32_to_cpu (lb->valid));
#else
  (void) lb;
#endif
  return;
}

static inline void dump_command (command * cmd) {
#ifdef DEBUG_AMBASSADOR
  unsigned int i;
  PRINTDB (DBG_CMD, "cmd @ %p, req: %08x, pars:",
	   cmd, /*be32_to_cpu*/ (cmd->request));
  for (i = 0; i < 3; ++i)
    PRINTDM (DBG_CMD, " %08x", /*be32_to_cpu*/ (cmd->args.par[i]));
  PRINTDE (DBG_CMD, "");
#else
  (void) cmd;
#endif
  return;
}

static inline void dump_skb (char * prefix, unsigned int vc, struct sk_buff * skb) {
#ifdef DEBUG_AMBASSADOR
  unsigned int i;
  unsigned char * data = skb->data;
  PRINTDB (DBG_DATA, "%s(%u) ", prefix, vc);
  for (i=0; i<skb->len && i < 256;i++)
    PRINTDM (DBG_DATA, "%02x ", data[i]);
  PRINTDE (DBG_DATA,"");
#else
  (void) prefix;
  (void) vc;
  (void) skb;
#endif
  return;
}

/********** check memory areas for use by Ambassador **********/

/* see limitations under Hardware Features */

static inline int check_area (void * start, size_t length) {
  // assumes length > 0
  const u32 fourmegmask = -1 << 22;
  const u32 twofivesixmask = -1 << 8;
  const u32 starthole = 0xE0000000;
  u32 startaddress = virt_to_bus (start);
  u32 lastaddress = startaddress+length-1;
  if ((startaddress ^ lastaddress) & fourmegmask ||
      (startaddress & twofivesixmask) == starthole) {
    PRINTK (KERN_ERR, "check_area failure: [%x,%x] - mail maintainer!",
	    startaddress, lastaddress);
    return -1;
  } else {
    return 0;
  }
}

/********** free an skb (as per ATM device driver documentation) **********/

static inline void amb_kfree_skb (struct sk_buff * skb) {
  if (ATM_SKB(skb)->vcc->pop) {
    ATM_SKB(skb)->vcc->pop (ATM_SKB(skb)->vcc, skb);
  } else {
    dev_kfree_skb_any (skb);
  }
}

/********** TX completion **********/

static inline void tx_complete (amb_dev * dev, tx_out * tx) {
  tx_simple * tx_descr = bus_to_virt (tx->handle);
  struct sk_buff * skb = tx_descr->skb;
  
  PRINTD (DBG_FLOW|DBG_TX, "tx_complete %p %p", dev, tx);
  
  // VC layer stats
  atomic_inc(&ATM_SKB(skb)->vcc->stats->tx);
  
  // free the descriptor
  kfree (tx_descr);
  
  // free the skb
  amb_kfree_skb (skb);
  
  dev->stats.tx_ok++;
  return;
}

/********** RX completion **********/

static void rx_complete (amb_dev * dev, rx_out * rx) {
  struct sk_buff * skb = bus_to_virt (rx->handle);
  u16 vc = be16_to_cpu (rx->vc);
  // unused: u16 lec_id = be16_to_cpu (rx->lec_id);
  u16 status = be16_to_cpu (rx->status);
  u16 rx_len = be16_to_cpu (rx->length);
  
  PRINTD (DBG_FLOW|DBG_RX, "rx_complete %p %p (len=%hu)", dev, rx, rx_len);
  
  // XXX move this in and add to VC stats ???
  if (!status) {
    struct atm_vcc * atm_vcc = dev->rxer[vc];
    dev->stats.rx.ok++;
    
    if (atm_vcc) {
      
      if (rx_len <= atm_vcc->qos.rxtp.max_sdu) {
	
	if (atm_charge (atm_vcc, skb->truesize)) {
	  
	  // prepare socket buffer
	  ATM_SKB(skb)->vcc = atm_vcc;
	  skb_put (skb, rx_len);
	  
	  dump_skb ("<<<", vc, skb);
	  
	  // VC layer stats
	  atomic_inc(&atm_vcc->stats->rx);
	  skb->stamp = xtime;
	  // end of our responsability
	  atm_vcc->push (atm_vcc, skb);
	  return;
	  
	} else {
	  // someone fix this (message), please!
	  PRINTD (DBG_INFO|DBG_RX, "dropped thanks to atm_charge (vc %hu, truesize %u)", vc, skb->truesize);
	  // drop stats incremented in atm_charge
	}
	
      } else {
      	PRINTK (KERN_INFO, "dropped over-size frame");
	// should we count this?
	atomic_inc(&atm_vcc->stats->rx_drop);
      }
      
    } else {
      PRINTD (DBG_WARN|DBG_RX, "got frame but RX closed for channel %hu", vc);
      // this is an adapter bug, only in new version of microcode
    }
    
  } else {
    dev->stats.rx.error++;
    if (status & CRC_ERR)
      dev->stats.rx.badcrc++;
    if (status & LEN_ERR)
      dev->stats.rx.toolong++;
    if (status & ABORT_ERR)
      dev->stats.rx.aborted++;
    if (status & UNUSED_ERR)
      dev->stats.rx.unused++;
  }
  
  dev_kfree_skb_any (skb);
  return;
}

/*
  
  Note on queue handling.
  
  Here "give" and "take" refer to queue entries and a queue (pair)
  rather than frames to or from the host or adapter. Empty frame
  buffers are given to the RX queue pair and returned unused or
  containing RX frames. TX frames (well, pointers to TX fragment
  lists) are given to the TX queue pair, completions are returned.
  
*/

/********** command queue **********/

// I really don't like this, but it's the best I can do at the moment

// also, the callers are responsible for byte order as the microcode
// sometimes does 16-bit accesses (yuk yuk yuk)

static int command_do (amb_dev * dev, command * cmd) {
  amb_cq * cq = &dev->cq;
  volatile amb_cq_ptrs * ptrs = &cq->ptrs;
  command * my_slot;
  unsigned long timeout;
  
  PRINTD (DBG_FLOW|DBG_CMD, "command_do %p", dev);
  
  if (test_bit (dead, &dev->flags))
    return 0;
  
  spin_lock (&cq->lock);
  
  // if not full...
  if (cq->pending < cq->maximum) {
    // remember my slot for later
    my_slot = ptrs->in;
    PRINTD (DBG_CMD, "command in slot %p", my_slot);
    
    dump_command (cmd);
    
    // copy command in
    *ptrs->in = *cmd;
    cq->pending++;
    ptrs->in = NEXTQ (ptrs->in, ptrs->start, ptrs->limit);
    
    // mail the command
    wr_mem (dev, offsetof(amb_mem, mb.adapter.cmd_address), virt_to_bus (ptrs->in));
    
    // prepare to wait for cq->pending milliseconds
    // effectively one centisecond on i386
    timeout = (cq->pending*HZ+999)/1000;
    
    if (cq->pending > cq->high)
      cq->high = cq->pending;
    spin_unlock (&cq->lock);
    
    while (timeout) {
      // go to sleep
      // PRINTD (DBG_CMD, "wait: sleeping %lu for command", timeout);
      set_current_state(TASK_UNINTERRUPTIBLE);
      timeout = schedule_timeout (timeout);
    }
    
    // wait for my slot to be reached (all waiters are here or above, until...)
    while (ptrs->out != my_slot) {
      PRINTD (DBG_CMD, "wait: command slot (now at %p)", ptrs->out);
      set_current_state(TASK_UNINTERRUPTIBLE);
      schedule();
    }
    
    // wait on my slot (... one gets to its slot, and... )
    while (ptrs->out->request != cpu_to_be32 (SRB_COMPLETE)) {
      PRINTD (DBG_CMD, "wait: command slot completion");
      set_current_state(TASK_UNINTERRUPTIBLE);
      schedule();
    }
    
    PRINTD (DBG_CMD, "command complete");
    // update queue (... moves the queue along to the next slot)
    spin_lock (&cq->lock);
    cq->pending--;
    // copy command out
    *cmd = *ptrs->out;
    ptrs->out = NEXTQ (ptrs->out, ptrs->start, ptrs->limit);
    spin_unlock (&cq->lock);
    
    return 0;
  } else {
    cq->filled++;
    spin_unlock (&cq->lock);
    return -EAGAIN;
  }
  
}

/********** TX queue pair **********/

static inline int tx_give (amb_dev * dev, tx_in * tx) {
  amb_txq * txq = &dev->txq;
  unsigned long flags;
  
  PRINTD (DBG_FLOW|DBG_TX, "tx_give %p", dev);

  if (test_bit (dead, &dev->flags))
    return 0;
  
  spin_lock_irqsave (&txq->lock, flags);
  
  if (txq->pending < txq->maximum) {
    PRINTD (DBG_TX, "TX in slot %p", txq->in.ptr);

    *txq->in.ptr = *tx;
    txq->pending++;
    txq->in.ptr = NEXTQ (txq->in.ptr, txq->in.start, txq->in.limit);
    // hand over the TX and ring the bell
    wr_mem (dev, offsetof(amb_mem, mb.adapter.tx_address), virt_to_bus (txq->in.ptr));
    wr_mem (dev, offsetof(amb_mem, doorbell), TX_FRAME);
    
    if (txq->pending > txq->high)
      txq->high = txq->pending;
    spin_unlock_irqrestore (&txq->lock, flags);
    return 0;
  } else {
    txq->filled++;
    spin_unlock_irqrestore (&txq->lock, flags);
    return -EAGAIN;
  }
}

static inline int tx_take (amb_dev * dev) {
  amb_txq * txq = &dev->txq;
  unsigned long flags;
  
  PRINTD (DBG_FLOW|DBG_TX, "tx_take %p", dev);
  
  spin_lock_irqsave (&txq->lock, flags);
  
  if (txq->pending && txq->out.ptr->handle) {
    // deal with TX completion
    tx_complete (dev, txq->out.ptr);
    // mark unused again
    txq->out.ptr->handle = 0;
    // remove item
    txq->pending--;
    txq->out.ptr = NEXTQ (txq->out.ptr, txq->out.start, txq->out.limit);
    
    spin_unlock_irqrestore (&txq->lock, flags);
    return 0;
  } else {
    
    spin_unlock_irqrestore (&txq->lock, flags);
    return -1;
  }
}

/********** RX queue pairs **********/

static inline int rx_give (amb_dev * dev, rx_in * rx, unsigned char pool) {
  amb_rxq * rxq = &dev->rxq[pool];
  unsigned long flags;
  
  PRINTD (DBG_FLOW|DBG_RX, "rx_give %p[%hu]", dev, pool);
  
  spin_lock_irqsave (&rxq->lock, flags);
  
  if (rxq->pending < rxq->maximum) {
    PRINTD (DBG_RX, "RX in slot %p", rxq->in.ptr);

    *rxq->in.ptr = *rx;
    rxq->pending++;
    rxq->in.ptr = NEXTQ (rxq->in.ptr, rxq->in.start, rxq->in.limit);
    // hand over the RX buffer
    wr_mem (dev, offsetof(amb_mem, mb.adapter.rx_address[pool]), virt_to_bus (rxq->in.ptr));
    
    spin_unlock_irqrestore (&rxq->lock, flags);
    return 0;
  } else {
    spin_unlock_irqrestore (&rxq->lock, flags);
    return -1;
  }
}

static inline int rx_take (amb_dev * dev, unsigned char pool) {
  amb_rxq * rxq = &dev->rxq[pool];
  unsigned long flags;
  
  PRINTD (DBG_FLOW|DBG_RX, "rx_take %p[%hu]", dev, pool);
  
  spin_lock_irqsave (&rxq->lock, flags);
  
  if (rxq->pending && (rxq->out.ptr->status || rxq->out.ptr->length)) {
    // deal with RX completion
    rx_complete (dev, rxq->out.ptr);
    // mark unused again
    rxq->out.ptr->status = 0;
    rxq->out.ptr->length = 0;
    // remove item
    rxq->pending--;
    rxq->out.ptr = NEXTQ (rxq->out.ptr, rxq->out.start, rxq->out.limit);
    
    if (rxq->pending < rxq->low)
      rxq->low = rxq->pending;
    spin_unlock_irqrestore (&rxq->lock, flags);
    return 0;
  } else {
    if (!rxq->pending && rxq->buffers_wanted)
      rxq->emptied++;
    spin_unlock_irqrestore (&rxq->lock, flags);
    return -1;
  }
}

/********** RX Pool handling **********/

/* pre: buffers_wanted = 0, post: pending = 0 */
static inline void drain_rx_pool (amb_dev * dev, unsigned char pool) {
  amb_rxq * rxq = &dev->rxq[pool];
  
  PRINTD (DBG_FLOW|DBG_POOL, "drain_rx_pool %p %hu", dev, pool);
  
  if (test_bit (dead, &dev->flags))
    return;
  
  /* we are not quite like the fill pool routines as we cannot just
     remove one buffer, we have to remove all of them, but we might as
     well pretend... */
  if (rxq->pending > rxq->buffers_wanted) {
    command cmd;
    cmd.request = cpu_to_be32 (SRB_FLUSH_BUFFER_Q);
    cmd.args.flush.flags = cpu_to_be32 (pool << SRB_POOL_SHIFT);
    while (command_do (dev, &cmd))
      schedule();
    /* the pool may also be emptied via the interrupt handler */
    while (rxq->pending > rxq->buffers_wanted)
      if (rx_take (dev, pool))
	schedule();
  }
  
  return;
}

#ifdef MODULE
static void drain_rx_pools (amb_dev * dev) {
  unsigned char pool;
  
  PRINTD (DBG_FLOW|DBG_POOL, "drain_rx_pools %p", dev);
  
  for (pool = 0; pool < NUM_RX_POOLS; ++pool)
    drain_rx_pool (dev, pool);
  
  return;
}
#endif

static inline void fill_rx_pool (amb_dev * dev, unsigned char pool, int priority) {
  rx_in rx;
  amb_rxq * rxq;
  
  PRINTD (DBG_FLOW|DBG_POOL, "fill_rx_pool %p %hu %x", dev, pool, priority);
  
  if (test_bit (dead, &dev->flags))
    return;
  
  rxq = &dev->rxq[pool];
  while (rxq->pending < rxq->maximum && rxq->pending < rxq->buffers_wanted) {
    
    struct sk_buff * skb = alloc_skb (rxq->buffer_size, priority);
    if (!skb) {
      PRINTD (DBG_SKB|DBG_POOL, "failed to allocate skb for RX pool %hu", pool);
      return;
    }
    if (check_area (skb->data, skb->truesize)) {
      dev_kfree_skb_any (skb);
      return;
    }
    // cast needed as there is no %? for pointer differences
    PRINTD (DBG_SKB, "allocated skb at %p, head %p, area %li",
	    skb, skb->head, (long) (skb->end - skb->head));
    rx.handle = virt_to_bus (skb);
    rx.host_address = cpu_to_be32 (virt_to_bus (skb->data));
    if (rx_give (dev, &rx, pool))
      dev_kfree_skb_any (skb);
    
  }
  
  return;
}

// top up all RX pools (can also be called as a bottom half)
static void fill_rx_pools (amb_dev * dev) {
  unsigned char pool;
  
  PRINTD (DBG_FLOW|DBG_POOL, "fill_rx_pools %p", dev);
  
  for (pool = 0; pool < NUM_RX_POOLS; ++pool)
    fill_rx_pool (dev, pool, GFP_ATOMIC);
  
  return;
}

/********** enable host interrupts **********/

static inline void interrupts_on (amb_dev * dev) {
  wr_plain (dev, offsetof(amb_mem, interrupt_control),
	    rd_plain (dev, offsetof(amb_mem, interrupt_control))
	    | AMB_INTERRUPT_BITS);
}

/********** disable host interrupts **********/

static inline void interrupts_off (amb_dev * dev) {
  wr_plain (dev, offsetof(amb_mem, interrupt_control),
	    rd_plain (dev, offsetof(amb_mem, interrupt_control))
	    &~ AMB_INTERRUPT_BITS);
}

/********** interrupt handling **********/

static void interrupt_handler (int irq, void * dev_id, struct pt_regs * pt_regs) {
  amb_dev * dev = amb_devs;
  (void) pt_regs;
  
  PRINTD (DBG_IRQ|DBG_FLOW, "interrupt_handler: %p", dev_id);
  
  if (!dev_id) {
    PRINTD (DBG_IRQ|DBG_ERR, "irq with NULL dev_id: %d", irq);
    return;
  }
  // Did one of our cards generate the interrupt?
  while (dev) {
    if (dev == dev_id)
      break;
    dev = dev->prev;
  }
  // impossible - unless we add the device to our list after both
  // registering the IRQ handler for it and enabling interrupts, AND
  // the card generates an IRQ at startup - should not happen again
  if (!dev) {
    PRINTD (DBG_IRQ, "irq for unknown device: %d", irq);
    return;
  }
  // impossible - unless we have memory corruption of dev or kernel
  if (irq != dev->irq) {
    PRINTD (DBG_IRQ|DBG_ERR, "irq mismatch: %d", irq);
    return;
  }
  
  {
    u32 interrupt = rd_plain (dev, offsetof(amb_mem, interrupt));
  
    // for us or someone else sharing the same interrupt
    if (!interrupt) {
      PRINTD (DBG_IRQ, "irq not for me: %d", irq);
      return;
    }
    
    // definitely for us
    PRINTD (DBG_IRQ, "FYI: interrupt was %08x", interrupt);
    wr_plain (dev, offsetof(amb_mem, interrupt), -1);
  }
  
  {
    unsigned int irq_work = 0;
    unsigned char pool;
    for (pool = 0; pool < NUM_RX_POOLS; ++pool)
      while (!rx_take (dev, pool))
	++irq_work;
    while (!tx_take (dev))
      ++irq_work;
  
    if (irq_work) {
#ifdef FILL_RX_POOLS_IN_BH
      queue_task (&dev->bh, &tq_immediate);
      mark_bh (IMMEDIATE_BH);
#else
      fill_rx_pools (dev);
#endif

      PRINTD (DBG_IRQ, "work done: %u", irq_work);
    } else {
      PRINTD (DBG_IRQ|DBG_WARN, "no work done");
    }
  }
  
  PRINTD (DBG_IRQ|DBG_FLOW, "interrupt_handler done: %p", dev_id);
  return;
}

/********** don't panic... yeah, right **********/

#ifdef DEBUG_AMBASSADOR
static void dont_panic (amb_dev * dev) {
  amb_cq * cq = &dev->cq;
  volatile amb_cq_ptrs * ptrs = &cq->ptrs;
  amb_txq * txq;
  amb_rxq * rxq;
  command * cmd;
  tx_in * tx;
  tx_simple * tx_descr;
  unsigned char pool;
  rx_in * rx;
  
  unsigned long flags;
  save_flags (flags);
  cli();
  
  PRINTK (KERN_INFO, "don't panic - putting adapter into reset");
  wr_plain (dev, offsetof(amb_mem, reset_control),
	    rd_plain (dev, offsetof(amb_mem, reset_control)) | AMB_RESET_BITS);
  
  PRINTK (KERN_INFO, "marking all commands complete");
  for (cmd = ptrs->start; cmd < ptrs->limit; ++cmd)
    cmd->request = cpu_to_be32 (SRB_COMPLETE);

  PRINTK (KERN_INFO, "completing all TXs");
  txq = &dev->txq;
  tx = txq->in.ptr;
  while (txq->pending--) {
    if (tx == txq->in.start)
      tx = txq->in.limit;
    --tx;
    tx_descr = bus_to_virt (be32_to_cpu (tx->tx_descr_addr));
    amb_kfree_skb (tx_descr->skb);
    kfree (tx_descr);
  }
  
  PRINTK (KERN_INFO, "freeing all RX buffers");
  for (pool = 0; pool < NUM_RX_POOLS; ++pool) {
    rxq = &dev->rxq[pool];
    rx = rxq->in.ptr;
    while (rxq->pending--) {
      if (rx == rxq->in.start)
	rx = rxq->in.limit;
      --rx;
      dev_kfree_skb_any (bus_to_virt (rx->handle));
    }
  }
  
  PRINTK (KERN_INFO, "don't panic over - close all VCs and rmmod");
  set_bit (dead, &dev->flags);
  restore_flags (flags);
  return;
}
#endif

/********** make rate (not quite as much fun as Horizon) **********/

static unsigned int make_rate (unsigned int rate, rounding r,
			       u16 * bits, unsigned int * actual) {
  unsigned char exp = -1; // hush gcc
  unsigned int man = -1;  // hush gcc
  
  PRINTD (DBG_FLOW|DBG_QOS, "make_rate %u", rate);
  
  // rates in cells per second, ITU format (nasty 16-bit floating-point)
  // given 5-bit e and 9-bit m:
  // rate = EITHER (1+m/2^9)*2^e    OR 0
  // bits = EITHER 1<<14 | e<<9 | m OR 0
  // (bit 15 is "reserved", bit 14 "non-zero")
  // smallest rate is 0 (special representation)
  // largest rate is (1+511/512)*2^31 = 4290772992 (< 2^32-1)
  // smallest non-zero rate is (1+0/512)*2^0 = 1 (> 0)
  // simple algorithm:
  // find position of top bit, this gives e
  // remove top bit and shift (rounding if feeling clever) by 9-e
  
  // ucode bug: please don't set bit 14! so 0 rate not representable
  
  if (rate > 0xffc00000U) {
    // larger than largest representable rate
    
    if (r == round_up) {
	return -EINVAL;
    } else {
      exp = 31;
      man = 511;
    }
    
  } else if (rate) {
    // representable rate
    
    exp = 31;
    man = rate;
    
    // invariant: rate = man*2^(exp-31)
    while (!(man & (1<<31))) {
      exp = exp - 1;
      man = man<<1;
    }
    
    // man has top bit set
    // rate = (2^31+(man-2^31))*2^(exp-31)
    // rate = (1+(man-2^31)/2^31)*2^exp
    man = man<<1;
    man &= 0xffffffffU; // a nop on 32-bit systems
    // rate = (1+man/2^32)*2^exp
    
    // exp is in the range 0 to 31, man is in the range 0 to 2^32-1
    // time to lose significance... we want m in the range 0 to 2^9-1
    // rounding presents a minor problem... we first decide which way
    // we are rounding (based on given rounding direction and possibly
    // the bits of the mantissa that are to be discarded).
    
    switch (r) {
      case round_down: {
	// just truncate
	man = man>>(32-9);
	break;
      }
      case round_up: {
	// check all bits that we are discarding
	if (man & (-1>>9)) {
	  man = (man>>(32-9)) + 1;
	  if (man == (1<<9)) {
	    // no need to check for round up outside of range
	    man = 0;
	    exp += 1;
	  }
	} else {
	  man = (man>>(32-9));
	}
	break;
      }
      case round_nearest: {
	// check msb that we are discarding
	if (man & (1<<(32-9-1))) {
	  man = (man>>(32-9)) + 1;
	  if (man == (1<<9)) {
	    // no need to check for round up outside of range
	    man = 0;
	    exp += 1;
	  }
	} else {
	  man = (man>>(32-9));
	}
	break;
      }
    }
    
  } else {
    // zero rate - not representable
    
    if (r == round_down) {
      return -EINVAL;
    } else {
      exp = 0;
      man = 0;
    }
    
  }
  
  PRINTD (DBG_QOS, "rate: man=%u, exp=%hu", man, exp);
  
  if (bits)
    *bits = /* (1<<14) | */ (exp<<9) | man;
  
  if (actual)
    *actual = (exp >= 9)
      ? (1 << exp) + (man << (exp-9))
      : (1 << exp) + ((man + (1<<(9-exp-1))) >> (9-exp));
  
  return 0;
}

/********** Linux ATM Operations **********/

// some are not yet implemented while others do not make sense for
// this device

/********** Open a VC **********/

static int amb_open (struct atm_vcc * atm_vcc, short vpi, int vci) {
  int error;
  
  struct atm_qos * qos;
  struct atm_trafprm * txtp;
  struct atm_trafprm * rxtp;
  u16 tx_rate_bits;
  u16 tx_vc_bits = -1; // hush gcc
  u16 tx_frame_bits = -1; // hush gcc
  
  amb_dev * dev = AMB_DEV(atm_vcc->dev);
  amb_vcc * vcc;
  unsigned char pool = -1; // hush gcc
  
  PRINTD (DBG_FLOW|DBG_VCC, "amb_open %x %x", vpi, vci);
  
#ifdef ATM_VPI_UNSPEC
  // UNSPEC is deprecated, remove this code eventually
  if (vpi == ATM_VPI_UNSPEC || vci == ATM_VCI_UNSPEC) {
    PRINTK (KERN_WARNING, "rejecting open with unspecified VPI/VCI (deprecated)");
    return -EINVAL;
  }
#endif
  
  // deal with possibly wildcarded VCs
  error = atm_find_ci (atm_vcc, &vpi, &vci);
  if (error) {
    PRINTD (DBG_WARN|DBG_VCC, "atm_find_ci failed!");
    return error;
  }
  PRINTD (DBG_VCC, "atm_find_ci gives %x %x", vpi, vci);
  
  if (!(0 <= vpi && vpi < (1<<NUM_VPI_BITS) &&
	0 <= vci && vci < (1<<NUM_VCI_BITS))) {
    PRINTD (DBG_WARN|DBG_VCC, "VPI/VCI out of range: %hd/%d", vpi, vci);
    return -EINVAL;
  }
  
  qos = &atm_vcc->qos;
  
  if (qos->aal != ATM_AAL5) {
    PRINTD (DBG_QOS, "AAL not supported");
    return -EINVAL;
  }
  
  // traffic parameters
  
  PRINTD (DBG_QOS, "TX:");
  txtp = &qos->txtp;
  if (txtp->traffic_class != ATM_NONE) {
    switch (txtp->traffic_class) {
      case ATM_UBR: {
	// we take "the PCR" as a rate-cap
	int pcr = atm_pcr_goal (txtp);
	if (!pcr) {
	  // no rate cap
	  tx_rate_bits = 0;
	  tx_vc_bits = TX_UBR;
	  tx_frame_bits = TX_FRAME_NOTCAP;
	} else {
	  rounding r;
	  if (pcr < 0) {
	    r = round_down;
	    pcr = -pcr;
	  } else {
	    r = round_up;
	  }
	  error = make_rate (pcr, r, &tx_rate_bits, 0);
	  tx_vc_bits = TX_UBR_CAPPED;
	  tx_frame_bits = TX_FRAME_CAPPED;
	}
	break;
      }
#if 0
      case ATM_ABR: {
	pcr = atm_pcr_goal (txtp);
	PRINTD (DBG_QOS, "pcr goal = %d", pcr);
	break;
      }
#endif
      default: {
	// PRINTD (DBG_QOS, "request for non-UBR/ABR denied");
	PRINTD (DBG_QOS, "request for non-UBR denied");
	return -EINVAL;
      }
    }
    PRINTD (DBG_QOS, "tx_rate_bits=%hx, tx_vc_bits=%hx",
	    tx_rate_bits, tx_vc_bits);
  }
  
  PRINTD (DBG_QOS, "RX:");
  rxtp = &qos->rxtp;
  if (rxtp->traffic_class == ATM_NONE) {
    // do nothing
  } else {
    // choose an RX pool (arranged in increasing size)
    for (pool = 0; pool < NUM_RX_POOLS; ++pool)
      if ((unsigned int) rxtp->max_sdu <= dev->rxq[pool].buffer_size) {
	PRINTD (DBG_VCC|DBG_QOS|DBG_POOL, "chose pool %hu (max_sdu %u <= %u)",
		pool, rxtp->max_sdu, dev->rxq[pool].buffer_size);
	break;
      }
    if (pool == NUM_RX_POOLS) {
      PRINTD (DBG_WARN|DBG_VCC|DBG_QOS|DBG_POOL,
	      "no pool suitable for VC (RX max_sdu %d is too large)",
	      rxtp->max_sdu);
      return -EINVAL;
    }
    
    switch (rxtp->traffic_class) {
      case ATM_UBR: {
	break;
      }
#if 0
      case ATM_ABR: {
	pcr = atm_pcr_goal (rxtp);
	PRINTD (DBG_QOS, "pcr goal = %d", pcr);
	break;
      }
#endif
      default: {
	// PRINTD (DBG_QOS, "request for non-UBR/ABR denied");
	PRINTD (DBG_QOS, "request for non-UBR denied");
	return -EINVAL;
      }
    }
  }
  
  // get space for our vcc stuff
  vcc = kmalloc (sizeof(amb_vcc), GFP_KERNEL);
  if (!vcc) {
    PRINTK (KERN_ERR, "out of memory!");
    return -ENOMEM;
  }
  atm_vcc->dev_data = (void *) vcc;
  
  // no failures beyond this point
  
  // we are not really "immediately before allocating the connection
  // identifier in hardware", but it will just have to do!
  set_bit(ATM_VF_ADDR,&atm_vcc->flags);
  
  if (txtp->traffic_class != ATM_NONE) {
    command cmd;
    
    vcc->tx_frame_bits = tx_frame_bits;
    
    down (&dev->vcc_sf);
    if (dev->rxer[vci]) {
      // RXer on the channel already, just modify rate...
      cmd.request = cpu_to_be32 (SRB_MODIFY_VC_RATE);
      cmd.args.modify_rate.vc = cpu_to_be32 (vci);  // vpi 0
      cmd.args.modify_rate.rate = cpu_to_be32 (tx_rate_bits << SRB_RATE_SHIFT);
      while (command_do (dev, &cmd))
	schedule();
      // ... and TX flags, preserving the RX pool
      cmd.request = cpu_to_be32 (SRB_MODIFY_VC_FLAGS);
      cmd.args.modify_flags.vc = cpu_to_be32 (vci);  // vpi 0
      cmd.args.modify_flags.flags = cpu_to_be32
	( (AMB_VCC(dev->rxer[vci])->rx_info.pool << SRB_POOL_SHIFT)
	  | (tx_vc_bits << SRB_FLAGS_SHIFT) );
      while (command_do (dev, &cmd))
	schedule();
    } else {
      // no RXer on the channel, just open (with pool zero)
      cmd.request = cpu_to_be32 (SRB_OPEN_VC);
      cmd.args.open.vc = cpu_to_be32 (vci);  // vpi 0
      cmd.args.open.flags = cpu_to_be32 (tx_vc_bits << SRB_FLAGS_SHIFT);
      cmd.args.open.rate = cpu_to_be32 (tx_rate_bits << SRB_RATE_SHIFT);
      while (command_do (dev, &cmd))
	schedule();
    }
    dev->txer[vci].tx_present = 1;
    up (&dev->vcc_sf);
  }
  
  if (rxtp->traffic_class != ATM_NONE) {
    command cmd;
    
    vcc->rx_info.pool = pool;
    
    down (&dev->vcc_sf); 
    /* grow RX buffer pool */
    if (!dev->rxq[pool].buffers_wanted)
      dev->rxq[pool].buffers_wanted = rx_lats;
    dev->rxq[pool].buffers_wanted += 1;
    fill_rx_pool (dev, pool, GFP_KERNEL);
    
    if (dev->txer[vci].tx_present) {
      // TXer on the channel already
      // switch (from pool zero) to this pool, preserving the TX bits
      cmd.request = cpu_to_be32 (SRB_MODIFY_VC_FLAGS);
      cmd.args.modify_flags.vc = cpu_to_be32 (vci);  // vpi 0
      cmd.args.modify_flags.flags = cpu_to_be32
	( (pool << SRB_POOL_SHIFT)
	  | (dev->txer[vci].tx_vc_bits << SRB_FLAGS_SHIFT) );
    } else {
      // no TXer on the channel, open the VC (with no rate info)
      cmd.request = cpu_to_be32 (SRB_OPEN_VC);
      cmd.args.open.vc = cpu_to_be32 (vci);  // vpi 0
      cmd.args.open.flags = cpu_to_be32 (pool << SRB_POOL_SHIFT);
      cmd.args.open.rate = cpu_to_be32 (0);
    }
    while (command_do (dev, &cmd))
      schedule();
    // this link allows RX frames through
    dev->rxer[vci] = atm_vcc;
    up (&dev->vcc_sf);
  }
  
  // set elements of vcc
  atm_vcc->vpi = vpi; // 0
  atm_vcc->vci = vci;
  
  // indicate readiness
  set_bit(ATM_VF_READY,&atm_vcc->flags);
  
  return 0;
}

/********** Close a VC **********/

static void amb_close (struct atm_vcc * atm_vcc) {
  amb_dev * dev = AMB_DEV (atm_vcc->dev);
  amb_vcc * vcc = AMB_VCC (atm_vcc);
  u16 vci = atm_vcc->vci;
  
  PRINTD (DBG_VCC|DBG_FLOW, "amb_close");
  
  // indicate unreadiness
  clear_bit(ATM_VF_READY,&atm_vcc->flags);
  
  // disable TXing
  if (atm_vcc->qos.txtp.traffic_class != ATM_NONE) {
    command cmd;
    
    down (&dev->vcc_sf);
    if (dev->rxer[vci]) {
      // RXer still on the channel, just modify rate... XXX not really needed
      cmd.request = cpu_to_be32 (SRB_MODIFY_VC_RATE);
      cmd.args.modify_rate.vc = cpu_to_be32 (vci);  // vpi 0
      cmd.args.modify_rate.rate = cpu_to_be32 (0);
      // ... and clear TX rate flags (XXX to stop RM cell output?), preserving RX pool
    } else {
      // no RXer on the channel, close channel
      cmd.request = cpu_to_be32 (SRB_CLOSE_VC);
      cmd.args.close.vc = cpu_to_be32 (vci); // vpi 0
    }
    dev->txer[vci].tx_present = 0;
    while (command_do (dev, &cmd))
      schedule();
    up (&dev->vcc_sf);
  }
  
  // disable RXing
  if (atm_vcc->qos.rxtp.traffic_class != ATM_NONE) {
    command cmd;
    
    // this is (the?) one reason why we need the amb_vcc struct
    unsigned char pool = vcc->rx_info.pool;
    
    down (&dev->vcc_sf);
    if (dev->txer[vci].tx_present) {
      // TXer still on the channel, just go to pool zero XXX not really needed
      cmd.request = cpu_to_be32 (SRB_MODIFY_VC_FLAGS);
      cmd.args.modify_flags.vc = cpu_to_be32 (vci);  // vpi 0
      cmd.args.modify_flags.flags = cpu_to_be32
	(dev->txer[vci].tx_vc_bits << SRB_FLAGS_SHIFT);
    } else {
      // no TXer on the channel, close the VC
      cmd.request = cpu_to_be32 (SRB_CLOSE_VC);
      cmd.args.close.vc = cpu_to_be32 (vci); // vpi 0
    }
    // forget the rxer - no more skbs will be pushed
    if (atm_vcc != dev->rxer[vci])
      PRINTK (KERN_ERR, "%s vcc=%p rxer[vci]=%p",
	      "arghhh! we're going to die!",
	      vcc, dev->rxer[vci]);
    dev->rxer[vci] = 0;
    while (command_do (dev, &cmd))
      schedule();
    
    /* shrink RX buffer pool */
    dev->rxq[pool].buffers_wanted -= 1;
    if (dev->rxq[pool].buffers_wanted == rx_lats) {
      dev->rxq[pool].buffers_wanted = 0;
      drain_rx_pool (dev, pool);
    }
    up (&dev->vcc_sf);
  }
  
  // free our structure
  kfree (vcc);
  
  // say the VPI/VCI is free again
  clear_bit(ATM_VF_ADDR,&atm_vcc->flags);

  return;
}

/********** DebugIoctl **********/

#if 0
static int amb_ioctl (struct atm_dev * dev, unsigned int cmd, void * arg) {
  unsigned short newdebug;
  if (cmd == AMB_SETDEBUG) {
    if (!capable(CAP_NET_ADMIN))
      return -EPERM;
    if (copy_from_user (&newdebug, arg, sizeof(newdebug))) {
      // moan
      return -EFAULT;
    } else {
      debug = newdebug;
      return 0;
    }
  } else if (cmd == AMB_DONTPANIC) {
    if (!capable(CAP_NET_ADMIN))
      return -EPERM;
    dont_panic (dev);
  } else {
    // moan
    return -ENOIOCTLCMD;
  }
}
#endif

/********** Set socket options for a VC **********/

// int amb_getsockopt (struct atm_vcc * atm_vcc, int level, int optname, void * optval, int optlen);

/********** Set socket options for a VC **********/

// int amb_setsockopt (struct atm_vcc * atm_vcc, int level, int optname, void * optval, int optlen);

/********** Send **********/

static int amb_send (struct atm_vcc * atm_vcc, struct sk_buff * skb) {
  amb_dev * dev = AMB_DEV(atm_vcc->dev);
  amb_vcc * vcc = AMB_VCC(atm_vcc);
  u16 vc = atm_vcc->vci;
  unsigned int tx_len = skb->len;
  unsigned char * tx_data = skb->data;
  tx_simple * tx_descr;
  tx_in tx;
  
  if (test_bit (dead, &dev->flags))
    return -EIO;
  
  PRINTD (DBG_FLOW|DBG_TX, "amb_send vc %x data %p len %u",
	  vc, tx_data, tx_len);
  
  dump_skb (">>>", vc, skb);
  
  if (!dev->txer[vc].tx_present) {
    PRINTK (KERN_ERR, "attempt to send on RX-only VC %x", vc);
    return -EBADFD;
  }
  
  // this is a driver private field so we have to set it ourselves,
  // despite the fact that we are _required_ to use it to check for a
  // pop function
  ATM_SKB(skb)->vcc = atm_vcc;
  
  if (skb->len > (size_t) atm_vcc->qos.txtp.max_sdu) {
    PRINTK (KERN_ERR, "sk_buff length greater than agreed max_sdu, dropping...");
    return -EIO;
  }
  
  if (check_area (skb->data, skb->len)) {
    atomic_inc(&atm_vcc->stats->tx_err);
    return -ENOMEM; // ?
  }
  
  // allocate memory for fragments
  tx_descr = kmalloc (sizeof(tx_simple), GFP_KERNEL);
  if (!tx_descr) {
    PRINTK (KERN_ERR, "could not allocate TX descriptor");
    return -ENOMEM;
  }
  if (check_area (tx_descr, sizeof(tx_simple))) {
    kfree (tx_descr);
    return -ENOMEM;
  }
  PRINTD (DBG_TX, "fragment list allocated at %p", tx_descr);
  
  tx_descr->skb = skb;
  
  tx_descr->tx_frag.bytes = cpu_to_be32 (tx_len);
  tx_descr->tx_frag.address = cpu_to_be32 (virt_to_bus (tx_data));
  
  tx_descr->tx_frag_end.handle = virt_to_bus (tx_descr);
  tx_descr->tx_frag_end.vc = 0;
  tx_descr->tx_frag_end.next_descriptor_length = 0;
  tx_descr->tx_frag_end.next_descriptor = 0;
#ifdef AMB_NEW_MICROCODE
  tx_descr->tx_frag_end.cpcs_uu = 0;
  tx_descr->tx_frag_end.cpi = 0;
  tx_descr->tx_frag_end.pad = 0;
#endif
  
  tx.vc = cpu_to_be16 (vcc->tx_frame_bits | vc);
  tx.tx_descr_length = cpu_to_be16 (sizeof(tx_frag)+sizeof(tx_frag_end));
  tx.tx_descr_addr = cpu_to_be32 (virt_to_bus (&tx_descr->tx_frag));
  
#ifdef DEBUG_AMBASSADOR
  /* wey-hey! */
  if (vc == 1023) {
    unsigned int i;
    unsigned short d = 0;
    char * s = skb->data;
    switch (*s++) {
      case 'D': {
	for (i = 0; i < 4; ++i) {
	  d = (d<<4) | ((*s <= '9') ? (*s - '0') : (*s - 'a' + 10));
	  ++s;
	}
	PRINTK (KERN_INFO, "debug bitmap is now %hx", debug = d);
	break;
      }
      case 'R': {
	if (*s++ == 'e' && *s++ == 's' && *s++ == 'e' && *s++ == 't')
	  dont_panic (dev);
	break;
      }
      default: {
	break;
      }
    }
  }
#endif
  
  while (tx_give (dev, &tx))
    schedule();
  return 0;
}

/********** Scatter Gather Send Capability **********/

static int amb_sg_send (struct atm_vcc * atm_vcc,
			unsigned long start,
			unsigned long size) {
  PRINTD (DBG_FLOW|DBG_VCC, "amb_sg_send: never");
  return 0;
  if (atm_vcc->qos.aal == ATM_AAL5) {
    PRINTD (DBG_FLOW|DBG_VCC, "amb_sg_send: yes");
    return 1;
  } else {
    PRINTD (DBG_FLOW|DBG_VCC, "amb_sg_send: no");
    return 0;
  }
  PRINTD (DBG_FLOW|DBG_VCC, "amb_sg_send: always");
  return 1;
}

/********** Send OAM **********/

// static int amb_send_oam (struct atm_vcc * atm_vcc, void * cell, int flags);

/********** Feedback to Driver **********/

// void amb_feedback (struct atm_vcc * atm_vcc, struct sk_buff * skb,
// unsigned long start, unsigned long dest, int len);

/********** Change QoS on a VC **********/

// int amb_change_qos (struct atm_vcc * atm_vcc, struct atm_qos * qos, int flags);

/********** Free RX Socket Buffer **********/

#if 0
static void amb_free_rx_skb (struct atm_vcc * atm_vcc, struct sk_buff * skb) {
  amb_dev * dev = AMB_DEV (atm_vcc->dev);
  amb_vcc * vcc = AMB_VCC (atm_vcc);
  unsigned char pool = vcc->rx_info.pool;
  rx_in rx;
  
  // This may be unsafe for various reasons that I cannot really guess
  // at. However, I note that the ATM layer calls kfree_skb rather
  // than dev_kfree_skb at this point so we are least covered as far
  // as buffer locking goes. There may be bugs if pcap clones RX skbs.

  PRINTD (DBG_FLOW|DBG_SKB, "amb_rx_free skb %p (atm_vcc %p, vcc %p)",
	  skb, atm_vcc, vcc);
  
  rx.handle = virt_to_bus (skb);
  rx.host_address = cpu_to_be32 (virt_to_bus (skb->data));
  
  skb->data = skb->head;
  skb->tail = skb->head;
  skb->len = 0;
  
  if (!rx_give (dev, &rx, pool)) {
    // success
    PRINTD (DBG_SKB|DBG_POOL, "recycled skb for pool %hu", pool);
    return;
  }
  
  // just do what the ATM layer would have done
  dev_kfree_skb_any (skb);
  
  return;
}
#endif

/********** Proc File Output **********/

static int amb_proc_read (struct atm_dev * atm_dev, loff_t * pos, char * page) {
  amb_dev * dev = AMB_DEV (atm_dev);
  int left = *pos;
  unsigned char pool;
  
  PRINTD (DBG_FLOW, "amb_proc_read");
  
  /* more diagnostics here? */
  
  if (!left--) {
    amb_stats * s = &dev->stats;
    return sprintf (page,
		    "frames: TX OK %lu, RX OK %lu, RX bad %lu "
		    "(CRC %lu, long %lu, aborted %lu, unused %lu).\n",
		    s->tx_ok, s->rx.ok, s->rx.error,
		    s->rx.badcrc, s->rx.toolong,
		    s->rx.aborted, s->rx.unused);
  }
  
  if (!left--) {
    amb_cq * c = &dev->cq;
    return sprintf (page, "cmd queue [cur/hi/max]: %u/%u/%u. ",
		    c->pending, c->high, c->maximum);
  }
  
  if (!left--) {
    amb_txq * t = &dev->txq;
    return sprintf (page, "TX queue [cur/max high full]: %u/%u %u %u.\n",
		    t->pending, t->maximum, t->high, t->filled);
  }
  
  if (!left--) {
    unsigned int count = sprintf (page, "RX queues [cur/max/req low empty]:");
    for (pool = 0; pool < NUM_RX_POOLS; ++pool) {
      amb_rxq * r = &dev->rxq[pool];
      count += sprintf (page+count, " %u/%u/%u %u %u",
			r->pending, r->maximum, r->buffers_wanted, r->low, r->emptied);
    }
    count += sprintf (page+count, ".\n");
    return count;
  }
  
  if (!left--) {
    unsigned int count = sprintf (page, "RX buffer sizes:");
    for (pool = 0; pool < NUM_RX_POOLS; ++pool) {
      amb_rxq * r = &dev->rxq[pool];
      count += sprintf (page+count, " %u", r->buffer_size);
    }
    count += sprintf (page+count, ".\n");
    return count;
  }
  
#if 0
  if (!left--) {
    // suni block etc?
  }
#endif
  
  return 0;
}

/********** Operation Structure **********/

static const struct atmdev_ops amb_ops = {
  open:		amb_open,
  close:	amb_close,
  send:		amb_send,
  sg_send:	amb_sg_send,
  proc_read:	amb_proc_read,
  owner:	THIS_MODULE,
};

/********** housekeeping **********/

static inline void set_timer (struct timer_list * timer, unsigned long delay) {
  timer->expires = jiffies + delay;
  add_timer (timer);
  return;
}

static void do_housekeeping (unsigned long arg) {
  amb_dev * dev = amb_devs;
  // data is set to zero at module unload
  (void) arg;
  
  if (housekeeping.data) {
    while (dev) {
      
      // could collect device-specific (not driver/atm-linux) stats here
      
      // last resort refill once every ten seconds
      fill_rx_pools (dev);
      
      dev = dev->prev;
    }
    set_timer (&housekeeping, 10*HZ);
  }
  
  return;
}

/********** creation of communication queues **********/

static int __init create_queues (amb_dev * dev, unsigned int cmds,
				 unsigned int txs, unsigned int * rxs,
				 unsigned int * rx_buffer_sizes) {
  unsigned char pool;
  size_t total = 0;
  void * memory;
  void * limit;
  
  PRINTD (DBG_FLOW, "create_queues %p", dev);
  
  total += cmds * sizeof(command);
  
  total += txs * (sizeof(tx_in) + sizeof(tx_out));
  
  for (pool = 0; pool < NUM_RX_POOLS; ++pool)
    total += rxs[pool] * (sizeof(rx_in) + sizeof(rx_out));
  
  memory = kmalloc (total, GFP_KERNEL);
  if (!memory) {
    PRINTK (KERN_ERR, "could not allocate queues");
    return -ENOMEM;
  }
  if (check_area (memory, total)) {
    PRINTK (KERN_ERR, "queues allocated in nasty area");
    kfree (memory);
    return -ENOMEM;
  }
  
  limit = memory + total;
  PRINTD (DBG_INIT, "queues from %p to %p", memory, limit);
  
  PRINTD (DBG_CMD, "command queue at %p", memory);
  
  {
    command * cmd = memory;
    amb_cq * cq = &dev->cq;
    
    cq->pending = 0;
    cq->high = 0;
    cq->maximum = cmds - 1;
    
    cq->ptrs.start = cmd;
    cq->ptrs.in = cmd;
    cq->ptrs.out = cmd;
    cq->ptrs.limit = cmd + cmds;
    
    memory = cq->ptrs.limit;
  }
  
  PRINTD (DBG_TX, "TX queue pair at %p", memory);
  
  {
    tx_in * in = memory;
    tx_out * out;
    amb_txq * txq = &dev->txq;
    
    txq->pending = 0;
    txq->high = 0;
    txq->filled = 0;
    txq->maximum = txs - 1;
    
    txq->in.start = in;
    txq->in.ptr = in;
    txq->in.limit = in + txs;
    
    memory = txq->in.limit;
    out = memory;
    
    txq->out.start = out;
    txq->out.ptr = out;
    txq->out.limit = out + txs;
    
    memory = txq->out.limit;
  }
  
  PRINTD (DBG_RX, "RX queue pairs at %p", memory);
  
  for (pool = 0; pool < NUM_RX_POOLS; ++pool) {
    rx_in * in = memory;
    rx_out * out;
    amb_rxq * rxq = &dev->rxq[pool];
    
    rxq->buffer_size = rx_buffer_sizes[pool];
    rxq->buffers_wanted = 0;
    
    rxq->pending = 0;
    rxq->low = rxs[pool] - 1;
    rxq->emptied = 0;
    rxq->maximum = rxs[pool] - 1;
    
    rxq->in.start = in;
    rxq->in.ptr = in;
    rxq->in.limit = in + rxs[pool];
    
    memory = rxq->in.limit;
    out = memory;
    
    rxq->out.start = out;
    rxq->out.ptr = out;
    rxq->out.limit = out + rxs[pool];
    
    memory = rxq->out.limit;
  }
  
  if (memory == limit) {
    return 0;
  } else {
    PRINTK (KERN_ERR, "bad queue alloc %p != %p (tell maintainer)", memory, limit);
    kfree (limit - total);
    return -ENOMEM;
  }
  
}

/********** destruction of communication queues **********/

static void destroy_queues (amb_dev * dev) {
  // all queues assumed empty
  void * memory = dev->cq.ptrs.start;
  // includes txq.in, txq.out, rxq[].in and rxq[].out
  
  PRINTD (DBG_FLOW, "destroy_queues %p", dev);
  
  PRINTD (DBG_INIT, "freeing queues at %p", memory);
  kfree (memory);
  
  return;
}

/********** basic loader commands and error handling **********/

static int __init do_loader_command (volatile loader_block * lb,
				     const amb_dev * dev, loader_command cmd) {
  // centisecond timeouts - guessing away here
  unsigned int command_timeouts [] = {
    [host_memory_test]     = 15,
    [read_adapter_memory]  = 2,
    [write_adapter_memory] = 2,
    [adapter_start]        = 50,
    [get_version_number]   = 10,
    [interrupt_host]       = 1,
    [flash_erase_sector]   = 1,
    [adap_download_block]  = 1,
    [adap_erase_flash]     = 1,
    [adap_run_in_iram]     = 1,
    [adap_end_download]    = 1
  };
  
  unsigned int command_successes [] = {
    [host_memory_test]     = COMMAND_PASSED_TEST,
    [read_adapter_memory]  = COMMAND_READ_DATA_OK,
    [write_adapter_memory] = COMMAND_WRITE_DATA_OK,
    [adapter_start]        = COMMAND_COMPLETE,
    [get_version_number]   = COMMAND_COMPLETE,
    [interrupt_host]       = COMMAND_COMPLETE,
    [flash_erase_sector]   = COMMAND_COMPLETE,
    [adap_download_block]  = COMMAND_COMPLETE,
    [adap_erase_flash]     = COMMAND_COMPLETE,
    [adap_run_in_iram]     = COMMAND_COMPLETE,
    [adap_end_download]    = COMMAND_COMPLETE
  };
  
  int decode_loader_result (loader_command cmd, u32 result) {
    int res;
    const char * msg;
    
    if (result == command_successes[cmd])
      return 0;
    
    switch (result) {
      case BAD_COMMAND:
	res = -EINVAL;
	msg = "bad command";
	break;
      case COMMAND_IN_PROGRESS:
	res = -ETIMEDOUT;
	msg = "command in progress";
	break;
      case COMMAND_PASSED_TEST:
	res = 0;
	msg = "command passed test";
	break;
      case COMMAND_FAILED_TEST:
	res = -EIO;
	msg = "command failed test";
	break;
      case COMMAND_READ_DATA_OK:
	res = 0;
	msg = "command read data ok";
	break;
      case COMMAND_READ_BAD_ADDRESS:
	res = -EINVAL;
	msg = "command read bad address";
	break;
      case COMMAND_WRITE_DATA_OK:
	res = 0;
	msg = "command write data ok";
	break;
      case COMMAND_WRITE_BAD_ADDRESS:
	res = -EINVAL;
	msg = "command write bad address";
	break;
      case COMMAND_WRITE_FLASH_FAILURE:
	res = -EIO;
	msg = "command write flash failure";
	break;
      case COMMAND_COMPLETE:
	res = 0;
	msg = "command complete";
	break;
      case COMMAND_FLASH_ERASE_FAILURE:
	res = -EIO;
	msg = "command flash erase failure";
	break;
      case COMMAND_WRITE_BAD_DATA:
	res = -EINVAL;
	msg = "command write bad data";
	break;
      default:
	res = -EINVAL;
	msg = "unknown error";
	PRINTD (DBG_LOAD|DBG_ERR, "decode_loader_result got %d=%x !",
		result, result);
	break;
    }
    
    PRINTK (KERN_ERR, "%s", msg);
    return res;
  }
  
  unsigned long timeout;
  
  PRINTD (DBG_FLOW|DBG_LOAD, "do_loader_command");
  
  /* do a command
     
     Set the return value to zero, set the command type and set the
     valid entry to the right magic value. The payload is already
     correctly byte-ordered so we leave it alone. Hit the doorbell
     with the bus address of this structure.
     
  */
  
  lb->result = 0;
  lb->command = cpu_to_be32 (cmd);
  lb->valid = cpu_to_be32 (DMA_VALID);
  // dump_registers (dev);
  // dump_loader_block (lb);
  wr_mem (dev, offsetof(amb_mem, doorbell), virt_to_bus (lb) & ~onegigmask);
  
  timeout = command_timeouts[cmd] * HZ/100;
  
  while (!lb->result || lb->result == cpu_to_be32 (COMMAND_IN_PROGRESS))
    if (timeout) {
      set_current_state(TASK_UNINTERRUPTIBLE);
      timeout = schedule_timeout (timeout);
    } else {
      PRINTD (DBG_LOAD|DBG_ERR, "command %d timed out", cmd);
      dump_registers (dev);
      dump_loader_block (lb);
      return -ETIMEDOUT;
    }
  
  if (cmd == adapter_start) {
    // wait for start command to acknowledge...
    timeout = HZ/10;
    while (rd_plain (dev, offsetof(amb_mem, doorbell)))
      if (timeout) {
	timeout = schedule_timeout (timeout);
      } else {
	PRINTD (DBG_LOAD|DBG_ERR, "start command did not clear doorbell, res=%08x",
		be32_to_cpu (lb->result));
	dump_registers (dev);
	return -ETIMEDOUT;
      }
    return 0;
  } else {
    return decode_loader_result (cmd, be32_to_cpu (lb->result));
  }
  
}

/* loader: determine loader version */

static int __init get_loader_version (loader_block * lb,
				      const amb_dev * dev, u32 * version) {
  int res;
  
  PRINTD (DBG_FLOW|DBG_LOAD, "get_loader_version");
  
  res = do_loader_command (lb, dev, get_version_number);
  if (res)
    return res;
  if (version)
    *version = be32_to_cpu (lb->payload.version);
  return 0;
}

/* loader: write memory data blocks */

static int __init loader_write (loader_block * lb,
				const amb_dev * dev, const u32 * data,
				u32 address, unsigned int count) {
  unsigned int i;
  transfer_block * tb = &lb->payload.transfer;
  
  PRINTD (DBG_FLOW|DBG_LOAD, "loader_write");
  
  if (count > MAX_TRANSFER_DATA)
    return -EINVAL;
  tb->address = cpu_to_be32 (address);
  tb->count = cpu_to_be32 (count);
  for (i = 0; i < count; ++i)
    tb->data[i] = cpu_to_be32 (data[i]);
  return do_loader_command (lb, dev, write_adapter_memory);
}

/* loader: verify memory data blocks */

static int __init loader_verify (loader_block * lb,
				 const amb_dev * dev, const u32 * data,
				 u32 address, unsigned int count) {
  unsigned int i;
  transfer_block * tb = &lb->payload.transfer;
  int res;
  
  PRINTD (DBG_FLOW|DBG_LOAD, "loader_verify");
  
  if (count > MAX_TRANSFER_DATA)
    return -EINVAL;
  tb->address = cpu_to_be32 (address);
  tb->count = cpu_to_be32 (count);
  res = do_loader_command (lb, dev, read_adapter_memory);
  if (!res)
    for (i = 0; i < count; ++i)
      if (tb->data[i] != cpu_to_be32 (data[i])) {
	res = -EINVAL;
	break;
      }
  return res;
}

/* loader: start microcode */

static int __init loader_start (loader_block * lb,
				const amb_dev * dev, u32 address) {
  PRINTD (DBG_FLOW|DBG_LOAD, "loader_start");
  
  lb->payload.start = cpu_to_be32 (address);
  return do_loader_command (lb, dev, adapter_start);
}

/********** reset card **********/

static int amb_reset (amb_dev * dev, int diags) {
  u32 word;
  
  PRINTD (DBG_FLOW|DBG_LOAD, "amb_reset");
  
  word = rd_plain (dev, offsetof(amb_mem, reset_control));
  // put card into reset state
  wr_plain (dev, offsetof(amb_mem, reset_control), word | AMB_RESET_BITS);
  // wait a short while
  udelay (10);
#if 1
  // put card into known good state
  wr_plain (dev, offsetof(amb_mem, interrupt_control), AMB_DOORBELL_BITS);
  // clear all interrupts just in case
  wr_plain (dev, offsetof(amb_mem, interrupt), -1);
#endif
  // clear self-test done flag
  wr_plain (dev, offsetof(amb_mem, mb.loader.ready), 0);
  // take card out of reset state
  wr_plain (dev, offsetof(amb_mem, reset_control), word &~ AMB_RESET_BITS);
  
  if (diags) { 
    unsigned long timeout;
    // 4.2 second wait
    timeout = HZ*42/10;
    while (timeout) {
      set_current_state(TASK_UNINTERRUPTIBLE);
      timeout = schedule_timeout (timeout);
    }
    // half second time-out
    timeout = HZ/2;
    while (!rd_plain (dev, offsetof(amb_mem, mb.loader.ready)))
      if (timeout) {
        set_current_state(TASK_UNINTERRUPTIBLE);
	timeout = schedule_timeout (timeout);
      } else {
	PRINTD (DBG_LOAD|DBG_ERR, "reset timed out");
	return -ETIMEDOUT;
      }
    
    // get results of self-test
    // XXX double check byte-order
    word = rd_mem (dev, offsetof(amb_mem, mb.loader.result));
    if (word & SELF_TEST_FAILURE) {
      void sf (const char * msg) {
	PRINTK (KERN_ERR, "self-test failed: %s", msg);
      }
      if (word & GPINT_TST_FAILURE)
	sf ("interrupt");
      if (word & SUNI_DATA_PATTERN_FAILURE)
	sf ("SUNI data pattern");
      if (word & SUNI_DATA_BITS_FAILURE)
	sf ("SUNI data bits");
      if (word & SUNI_UTOPIA_FAILURE)
	sf ("SUNI UTOPIA interface");
      if (word & SUNI_FIFO_FAILURE)
	sf ("SUNI cell buffer FIFO");
      if (word & SRAM_FAILURE)
	sf ("bad SRAM");
      // better return value?
      return -EIO;
    }
    
  }
  return 0;
}

/********** transfer and start the microcode **********/

static int __init ucode_init (loader_block * lb, amb_dev * dev) {
  unsigned int i = 0;
  unsigned int total = 0;
  const u32 * pointer = ucode_data;
  u32 address;
  unsigned int count;
  int res;
  
  PRINTD (DBG_FLOW|DBG_LOAD, "ucode_init");
  
  while (address = ucode_regions[i].start,
	 count = ucode_regions[i].count) {
    PRINTD (DBG_LOAD, "starting region (%x, %u)", address, count);
    while (count) {
      unsigned int words;
      if (count <= MAX_TRANSFER_DATA)
	words = count;
      else
	words = MAX_TRANSFER_DATA;
      total += words;
      res = loader_write (lb, dev, pointer, address, words);
      if (res)
	return res;
      res = loader_verify (lb, dev, pointer, address, words);
      if (res)
	return res;
      count -= words;
      address += sizeof(u32) * words;
      pointer += words;
    }
    i += 1;
  }
  if (*pointer == 0xdeadbeef) {
    return loader_start (lb, dev, ucode_start);
  } else {
    // cast needed as there is no %? for pointer differnces
    PRINTD (DBG_LOAD|DBG_ERR,
	    "offset=%li, *pointer=%x, address=%x, total=%u",
	    (long) (pointer - ucode_data), *pointer, address, total);
    PRINTK (KERN_ERR, "incorrect microcode data");
    return -ENOMEM;
  }
}

/********** give adapter parameters **********/

static int __init amb_talk (amb_dev * dev) {
  adap_talk_block a;
  unsigned char pool;
  unsigned long timeout;
  
  u32 x (void * addr) {
    return cpu_to_be32 (virt_to_bus (addr));
  }
  
  PRINTD (DBG_FLOW, "amb_talk %p", dev);
  
  a.command_start = x (dev->cq.ptrs.start);
  a.command_end   = x (dev->cq.ptrs.limit);
  a.tx_start      = x (dev->txq.in.start);
  a.tx_end        = x (dev->txq.in.limit);
  a.txcom_start   = x (dev->txq.out.start);
  a.txcom_end     = x (dev->txq.out.limit);
  
  for (pool = 0; pool < NUM_RX_POOLS; ++pool) {
    // the other "a" items are set up by the adapter
    a.rec_struct[pool].buffer_start = x (dev->rxq[pool].in.start);
    a.rec_struct[pool].buffer_end   = x (dev->rxq[pool].in.limit);
    a.rec_struct[pool].rx_start     = x (dev->rxq[pool].out.start);
    a.rec_struct[pool].rx_end       = x (dev->rxq[pool].out.limit);
    a.rec_struct[pool].buffer_size = cpu_to_be32 (dev->rxq[pool].buffer_size);
  }
  
#ifdef AMB_NEW_MICROCODE
  // disable fast PLX prefetching
  a.init_flags = 0;
#endif
  
  // pass the structure
  wr_mem (dev, offsetof(amb_mem, doorbell), virt_to_bus (&a));
  
  // 2.2 second wait (must not touch doorbell during 2 second DMA test)
  timeout = HZ*22/10;
  while (timeout)
    timeout = schedule_timeout (timeout);
  // give the adapter another half second?
  timeout = HZ/2;
  while (rd_plain (dev, offsetof(amb_mem, doorbell)))
    if (timeout) {
      timeout = schedule_timeout (timeout);
    } else {
      PRINTD (DBG_INIT|DBG_ERR, "adapter init timed out");
      return -ETIMEDOUT;
    }
  
  return 0;
}

// get microcode version
static void __init amb_ucode_version (amb_dev * dev) {
  u32 major;
  u32 minor;
  command cmd;
  cmd.request = cpu_to_be32 (SRB_GET_VERSION);
  while (command_do (dev, &cmd)) {
    set_current_state(TASK_UNINTERRUPTIBLE);
    schedule();
  }
  major = be32_to_cpu (cmd.args.version.major);
  minor = be32_to_cpu (cmd.args.version.minor);
  PRINTK (KERN_INFO, "microcode version is %u.%u", major, minor);
}

// get end station address
static void __init amb_esi (amb_dev * dev, u8 * esi) {
  u32 lower4;
  u16 upper2;
  command cmd;
  
  // swap bits within byte to get Ethernet ordering
  u8 bit_swap (u8 byte) {
    const u8 swap[] = {
      0x0, 0x8, 0x4, 0xc,
      0x2, 0xa, 0x6, 0xe,
      0x1, 0x9, 0x5, 0xd,
      0x3, 0xb, 0x7, 0xf
    };
    return ((swap[byte & 0xf]<<4) | swap[byte>>4]);
  }
  
  cmd.request = cpu_to_be32 (SRB_GET_BIA);
  while (command_do (dev, &cmd)) {
    set_current_state(TASK_UNINTERRUPTIBLE);
    schedule();
  }
  lower4 = be32_to_cpu (cmd.args.bia.lower4);
  upper2 = be32_to_cpu (cmd.args.bia.upper2);
  PRINTD (DBG_LOAD, "BIA: lower4: %08x, upper2 %04x", lower4, upper2);
  
  if (esi) {
    unsigned int i;
    
    PRINTDB (DBG_INIT, "ESI:");
    for (i = 0; i < ESI_LEN; ++i) {
      if (i < 4)
	  esi[i] = bit_swap (lower4>>(8*i));
      else
	  esi[i] = bit_swap (upper2>>(8*(i-4)));
      PRINTDM (DBG_INIT, " %02x", esi[i]);
    }
    
    PRINTDE (DBG_INIT, "");
  }
  
  return;
}

static int __init amb_init (amb_dev * dev) {
  loader_block lb;
  
  void fixup_plx_window (void) {
    // fix up the PLX-mapped window base address to match the block
    unsigned long blb;
    u32 mapreg;
    blb = virt_to_bus (&lb);
    // the kernel stack had better not ever cross a 1Gb boundary!
    mapreg = rd_plain (dev, offsetof(amb_mem, stuff[10]));
    mapreg &= ~onegigmask;
    mapreg |= blb & onegigmask;
    wr_plain (dev, offsetof(amb_mem, stuff[10]), mapreg);
    return;
  }
  
  u32 version;
  
  if (amb_reset (dev, 1)) {
    PRINTK (KERN_ERR, "card reset failed!");
  } else {
    fixup_plx_window ();
    
    if (get_loader_version (&lb, dev, &version)) {
      PRINTK (KERN_INFO, "failed to get loader version");
    } else {
      PRINTK (KERN_INFO, "loader version is %08x", version);
      
      if (ucode_init (&lb, dev)) {
	PRINTK (KERN_ERR, "microcode failure");
      } else if (create_queues (dev, cmds, txs, rxs, rxs_bs)) {
	PRINTK (KERN_ERR, "failed to get memory for queues");
      } else {
	
	if (amb_talk (dev)) {
	  PRINTK (KERN_ERR, "adapter did not accept queues");
	} else {
	  
	  amb_ucode_version (dev);
	  return 0;
	  
	} /* amb_talk */
	
	destroy_queues (dev);
      } /* create_queues, ucode_init */
      
      amb_reset (dev, 0);
    } /* get_loader_version */
    
  } /* amb_reset */
  
  return -1;
}

static int __init amb_probe (void) {
  struct pci_dev * pci_dev;
  int devs;
  
  void __init do_pci_device (void) {
    amb_dev * dev;
    
    // read resources from PCI configuration space
    u8 irq = pci_dev->irq;
    u32 * membase = bus_to_virt (pci_resource_start (pci_dev, 0));
    u32 iobase = pci_resource_start (pci_dev, 1);
    
    void setup_dev (void) {
      unsigned char pool;
      memset (dev, 0, sizeof(amb_dev));
      
      // set up known dev items straight away
      dev->pci_dev = pci_dev; 
      
      dev->iobase = iobase;
      dev->irq = irq; 
      dev->membase = membase;
      
      // flags (currently only dead)
      dev->flags = 0;
      
      // Allocate cell rates (fibre)
      // ATM_OC3_PCR = 1555200000/8/270*260/53 - 29/53
      // to be really pedantic, this should be ATM_OC3c_PCR
      dev->tx_avail = ATM_OC3_PCR;
      dev->rx_avail = ATM_OC3_PCR;
      
#ifdef FILL_RX_POOLS_IN_BH
      // initialise bottom half
      INIT_LIST_HEAD(&dev->bh.list);
      dev->bh.sync = 0;
      dev->bh.routine = (void (*)(void *)) fill_rx_pools;
      dev->bh.data = dev;
#endif
      
      // semaphore for txer/rxer modifications - we cannot use a
      // spinlock as the critical region needs to switch processes
      init_MUTEX (&dev->vcc_sf);
      // queue manipulation spinlocks; we want atomic reads and
      // writes to the queue descriptors (handles IRQ and SMP)
      // consider replacing "int pending" -> "atomic_t available"
      // => problem related to who gets to move queue pointers
      spin_lock_init (&dev->cq.lock);
      spin_lock_init (&dev->txq.lock);
      for (pool = 0; pool < NUM_RX_POOLS; ++pool)
	spin_lock_init (&dev->rxq[pool].lock);
    }
    
    void setup_pci_dev (void) {
      unsigned char lat;
      
      /* XXX check return value */
      pci_enable_device (pci_dev);

      // enable bus master accesses
      pci_set_master (pci_dev);
      
      // frobnicate latency (upwards, usually)
      pci_read_config_byte (pci_dev, PCI_LATENCY_TIMER, &lat);
      if (pci_lat) {
	PRINTD (DBG_INIT, "%s PCI latency timer from %hu to %hu",
		"changing", lat, pci_lat);
	pci_write_config_byte (pci_dev, PCI_LATENCY_TIMER, pci_lat);
      } else if (lat < MIN_PCI_LATENCY) {
	PRINTK (KERN_INFO, "%s PCI latency timer from %hu to %hu",
		"increasing", lat, MIN_PCI_LATENCY);
	pci_write_config_byte (pci_dev, PCI_LATENCY_TIMER, MIN_PCI_LATENCY);
      }
    }
    
    PRINTD (DBG_INFO, "found Madge ATM adapter (amb) at"
	    " IO %x, IRQ %u, MEM %p", iobase, irq, membase);
    
    // check IO region
    if (check_region (iobase, AMB_EXTENT)) {
      PRINTK (KERN_ERR, "IO range already in use!");
      return;
    }
    
    dev = kmalloc (sizeof(amb_dev), GFP_KERNEL);
    if (!dev) {
      // perhaps we should be nice: deregister all adapters and abort?
      PRINTK (KERN_ERR, "out of memory!");
      return;
    }
    
    setup_dev();
    
    if (amb_init (dev)) {
      PRINTK (KERN_ERR, "adapter initialisation failure");
    } else {
      
      setup_pci_dev();
      
      // grab (but share) IRQ and install handler
      if (request_irq (irq, interrupt_handler, SA_SHIRQ, DEV_LABEL, dev)) {
	PRINTK (KERN_ERR, "request IRQ failed!");
	// free_irq is at "endif"
      } else {
	
	// reserve IO region
	request_region (iobase, AMB_EXTENT, DEV_LABEL);
	
	dev->atm_dev = atm_dev_register (DEV_LABEL, &amb_ops, -1, NULL);
	if (!dev->atm_dev) {
	  PRINTD (DBG_ERR, "failed to register Madge ATM adapter");
	} else {
	  
	  PRINTD (DBG_INFO, "registered Madge ATM adapter (no. %d) (%p) at %p",
		  dev->atm_dev->number, dev, dev->atm_dev);
	  dev->atm_dev->dev_data = (void *) dev;
	  
	  // register our address
	  amb_esi (dev, dev->atm_dev->esi);
	  
	  // 0 bits for vpi, 10 bits for vci
	  dev->atm_dev->ci_range.vpi_bits = NUM_VPI_BITS;
	  dev->atm_dev->ci_range.vci_bits = NUM_VCI_BITS;
	  
	  // update count and linked list
	  ++devs;
	  dev->prev = amb_devs;
	  amb_devs = dev;
	  
	  // enable host interrupts
	  interrupts_on (dev);
	  
	  // success
	  return;
	  
	  // not currently reached
	  atm_dev_deregister (dev->atm_dev);
	} /* atm_dev_register */
	
	release_region (iobase, AMB_EXTENT);
	free_irq (irq, dev);
      } /* request_region, request_irq */
      
      amb_reset (dev, 0);
    } /* amb_init */
    
    kfree (dev);
  } /* kmalloc, end-of-fn */
  
  PRINTD (DBG_FLOW, "amb_probe");
  
  if (!pci_present())
    return 0;
  
  devs = 0;
  pci_dev = NULL;
  while ((pci_dev = pci_find_device
          (PCI_VENDOR_ID_MADGE, PCI_DEVICE_ID_MADGE_AMBASSADOR, pci_dev)
          ))
    do_pci_device();
  
  pci_dev = NULL;
  while ((pci_dev = pci_find_device
          (PCI_VENDOR_ID_MADGE, PCI_DEVICE_ID_MADGE_AMBASSADOR_BAD, pci_dev)
          ))
    PRINTK (KERN_ERR, "skipped broken (PLX rev 2) card");
  
  return devs;
}

static void __init amb_check_args (void) {
  unsigned char pool;
  unsigned int max_rx_size;
  
#ifdef DEBUG_AMBASSADOR
  PRINTK (KERN_NOTICE, "debug bitmap is %hx", debug &= DBG_MASK);
#else
  if (debug)
    PRINTK (KERN_NOTICE, "no debugging support");
#endif
  
  if (cmds < MIN_QUEUE_SIZE)
    PRINTK (KERN_NOTICE, "cmds has been raised to %u",
	    cmds = MIN_QUEUE_SIZE);
  
  if (txs < MIN_QUEUE_SIZE)
    PRINTK (KERN_NOTICE, "txs has been raised to %u",
	    txs = MIN_QUEUE_SIZE);
  
  for (pool = 0; pool < NUM_RX_POOLS; ++pool)
    if (rxs[pool] < MIN_QUEUE_SIZE)
      PRINTK (KERN_NOTICE, "rxs[%hu] has been raised to %u",
	      pool, rxs[pool] = MIN_QUEUE_SIZE);
  
  // buffers sizes should be greater than zero and strictly increasing
  max_rx_size = 0;
  for (pool = 0; pool < NUM_RX_POOLS; ++pool)
    if (rxs_bs[pool] <= max_rx_size)
      PRINTK (KERN_NOTICE, "useless pool (rxs_bs[%hu] = %u)",
	      pool, rxs_bs[pool]);
    else
      max_rx_size = rxs_bs[pool];
  
  if (rx_lats < MIN_RX_BUFFERS)
    PRINTK (KERN_NOTICE, "rx_lats has been raised to %u",
	    rx_lats = MIN_RX_BUFFERS);
  
  return;
}

/********** module stuff **********/

#ifdef MODULE
EXPORT_NO_SYMBOLS;

MODULE_AUTHOR(maintainer_string);
MODULE_DESCRIPTION(description_string);
MODULE_LICENSE("GPL");
MODULE_PARM(debug,   "h");
MODULE_PARM(cmds,    "i");
MODULE_PARM(txs,     "i");
MODULE_PARM(rxs,     __MODULE_STRING(NUM_RX_POOLS) "i");
MODULE_PARM(rxs_bs,  __MODULE_STRING(NUM_RX_POOLS) "i");
MODULE_PARM(rx_lats, "i");
MODULE_PARM(pci_lat, "b");
MODULE_PARM_DESC(debug,   "debug bitmap, see .h file");
MODULE_PARM_DESC(cmds,    "number of command queue entries");
MODULE_PARM_DESC(txs,     "number of TX queue entries");
MODULE_PARM_DESC(rxs,     "number of RX queue entries [" __MODULE_STRING(NUM_RX_POOLS) "]");
MODULE_PARM_DESC(rxs_bs,  "size of RX buffers [" __MODULE_STRING(NUM_RX_POOLS) "]");
MODULE_PARM_DESC(rx_lats, "number of extra buffers to cope with RX latencies");
MODULE_PARM_DESC(pci_lat, "PCI latency in bus cycles");

/********** module entry **********/

int init_module (void) {
  int devs;
  
  PRINTD (DBG_FLOW|DBG_INIT, "init_module");
  
  // sanity check - cast needed as printk does not support %Zu
  if (sizeof(amb_mem) != 4*16 + 4*12) {
    PRINTK (KERN_ERR, "Fix amb_mem (is %lu words).",
	    (unsigned long) sizeof(amb_mem));
    return -ENOMEM;
  }
  
  show_version();
  
  amb_check_args();
  
  // get the juice
  devs = amb_probe();
  
  if (devs) {
    init_timer (&housekeeping);
    housekeeping.function = do_housekeeping;
    // paranoia
    housekeeping.data = 1;
    set_timer (&housekeeping, 0);
  } else {
    PRINTK (KERN_INFO, "no (usable) adapters found");
  }
  
  return devs ? 0 : -ENODEV;
}

/********** module exit **********/

void cleanup_module (void) {
  amb_dev * dev;
  
  PRINTD (DBG_FLOW|DBG_INIT, "cleanup_module");
  
  // paranoia
  housekeeping.data = 0;
  del_timer (&housekeeping);
  
  while (amb_devs) {
    dev = amb_devs;
    amb_devs = dev->prev;
    
    PRINTD (DBG_INFO|DBG_INIT, "closing %p (atm_dev = %p)", dev, dev->atm_dev);
    // the drain should not be necessary
    drain_rx_pools (dev);
    interrupts_off (dev);
    amb_reset (dev, 0);
    destroy_queues (dev);
    atm_dev_deregister (dev->atm_dev);
    free_irq (dev->irq, dev);
    release_region (dev->iobase, AMB_EXTENT);
    kfree (dev);
  }
  
  return;
}

#else

/********** monolithic entry **********/

int __init amb_detect (void) {
  int devs;
  
  // sanity check - cast needed as printk does not support %Zu
  if (sizeof(amb_mem) != 4*16 + 4*12) {
    PRINTK (KERN_ERR, "Fix amb_mem (is %lu words).",
	    (unsigned long) sizeof(amb_mem));
    return 0;
  }
  
  show_version();
  
  amb_check_args();
  
  // get the juice
  devs = amb_probe();
  
  if (devs) {
    init_timer (&housekeeping);
    housekeeping.function = do_housekeeping;
    // paranoia
    housekeeping.data = 1;
    set_timer (&housekeeping, 0);
  } else {
    PRINTK (KERN_INFO, "no (usable) adapters found");
  }
  
  return devs;
}

#endif
