/*
 * dv1394.c - DV input/output over IEEE 1394 on OHCI chips
 *   Copyright (C)2001 Daniel Maas <dmaas@dcine.com>
 *     receive, proc_fs by Dan Dennedy <dan@dennedy.org>
 *
 * based on:
 *  video1394.c - video driver for OHCI 1394 boards
 *  Copyright (C)1999,2000 Sebastien Rougeaux <sebastien.rougeaux@anu.edu.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
  OVERVIEW

  I designed dv1394 as a "pipe" that you can use to shoot DV onto a
  FireWire bus. In transmission mode, dv1394 does the following:

   1. accepts contiguous frames of DV data from user-space, via write()
      or mmap() (see dv1394.h for the complete API)
   2. wraps IEC 61883 packets around the DV data, inserting
      empty synchronization packets as necessary
   3. assigns accurate SYT timestamps to the outgoing packets
   4. shoots them out using the OHCI card's IT DMA engine

   Thanks to Dan Dennedy, we now have a receive mode that does the following:

   1. accepts raw IEC 61883 packets from the OHCI card
   2. re-assembles the DV data payloads into contiguous frames,
      discarding empty packets
   3. sends the DV data to user-space via read() or mmap()
*/

/*
  TODO:

  - tunable frame-drop behavior: either loop last frame, or halt transmission
  
  - use a scatter/gather buffer for DMA programs (f->descriptor_pool)
    so that we don't rely on allocating 64KB of contiguous kernel memory
    via pci_alloc_consistent()
    
  DONE:
  - during reception, better handling of dropped frames and continuity errors
  - during reception, prevent DMA from bypassing the irq tasklets
  - reduce irq rate during reception (1/250 packets).
  - add many more internal buffers during reception with scatter/gather dma.
  - add dbc (continuity) checking on receive, increment status.dropped_frames
    if not continuous.
  - restart IT DMA after a bus reset
  - safely obtain and release ISO Tx channels in cooperation with OHCI driver
  - map received DIF blocks to their proper location in DV frame (ensure
    recovery if dropped packet)
  - handle bus resets gracefully (OHCI card seems to take care of this itself(!))
  - do not allow resizing the user_buf once allocated; eliminate nuke_buffer_mappings
  - eliminated #ifdef DV1394_DEBUG_LEVEL by inventing macros debug_printk and irq_printk
  - added wmb() and mb() to places where PCI read/write ordering needs to be enforced
  - set video->id correctly
  - store video_cards in an array indexed by OHCI card ID, rather than a list
  - implement DMA context allocation to cooperate with other users of the OHCI
  - fix all XXX showstoppers
  - disable IR/IT DMA interrupts on shutdown
  - flush pci writes to the card by issuing a read
  - devfs and character device dispatching (* needs testing with Linux 2.2.x)
  - switch over to the new kernel DMA API (pci_map_*()) (* needs testing on platforms with IOMMU!)
  - keep all video_cards in a list (for open() via chardev), set file->private_data = video
  - dv1394_poll should indicate POLLIN when receiving buffers are available
  - add proc fs interface to set cip_n, cip_d, syt_offset, and video signal
  - expose xmit and recv as separate devices (not exclusive)
  - expose NTSC and PAL as separate devices (can be overridden)
  - read/edit channel in procfs

*/
     
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/smp_lock.h>
#include <linux/bitops.h>
#include <asm/byteorder.h>
#include <asm/atomic.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/wrapper.h>
#include <linux/vmalloc.h>
#include <linux/string.h>

#include "ieee1394.h"
#include "ieee1394_types.h"
#include "nodemgr.h"
#include "hosts.h"
#include "ieee1394_core.h"
#include "highlevel.h"	
#include "dv1394.h"
#include "dv1394-private.h"

#include "ohci1394.h"

#ifndef virt_to_page
#define virt_to_page(x) MAP_NR(x)
#endif

#ifndef vmalloc_32
#define vmalloc_32(x) vmalloc(x)
#endif


/* DEBUG LEVELS:
   0 - no debugging messages
   1 - some debugging messages, but none during DMA frame transmission
   2 - lots of messages, including during DMA frame transmission
       (will cause undeflows if your machine is too slow!)
*/

#define DV1394_DEBUG_LEVEL 0

/* for debugging use ONLY: allow more than one open() of the device */
/* #define DV1394_ALLOW_MORE_THAN_ONE_OPEN 1 */

#if DV1394_DEBUG_LEVEL >= 2
#define irq_printk( args... ) printk( args )
#else
#define irq_printk( args... )
#endif

#if DV1394_DEBUG_LEVEL >= 1
#define debug_printk( args... ) printk( args)
#else
#define debug_printk( args... )
#endif

/* issue a dummy PCI read to force the preceding write
   to be posted to the PCI bus immediately */

static inline void flush_pci_write(struct ti_ohci *ohci)
{
	mb();
	reg_read(ohci, OHCI1394_IsochronousCycleTimer);
}

static void it_tasklet_func(unsigned long data);
static void ir_tasklet_func(unsigned long data);

/* GLOBAL DATA */

/* list of all video_cards */
static LIST_HEAD(dv1394_cards);
static spinlock_t dv1394_cards_lock = SPIN_LOCK_UNLOCKED;

static LIST_HEAD(dv1394_devfs);
struct dv1394_devfs_entry {
	struct list_head list;
    devfs_handle_t devfs;
	char name[32];
	struct dv1394_devfs_entry *parent;
};
static spinlock_t dv1394_devfs_lock = SPIN_LOCK_UNLOCKED;

/* translate from a struct file* to the corresponding struct video_card* */

static inline struct video_card* file_to_video_card(struct file *file)
{
	return (struct video_card*) file->private_data;
}

/*** FRAME METHODS *********************************************************/

static void frame_reset(struct frame *f)
{
	f->state = FRAME_CLEAR;
	f->done = 0;
	f->n_packets = 0;
	f->frame_begin_timestamp = NULL;
	f->assigned_timestamp = 0;
	f->cip_syt1 = NULL;
	f->cip_syt2 = NULL;
	f->mid_frame_timestamp = NULL;
	f->frame_end_timestamp = NULL;
	f->frame_end_branch = NULL;
}

static struct frame* frame_new(unsigned int frame_num, struct video_card *video)
{
	struct frame *f = kmalloc(sizeof(*f), GFP_KERNEL);
	if (!f)
		return NULL;

	f->video = video;
	f->frame_num = frame_num;

	f->header_pool = pci_alloc_consistent(f->video->ohci->dev, PAGE_SIZE, &f->header_pool_dma);
	if (!f->header_pool) {
		printk(KERN_ERR "dv1394: failed to allocate CIP header pool\n");
		kfree(f);
		return NULL;
	}

	debug_printk("dv1394: frame_new: allocated CIP header pool at virt 0x%08lx (contig) dma 0x%08lx size %ld\n",
		     (unsigned long) f->header_pool, (unsigned long) f->header_pool_dma, PAGE_SIZE);
	
	f->descriptor_pool_size = MAX_PACKETS * sizeof(struct DMA_descriptor_block);
	/* make it an even # of pages */
	f->descriptor_pool_size += PAGE_SIZE - (f->descriptor_pool_size%PAGE_SIZE);

	f->descriptor_pool = pci_alloc_consistent(f->video->ohci->dev,
						  f->descriptor_pool_size,
						  &f->descriptor_pool_dma);
	if (!f->descriptor_pool) {
		pci_free_consistent(f->video->ohci->dev, PAGE_SIZE, f->header_pool, f->header_pool_dma);
		kfree(f);
		return NULL;
	}
	
	debug_printk("dv1394: frame_new: allocated DMA program memory at virt 0x%08lx (contig) dma 0x%08lx size %ld\n",
		     (unsigned long) f->descriptor_pool, (unsigned long) f->descriptor_pool_dma, f->descriptor_pool_size);
	
	f->data = 0;
	frame_reset(f);

	return f;
}

static void frame_delete(struct frame *f)
{
	pci_free_consistent(f->video->ohci->dev, PAGE_SIZE, f->header_pool, f->header_pool_dma);
	pci_free_consistent(f->video->ohci->dev, f->descriptor_pool_size, f->descriptor_pool, f->descriptor_pool_dma);
	kfree(f);
}




/* 
   frame_prepare() - build the DMA program for transmitting
   
   Frame_prepare() must be called OUTSIDE the video->spinlock.
   However, frame_prepare() must still be serialized, so
   it should be called WITH the video->sem taken.
 */

static void frame_prepare(struct video_card *video, unsigned int this_frame)
{
	struct frame *f = video->frames[this_frame];
	int last_frame;

	struct DMA_descriptor_block *block;
	dma_addr_t block_dma;
	struct CIP_header *cip;
	dma_addr_t cip_dma;
	
	unsigned int n_descriptors, full_packets, packets_per_frame, payload_size;

	/* these flags denote packets that need special attention */
	int empty_packet, first_packet, last_packet, mid_packet;

	u32 *branch_address, *last_branch_address = NULL;
	unsigned long data_p;
	int first_packet_empty = 0;
	u32 cycleTimer, ct_sec, ct_cyc, ct_off;
	unsigned long irq_flags;

	irq_printk("frame_prepare( %d ) ---------------------\n", this_frame);
	
	full_packets = 0;



	if (video->pal_or_ntsc == DV1394_PAL)
		packets_per_frame = DV1394_PAL_PACKETS_PER_FRAME;
	else
		packets_per_frame = DV1394_NTSC_PACKETS_PER_FRAME;

	while ( full_packets < packets_per_frame ) {
		empty_packet = first_packet = last_packet = mid_packet = 0;

		data_p = f->data + full_packets * 480;

		/************************************************/
		/* allocate a descriptor block and a CIP header */
		/************************************************/

		/* note: these should NOT cross a page boundary (DMA restriction) */

		if (f->n_packets >= MAX_PACKETS) {
			printk(KERN_ERR "dv1394: FATAL ERROR: max packet count exceeded\n");
			return;
		}

		/* the block surely won't cross a page boundary, 
		   since an even number of descriptor_blocks fit on a page */
		block = &(f->descriptor_pool[f->n_packets]);

		/* DMA address of the block = offset of block relative
		    to the kernel base address of the descriptor pool
		    + DMA base address of the descriptor pool */
		block_dma = ((unsigned long) block - (unsigned long) f->descriptor_pool) + f->descriptor_pool_dma;
		

		/* the whole CIP pool fits on one page, so no worries about boundaries */
		if ( ((unsigned long) &(f->header_pool[f->n_packets]) - (unsigned long) f->header_pool) 
		    > PAGE_SIZE) {
			printk(KERN_ERR "dv1394: FATAL ERROR: no room to allocate CIP header\n");
			return;
		}

		cip = &(f->header_pool[f->n_packets]);
		
		/* DMA address of the CIP header = offset of cip
		   relative to kernel base address of the header pool
		   + DMA base address of the header pool */
		cip_dma = (unsigned long) cip % PAGE_SIZE + f->header_pool_dma;
		
		/* is this an empty packet? */

		if (video->cip_accum > (video->cip_d - video->cip_n)) {
			empty_packet = 1;
			payload_size = 8;
			video->cip_accum -= (video->cip_d - video->cip_n);
		} else {
			payload_size = 488;
			video->cip_accum += video->cip_n;
		}

		/* there are three important packets each frame:

		   the first packet in the frame - we ask the card to record the timestamp when
		                                   this packet is actually sent, so we can monitor
						   how accurate our timestamps are. Also, the first
						   packet serves as a semaphore to let us know that
						   it's OK to free the *previous* frame's DMA buffer

		   the last packet in the frame -  this packet is used to detect buffer underflows.
		                                   if this is the last ready frame, the last DMA block
						   will have a branch back to the beginning of the frame
						   (so that the card will re-send the frame on underflow).
						   if this branch gets taken, we know that at least one
						   frame has been dropped. When the next frame is ready,
						   the branch is pointed to its first packet, and the
						   semaphore is disabled.

		   a "mid" packet slightly before the end of the frame - this packet should trigger
		                   an interrupt so we can go and assign a timestamp to the first packet
				   in the next frame. We don't use the very last packet in the frame
				   for this purpose, because that would leave very little time to set
				   the timestamp before DMA starts on the next frame.
		*/
		
		if (f->n_packets == 0) {
			first_packet = 1;
		} else if ( full_packets == (packets_per_frame-1) ) {
			last_packet = 1;
		} else if (f->n_packets == packets_per_frame) {
			mid_packet = 1;
		}
		

		/********************/
		/* setup CIP header */
		/********************/

		/* the timestamp will be written later from the
		   mid-frame interrupt handler. For now we just
		   store the address of the CIP header(s) that
		   need a timestamp. */

		/* first packet in the frame needs a timestamp */
		if (first_packet) {
			f->cip_syt1 = cip;
			if (empty_packet)
				first_packet_empty = 1;

		} else if (first_packet_empty && (f->n_packets == 1) ) {
			/* if the first packet was empty, the second
			   packet's CIP header also needs a timestamp */
			f->cip_syt2 = cip;
		}

		fill_cip_header(cip,
				/* the node ID number of the OHCI card */
				reg_read(video->ohci, OHCI1394_NodeID) & 0x3F,
				video->continuity_counter, 
				video->pal_or_ntsc,
				0xFFFF /* the timestamp is filled in later */);
		
		/* advance counter, only for full packets */
		if ( ! empty_packet )
			video->continuity_counter++;

		/******************************/
		/* setup DMA descriptor block */
		/******************************/

		/* first descriptor - OUTPUT_MORE_IMMEDIATE, for the controller's IT header */
		fill_output_more_immediate( &(block->u.out.omi), 1, video->channel, 0, payload_size);

		if (empty_packet) {
			/* second descriptor - OUTPUT_LAST for CIP header */
			fill_output_last( &(block->u.out.u.empty.ol),

					  /* want completion status on all interesting packets */
					  (first_packet || mid_packet || last_packet) ? 1 : 0,

					  /* want interrupts on all interesting packets */
					  (first_packet || mid_packet || last_packet) ? 1 : 0,

					  sizeof(struct CIP_header), /* data size */
					  cip_dma);
			
			if (first_packet)
				f->frame_begin_timestamp = &(block->u.out.u.empty.ol.q[3]);
			else if (mid_packet)
				f->mid_frame_timestamp = &(block->u.out.u.empty.ol.q[3]);
			else if (last_packet) {
				f->frame_end_timestamp = &(block->u.out.u.empty.ol.q[3]);
				f->frame_end_branch = &(block->u.out.u.empty.ol.q[2]);
			}

			branch_address = &(block->u.out.u.empty.ol.q[2]);
			n_descriptors = 3;
			if (first_packet)
				f->first_n_descriptors = n_descriptors;

		} else { /* full packet */

			/* second descriptor - OUTPUT_MORE for CIP header */
			fill_output_more( &(block->u.out.u.full.om),
					  sizeof(struct CIP_header), /* data size */
					  cip_dma);

			
			/* third (and possibly fourth) descriptor - for DV data */
			/* the 480-byte payload can cross a page boundary; if so,
			   we need to split it into two DMA descriptors */

			/* does the 480-byte data payload cross a page boundary? */
			if ( (PAGE_SIZE- ((unsigned long)data_p % PAGE_SIZE) ) < 480 ) {

				/* page boundary crossed */

				fill_output_more( &(block->u.out.u.full.u.cross.om),
						  /* data size - how much of data_p fits on the first page */
						  PAGE_SIZE - (data_p % PAGE_SIZE),

						  /* DMA address of data_p */
						  dma_region_offset_to_bus(&video->dv_buf,
									   data_p - (unsigned long) video->dv_buf.kvirt));

				fill_output_last( &(block->u.out.u.full.u.cross.ol),
					  
						  /* want completion status on all interesting packets */
						  (first_packet || mid_packet || last_packet) ? 1 : 0, 

						  /* want interrupt on all interesting packets */
						  (first_packet || mid_packet || last_packet) ? 1 : 0,

						  /* data size - remaining portion of data_p */
						  480 - (PAGE_SIZE - (data_p % PAGE_SIZE)),

						  /* DMA address of data_p + PAGE_SIZE - (data_p % PAGE_SIZE) */
						  dma_region_offset_to_bus(&video->dv_buf,
									   data_p + PAGE_SIZE - (data_p % PAGE_SIZE) - (unsigned long) video->dv_buf.kvirt));

				if (first_packet)
					f->frame_begin_timestamp = &(block->u.out.u.full.u.cross.ol.q[3]);
				else if (mid_packet)
					f->mid_frame_timestamp = &(block->u.out.u.full.u.cross.ol.q[3]);
				else if (last_packet) {
					f->frame_end_timestamp = &(block->u.out.u.full.u.cross.ol.q[3]);
					f->frame_end_branch = &(block->u.out.u.full.u.cross.ol.q[2]);
				}

				branch_address = &(block->u.out.u.full.u.cross.ol.q[2]);

				n_descriptors = 5;
				if (first_packet)
					f->first_n_descriptors = n_descriptors;
				
				full_packets++;

			} else {
				/* fits on one page */

				fill_output_last( &(block->u.out.u.full.u.nocross.ol),
					  
						  /* want completion status on all interesting packets */
						  (first_packet || mid_packet || last_packet) ? 1 : 0,

						  /* want interrupt on all interesting packets */
						  (first_packet || mid_packet || last_packet) ? 1 : 0,

						  480, /* data size (480 bytes of DV data) */

						  
						  /* DMA address of data_p */
						  dma_region_offset_to_bus(&video->dv_buf,
									   data_p - (unsigned long) video->dv_buf.kvirt));
				
				if (first_packet)
					f->frame_begin_timestamp = &(block->u.out.u.full.u.nocross.ol.q[3]);
				else if (mid_packet)
					f->mid_frame_timestamp = &(block->u.out.u.full.u.nocross.ol.q[3]);
				else if (last_packet) {
					f->frame_end_timestamp = &(block->u.out.u.full.u.nocross.ol.q[3]);
					f->frame_end_branch = &(block->u.out.u.full.u.nocross.ol.q[2]);
				}

				branch_address = &(block->u.out.u.full.u.nocross.ol.q[2]);

				n_descriptors = 4;
				if (first_packet)
					f->first_n_descriptors = n_descriptors;

				full_packets++;
			}
		}
		
		/* link this descriptor block into the DMA program by filling in 
		   the branch address of the previous block */

		/* note: we are not linked into the active DMA chain yet */

		if (last_branch_address) {
			*(last_branch_address) = cpu_to_le32(block_dma | n_descriptors);
		}

		last_branch_address = branch_address;


		f->n_packets++;
		
	}

	/* when we first assemble a new frame, set the final branch 
	   to loop back up to the top */
	*(f->frame_end_branch) = cpu_to_le32(f->descriptor_pool_dma | f->first_n_descriptors);

	/* make the latest version of this frame visible to the PCI card */
	dma_region_sync(&video->dv_buf, f->data - (unsigned long) video->dv_buf.kvirt, video->frame_size);

	/* lock against DMA interrupt */
	spin_lock_irqsave(&video->spinlock, irq_flags);

	f->state = FRAME_READY;

	video->n_clear_frames--;

	last_frame = video->first_clear_frame - 1;
	if (last_frame == -1)
		last_frame = video->n_frames-1;

	video->first_clear_frame = (video->first_clear_frame + 1) % video->n_frames;

	irq_printk("   frame %d prepared, active_frame = %d, n_clear_frames = %d, first_clear_frame = %d\n last=%d\n",
		   this_frame, video->active_frame, video->n_clear_frames, video->first_clear_frame, last_frame);

	irq_printk("   begin_ts %08lx mid_ts %08lx end_ts %08lx end_br %08lx\n",
		   (unsigned long) f->frame_begin_timestamp, 
		   (unsigned long) f->mid_frame_timestamp, 
		   (unsigned long) f->frame_end_timestamp, 
		   (unsigned long) f->frame_end_branch);
	
	if (video->active_frame != -1) {

		/* if DMA is already active, we are almost done */
		/* just link us onto the active DMA chain */
		if (video->frames[last_frame]->frame_end_branch) {
			u32 temp;

			/* point the previous frame's tail to this frame's head */
			*(video->frames[last_frame]->frame_end_branch) = cpu_to_le32(f->descriptor_pool_dma | f->first_n_descriptors);

			/* this write MUST precede the next one, or we could silently drop frames */
			wmb();
			
			/* disable the want_status semaphore on the last packet */
			temp = le32_to_cpu(*(video->frames[last_frame]->frame_end_branch - 2));
			temp &= 0xF7CFFFFF;
			*(video->frames[last_frame]->frame_end_branch - 2) = cpu_to_le32(temp);

			/* flush these writes to memory ASAP */
			flush_pci_write(video->ohci);

			/* NOTE:
			   ideally the writes should be "atomic": if
			   the OHCI card reads the want_status flag in
			   between them, we'll falsely report a
			   dropped frame. Hopefully this window is too
			   small to really matter, and the consequence
			   is rather harmless. */
			

			irq_printk("     new frame %d linked onto DMA chain\n", this_frame);

		} else {
			printk(KERN_ERR "dv1394: last frame not ready???\n");
		}

	} else {
		
		u32 transmit_sec, transmit_cyc;
		u32 ts_cyc, ts_off;

		/* DMA is stopped, so this is the very first frame */
		video->active_frame = this_frame;
		
	        /* set CommandPtr to address and size of first descriptor block */
		reg_write(video->ohci, video->ohci_IsoXmitCommandPtr,
			  video->frames[video->active_frame]->descriptor_pool_dma |
			  f->first_n_descriptors);

		/* assign a timestamp based on the current cycle time...
		   We'll tell the card to begin DMA 100 cycles from now,
		   and assign a timestamp 103 cycles from now */

		cycleTimer = reg_read(video->ohci, OHCI1394_IsochronousCycleTimer);

		ct_sec = cycleTimer >> 25;
		ct_cyc = (cycleTimer >> 12) & 0x1FFF;
		ct_off = cycleTimer & 0xFFF;

		transmit_sec = ct_sec;
		transmit_cyc = ct_cyc + 100;

		transmit_sec += transmit_cyc/8000;
		transmit_cyc %= 8000;
		
		ts_off = ct_off;
		ts_cyc = transmit_cyc + 3;
		ts_cyc %= 8000;

		f->assigned_timestamp = (ts_cyc&0xF) << 12;

		/* now actually write the timestamp into the appropriate CIP headers */
		if (f->cip_syt1) {
			f->cip_syt1->b[6] = f->assigned_timestamp >> 8;
			f->cip_syt1->b[7] = f->assigned_timestamp & 0xFF;
		}
		if (f->cip_syt2) {
			f->cip_syt2->b[6] = f->assigned_timestamp >> 8;
			f->cip_syt2->b[7] = f->assigned_timestamp & 0xFF;
		}
		
		/* --- start DMA --- */

		/* clear all bits in ContextControl register */

		reg_write(video->ohci, video->ohci_IsoXmitContextControlClear, 0xFFFFFFFF);
		wmb();

		/* the OHCI card has the ability to start ISO transmission on a
		   particular cycle (start-on-cycle). This way we can ensure that
		   the first DV frame will have an accurate timestamp.
		   
		   However, start-on-cycle only appears to work if the OHCI card 
		   is cycle master! Since the consequences of messing up the first
		   timestamp are minimal*, just disable start-on-cycle for now.

		   * my DV deck drops the first few frames before it "locks in;"
		     so the first frame having an incorrect timestamp is inconsequential.
		*/

#if 0
		reg_write(video->ohci, video->ohci_IsoXmitContextControlSet,
			  (1 << 31) /* enable start-on-cycle */
			  | ( (transmit_sec & 0x3) << 29)
			  | (transmit_cyc << 16));
		wmb();
#endif

		video->dma_running = 1;

		/* set the 'run' bit */
		reg_write(video->ohci, video->ohci_IsoXmitContextControlSet, 0x8000);
		flush_pci_write(video->ohci);
		
		/* --- DMA should be running now --- */

		debug_printk("    Cycle = %4u ContextControl = %08x CmdPtr = %08x\n",
			     (reg_read(video->ohci, OHCI1394_IsochronousCycleTimer) >> 12) & 0x1FFF,
			     reg_read(video->ohci, video->ohci_IsoXmitContextControlSet),
			     reg_read(video->ohci, video->ohci_IsoXmitCommandPtr));

		debug_printk("    DMA start - current cycle %4u, transmit cycle %4u (%2u), assigning ts cycle %2u\n",
			     ct_cyc, transmit_cyc, transmit_cyc & 0xF, ts_cyc & 0xF);

#if DV1394_DEBUG_LEVEL >= 2
		{
			/* check if DMA is really running */
			int i = 0;
			while (i < 20) {
				mb();
				mdelay(1);
				if (reg_read(video->ohci, video->ohci_IsoXmitContextControlSet) & (1 << 10)) {
					printk("DMA ACTIVE after %d msec\n", i);
					break;
				}
				i++;
			}

			printk("set = %08x, cmdPtr = %08x\n", 
			       reg_read(video->ohci, video->ohci_IsoXmitContextControlSet),
			       reg_read(video->ohci, video->ohci_IsoXmitCommandPtr)
			       );
			
			if ( ! (reg_read(video->ohci, video->ohci_IsoXmitContextControlSet) &  (1 << 10)) ) {
				printk("DMA did NOT go active after 20ms, event = %x\n", 
				       reg_read(video->ohci, video->ohci_IsoXmitContextControlSet) & 0x1F);
			} else
				printk("DMA is RUNNING!\n");
		}
#endif
		
	}


	spin_unlock_irqrestore(&video->spinlock, irq_flags);
}



/*** RECEIVE FUNCTIONS *****************************************************/

/* 
	frame method put_packet

	map and copy the packet data to its location in the frame 
	based upon DIF section and sequence 
*/

static void inline
frame_put_packet (struct frame *f, struct packet *p)
{
	int section_type = p->data[0] >> 5;           /* section type is in bits 5 - 7 */
	int dif_sequence = p->data[1] >> 4;           /* dif sequence number is in bits 4 - 7 */
	int dif_block = p->data[2];

	/* sanity check */
	if (dif_sequence > 11 || dif_block > 149) return;
		
	switch (section_type) {
	case 0:           /* 1 Header block */
	        memcpy( (void *) f->data + dif_sequence * 150 * 80, p->data, 480);
	        break;
	
	case 1:           /* 2 Subcode blocks */
	        memcpy( (void *) f->data + dif_sequence * 150 * 80 + (1 + dif_block) * 80, p->data, 480);
	        break;
	
	case 2:           /* 3 VAUX blocks */
	        memcpy( (void *) f->data + dif_sequence * 150 * 80 + (3 + dif_block) * 80, p->data, 480);
	        break;
	
	case 3:           /* 9 Audio blocks interleaved with video */
	        memcpy( (void *) f->data + dif_sequence * 150 * 80 + (6 + dif_block * 16) * 80, p->data, 480);
	        break;
	
	case 4:           /* 135 Video blocks interleaved with audio */
	        memcpy( (void *) f->data + dif_sequence * 150 * 80 + (7 + (dif_block / 15) + dif_block) * 80, p->data, 480);
	        break;
	
	default:           /* we can not handle any other data */
	        break;
	}
}


static void start_dma_receive(struct video_card *video)
{
	if (video->first_run == 1) {
		video->first_run = 0;
		
		/* start DMA once all of the frames are READY */
		video->n_clear_frames = 0;
		video->first_clear_frame = -1;
		video->current_packet = 0;
		video->active_frame = 0;
		
		/* reset iso recv control register */
		reg_write(video->ohci, video->ohci_IsoRcvContextControlClear, 0xFFFFFFFF);
		wmb();
		
		/* clear bufferFill, set isochHeader and speed (0=100) */
		reg_write(video->ohci, video->ohci_IsoRcvContextControlSet, 0x40000000);
	
		/* match on all tags, listen on channel */
		reg_write(video->ohci, video->ohci_IsoRcvContextMatch, 0xf0000000 | video->channel);
		
		/* address and first descriptor block + Z=1 */
		reg_write(video->ohci, video->ohci_IsoRcvCommandPtr,             
			  video->frames[0]->descriptor_pool_dma | 1); /* Z=1 */
		wmb();

		video->dma_running = 1;

		/* run */
		reg_write(video->ohci, video->ohci_IsoRcvContextControlSet, 0x8000);
		flush_pci_write(video->ohci);
		
		debug_printk("dv1394: DMA started\n");
	
#if DV1394_DEBUG_LEVEL >= 2
		{
			int i;
		
			for (i = 0; i < 1000; ++i) {
				mdelay(1);
				if (reg_read(video->ohci, video->ohci_IsoRcvContextControlSet) & (1 << 10)) {
					printk("DMA ACTIVE after %d msec\n", i);
					break;
				}
			}
			if ( reg_read(video->ohci, video->ohci_IsoRcvContextControlSet) &  (1 << 11) ) {
				printk("DEAD, event = %x\n", 
					   reg_read(video->ohci, video->ohci_IsoRcvContextControlSet) & 0x1F);
			} else
				printk("RUNNING!\n");
		}
#endif
	} 
	else if ( reg_read(video->ohci, video->ohci_IsoRcvContextControlSet) &  (1 << 11) ) {
		debug_printk("DEAD, event = %x\n", 
			     reg_read(video->ohci, video->ohci_IsoRcvContextControlSet) & 0x1F);

		/* wake */
		reg_write(video->ohci, video->ohci_IsoRcvContextControlSet, (1 << 12));
	}
}


/* 
   receive_packets() - build the DMA program for receiving
*/

static void receive_packets(struct video_card *video)
{
	struct DMA_descriptor_block *block = NULL;
	dma_addr_t block_dma = 0;
	struct packet *data = NULL;
	dma_addr_t data_dma = 0;
	u32 *last_branch_address = NULL;
	unsigned long irq_flags;
	int want_interrupt = 0;
	struct frame *f = NULL;
	int i, j;

	spin_lock_irqsave(&video->spinlock, irq_flags);

	for (j = 0; j < video->n_frames; j++) {

		/* connect frames */
		if (j > 0 && f != NULL && f->frame_end_branch != NULL)
			*(f->frame_end_branch) = cpu_to_le32(video->frames[j]->descriptor_pool_dma | 1); /* set Z=1 */

		f = video->frames[j];

		for (i = 0; i < MAX_PACKETS; i++) {
			/* locate a descriptor block and packet from the buffer */
			block = &(f->descriptor_pool[i]);
			block_dma = ((unsigned long) block - (unsigned long) f->descriptor_pool) + f->descriptor_pool_dma;
			
			data = ((struct packet*)video->packet_buf.kvirt) + f->frame_num * MAX_PACKETS + i;
			data_dma = dma_region_offset_to_bus( &video->packet_buf, 
							     ((unsigned long) data - (unsigned long) video->packet_buf.kvirt) );
			
			/* setup DMA descriptor block */
			want_interrupt = ((i % (MAX_PACKETS/2)) == 0 || i == (MAX_PACKETS-1));
			fill_input_last( &(block->u.in.il), want_interrupt, 512, data_dma);
	
			/* link descriptors */
			last_branch_address = f->frame_end_branch;
	
			if (last_branch_address != NULL)
				*(last_branch_address) = cpu_to_le32(block_dma | 1); /* set Z=1 */
	
			f->frame_end_branch = &(block->u.in.il.q[2]);
		}
	
	} /* next j */

	spin_unlock_irqrestore(&video->spinlock, irq_flags);

}



/*** MANAGEMENT FUNCTIONS **************************************************/

static int do_dv1394_init(struct video_card *video, struct dv1394_init *init)
{
	unsigned long flags, new_buf_size;
	int i;
	u64 chan_mask;
	int retval = -EINVAL;

	debug_printk("dv1394: initialising %d\n", video->id);
	if (init->api_version != DV1394_API_VERSION)
		return -EINVAL;
	
	/* first sanitize all the parameters */
	if ( (init->n_frames < 2) || (init->n_frames > DV1394_MAX_FRAMES) )
		return -EINVAL;

	if ( (init->format != DV1394_NTSC) && (init->format != DV1394_PAL) )
		return -EINVAL;

	if ( (init->syt_offset == 0) || (init->syt_offset > 50) )
		/* default SYT offset is 3 cycles */
		init->syt_offset = 3;

	if ( (init->channel > 63) || (init->channel < 0) )
		init->channel = 63;

	chan_mask = (u64)1 << init->channel;

	/* calculate what size DMA buffer is needed */
	if (init->format == DV1394_NTSC)
		new_buf_size = DV1394_NTSC_FRAME_SIZE * init->n_frames;
	else
		new_buf_size = DV1394_PAL_FRAME_SIZE * init->n_frames;

	/* round up to PAGE_SIZE */
	if (new_buf_size % PAGE_SIZE) new_buf_size += PAGE_SIZE - (new_buf_size % PAGE_SIZE);

	/* don't allow the user to allocate the DMA buffer more than once */
	if (video->dv_buf.kvirt && video->dv_buf_size != new_buf_size) {
		printk("dv1394: re-sizing the DMA buffer is not allowed\n");
		return -EINVAL;
	}

	/* shutdown the card if it's currently active */
	/* (the card should not be reset if the parameters are screwy) */

	do_dv1394_shutdown(video, 0);
	
	/* try to claim the ISO channel */
	spin_lock_irqsave(&video->ohci->IR_channel_lock, flags);
	if (video->ohci->ISO_channel_usage & chan_mask) {
		spin_unlock_irqrestore(&video->ohci->IR_channel_lock, flags);
		retval = -EBUSY;
		goto err;
	}
	video->ohci->ISO_channel_usage |= chan_mask;
	spin_unlock_irqrestore(&video->ohci->IR_channel_lock, flags);

	video->channel = init->channel;

	/* initialize misc. fields of video */
	video->n_frames = init->n_frames;
	video->pal_or_ntsc = init->format;

	video->cip_accum = 0;
	video->continuity_counter = 0;

	video->active_frame = -1;
	video->first_clear_frame = 0;
	video->n_clear_frames = video->n_frames;
	video->dropped_frames = 0;

	video->write_off = 0;

	video->first_run = 1;
	video->current_packet = -1;
	video->first_frame = 0;

	if (video->pal_or_ntsc == DV1394_NTSC) {
		video->cip_n = init->cip_n != 0 ? init->cip_n : CIP_N_NTSC;
		video->cip_d = init->cip_d != 0 ? init->cip_d : CIP_D_NTSC;
		video->frame_size = DV1394_NTSC_FRAME_SIZE;
	} else {
		video->cip_n = init->cip_n != 0 ? init->cip_n : CIP_N_PAL;
		video->cip_d = init->cip_d != 0 ? init->cip_d : CIP_D_PAL;
		video->frame_size = DV1394_PAL_FRAME_SIZE;
	}

	video->syt_offset = init->syt_offset;
	
	/* find and claim DMA contexts on the OHCI card */

	if (video->ohci_it_ctx == -1) {
		ohci1394_init_iso_tasklet(&video->it_tasklet, OHCI_ISO_TRANSMIT,
					  it_tasklet_func, (unsigned long) video);

		if (ohci1394_register_iso_tasklet(video->ohci, &video->it_tasklet) < 0) {	
			printk(KERN_ERR "dv1394: could not find an available IT DMA context\n");
			retval = -EBUSY;
			goto err;
		}
		
		video->ohci_it_ctx = video->it_tasklet.context;
		debug_printk("dv1394: claimed IT DMA context %d\n", video->ohci_it_ctx);
	}

	if (video->ohci_ir_ctx == -1) {
		ohci1394_init_iso_tasklet(&video->ir_tasklet, OHCI_ISO_RECEIVE,
					  ir_tasklet_func, (unsigned long) video);

		if (ohci1394_register_iso_tasklet(video->ohci, &video->ir_tasklet) < 0) {
			printk(KERN_ERR "dv1394: could not find an available IR DMA context\n");
			retval = -EBUSY;
			goto err;
		}
		video->ohci_ir_ctx = video->ir_tasklet.context;
		debug_printk("dv1394: claimed IR DMA context %d\n", video->ohci_ir_ctx);
	}
	
	/* allocate struct frames */
	for (i = 0; i < init->n_frames; i++) {
		video->frames[i] = frame_new(i, video);

		if (!video->frames[i]) {
			printk(KERN_ERR "dv1394: Cannot allocate frame structs\n");
			retval = -ENOMEM;
			goto err;
		}
	}

	if (!video->dv_buf.kvirt) {
		/* allocate the ringbuffer */
		retval = dma_region_alloc(&video->dv_buf, new_buf_size, video->ohci->dev, PCI_DMA_TODEVICE);
		if (retval)
			goto err;
					  
		video->dv_buf_size = new_buf_size;

		debug_printk("dv1394: Allocated %d frame buffers, total %u pages (%u DMA pages), %lu bytes\n", 
			     video->n_frames, video->dv_buf.n_pages,
			     video->dv_buf.n_dma_pages, video->dv_buf_size);
	}
	
	/* set up the frame->data pointers */
	for (i = 0; i < video->n_frames; i++)
		video->frames[i]->data = (unsigned long) video->dv_buf.kvirt + i * video->frame_size;

	if (!video->packet_buf.kvirt) {
		/* allocate packet buffer */
		video->packet_buf_size = sizeof(struct packet) * video->n_frames * MAX_PACKETS;
		if (video->packet_buf_size % PAGE_SIZE)
			video->packet_buf_size += PAGE_SIZE - (video->packet_buf_size % PAGE_SIZE);
		
		retval = dma_region_alloc(&video->packet_buf, video->packet_buf_size,
					  video->ohci->dev, PCI_DMA_FROMDEVICE);
		if (retval)
			goto err;
		
		debug_printk("dv1394: Allocated %d packets in buffer, total %u pages (%u DMA pages), %lu bytes\n", 
				 video->n_frames*MAX_PACKETS, video->packet_buf.n_pages,
				 video->packet_buf.n_dma_pages, video->packet_buf_size);
	}
	
	/* set up register offsets for IT context */
	/* IT DMA context registers are spaced 16 bytes apart */
	video->ohci_IsoXmitContextControlSet = OHCI1394_IsoXmitContextControlSet+16*video->ohci_it_ctx;
	video->ohci_IsoXmitContextControlClear = OHCI1394_IsoXmitContextControlClear+16*video->ohci_it_ctx;
	video->ohci_IsoXmitCommandPtr = OHCI1394_IsoXmitCommandPtr+16*video->ohci_it_ctx;

	/* enable interrupts for IT context */
	reg_write(video->ohci, OHCI1394_IsoXmitIntMaskSet, (1 << video->ohci_it_ctx));
	debug_printk("dv1394: interrupts enabled for IT context %d\n", video->ohci_it_ctx);

	/* set up register offsets for IR context */
	/* IR DMA context registers are spaced 32 bytes apart */
	video->ohci_IsoRcvContextControlSet = OHCI1394_IsoRcvContextControlSet+32*video->ohci_ir_ctx;
	video->ohci_IsoRcvContextControlClear = OHCI1394_IsoRcvContextControlClear+32*video->ohci_ir_ctx;
	video->ohci_IsoRcvCommandPtr = OHCI1394_IsoRcvCommandPtr+32*video->ohci_ir_ctx;
	video->ohci_IsoRcvContextMatch = OHCI1394_IsoRcvContextMatch+32*video->ohci_ir_ctx;

	/* enable interrupts for IR context */
	reg_write(video->ohci, OHCI1394_IsoRecvIntMaskSet, (1 << video->ohci_ir_ctx) );
	debug_printk("dv1394: interrupts enabled for IR context %d\n", video->ohci_ir_ctx);
	
	return 0;

err:
	do_dv1394_shutdown(video, 1);
	return retval;
}

/* if the user doesn't bother to call ioctl(INIT) before starting
   mmap() or read()/write(), just give him some default values */

static int do_dv1394_init_default(struct video_card *video)
{
	struct dv1394_init init;

	init.api_version = DV1394_API_VERSION;
	init.n_frames = DV1394_MAX_FRAMES / 4;
	/* the following are now set via proc_fs or devfs */
	init.channel = video->channel;
	init.format = video->pal_or_ntsc;
	init.cip_n = video->cip_n; 
	init.cip_d = video->cip_d;
	init.syt_offset = video->syt_offset;

	return do_dv1394_init(video, &init);
}

/* do NOT call from interrupt context */
static void stop_dma(struct video_card *video)
{
	unsigned long flags;
	int i;

	/* no interrupts */
	spin_lock_irqsave(&video->spinlock, flags);

	video->dma_running = 0;

	if ( (video->ohci_it_ctx == -1) && (video->ohci_ir_ctx == -1) )
		goto out;

	/* stop DMA if in progress */
	if ( (video->active_frame != -1) ||
	    (reg_read(video->ohci, video->ohci_IsoXmitContextControlClear) & (1 << 10)) ||
	    (reg_read(video->ohci, video->ohci_IsoRcvContextControlClear) &  (1 << 10)) ) {

		/* clear the .run bits */
		reg_write(video->ohci, video->ohci_IsoXmitContextControlClear, (1 << 15));
		reg_write(video->ohci, video->ohci_IsoRcvContextControlClear, (1 << 15));
		flush_pci_write(video->ohci);
		
		video->active_frame = -1;
		video->first_run = 1;
		
		/* wait until DMA really stops */
		i = 0;
		while (i < 1000) {
				
			/* wait 0.1 millisecond */
			udelay(100); 
			
			if ( (reg_read(video->ohci, video->ohci_IsoXmitContextControlClear) & (1 << 10)) ||
			    (reg_read(video->ohci, video->ohci_IsoRcvContextControlClear)  & (1 << 10)) ) {
				/* still active */
				debug_printk("dv1394: stop_dma: DMA not stopped yet\n" );
				mb();
			} else {
				debug_printk("dv1394: stop_dma: DMA stopped safely after %d ms\n", i/10);
				break;
			}
				
			i++;
		}
			
		if (i == 1000) {
			printk(KERN_ERR "dv1394: stop_dma: DMA still going after %d ms!\n", i/10);
		}
	}
	else
		debug_printk("dv1394: stop_dma: already stopped.\n");

out:
	spin_unlock_irqrestore(&video->spinlock, flags);
}



static void do_dv1394_shutdown(struct video_card *video, int free_dv_buf)
{
	int i;
	
	debug_printk("dv1394: shutdown...\n");

	/* stop DMA if in progress */
	stop_dma(video);
	
	/* release the DMA contexts */
	if (video->ohci_it_ctx != -1) {
		video->ohci_IsoXmitContextControlSet = 0;
		video->ohci_IsoXmitContextControlClear = 0;
		video->ohci_IsoXmitCommandPtr = 0;

		/* disable interrupts for IT context */
		reg_write(video->ohci, OHCI1394_IsoXmitIntMaskClear, (1 << video->ohci_it_ctx));
		
		/* remove tasklet */
		ohci1394_unregister_iso_tasklet(video->ohci, &video->it_tasklet);
		debug_printk("dv1394: IT context %d released\n", video->ohci_it_ctx);
		video->ohci_it_ctx = -1;
	}

	if (video->ohci_ir_ctx != -1) {
		video->ohci_IsoRcvContextControlSet = 0;
		video->ohci_IsoRcvContextControlClear = 0;
		video->ohci_IsoRcvCommandPtr = 0;
		video->ohci_IsoRcvContextMatch = 0;

		/* disable interrupts for IR context */
		reg_write(video->ohci, OHCI1394_IsoRecvIntMaskClear, (1 << video->ohci_ir_ctx));

		/* remove tasklet */
		ohci1394_unregister_iso_tasklet(video->ohci, &video->ir_tasklet);
		debug_printk("dv1394: IR context %d released\n", video->ohci_ir_ctx);
		video->ohci_ir_ctx = -1;
	}

	/* release the ISO channel */
	if (video->channel != -1) {
		u64 chan_mask;
		unsigned long flags;
		
		chan_mask = (u64)1 << video->channel;
		
		spin_lock_irqsave(&video->ohci->IR_channel_lock, flags);
		video->ohci->ISO_channel_usage &= ~(chan_mask);
		spin_unlock_irqrestore(&video->ohci->IR_channel_lock, flags);
		
		video->channel = -1;
	}
	
	/* free the frame structs */
	for (i = 0; i < DV1394_MAX_FRAMES; i++) {
		if (video->frames[i])
			frame_delete(video->frames[i]);
		video->frames[i] = NULL;
	}

	video->n_frames = 0;
		
	/* we can't free the DMA buffer unless it is guaranteed that
	   no more user-space mappings exist */
	
	if (free_dv_buf) {
		dma_region_free(&video->dv_buf);
		video->dv_buf_size = 0;
	}

	/* free packet buffer */
	dma_region_free(&video->packet_buf);
	video->packet_buf_size = 0;

	debug_printk("dv1394: shutdown OK\n");
}

/*
       **********************************
       *** MMAP() THEORY OF OPERATION ***
       **********************************

        The ringbuffer cannot be re-allocated or freed while
        a user program maintains a mapping of it. (note that a mapping
	can persist even after the device fd is closed!)

	So, only let the user process allocate the DMA buffer once.
	To resize or deallocate it, you must close the device file
	and open it again.

	Previously Dan M. hacked out a scheme that allowed the DMA
	buffer to change by forcefully unmapping it from the user's
	address space. It was prone to error because it's very hard to
	track all the places the buffer could have been mapped (we
	would have had to walk the vma list of every process in the
	system to be sure we found all the mappings!). Instead, we
	force the user to choose one buffer size and stick with
	it. This small sacrifice is worth the huge reduction in
	error-prone code in dv1394.
*/

int dv1394_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct video_card *video = file_to_video_card(file);
	int retval = -EINVAL;

	/* serialize mmap */
	down(&video->sem);

	if ( ! video_card_initialized(video) ) {
		retval = do_dv1394_init_default(video);
		if (retval)
			goto out;
	}

	retval = dma_region_mmap(&video->dv_buf, file, vma);
out:
	up(&video->sem);
	return retval;
}

/*** DEVICE FILE INTERFACE *************************************************/

/* no need to serialize, multiple threads OK */
static unsigned int dv1394_poll(struct file *file, struct poll_table_struct *wait)
{
	struct video_card *video = file_to_video_card(file);
	unsigned int mask = 0;
	unsigned long flags;

	poll_wait(file, &video->waitq, wait);

	spin_lock_irqsave(&video->spinlock, flags);
	if ( video->n_frames == 0 ) {

	} else if ( video->active_frame == -1 ) {
		/* nothing going on */
		mask |= POLLOUT;
	} else {
		/* any clear/ready buffers? */
		if (video->n_clear_frames >0)
			mask |= POLLOUT | POLLIN;
	}
	spin_unlock_irqrestore(&video->spinlock, flags);

	return mask;
}

static int dv1394_fasync(int fd, struct file *file, int on)
{
	/* I just copied this code verbatim from Alan Cox's mouse driver example
	   (linux/Documentation/DocBook/) */
	
	struct video_card *video = file_to_video_card(file);
	
	int retval = fasync_helper(fd, file, on, &video->fasync);
	 
	if (retval < 0)
		return retval;
        return 0;
}

static ssize_t dv1394_write(struct file *file, const char *buffer, size_t count, loff_t *ppos)
{
	struct video_card *video = file_to_video_card(file);
	DECLARE_WAITQUEUE(wait, current);
	ssize_t ret;
	size_t cnt;
	unsigned long flags;
	int target_frame;

	/* serialize this to prevent multi-threaded mayhem */
	if (file->f_flags & O_NONBLOCK) {
		if (down_trylock(&video->sem))
			return -EAGAIN;
	} else {
		if (down_interruptible(&video->sem))
			return -ERESTARTSYS;
	}

	if ( !video_card_initialized(video) ) {
		ret = do_dv1394_init_default(video);
		if (ret) {
			up(&video->sem);
			return ret;
		}
	}

	ret = 0;
	add_wait_queue(&video->waitq, &wait);
	
	while (count > 0) {

		/* must set TASK_INTERRUPTIBLE *before* checking for free
		   buffers; otherwise we could miss a wakeup if the interrupt
		   fires between the check and the schedule() */
		
		set_current_state(TASK_INTERRUPTIBLE);
		
		spin_lock_irqsave(&video->spinlock, flags);
		
		target_frame = video->first_clear_frame;
		
		spin_unlock_irqrestore(&video->spinlock, flags);

		if (video->frames[target_frame]->state == FRAME_CLEAR) {

			/* how much room is left in the target frame buffer */
			cnt = video->frame_size - (video->write_off - target_frame * video->frame_size);

		} else {
			/* buffer is already used */
			cnt = 0;
		}

		if (cnt > count)
			cnt = count;

		if (cnt <= 0) { 
			/* no room left, gotta wait */
			if (file->f_flags & O_NONBLOCK) {
				if (!ret)
					ret = -EAGAIN;
				break;
			}
			if (signal_pending(current)) {
				if (!ret)
					ret = -ERESTARTSYS;
				break;
			}

			schedule();
			
			continue; /* start over from 'while(count > 0)...' */
		}

		if (copy_from_user(video->dv_buf.kvirt + video->write_off, buffer, cnt)) {
			if (!ret)
				ret = -EFAULT;
			break;
		}

		video->write_off = (video->write_off + cnt) % (video->n_frames * video->frame_size);

		count -= cnt;
		buffer += cnt;
		ret += cnt;

		if (video->write_off == video->frame_size * ((target_frame + 1) % video->n_frames))
				frame_prepare(video, target_frame);
	}
	
	remove_wait_queue(&video->waitq, &wait);
	set_current_state(TASK_RUNNING);
	up(&video->sem);
	return ret;
}


static ssize_t dv1394_read(struct file *file,  char *buffer, size_t count, loff_t *ppos)
{
	struct video_card *video = file_to_video_card(file);
	DECLARE_WAITQUEUE(wait, current);
	ssize_t ret;
	size_t cnt;
	unsigned long flags;
	int target_frame;

	/* serialize this to prevent multi-threaded mayhem */
	if (file->f_flags & O_NONBLOCK) {
		if (down_trylock(&video->sem))
			return -EAGAIN;
	} else {
		if (down_interruptible(&video->sem))
			return -ERESTARTSYS;
	}

	if ( !video_card_initialized(video) ) {
		ret = do_dv1394_init_default(video);
		if (ret) {
			up(&video->sem);
			return ret;
		}
		video->continuity_counter = -1;
		
		receive_packets(video);
		
		start_dma_receive(video);
	}

	ret = 0;
	add_wait_queue(&video->waitq, &wait);

	while (count > 0) {

		/* must set TASK_INTERRUPTIBLE *before* checking for free
		   buffers; otherwise we could miss a wakeup if the interrupt
		   fires between the check and the schedule() */
		
		set_current_state(TASK_INTERRUPTIBLE);

		spin_lock_irqsave(&video->spinlock, flags);

		target_frame = video->first_clear_frame;

		spin_unlock_irqrestore(&video->spinlock, flags);

		if (target_frame >= 0 &&
			video->n_clear_frames > 0 &&
			video->frames[target_frame]->state == FRAME_CLEAR) {

			/* how much room is left in the target frame buffer */
			cnt = video->frame_size - (video->write_off - target_frame * video->frame_size);

		} else {
			/* buffer is already used */
			cnt = 0;
		}

		if (cnt > count)
			cnt = count;

		if (cnt <= 0) { 
			/* no room left, gotta wait */
			if (file->f_flags & O_NONBLOCK) {
				if (!ret)
					ret = -EAGAIN;
				break;
			}
			if (signal_pending(current)) {
				if (!ret)
					ret = -ERESTARTSYS;
				break;
			}

			schedule();
			
			continue; /* start over from 'while(count > 0)...' */
		}

		if (copy_to_user(buffer, video->dv_buf.kvirt + video->write_off, cnt)) {
				if (!ret)
					ret = -EFAULT;
				break;
		}

		video->write_off = (video->write_off + cnt) % (video->n_frames * video->frame_size);

		count -= cnt;
		buffer += cnt;
		ret += cnt;

		if (video->write_off == video->frame_size * ((target_frame + 1) % video->n_frames)) {
			spin_lock_irqsave(&video->spinlock, flags);
			video->n_clear_frames--;
			video->first_clear_frame = (video->first_clear_frame + 1) % video->n_frames;
			spin_unlock_irqrestore(&video->spinlock, flags);
		}
	}
	
	remove_wait_queue(&video->waitq, &wait);
	set_current_state(TASK_RUNNING);
	up(&video->sem);
	return ret;
}


/*** DEVICE IOCTL INTERFACE ************************************************/

/* I *think* the VFS serializes ioctl() for us, so we don't have to worry
   about situations like having two threads in here at once... */

static int dv1394_ioctl(struct inode *inode, struct file *file,
			   unsigned int cmd, unsigned long arg)
{
	struct video_card *video = file_to_video_card(file);
	unsigned long flags;
	int ret = -EINVAL;

	DECLARE_WAITQUEUE(wait, current);

	/* serialize this to prevent multi-threaded mayhem */
	if (file->f_flags & O_NONBLOCK) {
		if (down_trylock(&video->sem))
			return -EAGAIN;
	} else {
		if (down_interruptible(&video->sem))
			return -ERESTARTSYS;
	}

	switch(cmd)
	{
	case DV1394_SUBMIT_FRAMES:
	case DV1394_IOC_SUBMIT_FRAMES: {
		unsigned int n_submit;

		if ( !video_card_initialized(video) ) {
			ret = do_dv1394_init_default(video);
			if (ret)
				goto out;
		}

		n_submit = (unsigned int) arg;

		if (n_submit > video->n_frames) {
			ret = -EINVAL;
			goto out;
		}
			
		while (n_submit > 0) {

			add_wait_queue(&video->waitq, &wait);
			set_current_state(TASK_INTERRUPTIBLE);
				
			spin_lock_irqsave(&video->spinlock, flags);

			/* wait until video->first_clear_frame is really CLEAR */
			while (video->frames[video->first_clear_frame]->state != FRAME_CLEAR) {

				spin_unlock_irqrestore(&video->spinlock, flags);
					
				if (signal_pending(current)) {
					remove_wait_queue(&video->waitq, &wait);
					set_current_state(TASK_RUNNING);
					ret = -EINTR;
					goto out;
				}

				schedule();
				set_current_state(TASK_INTERRUPTIBLE);
				
				spin_lock_irqsave(&video->spinlock, flags);
			}
			spin_unlock_irqrestore(&video->spinlock, flags);

			remove_wait_queue(&video->waitq, &wait);
			set_current_state(TASK_RUNNING);
				
			frame_prepare(video, video->first_clear_frame);

			n_submit--;
		}

		ret = 0;
		break;
	}
	case DV1394_WAIT_FRAMES:
	case DV1394_IOC_WAIT_FRAMES: {
		unsigned int n_wait;

		if ( !video_card_initialized(video) ) {
			ret = -EINVAL;
			goto out;
		}
		
		n_wait = (unsigned int) arg;

		/* since we re-run the last frame on underflow, we will
		   never actually have n_frames clear frames; at most only
		   n_frames - 1 */

		if (n_wait > (video->n_frames-1) ) {
			ret = -EINVAL;
			goto out;
		}
			
		add_wait_queue(&video->waitq, &wait);
		set_current_state(TASK_INTERRUPTIBLE);
		
		spin_lock_irqsave(&video->spinlock, flags);

		while (video->n_clear_frames < n_wait) {
			
			spin_unlock_irqrestore(&video->spinlock, flags);
					
			if (signal_pending(current)) {
				remove_wait_queue(&video->waitq, &wait);
				set_current_state(TASK_RUNNING);
				ret = -EINTR;
				goto out;
			}

			schedule();
			set_current_state(TASK_INTERRUPTIBLE);
			
			spin_lock_irqsave(&video->spinlock, flags);
		}

		spin_unlock_irqrestore(&video->spinlock, flags);

		remove_wait_queue(&video->waitq, &wait);
		set_current_state(TASK_RUNNING);
		ret = 0;
		break;
	}
	case DV1394_RECEIVE_FRAMES:
	case DV1394_IOC_RECEIVE_FRAMES: {
		unsigned int n_recv;

		if ( !video_card_initialized(video) ) {
			ret = -EINVAL;
			goto out;
		}
		
		n_recv = (unsigned int) arg;

		/* at least one frame must be active */
		if (n_recv > (video->n_frames-1) ) {
			ret = -EINVAL;
			goto out;
		}
			
		spin_lock_irqsave(&video->spinlock, flags);

		/* release the clear frames */
		video->n_clear_frames -= n_recv;

		/* advance the clear frame cursor */
		video->first_clear_frame = (video->first_clear_frame + n_recv) % video->n_frames;

		/* reset dropped_frames */
		video->dropped_frames = 0;
			
		spin_unlock_irqrestore(&video->spinlock, flags);

		ret = 0;
		break;
	}
	case DV1394_START_RECEIVE:
	case DV1394_IOC_START_RECEIVE: {
		if ( !video_card_initialized(video) ) {
			ret = do_dv1394_init_default(video);
			if (ret)
				goto out;
		}
	
		video->continuity_counter = -1;
		
		receive_packets(video);
		
		start_dma_receive(video);

		ret = 0;
		break;
	}
	case DV1394_INIT:
	case DV1394_IOC_INIT: {
		struct dv1394_init init;
		if (arg == (unsigned long) NULL) {
			ret = do_dv1394_init_default(video);
		} else {
			if (copy_from_user(&init, (void*)arg, sizeof(init))) {
				ret = -EFAULT;
				goto out;
			}
			ret = do_dv1394_init(video, &init);
		}
		break;
	}
	case DV1394_SHUTDOWN:
	case DV1394_IOC_SHUTDOWN:
		do_dv1394_shutdown(video, 0);
		ret = 0;
		break;

	case DV1394_GET_STATUS:
        case DV1394_IOC_GET_STATUS: {
		struct dv1394_status status;

		if ( !video_card_initialized(video) ) {
			ret = -EINVAL;
			goto out;
		}

		status.init.api_version = DV1394_API_VERSION;
		status.init.channel = video->channel;
		status.init.n_frames = video->n_frames;
		status.init.format = video->pal_or_ntsc;
		status.init.cip_n = video->cip_n;
		status.init.cip_d = video->cip_d;
		status.init.syt_offset = video->syt_offset;

		status.first_clear_frame = video->first_clear_frame;

		/* the rest of the fields need to be locked against the interrupt */
		spin_lock_irqsave(&video->spinlock, flags);

		status.active_frame = video->active_frame;
		status.n_clear_frames = video->n_clear_frames;

		status.dropped_frames = video->dropped_frames;

		/* reset dropped_frames */
		video->dropped_frames = 0;
			
		spin_unlock_irqrestore(&video->spinlock, flags);

		if (copy_to_user((void*)arg, &status, sizeof(status))) {
			ret = -EFAULT;
			goto out;
		}

		ret = 0;
		break;
	}

	default:
		break;
	}

 out:
	up(&video->sem);
	return ret;
}



/*** DEVICE FILE INTERFACE CONTINUED ***************************************/

static int dv1394_open(struct inode *inode, struct file *file)
{
	struct video_card *video = NULL;

	/* if the device was opened through devfs, then file->private_data
	   has already been set to video by devfs */
	if (file->private_data) {
		video = (struct video_card*) file->private_data;
		
	} else {
		/* look up the card by ID */
		
		struct list_head *lh;
		unsigned long flags;
		
		spin_lock_irqsave(&dv1394_cards_lock, flags);
		if (!list_empty(&dv1394_cards)) {
			struct video_card *p;
			list_for_each(lh, &dv1394_cards) {
				p = list_entry(lh, struct video_card, list);
				if ((p->id) == ieee1394_file_to_instance(file)) {
					video = p;
					break;
				}
			}
		}
		spin_unlock_irqrestore(&dv1394_cards_lock, flags);

		if (!video) {
			debug_printk("dv1394: OHCI card %d not found", ieee1394_file_to_instance(file));
			return -ENODEV;
		}
		
		file->private_data = (void*) video;
	}
	
#ifndef DV1394_ALLOW_MORE_THAN_ONE_OPEN

	if ( test_and_set_bit(0, &video->open) ) {
		/* video is already open by someone else */
		return -EBUSY;
 	}

#endif

	return 0;
}


static int dv1394_release(struct inode *inode, struct file *file)
{
	struct video_card *video = file_to_video_card(file);

	/* OK to free the DMA buffer, no more mappings can exist */
	do_dv1394_shutdown(video, 1);

	/* clean up async I/O users */
	dv1394_fasync(-1, file, 0);
	
	/* give someone else a turn */
	clear_bit(0, &video->open);

	return 0;
}


/*** PROC_FS INTERFACE ******************************************************/
#ifdef CONFIG_PROC_FS
static LIST_HEAD(dv1394_procfs);
struct dv1394_procfs_entry {
	struct list_head list;
    struct proc_dir_entry *procfs;
	char name[32];
	struct dv1394_procfs_entry *parent;
};
static spinlock_t dv1394_procfs_lock = SPIN_LOCK_UNLOCKED;

static int dv1394_procfs_read( char *page, char **start, off_t off,
			int count, int *eof, void *data)
{
	struct video_card *video = (struct video_card*) data;

	snprintf( page, count, 
		"\
format=%s\n\
channel=%d\n\
cip_n=%lu\n\
cip_d=%lu\n\
syt_offset=%u\n",
		(video->pal_or_ntsc == DV1394_NTSC ? "NTSC" : "PAL"),
		video->channel,
		video->cip_n, video->cip_d, video->syt_offset );
	return strlen(page);
}

/* lifted from the stallion.c driver */
#undef  TOLOWER
#define TOLOWER(x)      ((((x) >= 'A') && ((x) <= 'Z')) ? ((x) + 0x20) : (x))
static unsigned long atol(char *str)
{
	unsigned long   val;
	int             base, c;
	char            *sp;

	val = 0;
	sp = str;
	if ((*sp == '0') && (*(sp+1) == 'x')) {
		base = 16;
		sp += 2;
	} else if (*sp == '0') {
		base = 8;
		sp++;
	} else {
		base = 10;
	}

	for (; (*sp != 0); sp++) {
		c = (*sp > '9') ? (TOLOWER(*sp) - 'a' + 10) : (*sp - '0');
		if ((c < 0) || (c >= base)) {
			printk(KERN_ERR "dv1394: atol() invalid argument %s\n", str);
			val = 0;
			break;
		}
		val = (val * base) + c;
	}
	return(val);
}

static int dv1394_procfs_write( struct file *file,
			const char *buffer, unsigned long count, void *data)
{
	int len = 0;
	char new_value[65];
	char *pos;
	struct video_card *video = (struct video_card*) data;
	
	if (count > 64)
		len = 64;
	else
		len = count;
				
	if (copy_from_user( new_value, buffer, len))
		return -EFAULT;
	
	new_value[len] = 0;
	pos = strchr(new_value, '=');
	if (pos != NULL) {
		int val_len = len - (pos-new_value) - 1;
		char buf[65];
		memset(buf, 0, 65);
		strncpy(buf, pos+1, val_len);
		if (buf[val_len-1] == '\n') buf[val_len-1] = 0;
		
		if (strnicmp( new_value, "format", (pos-new_value)) == 0) {
			if (strnicmp( buf, "NTSC", val_len) == 0)
				video->pal_or_ntsc = DV1394_NTSC;
			else if (strnicmp( buf, "PAL", val_len) == 0)
				video->pal_or_ntsc = DV1394_PAL;
				
		} else if (strnicmp( new_value, "cip_n", (pos-new_value)) == 0) {
			video->cip_n = atol(buf);
		} else if (strnicmp( new_value, "cip_d", (pos-new_value)) == 0) {
			video->cip_d = atol(buf);
		} else if (strnicmp( new_value, "syt_offset", (pos-new_value)) == 0) {
			video->syt_offset = atol(buf);
		} else if (strnicmp( new_value, "channel", (pos-new_value)) == 0) {
			video->channel = atol(buf);
		}
	}
	
	return len;
}

struct dv1394_procfs_entry *
dv1394_procfs_find( char *name)
{
	struct list_head *lh;
	struct dv1394_procfs_entry *p;
		
	spin_lock( &dv1394_procfs_lock);
	if (!list_empty(&dv1394_procfs)) {
		list_for_each(lh, &dv1394_procfs) {
			p = list_entry(lh, struct dv1394_procfs_entry, list);
			if (!strncmp(p->name, name, sizeof(p->name))) {
				spin_unlock( &dv1394_procfs_lock);
				return p;
			}
		}
	}
	spin_unlock( &dv1394_procfs_lock);
	return NULL;
}

static int dv1394_procfs_add_entry(struct video_card *video)
{
	char buf[32];
	struct dv1394_procfs_entry *p;
	struct dv1394_procfs_entry *parent;

	p = kmalloc(sizeof(struct dv1394_procfs_entry), GFP_KERNEL);
	if (!p) {
		printk(KERN_ERR "dv1394: cannot allocate dv1394_procfs_entry\n");
		goto err;
	}
	memset(p, 0, sizeof(struct dv1394_procfs_entry));
	
	snprintf(buf, sizeof(buf), "dv/host%d/%s", (video->id>>2),
						(video->pal_or_ntsc == DV1394_NTSC ? "NTSC" : "PAL"));
		
	parent = dv1394_procfs_find(buf);
	if (parent == NULL) {
		printk(KERN_ERR "dv1394: unable to locate parent procfs of %s\n", buf);
		goto err_free;
	}
	
	p->procfs = create_proc_entry( 
						(video->mode == MODE_RECEIVE ? "in" : "out"),
						0666, parent->procfs);

	if (p->procfs == NULL) {
		printk(KERN_ERR "dv1394: unable to create /proc/bus/ieee1394/%s/%s\n",
			parent->name,
			(video->mode == MODE_RECEIVE ? "in" : "out"));
		goto err_free;
	}
	
	p->procfs->owner = THIS_MODULE;
	p->procfs->data = video;
	p->procfs->read_proc = dv1394_procfs_read;
	p->procfs->write_proc = dv1394_procfs_write;

	spin_lock( &dv1394_procfs_lock);
	INIT_LIST_HEAD(&p->list);
	list_add_tail(&p->list, &dv1394_procfs);
	spin_unlock( &dv1394_procfs_lock);
	
	return 0;
	
 err_free:
	kfree(p);
 err:
	return -ENOMEM;
}

static int
dv1394_procfs_add_dir( char *name,
					struct dv1394_procfs_entry *parent, 
					struct dv1394_procfs_entry **out)
{
	struct dv1394_procfs_entry *p;

	p = kmalloc(sizeof(struct dv1394_procfs_entry), GFP_KERNEL);
	if (!p) {
		printk(KERN_ERR "dv1394: cannot allocate dv1394_procfs_entry\n");
		goto err;
	}
	memset(p, 0, sizeof(struct dv1394_procfs_entry));
	
	if (parent == NULL) {
		snprintf(p->name, sizeof(p->name), "%s", name);
		p->procfs = proc_mkdir( name, ieee1394_procfs_entry);
	} else {
		snprintf(p->name, sizeof(p->name), "%s/%s", parent->name, name);
		p->procfs = proc_mkdir( name, parent->procfs);
	}
	if (p->procfs == NULL) {
		printk(KERN_ERR "dv1394: unable to create /proc/bus/ieee1394/%s\n", p->name);
		goto err_free;
	}

	p->procfs->owner = THIS_MODULE;
	p->parent = parent;
	if (out != NULL) *out = p;

	spin_lock( &dv1394_procfs_lock);
	INIT_LIST_HEAD(&p->list);
	list_add_tail(&p->list, &dv1394_procfs);
	spin_unlock( &dv1394_procfs_lock);

	return 0;
	
 err_free:
	kfree(p);
 err:
	return -ENOMEM;
}

void dv1394_procfs_del( char *name)
{
	struct dv1394_procfs_entry *p = dv1394_procfs_find(name);
	if (p != NULL) {
		if (p->parent == NULL)
			remove_proc_entry(p->name, ieee1394_procfs_entry);
		else
			remove_proc_entry(p->name, p->parent->procfs);
		
		spin_lock( &dv1394_procfs_lock);
		list_del(&p->list);
		spin_unlock( &dv1394_procfs_lock);
		kfree(p);
	}
}
#endif /* CONFIG_PROC_FS */

/*** DEVICE DRIVER HANDLERS ************************************************/

static void it_tasklet_func(unsigned long data)
{
	int wake = 0;
	struct video_card *video = (struct video_card*) data;

	spin_lock(&video->spinlock);

	if (!video->dma_running)
		goto out;

	irq_printk("ContextControl = %08x, CommandPtr = %08x\n", 
	       reg_read(video->ohci, video->ohci_IsoXmitContextControlSet),
	       reg_read(video->ohci, video->ohci_IsoXmitCommandPtr)
	       );

	
	if ( (video->ohci_it_ctx != -1) &&
	    (reg_read(video->ohci, video->ohci_IsoXmitContextControlSet) & (1 << 10)) ) {

		struct frame *f;
		unsigned int frame, i;

		
		if (video->active_frame == -1)
			frame = 0;
		else
			frame = video->active_frame;

		/* check all the DMA-able frames */
		for (i = 0; i < video->n_frames; i++, frame = (frame+1) % video->n_frames) {

			irq_printk("IRQ checking frame %d...", frame);
			f = video->frames[frame];
			if (f->state != FRAME_READY) {
				irq_printk("clear, skipping\n");
				/* we don't own this frame */
				continue;
			}

			irq_printk("DMA\n");

			/* check the frame begin semaphore to see if we can free the previous frame */
			if ( *(f->frame_begin_timestamp) ) {
				int prev_frame;
				struct frame *prev_f;

				

				/* don't reset, need this later *(f->frame_begin_timestamp) = 0; */
				irq_printk("  BEGIN\n");

				prev_frame = frame - 1;
				if (prev_frame == -1)
					prev_frame += video->n_frames;
				prev_f = video->frames[prev_frame];
				
				/* make sure we can actually garbage collect
				   this frame */
				if ( (prev_f->state == FRAME_READY) &&
				    prev_f->done && (!f->done) ) 
				{
					frame_reset(prev_f);
					video->n_clear_frames++;
					wake = 1;
					video->active_frame = frame;

					irq_printk("  BEGIN - freeing previous frame %d, new active frame is %d\n", prev_frame, frame);
				} else {
					irq_printk("  BEGIN - can't free yet\n");
				}

				f->done = 1;
			}

		     
			/* see if we need to set the timestamp for the next frame */
			if ( *(f->mid_frame_timestamp) ) {
				struct frame *next_frame;
				u32 begin_ts, ts_cyc, ts_off;

				*(f->mid_frame_timestamp) = 0;

				begin_ts = le32_to_cpu(*(f->frame_begin_timestamp));

				irq_printk("  MIDDLE - first packet was sent at cycle %4u (%2u), assigned timestamp was (%2u) %4u\n",
					   begin_ts & 0x1FFF, begin_ts & 0xF,
					   f->assigned_timestamp >> 12, f->assigned_timestamp & 0xFFF);

				/* prepare next frame and assign timestamp */
				next_frame = video->frames[ (frame+1) % video->n_frames ];

				if (next_frame->state == FRAME_READY) {
					irq_printk("  MIDDLE - next frame is ready, good\n");
				} else {
					debug_printk("dv1394: Underflow! At least one frame has been dropped.\n");
					next_frame = f;
				}

				/* set the timestamp to the timestamp of the last frame sent,
				   plus the length of the last frame sent, plus the syt latency */
				ts_cyc = begin_ts & 0xF;
				/* advance one frame, plus syt latency (typically 2-3) */
				ts_cyc += f->n_packets + video->syt_offset ; 

				ts_off = 0; 

				ts_cyc += ts_off/3072;
				ts_off %= 3072;

				next_frame->assigned_timestamp = ((ts_cyc&0xF) << 12) + ts_off;
				if (next_frame->cip_syt1) {
					next_frame->cip_syt1->b[6] = next_frame->assigned_timestamp >> 8;
					next_frame->cip_syt1->b[7] = next_frame->assigned_timestamp & 0xFF;
				}
				if (next_frame->cip_syt2) {
					next_frame->cip_syt2->b[6] = next_frame->assigned_timestamp >> 8;
					next_frame->cip_syt2->b[7] = next_frame->assigned_timestamp & 0xFF;
				}

			}

			/* see if the frame looped */
			if ( *(f->frame_end_timestamp) ) {

				*(f->frame_end_timestamp) = 0;

				debug_printk("  END - the frame looped at least once\n");

				video->dropped_frames++;
			}


		
		} /* for (each frame) */
	}

	if (wake) {
		kill_fasync(&video->fasync, SIGIO, POLL_OUT);
		
		/* wake readers/writers/ioctl'ers */
		wake_up_interruptible(&video->waitq);
	}

out:
	spin_unlock(&video->spinlock);
}

static void ir_tasklet_func(unsigned long data)
{
	int wake = 0;
	struct video_card *video = (struct video_card*) data;

	spin_lock(&video->spinlock);

	if (!video->dma_running)
		goto out;
	
	if ( (video->ohci_ir_ctx != -1) &&
	    (reg_read(video->ohci, video->ohci_IsoRcvContextControlSet) & (1 << 10)) )
	{ 

		int sof=0; /* start-of-frame flag */
		struct frame *f;
		u16 packet_length, packet_time;
		int i, dbc=0;
		struct DMA_descriptor_block *block = NULL;
		u16 xferstatus;

		int next_i, prev_i;
		struct DMA_descriptor_block *next = NULL;
		dma_addr_t next_dma = 0;
		struct DMA_descriptor_block *prev = NULL;

		/* loop over all descriptors in all frames */
		for (i = 0; i < video->n_frames*MAX_PACKETS; i++) {
			struct packet *p = dma_region_i(&video->packet_buf, struct packet, video->current_packet);

			/* make sure we are seeing the latest changes to p */
			dma_region_sync(&video->packet_buf,
					(unsigned long) p - (unsigned long) video->packet_buf.kvirt,
					sizeof(struct packet));
					
			packet_length = le16_to_cpu(p->data_length);
			packet_time   = le16_to_cpu(p->timestamp);
	
			irq_printk("received packet %02d, timestamp=%04x, length=%04x, sof=%02x%02x\n", video->current_packet,
				   packet_time, packet_length, 
				   p->data[0], p->data[1]);
			
			/* get the descriptor based on packet_buffer cursor */
			f = video->frames[video->current_packet / MAX_PACKETS];
			block = &(f->descriptor_pool[video->current_packet % MAX_PACKETS]);
			xferstatus = le32_to_cpu(block->u.in.il.q[3]) >> 16;
			xferstatus &= 0x1F;
			irq_printk("ir_tasklet_func: xferStatus/resCount [%d] = 0x%08x\n", i, le32_to_cpu(block->u.in.il.q[3]) );

			/* get the current frame */
			f = video->frames[video->active_frame];
		
			/* exclude empty packet */
			if (packet_length > 8 && xferstatus == 0x11) {
				/* check for start of frame */
				/* DRD> Changed to check section type ([0]>>5==0) 
				   and dif sequence ([1]>>4==0) */
				sof = ( (p->data[0] >> 5) == 0 && (p->data[1] >> 4) == 0);
	
				dbc = (int) (p->cip_h1 >> 24);
				if ( video->continuity_counter != -1 && dbc > ((video->continuity_counter + 1) % 256) )
				{
					printk(KERN_WARNING "dv1394: discontinuity detected, dropping all frames\n" );
					video->dropped_frames += video->n_clear_frames + 1;
					video->first_frame = 0;
					video->n_clear_frames = 0;
					video->first_clear_frame = -1;
				}
				video->continuity_counter = dbc;
				
				if (!video->first_frame) {
					if (sof) {
						video->first_frame = 1;
					}
	
				} else if (sof) {
					/* close current frame */
					frame_reset(f);  /* f->state = STATE_CLEAR */
					video->n_clear_frames++;
					if (video->n_clear_frames > video->n_frames) {
						video->dropped_frames++;
						printk(KERN_WARNING "dv1394: dropped a frame during reception\n" );
						video->n_clear_frames = video->n_frames-1;
						video->first_clear_frame = (video->first_clear_frame + 1) % video->n_frames;
					}
					if (video->first_clear_frame == -1)
						video->first_clear_frame = video->active_frame;
	
					/* get the next frame */
					video->active_frame = (video->active_frame + 1) % video->n_frames;
					f = video->frames[video->active_frame];
					irq_printk("   frame received, active_frame = %d, n_clear_frames = %d, first_clear_frame = %d\n",
						   video->active_frame, video->n_clear_frames, video->first_clear_frame);
				}
				if (video->first_frame) {
					if (sof) {
						/* open next frame */
						f->state = FRAME_READY;
					}
					
					/* copy to buffer */
					if (f->n_packets > (video->frame_size / 480)) {
						printk(KERN_ERR "frame buffer overflow during receive\n");
					}
					
					frame_put_packet(f, p);
					
				} /* first_frame */
			}
			
			/* stop, end of ready packets */
			else if (xferstatus == 0) {
				break;
			}
	 
			/* reset xferStatus & resCount */
			block->u.in.il.q[3] = cpu_to_le32(512);

			/* terminate dma chain at this (next) packet */
			next_i = video->current_packet;
			f = video->frames[next_i / MAX_PACKETS];
			next = &(f->descriptor_pool[next_i % MAX_PACKETS]);
			next_dma = ((unsigned long) block - (unsigned long) f->descriptor_pool) + f->descriptor_pool_dma;
			next->u.in.il.q[0] |= 3 << 20; /* enable interrupt */
			next->u.in.il.q[2] = 0; /* disable branch */
			
			/* link previous to next */
			prev_i = (next_i == 0) ? (MAX_PACKETS * video->n_frames - 1) : (next_i - 1);
			f = video->frames[prev_i / MAX_PACKETS];
			prev = &(f->descriptor_pool[prev_i % MAX_PACKETS]);
			if (prev_i % (MAX_PACKETS/2)) {
				prev->u.in.il.q[0] &= ~(3 << 20); /* no interrupt */
			} else {
				prev->u.in.il.q[0] |= 3 << 20; /* enable interrupt */
			}
			prev->u.in.il.q[2] = cpu_to_le32(next_dma | 1); /* set Z=1 */
			wmb();

			/* wake up DMA in case it fell asleep */
			reg_write(video->ohci, video->ohci_IsoRcvContextControlSet, (1 << 12));

			/* advance packet_buffer cursor */
			video->current_packet = (video->current_packet + 1) % (MAX_PACKETS * video->n_frames);
			
		} /* for all packets */
 
		wake = 1; /* why the hell not? */
		
	} /* receive interrupt */
	
	if (wake) {
		kill_fasync(&video->fasync, SIGIO, POLL_IN);

		/* wake readers/writers/ioctl'ers */
		wake_up_interruptible(&video->waitq);
	}
	
out:
	spin_unlock(&video->spinlock);
}

static struct file_operations dv1394_fops=
{
	.owner =	THIS_MODULE,
	.poll =         dv1394_poll,
	.ioctl =	dv1394_ioctl,
	.mmap =		dv1394_mmap,
	.open =		dv1394_open,
	.write =        dv1394_write,
	.read =         dv1394_read,
	.release =	dv1394_release,
	.fasync =       dv1394_fasync,
};


/*** DEVFS HELPERS *********************************************************/

struct dv1394_devfs_entry *
dv1394_devfs_find( char *name)
{
	struct list_head *lh;
	struct dv1394_devfs_entry *p;

	spin_lock( &dv1394_devfs_lock);
	if (!list_empty(&dv1394_devfs)) {
		list_for_each(lh, &dv1394_devfs) {
			p = list_entry(lh, struct dv1394_devfs_entry, list);
			if (!strncmp(p->name, name, sizeof(p->name))) {
				goto found;
			}
		}
	}
	p = NULL;
	
found:
	spin_unlock( &dv1394_devfs_lock);
	return p;
}

#ifdef CONFIG_DEVFS_FS
static int dv1394_devfs_add_entry(struct video_card *video)
{
	char buf[32];
	struct dv1394_devfs_entry *p;
	struct dv1394_devfs_entry *parent;

	p = kmalloc(sizeof(struct dv1394_devfs_entry), GFP_KERNEL);
	if (!p) {
		printk(KERN_ERR "dv1394: cannot allocate dv1394_devfs_entry\n");
		goto err;
	}
	memset(p, 0, sizeof(struct dv1394_devfs_entry));
	
	snprintf(buf, sizeof(buf), "dv/host%d/%s", (video->id>>2),
						(video->pal_or_ntsc == DV1394_NTSC ? "NTSC" : "PAL"));
		
	parent = dv1394_devfs_find(buf);
	if (parent == NULL) {
		printk(KERN_ERR "dv1394: unable to locate parent devfs of %s\n", buf);
		goto err_free;
	}
	
	video->devfs_handle = devfs_register(
						 parent->devfs,
					     (video->mode == MODE_RECEIVE ? "in" : "out"),
						 DEVFS_FL_NONE,
					     IEEE1394_MAJOR,
					     IEEE1394_MINOR_BLOCK_DV1394*16 + video->id,
					     S_IFCHR | S_IRUGO | S_IWUGO,
					     &dv1394_fops,
					     (void*) video);
	p->devfs = video->devfs_handle;

	if (p->devfs == NULL) {
		printk(KERN_ERR "dv1394: unable to create /dev/ieee1394/%s/%s\n",
			parent->name,
			(video->mode == MODE_RECEIVE ? "in" : "out"));
		goto err_free;
	}
	
	spin_lock( &dv1394_devfs_lock);
	INIT_LIST_HEAD(&p->list);
	list_add_tail(&p->list, &dv1394_devfs);
	spin_unlock( &dv1394_devfs_lock);
	
	return 0;
	
 err_free:
	kfree(p);
 err:
	return -ENOMEM;
}

static int
dv1394_devfs_add_dir( char *name,
					struct dv1394_devfs_entry *parent, 
					struct dv1394_devfs_entry **out)
{
	struct dv1394_devfs_entry *p;

	p = kmalloc(sizeof(struct dv1394_devfs_entry), GFP_KERNEL);
	if (!p) {
		printk(KERN_ERR "dv1394: cannot allocate dv1394_devfs_entry\n");
		goto err;
	}
	memset(p, 0, sizeof(struct dv1394_devfs_entry));
	
	if (parent == NULL) {
		snprintf(p->name, sizeof(p->name), "%s", name);
		p->devfs = devfs_mk_dir(ieee1394_devfs_handle, name, NULL);
	} else {
		snprintf(p->name, sizeof(p->name), "%s/%s", parent->name, name);
		p->devfs = devfs_mk_dir(parent->devfs, name, NULL);
	}
	if (p->devfs == NULL) {
		printk(KERN_ERR "dv1394: unable to create /dev/ieee1394/%s\n", p->name);
		goto err_free;
	}

	p->parent = parent;
	if (out != NULL) *out = p;

	spin_lock( &dv1394_devfs_lock);
	INIT_LIST_HEAD(&p->list);
	list_add_tail(&p->list, &dv1394_devfs);
	spin_unlock( &dv1394_devfs_lock);

	return 0;
	
 err_free:
	kfree(p);
 err:
	return -ENOMEM;
}

void dv1394_devfs_del( char *name)
{
	struct dv1394_devfs_entry *p = dv1394_devfs_find(name);
	if (p != NULL) {
		devfs_unregister(p->devfs);
		
		spin_lock( &dv1394_devfs_lock);
		list_del(&p->list);
		spin_unlock( &dv1394_devfs_lock);
		kfree(p);
	}
}
#endif /* CONFIG_DEVFS_FS */


/*** HOTPLUG STUFF **********************************************************/
/*
 * Export information about protocols/devices supported by this driver.
 */
static struct ieee1394_device_id dv1394_id_table[] = {
	{
		.match_flags	= IEEE1394_MATCH_SPECIFIER_ID | IEEE1394_MATCH_VERSION,
		.specifier_id	= AVC_UNIT_SPEC_ID_ENTRY & 0xffffff,
		.version	= AVC_SW_VERSION_ENTRY & 0xffffff
	},
	{ }
};

static struct hpsb_protocol_driver dv1394_driver = {
	.name =		"DV/1394 Driver",
	.id_table = 	dv1394_id_table,
};

MODULE_DEVICE_TABLE(ieee1394, dv1394_id_table);


/*** IEEE1394 HPSB CALLBACKS ***********************************************/

static int dv1394_init(struct ti_ohci *ohci, enum pal_or_ntsc format, enum modes mode)
{
	struct video_card *video;
	unsigned long flags;
	int i;

	video = kmalloc(sizeof(struct video_card), GFP_KERNEL);
	if (!video) {
		printk(KERN_ERR "dv1394: cannot allocate video_card\n");
		goto err;
	}
	
	memset(video, 0, sizeof(struct video_card));
	
	video->ohci = ohci;
	/* lower 2 bits of id indicate which of four "plugs"
	   per host */
	video->id = ohci->id << 2; 
	if (format == DV1394_NTSC)
		video->id |= mode;
	else
		video->id |= 2 + mode;

	video->ohci_it_ctx = -1;
	video->ohci_ir_ctx = -1;

	video->ohci_IsoXmitContextControlSet = 0;
	video->ohci_IsoXmitContextControlClear = 0;
	video->ohci_IsoXmitCommandPtr = 0;
	
	video->ohci_IsoRcvContextControlSet = 0;
	video->ohci_IsoRcvContextControlClear = 0;
	video->ohci_IsoRcvCommandPtr = 0;
	video->ohci_IsoRcvContextMatch = 0;
		
	video->n_frames = 0; /* flag that video is not initialized */
	video->channel = 63; /* default to broadcast channel */
	video->active_frame = -1;
	
	/* initialize the following for proc_fs */
	video->pal_or_ntsc = format;
	video->cip_n = 0; /* 0 = use builtin default */
	video->cip_d = 0;
	video->syt_offset = 0;
	video->mode = mode;

#ifdef CONFIG_PROC_FS
	if ( dv1394_procfs_add_entry(video) < 0 )
		goto err_free;
#endif

	for (i = 0; i < DV1394_MAX_FRAMES; i++)
		video->frames[i] = NULL;

	dma_region_init(&video->dv_buf);
	video->dv_buf_size = 0;
	dma_region_init(&video->packet_buf);
	video->packet_buf_size = 0;

	clear_bit(0, &video->open);
	spin_lock_init(&video->spinlock);
	video->dma_running = 0;
	init_MUTEX(&video->sem);
	init_waitqueue_head(&video->waitq);
	video->fasync = NULL;

	spin_lock_irqsave(&dv1394_cards_lock, flags);
	INIT_LIST_HEAD(&video->list);
	list_add_tail(&video->list, &dv1394_cards);
	spin_unlock_irqrestore(&dv1394_cards_lock, flags);
	
#ifdef CONFIG_DEVFS_FS
	if (dv1394_devfs_add_entry(video) < 0)
			goto err_free;
#endif

	debug_printk("dv1394: dv1394_init() OK on ID %d\n", video->id);
	
	return 0;

 err_free:
	kfree(video);
 err:
	return -1;
}

static void dv1394_un_init(struct video_card *video)
{
	char buf[32];
	
	/* obviously nobody has the driver open at this point */
	do_dv1394_shutdown(video, 1);
	snprintf(buf, sizeof(buf), "dv/host%d/%s/%s", (video->id >> 2),
		(video->pal_or_ntsc == DV1394_NTSC ? "NTSC" : "PAL"),
		(video->mode == MODE_RECEIVE ? "in" : "out")
		);
#ifdef CONFIG_DEVFS_FS
	dv1394_devfs_del(buf);
#endif
#ifdef CONFIG_PROC_FS
	dv1394_procfs_del(buf);
#endif
	list_del(&video->list);
	kfree(video);
}

	
static void dv1394_remove_host (struct hpsb_host *host)
{
	struct ti_ohci *ohci;
	struct video_card *video = NULL;
	unsigned long flags;
	struct list_head *lh, *templh;
	char buf[32];
	int	n;
	
	/* We only work with the OHCI-1394 driver */
	if (strcmp(host->driver->name, OHCI1394_DRIVER_NAME))
		return;

	ohci = (struct ti_ohci *)host->hostdata;


	/* find the corresponding video_cards */
	spin_lock_irqsave(&dv1394_cards_lock, flags);
	if (!list_empty(&dv1394_cards)) {
		list_for_each_safe(lh, templh, &dv1394_cards) {
			video = list_entry(lh, struct video_card, list);
			if ((video->id >> 2) == ohci->id)
				dv1394_un_init(video);
		}
	}
	spin_unlock_irqrestore(&dv1394_cards_lock, flags);

	n = (video->id >> 2);
#ifdef CONFIG_DEVFS_FS
	snprintf(buf, sizeof(buf), "dv/host%d/NTSC", n);
	dv1394_devfs_del(buf);
	snprintf(buf, sizeof(buf), "dv/host%d/PAL", n);
	dv1394_devfs_del(buf);
	snprintf(buf, sizeof(buf), "dv/host%d", n);
	dv1394_devfs_del(buf);
#endif

#ifdef CONFIG_PROC_FS
	snprintf(buf, sizeof(buf), "dv/host%d/NTSC", n);
	dv1394_procfs_del(buf);
	snprintf(buf, sizeof(buf), "dv/host%d/PAL", n);
	dv1394_procfs_del(buf);
	snprintf(buf, sizeof(buf), "dv/host%d", n);
	dv1394_procfs_del(buf);
#endif	
}

static void dv1394_add_host (struct hpsb_host *host)
{
	struct ti_ohci *ohci;
	char buf[16];

	/* We only work with the OHCI-1394 driver */
	if (strcmp(host->driver->name, OHCI1394_DRIVER_NAME))
		return;

	ohci = (struct ti_ohci *)host->hostdata;

#ifdef CONFIG_PROC_FS
{
	struct dv1394_procfs_entry *p;
	p = dv1394_procfs_find("dv");
	if (p != NULL) {
		snprintf(buf, sizeof(buf), "host%d", ohci->id);
		dv1394_procfs_add_dir(buf, p, &p);
		dv1394_procfs_add_dir("NTSC", p, NULL);
		dv1394_procfs_add_dir("PAL", p, NULL);
	}
}
#endif

#ifdef CONFIG_DEVFS_FS
{
	struct dv1394_devfs_entry *devfs_entry = dv1394_devfs_find("dv");
	if (devfs_entry != NULL) {
		snprintf(buf, sizeof(buf), "host%d", ohci->id);
		dv1394_devfs_add_dir(buf, devfs_entry, &devfs_entry);
		dv1394_devfs_add_dir("NTSC", devfs_entry, NULL);
		dv1394_devfs_add_dir("PAL", devfs_entry, NULL);
	}
}
#endif
	
	dv1394_init(ohci, DV1394_NTSC, MODE_RECEIVE);
	dv1394_init(ohci, DV1394_NTSC, MODE_TRANSMIT);
	dv1394_init(ohci, DV1394_PAL, MODE_RECEIVE);
	dv1394_init(ohci, DV1394_PAL, MODE_TRANSMIT);
}


/* Bus reset handler. In the event of a bus reset, we may need to
   re-start the DMA contexts - otherwise the user program would
   end up waiting forever.
*/

static void dv1394_host_reset(struct hpsb_host *host)
{
	struct ti_ohci *ohci;
	struct video_card *video = NULL;
	unsigned long flags;
	struct list_head *lh;
	
	/* We only work with the OHCI-1394 driver */
	if (strcmp(host->driver->name, OHCI1394_DRIVER_NAME))
		return;

	ohci = (struct ti_ohci *)host->hostdata;


	/* find the corresponding video_cards */
	spin_lock_irqsave(&dv1394_cards_lock, flags);
	if (!list_empty(&dv1394_cards)) {
		list_for_each(lh, &dv1394_cards) {
			video = list_entry(lh, struct video_card, list);
			if ((video->id >> 2) == ohci->id)
				break;
		}
	}
	spin_unlock_irqrestore(&dv1394_cards_lock, flags);

	if (!video)
		return;

	
	spin_lock_irqsave(&video->spinlock, flags);

	if (!video->dma_running)
		goto out;

	/* check IT context */
	if (video->ohci_it_ctx != -1) {
		u32 ctx;
		
		ctx = reg_read(video->ohci, video->ohci_IsoXmitContextControlSet);

		/* if (RUN but not ACTIVE) */
		if ( (ctx & (1<<15)) &&
		    !(ctx & (1<<10)) ) {

			debug_printk("dv1394: IT context stopped due to bus reset; waking it up\n");

			/* to be safe, assume a frame has been dropped. User-space programs
			   should handle this condition like an underflow. */
			video->dropped_frames++;
			
			/* for some reason you must clear, then re-set the RUN bit to restart DMA */
			
			/* clear RUN */
			reg_write(video->ohci, video->ohci_IsoXmitContextControlClear, (1 << 15));
			flush_pci_write(video->ohci);
			
			/* set RUN */
			reg_write(video->ohci, video->ohci_IsoXmitContextControlSet, (1 << 15));
			flush_pci_write(video->ohci);
			
			/* set the WAKE bit (just in case; this isn't strictly necessary) */
			reg_write(video->ohci, video->ohci_IsoXmitContextControlSet, (1 << 12));
			flush_pci_write(video->ohci);

			irq_printk("dv1394: AFTER IT restart ctx 0x%08x ptr 0x%08x\n",
				   reg_read(video->ohci, video->ohci_IsoXmitContextControlSet),
				   reg_read(video->ohci, video->ohci_IsoXmitCommandPtr));
		}
	}
	
	/* check IR context */
	if (video->ohci_ir_ctx != -1) {
		u32 ctx;
		
		ctx = reg_read(video->ohci, video->ohci_IsoRcvContextControlSet);

		/* if (RUN but not ACTIVE) */
		if ( (ctx & (1<<15)) &&
		    !(ctx & (1<<10)) ) {

			debug_printk("dv1394: IR context stopped due to bus reset; waking it up\n");

			/* to be safe, assume a frame has been dropped. User-space programs
			   should handle this condition like an overflow. */
			video->dropped_frames++;

			/* for some reason you must clear, then re-set the RUN bit to restart DMA */
			/* XXX this doesn't work for me, I can't get IR DMA to restart :[ */
			
			/* clear RUN */
			reg_write(video->ohci, video->ohci_IsoRcvContextControlClear, (1 << 15));
			flush_pci_write(video->ohci);
			
			/* set RUN */
			reg_write(video->ohci, video->ohci_IsoRcvContextControlSet, (1 << 15));
			flush_pci_write(video->ohci);
			
			/* set the WAKE bit (just in case; this isn't strictly necessary) */
			reg_write(video->ohci, video->ohci_IsoRcvContextControlSet, (1 << 12));
			flush_pci_write(video->ohci);

			irq_printk("dv1394: AFTER IR restart ctx 0x%08x ptr 0x%08x\n",
				   reg_read(video->ohci, video->ohci_IsoRcvContextControlSet),
				   reg_read(video->ohci, video->ohci_IsoRcvCommandPtr));
		}
	}

out:
	spin_unlock_irqrestore(&video->spinlock, flags);
	
	/* wake readers/writers/ioctl'ers */
	wake_up_interruptible(&video->waitq);
}

static struct hpsb_highlevel dv1394_highlevel = {
	.name =		"dv1394",
	.add_host =	dv1394_add_host,
	.remove_host =	dv1394_remove_host,
	.host_reset =   dv1394_host_reset,
};


/*** KERNEL MODULE HANDLERS ************************************************/

MODULE_AUTHOR("Dan Maas <dmaas@dcine.com>, Dan Dennedy <dan@dennedy.org>");
MODULE_DESCRIPTION("driver for DV input/output on OHCI board");
MODULE_SUPPORTED_DEVICE("dv1394");
MODULE_LICENSE("GPL");

static void __exit dv1394_exit_module(void)
{
	hpsb_unregister_protocol(&dv1394_driver);

	hpsb_unregister_highlevel(&dv1394_highlevel);
	ieee1394_unregister_chardev(IEEE1394_MINOR_BLOCK_DV1394);

#ifdef CONFIG_DEVFS_FS
	dv1394_devfs_del("dv");
#endif
#ifdef CONFIG_PROC_FS
	dv1394_procfs_del("dv");
#endif
}

static int __init dv1394_init_module(void)
{
	if (ieee1394_register_chardev(IEEE1394_MINOR_BLOCK_DV1394,
				      THIS_MODULE, &dv1394_fops)) {
		printk(KERN_ERR "dv1394: unable to register character device\n");
		return -EIO;
	}

#ifdef CONFIG_DEVFS_FS
	if (dv1394_devfs_add_dir("dv", NULL, NULL) < 0) {
		printk(KERN_ERR "dv1394: unable to create /dev/ieee1394/dv\n");
		ieee1394_unregister_chardev(IEEE1394_MINOR_BLOCK_DV1394);
		return -ENOMEM;
	}
#endif

#ifdef CONFIG_PROC_FS
	if (dv1394_procfs_add_dir("dv",NULL,NULL) < 0) {
		printk(KERN_ERR "dv1394: unable to create /proc/bus/ieee1394/dv\n");
		ieee1394_unregister_chardev(IEEE1394_MINOR_BLOCK_DV1394);
#ifdef CONFIG_DEVFS_FS
		dv1394_devfs_del("dv");
#endif
		return -ENOMEM;
	}
#endif

	hpsb_register_highlevel (&dv1394_highlevel);

	hpsb_register_protocol(&dv1394_driver);

	return 0;
}

module_init(dv1394_init_module);
module_exit(dv1394_exit_module);
