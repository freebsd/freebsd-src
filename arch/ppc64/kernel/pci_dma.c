/*
 * pci_dma.c
 * Copyright (C) 2001 Mike Corrigan & Dave Engebretsen, IBM Corporation
 *
 * Dynamic DMA mapping support.
 * 
 * Manages the TCE space assigned to this partition.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/init.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/pci.h>
#include <asm/io.h>
#include <asm/prom.h>
#include <asm/rtas.h>
#include <asm/ppcdebug.h>

#include <asm/iSeries/HvCallXm.h>
#include <asm/iSeries/LparData.h>
#include <asm/pci_dma.h>
#include <asm/pci-bridge.h>
#include <asm/iSeries/iSeries_pci.h>

#include <asm/machdep.h>

#include "pci.h"

/* #define DEBUG_TCE 1   */
/* #define MONITOR_TCE 1 */ /* Turn on to sanity check TCE generation. */


/* Initialize so this guy does not end up in the BSS section.
 * Only used to pass OF initialization data set in prom.c into the main 
 * kernel code -- data ultimately copied into tceTables[].
 */
extern struct _of_tce_table of_tce_table[];

extern struct pci_controller* hose_head;
extern struct pci_controller** hose_tail;
extern struct list_head iSeries_Global_Device_List;

struct TceTable   virtBusVethTceTable;	/* Tce table for virtual ethernet */
struct TceTable   virtBusVioTceTable;	/* Tce table for virtual I/O */

struct iSeries_Device_Node iSeries_veth_dev_node = { LogicalSlot: 0xFF, DevTceTable: &virtBusVethTceTable };
struct iSeries_Device_Node iSeries_vio_dev_node  = { LogicalSlot: 0xFF, DevTceTable: &virtBusVioTceTable };

struct pci_dev    iSeries_veth_dev_st = { sysdata: &iSeries_veth_dev_node };
struct pci_dev    iSeries_vio_dev_st  = { sysdata: &iSeries_vio_dev_node  };

struct pci_dev  * iSeries_veth_dev = &iSeries_veth_dev_st;
struct pci_dev  * iSeries_vio_dev  = &iSeries_vio_dev_st;

/* Device TceTable is stored in Device Node */
/* struct TceTable * tceTables[256]; */	/* Tce tables for 256 busses
					 * Bus 255 is the virtual bus
					 * zero indicates no bus defined
					 */
/* allocates a contiguous range of tces (power-of-2 size) */
static inline long alloc_tce_range(struct TceTable *, 
				   unsigned order );

/* allocates a contiguous range of tces (power-of-2 size)
 * assumes lock already held
 */
static long alloc_tce_range_nolock(struct TceTable *, 
				   unsigned order );

/* frees a contiguous range of tces (power-of-2 size) */
static inline void free_tce_range(struct TceTable *, 
				  long tcenum, 
				  unsigned order );

/* frees a contiguous rnage of tces (power-of-2 size)
 * assumes lock already held
 */
void free_tce_range_nolock(struct TceTable *, 
			   long tcenum, 
			   unsigned order );

/* allocates a range of tces and sets them to the pages  */
dma_addr_t get_tces(struct TceTable *, unsigned order, void *page,
		    unsigned numPages, int direction);

static long test_tce_range( struct TceTable *, 
			    long tcenum, 
			    unsigned order );

static unsigned fill_scatterlist_sg(struct scatterlist *sg, int nents, 
				    dma_addr_t dma_addr, 
				    unsigned long numTces );

static unsigned long num_tces_sg( struct scatterlist *sg, 
				  int nents );
	
static dma_addr_t create_tces_sg( struct TceTable *tbl, 
				  struct scatterlist *sg, 
			 	  int nents, 
				  unsigned numTces,
				  int direction );

static void getTceTableParmsiSeries(struct iSeries_Device_Node* DevNode,
				      struct TceTable *tce_table_parms );

static void getTceTableParmsPSeries( struct pci_controller *phb, 
				     struct device_node *dn,
				     struct TceTable *tce_table_parms );

static void getTceTableParmsPSeriesLP(struct pci_controller *phb,
				    struct device_node *dn,
				    struct TceTable *newTceTable );

static struct TceTable* findHwTceTable(struct TceTable * newTceTable );

void create_pci_bus_tce_table( unsigned long token );

u8 iSeries_Get_Bus( struct pci_dev * dv )
{
	return 0;
}

static inline struct TceTable *get_tce_table(struct pci_dev *dev)
{
	if (!dev)
		dev = ppc64_isabridge_dev;
	if (!dev)
		return NULL;
	if (systemcfg->platform == PLATFORM_ISERIES_LPAR) {
 		return ISERIES_DEVNODE(dev)->DevTceTable;
	} else {
		return PCI_GET_DN(dev)->tce_table;
	}
}

static unsigned long __inline__ count_leading_zeros64( unsigned long x )
{
	unsigned long lz;
	asm("cntlzd %0,%1" : "=r"(lz) : "r"(x));
	return lz;
}

static void tce_build_iSeries(struct TceTable *tbl, long tcenum, 
			       unsigned long uaddr, int direction )
{
	u64 setTceRc;
	union Tce tce;
	
	PPCDBG(PPCDBG_TCE, "build_tce: uaddr = 0x%lx\n", uaddr);
	PPCDBG(PPCDBG_TCE, "\ttcenum = 0x%lx, tbl = 0x%lx, index=%lx\n", 
	       tcenum, tbl, tbl->index);

	tce.wholeTce = 0;
	tce.tceBits.rpn = (virt_to_absolute(uaddr)) >> PAGE_SHIFT;

	/* If for virtual bus */
	if ( tbl->tceType == TCE_VB ) {
		tce.tceBits.valid = 1;
		tce.tceBits.allIo = 1;
		if ( direction != PCI_DMA_TODEVICE )
			tce.tceBits.readWrite = 1;
	} else {
		/* If for PCI bus */
		tce.tceBits.readWrite = 1; // Read allowed 
		if ( direction != PCI_DMA_TODEVICE )
			tce.tceBits.pciWrite = 1;
	}

	setTceRc = HvCallXm_setTce((u64)tbl->index, 
				   (u64)tcenum, 
				   tce.wholeTce );
	if(setTceRc) {
		panic("PCI_DMA: HvCallXm_setTce failed, Rc: 0x%lx\n", setTceRc);
	}
}

static void tce_build_pSeries(struct TceTable *tbl, long tcenum, 
			       unsigned long uaddr, int direction )
{
	union Tce tce;
	union Tce *tce_addr;
	
	PPCDBG(PPCDBG_TCE, "build_tce: uaddr = 0x%lx\n", uaddr);
	PPCDBG(PPCDBG_TCE, "\ttcenum = 0x%lx, tbl = 0x%lx, index=%lx\n", 
	       tcenum, tbl, tbl->index);

	tce.wholeTce = 0;
	tce.tceBits.rpn = (virt_to_absolute(uaddr)) >> PAGE_SHIFT;

	tce.tceBits.readWrite = 1; // Read allowed 
	if ( direction != PCI_DMA_TODEVICE ) tce.tceBits.pciWrite = 1;

	tce_addr = ((union Tce *)tbl->base) + tcenum;
	*tce_addr = (union Tce)tce.wholeTce;

}

/* 
 * Build a TceTable structure.  This contains a multi-level bit map which
 * is used to manage allocation of the tce space.
 */
struct TceTable *build_tce_table(struct TceTable * tbl)
{
	unsigned long bits, bytes, totalBytes;
	unsigned long numBits[NUM_TCE_LEVELS], numBytes[NUM_TCE_LEVELS];
	unsigned i, k, m;
	unsigned char * pos, * p, b;

	PPCDBG(PPCDBG_TCEINIT, "build_tce_table: tbl = 0x%lx\n", tbl);
	spin_lock_init( &(tbl->lock) );
	
	tbl->mlbm.maxLevel = 0;

	/* Compute number of bits and bytes for each level of the
	 * multi-level bit map
	 */ 
	totalBytes = 0;
	bits = tbl->size * (PAGE_SIZE / sizeof( union Tce ));
	
	for ( i=0; i<NUM_TCE_LEVELS; ++i ) {
		bytes = ((bits+63)/64) * 8;
		PPCDBG(PPCDBG_TCEINIT, "build_tce_table: level %d bits=%ld, bytes=%ld\n", i, bits, bytes );
		numBits[i] = bits;
		numBytes[i] = bytes;
		bits /= 2;
		totalBytes += bytes;
	}
	PPCDBG(PPCDBG_TCEINIT, "build_tce_table: totalBytes=%ld\n", totalBytes );
	
	pos = (char *)__get_free_pages( GFP_ATOMIC, get_order( totalBytes ));
 
	if ( pos == NULL ) {
		panic("PCI_DMA: Allocation failed in build_tce_table!\n");
	}

	/* For each level, fill in the pointer to the bit map,
	 * and turn on the last bit in the bit map (if the
	 * number of bits in the map is odd).  The highest
	 * level will get all of its bits turned on.
	 */
	memset( pos, 0, totalBytes );
	for (i=0; i<NUM_TCE_LEVELS; ++i) {
		if ( numBytes[i] ) {
			tbl->mlbm.level[i].map = pos;
			tbl->mlbm.maxLevel = i;

			if ( numBits[i] & 1 ) {
				p = pos + numBytes[i] - 1;
				m = (( numBits[i] % 8) - 1) & 7;
				*p = 0x80 >> m;
				PPCDBG(PPCDBG_TCEINIT, "build_tce_table: level %d last bit %x\n", i, 0x80>>m );
			}
		}
		else
			tbl->mlbm.level[i].map = 0;
		pos += numBytes[i];
		tbl->mlbm.level[i].numBits = numBits[i];
		tbl->mlbm.level[i].numBytes = numBytes[i];
	}

	/* For the highest level, turn on all the bits */
	
	i = tbl->mlbm.maxLevel;
	p = tbl->mlbm.level[i].map;
	m = numBits[i];
	PPCDBG(PPCDBG_TCEINIT, "build_tce_table: highest level (%d) has all bits set\n", i);
	for (k=0; k<numBytes[i]; ++k) {
		if ( m >= 8 ) {
			/* handle full bytes */
			*p++ = 0xff;
			m -= 8;
		}
		else if(m>0) {
			/* handle the last partial byte */
			b = 0x80;
			*p = 0;
			while (m) {
				*p |= b;
				b >>= 1;
				--m;
			}
		} else {
			break;
		}
	}

	return tbl;
}

static inline long alloc_tce_range( struct TceTable *tbl, unsigned order )
{
	long retval;
	unsigned long flags;
	
	/* Lock the tce allocation bitmap */
	spin_lock_irqsave( &(tbl->lock), flags );

	/* Do the actual work */
	retval = alloc_tce_range_nolock( tbl, order );
	
	/* Unlock the tce allocation bitmap */
	spin_unlock_irqrestore( &(tbl->lock), flags );

	return retval;
}

static long alloc_tce_range_nolock( struct TceTable *tbl, unsigned order )
{
	unsigned long numBits, numBytes;
	unsigned long i, bit, block, mask;
	long tcenum;
	u64 * map;

	/* If the order (power of 2 size) requested is larger than our
	 * biggest, indicate failure
	 */
	if(order >= NUM_TCE_LEVELS) {
		/* This can happen if block of TCE's are not found. This code      */
		/*  maybe in a recursive loop looking up the bit map for the range.*/
		panic("PCI_DMA: alloc_tce_range_nolock: invalid order: %d\n",order);
	}
	
	numBits =  tbl->mlbm.level[order].numBits;
	numBytes = tbl->mlbm.level[order].numBytes;
	map =      (u64 *)tbl->mlbm.level[order].map;

	/* Initialize return value to -1 (failure) */
	tcenum = -1;

	/* Loop through the bytes of the bitmap */
	for (i=0; i<numBytes/8; ++i) {
		if ( *map ) {
			/* A free block is found, compute the block
			 * number (of this size)
			 */
			bit = count_leading_zeros64( *map );
			block = (i * 64) + bit;    /* Bit count to free entry */

			/* turn off the bit in the map to indicate
			 * that the block is now in use
			 */
			mask = 0x1UL << (63 - bit);
			*map &= ~mask;

			/* compute the index into our tce table for
			 * the first tce in the block
			 */
			PPCDBG(PPCDBG_TCE, "alloc_tce_range_nolock: allocating block %ld, (byte=%ld, bit=%ld) order %d\n", block, i, bit, order );
			tcenum = block << order;
			return tcenum;
		}
		++map;
	}

#ifdef DEBUG_TCE
	if ( tcenum == -1 ) {
		PPCDBG(PPCDBG_TCE, "alloc_tce_range_nolock: no available blocks of order = %d\n", order );
		if ( order < tbl->mlbm.maxLevel ) {
			PPCDBG(PPCDBG_TCE, "alloc_tce_range_nolock: trying next bigger size\n" );
		}
		else {
			panic("PCI_DMA: alloc_tce_range_nolock: maximum size reached...failing\n");
		}
	}
#endif	
	
	/* If no block of the requested size was found, try the next
	 * size bigger.  If one of those is found, return the second
	 * half of the block to freespace and keep the first half
	 */
	if((tcenum == -1) && (order < (NUM_TCE_LEVELS - 1))) {
		tcenum = alloc_tce_range_nolock( tbl, order+1 );
		if ( tcenum != -1 ) {
			free_tce_range_nolock( tbl, tcenum+(1<<order), order );
		}
	}
	
	/* Return the index of the first tce in the block
	 * (or -1 if we failed)
	 */
	return tcenum;
}

static inline void free_tce_range(struct TceTable *tbl, 
				  long tcenum, unsigned order )
{
	unsigned long flags;

	/* Lock the tce allocation bitmap */
	spin_lock_irqsave( &(tbl->lock), flags );

	/* Do the actual work */
	free_tce_range_nolock( tbl, tcenum, order );
	
	/* Unlock the tce allocation bitmap */
	spin_unlock_irqrestore( &(tbl->lock), flags );

}

void free_tce_range_nolock(struct TceTable *tbl, 
			   long tcenum, unsigned order )
{
	unsigned long block;
	unsigned byte, bit, mask, b;
	unsigned char  * map, * bytep;

	if (order >= NUM_TCE_LEVELS) {
		panic("PCI_DMA: free_tce_range: invalid order: 0x%x\n",order);
		return;
	}

	block = tcenum >> order;

#ifdef MONITOR_TCE
	if ( tcenum != (block << order ) ) {
		printk("PCI_DMA: Free_tce_range: tcenum %lx misaligned for order %x\n",tcenum, order);
		return;
	}
	if ( block >= tbl->mlbm.level[order].numBits ) {
		printk("PCI_DMA: Free_tce_range: tcenum %lx is outside the range of this map (order %x, numBits %lx\n", 
		       tcenum, order, tbl->mlbm.level[order].numBits );
		return;
	}
	if ( test_tce_range( tbl, tcenum, order ) ) {
		printk("PCI_DMA: Freeing range not allocated: tTceTable %p, tcenum %lx, order %x\n",tbl, tcenum, order );
		return;
	}
#endif

	map = tbl->mlbm.level[order].map;
	byte  = block / 8;
	bit   = block % 8;
	mask  = 0x80 >> bit;
	bytep = map + byte;

#ifdef DEBUG_TCE
	PPCDBG(PPCDBG_TCE,"free_tce_range_nolock: freeing block %ld (byte=%d, bit=%d) of order %d\n",
	       block, byte, bit, order);
#endif	

#ifdef MONITOR_TCE
	if ( *bytep & mask ) {
		panic("PCI_DMA: Tce already free: TceTable %p, tcenum %lx, order %x\n",tbl,tcenum,order);
	}
#endif	

	*bytep |= mask;

	/* If there is a higher level in the bit map than this we may be
	 * able to buddy up this block with its partner.
	 *   If this is the highest level we can't buddy up
	 *   If this level has an odd number of bits and
	 *      we are freeing the last block we can't buddy up
	 * Don't buddy up if it's in the first 1/4 of the level
	 */
	if (( order < tbl->mlbm.maxLevel ) &&
	    ( block > (tbl->mlbm.level[order].numBits/4) ) &&
	    (( block < tbl->mlbm.level[order].numBits-1 ) ||
	      ( 0 == ( tbl->mlbm.level[order].numBits & 1)))) {
		/* See if we can buddy up the block we just freed */
		bit  &= 6;		/* get to the first of the buddy bits */
		mask  = 0xc0 >> bit;	/* build two bit mask */
		b     = *bytep & mask;	/* Get the two bits */
		if ( 0 == (b ^ mask) ) { /* If both bits are on */
			/* both of the buddy blocks are free we can combine them */
			*bytep ^= mask;	/* turn off the two bits */
			block = ( byte * 8 ) + bit; /* block of first of buddies */
			tcenum = block << order;
			/* free the buddied block */
			PPCDBG(PPCDBG_TCE, 
			       "free_tce_range: buddying blocks %ld & %ld\n",
			       block, block+1);
			free_tce_range_nolock( tbl, tcenum, order+1 ); 
		}	
	}
}

static long test_tce_range( struct TceTable *tbl, long tcenum, unsigned order )
{
	unsigned long block;
	unsigned byte, bit, mask, b;
	long	retval, retLeft, retRight;
	unsigned char  * map;
	
	map = tbl->mlbm.level[order].map;
	block = tcenum >> order;
	byte = block / 8;		/* Byte within bitmap */
	bit  = block % 8;		/* Bit within byte */
	mask = 0x80 >> bit;		
	b    = (*(map+byte) & mask );	/* 0 if block is allocated, else free */
	if ( b ) 
		retval = 1;		/* 1 == block is free */
	else
		retval = 0;		/* 0 == block is allocated */
	/* Test bits at all levels below this to ensure that all agree */

	if (order) {
		retLeft  = test_tce_range( tbl, tcenum, order-1 );
		retRight = test_tce_range( tbl, tcenum+(1<<(order-1)), order-1 );
		if ( retLeft || retRight ) {
			retval = 2;		
		}
	}

	/* Test bits at all levels above this to ensure that all agree */
	
	return retval;
}

inline dma_addr_t get_tces(struct TceTable *tbl, unsigned order,
			   void *page, unsigned numPages, int direction)
{
	long tcenum;
	unsigned long uaddr;
	unsigned i;
	dma_addr_t retTce = NO_TCE;

	uaddr = (unsigned long)page & PAGE_MASK;
	
	/* Allocate a range of tces */
	tcenum = alloc_tce_range(tbl, order);
	if (tcenum != -1) {
		/* We got the tces we wanted */
		tcenum += tbl->startOffset;	/* Offset into real TCE table */
		retTce = tcenum << PAGE_SHIFT;	/* Set the return dma address */
		/* Setup a tce for each page */
		for (i=0; i<numPages; ++i) {
			ppc_md.tce_build(tbl, tcenum, uaddr, direction); 
			++tcenum;
			uaddr += PAGE_SIZE;
		}
		/* Make sure the update is visible to hardware. 
		   sync required to synchronize the update to 
		   the TCE table with the MMIO that will send
		   the bus address to the IOA */
		__asm__ __volatile__ ("sync" : : : "memory");
	} else {
		panic("get_tces: TCE allocation failed. 0x%p 0x%x\n",
		      tbl, order);
	}

	return retTce; 
}

static void tce_free_one_iSeries( struct TceTable *tbl, long tcenum )
{
	u64 set_tce_rc;
	union Tce tce;
	tce.wholeTce = 0;
	set_tce_rc = HvCallXm_setTce((u64)tbl->index,
				   (u64)tcenum,
				   tce.wholeTce);
	if ( set_tce_rc ) 
		panic("PCI_DMA: HvCallXm_setTce failed, Rc: 0x%lx\n", set_tce_rc);

}

static void tce_free_one_pSeries( struct TceTable *tbl, long tcenum )
{
	union Tce tce;
	union Tce *tce_addr;

	tce.wholeTce = 0;

	tce_addr  = ((union Tce *)tbl->base) + tcenum;
	*tce_addr = (union Tce)tce.wholeTce;

}

void tce_free(struct TceTable *tbl, dma_addr_t dma_addr,
	      unsigned order, unsigned num_pages)
{
	long tcenum, total_tces, free_tce;
	unsigned i;

	total_tces = (tbl->size * (PAGE_SIZE / sizeof(union Tce)));
	
	tcenum = dma_addr >> PAGE_SHIFT;
	free_tce = tcenum - tbl->startOffset;

	if ( ( (free_tce + num_pages) > total_tces ) ||
	     ( tcenum < tbl->startOffset ) ) {
		printk("tce_free: invalid tcenum\n");
		printk("\ttcenum    = 0x%lx\n", tcenum); 
		printk("\tTCE Table = 0x%lx\n", (u64)tbl);
		printk("\tbus#      = 0x%lx\n", (u64)tbl->busNumber );
		printk("\tsize      = 0x%lx\n", (u64)tbl->size);
		printk("\tstartOff  = 0x%lx\n", (u64)tbl->startOffset );
		printk("\tindex     = 0x%lx\n", (u64)tbl->index);
		return;
	}
	
	for (i=0; i<num_pages; ++i) {
		ppc_md.tce_free_one(tbl, tcenum);
		++tcenum;
	}

	/* No sync (to make TCE change visible) is required here.
	   The lwsync when acquiring the lock in free_tce_range
	   is sufficient to synchronize with the bitmap.
	*/

	free_tce_range( tbl, free_tce, order );
}

void __init create_virtual_bus_tce_table(void)
{
	struct TceTable *t;
	struct TceTableManagerCB virtBusTceTableParms;
	u64 absParmsPtr;

	virtBusTceTableParms.busNumber = 255;	/* Bus 255 is the virtual bus */
	virtBusTceTableParms.virtualBusFlag = 0xff; /* Ask for virtual bus */
	
	absParmsPtr = virt_to_absolute( (u64)&virtBusTceTableParms );
	HvCallXm_getTceTableParms( absParmsPtr );
	
	virtBusVethTceTable.size = virtBusTceTableParms.size / 2;
	virtBusVethTceTable.busNumber = virtBusTceTableParms.busNumber;
	virtBusVethTceTable.startOffset = virtBusTceTableParms.startOffset;
	virtBusVethTceTable.index = virtBusTceTableParms.index;
	virtBusVethTceTable.tceType = TCE_VB;

	virtBusVioTceTable.size = virtBusTceTableParms.size - virtBusVethTceTable.size;
	virtBusVioTceTable.busNumber = virtBusTceTableParms.busNumber;
	virtBusVioTceTable.startOffset = virtBusTceTableParms.startOffset +
			virtBusVethTceTable.size * (PAGE_SIZE/sizeof(union Tce));
	virtBusVioTceTable.index = virtBusTceTableParms.index;
	virtBusVioTceTable.tceType = TCE_VB; 

	t = build_tce_table( &virtBusVethTceTable );
	if ( t ) {
		/* tceTables[255] = t; */
		//VirtBusVethTceTable = t;
		printk( "Virtual Bus VETH TCE table built successfully.\n");
		printk( "  TCE table size = %ld entries\n", 
				(unsigned long)t->size*(PAGE_SIZE/sizeof(union Tce)) );
		printk( "  TCE table token = %d\n",
				(unsigned)t->index );
		printk( "  TCE table start entry = 0x%lx\n",
				(unsigned long)t->startOffset );
	}
	else printk( "Virtual Bus VETH TCE table failed.\n");

	t = build_tce_table( &virtBusVioTceTable );
	if ( t ) {
		//VirtBusVioTceTable = t;
		printk( "Virtual Bus VIO TCE table built successfully.\n");
		printk( "  TCE table size = %ld entries\n", 
				(unsigned long)t->size*(PAGE_SIZE/sizeof(union Tce)) );
		printk( "  TCE table token = %d\n",
				(unsigned)t->index );
		printk( "  TCE table start entry = 0x%lx\n",
				(unsigned long)t->startOffset );
	}
	else printk( "Virtual Bus VIO TCE table failed.\n");
}

void create_tce_tables_for_buses(struct list_head *bus_list)
{
	struct pci_controller* phb;
	struct device_node *dn, *first_dn;
	int num_slots, num_slots_ilog2;
	int first_phb = 1;

	for (phb=hose_head;phb;phb=phb->next) {
		first_dn = ((struct device_node *)phb->arch_data)->child;
		/* Carve 2GB into the largest dma_window_size possible */
		for (dn = first_dn, num_slots = 0; dn != NULL; dn = dn->sibling)
			num_slots++;
		num_slots_ilog2 = __ilog2(num_slots);
		if ((1<<num_slots_ilog2) != num_slots)
			num_slots_ilog2++;
		phb->dma_window_size = 1 << (22 - num_slots_ilog2);
		/* Reserve 16MB of DMA space on the first PHB.
		 * We should probably be more careful and use firmware props.
		 * In reality this space is remapped, not lost.  But we don't
		 * want to get that smart to handle it -- too much work.
		 */
		phb->dma_window_base_cur = first_phb ? (1 << 12) : 0;
		first_phb = 0;
		for (dn = first_dn, num_slots = 0; dn != NULL; dn = dn->sibling) {
			create_pci_bus_tce_table((unsigned long)dn);
		}
	}
}

void create_tce_tables_for_busesLP(struct list_head *bus_list)
{
	struct list_head *ln;
	struct pci_bus *bus;
	struct device_node *busdn;
	u32 *dma_window;
	for (ln=bus_list->next; ln != bus_list; ln=ln->next) {
		bus = pci_bus_b(ln);
		busdn = PCI_GET_DN(bus);
		dma_window = (u32 *)get_property(busdn, "ibm,dma-window", 0);
		if (dma_window) {
			/* Busno hasn't been copied yet.
			 * Do it now because getTceTableParmsPSeriesLP needs it.
			 */
			busdn->busno = bus->number;
			create_pci_bus_tce_table((unsigned long)busdn);
		}
		/* look for a window on a bridge even if the PHB had one */
		create_tce_tables_for_busesLP(&bus->children);
	}
}

void create_tce_tables(void) {
	struct pci_dev *dev;
	struct device_node *dn, *mydn;

	if (systemcfg->platform == PLATFORM_PSERIES_LPAR) {
		create_tce_tables_for_busesLP(&pci_root_buses);
	}
	else {
		create_tce_tables_for_buses(&pci_root_buses);
	}
	/* Now copy the tce_table ptr from the bus devices down to every
	 * pci device_node.  This means get_tce_table() won't need to search
	 * up the device tree to find it.
	 */
	pci_for_each_dev(dev) {
		mydn = dn = PCI_GET_DN(dev);
		while (dn && dn->tce_table == NULL)
			dn = dn->parent;
		if (dn) {
			mydn->tce_table = dn->tce_table;
		}
	}
}


/*
 * iSeries token = iSeries_device_Node*
 * pSeries token = pci_controller*
 *
 */
void create_pci_bus_tce_table( unsigned long token ) {
	struct TceTable * newTceTable;

	PPCDBG(PPCDBG_TCE, "Entering create_pci_bus_tce_table.\n");
	PPCDBG(PPCDBG_TCE, "\ttoken = 0x%lx\n", token);

	newTceTable = (struct TceTable *)kmalloc( sizeof(struct TceTable), GFP_KERNEL );

	/*****************************************************************/
 	/* For the iSeries machines, the HvTce Table can be one of three */
 	/* flavors,                                                      */
 	/* - Single bus TCE table,                                       */
 	/* - Tce Table Share between buses,                              */
 	/* - Tce Table per logical slot.                                 */
	/*****************************************************************/
	if(systemcfg->platform == PLATFORM_ISERIES_LPAR) {

		struct iSeries_Device_Node* DevNode = (struct iSeries_Device_Node*)token;
		getTceTableParmsiSeries(DevNode,newTceTable);

		/* Look for existing TCE table for this device.          */
		DevNode->DevTceTable = findHwTceTable(newTceTable );
		if( DevNode->DevTceTable == NULL) {
			DevNode->DevTceTable = build_tce_table( newTceTable );
		}
		else {
		    /* We're using a shared table, free this new one.    */
		    kfree(newTceTable);
		}
		printk("Pci Device 0x%p TceTable: %p\n",DevNode,DevNode->DevTceTable);
 		return;
	}
	/* pSeries Leg */
	else {
		struct device_node *dn;
		struct pci_controller *phb;

		dn = (struct device_node *)token;
		phb = dn->phb;
		if (systemcfg->platform == PLATFORM_PSERIES)
			getTceTableParmsPSeries(phb, dn, newTceTable);
		else
			getTceTableParmsPSeriesLP(phb, dn, newTceTable);

		dn->tce_table  = build_tce_table( newTceTable );
	}
}

/***********************************************************************/
/* This function compares the known Tce tables to find a TceTable that */
/* has already been built for hardware TCEs.                           */
/* Search the complete(all devices) for a TCE table assigned.  If the  */
/* startOffset, index, and size match, then the TCE for this device has*/
/* already been built and it should be shared with this device         */
/***********************************************************************/
static struct TceTable* findHwTceTable(struct TceTable * newTceTable )
{
	struct list_head* Device_Node_Ptr    = iSeries_Global_Device_List.next;
	/* Cache the compare values. */
	u64  startOffset = newTceTable->startOffset;
	u64  index       = newTceTable->index;
	u64  size        = newTceTable->size;

	while(Device_Node_Ptr != &iSeries_Global_Device_List) {
		struct iSeries_Device_Node* CmprNode = (struct iSeries_Device_Node*)Device_Node_Ptr;
		if( CmprNode->DevTceTable != NULL &&
		    CmprNode->DevTceTable->tceType == TCE_PCI) {
			if( CmprNode->DevTceTable->startOffset == startOffset &&
			    CmprNode->DevTceTable->index       == index       &&
			    CmprNode->DevTceTable->size        == size        ) {
				printk("PCI TCE table matches 0x%p \n",CmprNode->DevTceTable);
				return CmprNode->DevTceTable;
			}
		}
		/* Get next Device Node in List             */
		Device_Node_Ptr = Device_Node_Ptr->next;
	}
	return NULL;
}

/***********************************************************************/
/* Call Hv with the architected data structure to get TCE table info.  */
/* info. Put the returned data into the Linux representation of the    */
/* TCE table data.                                                     */
/* The Hardware Tce table comes in three flavors.                      */ 
/* 1. TCE table shared between Buses.                                  */
/* 2. TCE table per Bus.                                               */
/* 3. TCE Table per IOA.                                               */
/***********************************************************************/
static void getTceTableParmsiSeries(struct iSeries_Device_Node* DevNode,
				    struct TceTable* newTceTable )
{
	struct TceTableManagerCB* pciBusTceTableParms = (struct TceTableManagerCB*)kmalloc( sizeof(struct TceTableManagerCB), GFP_KERNEL );
	if(pciBusTceTableParms == NULL) panic("PCI_DMA: TCE Table Allocation failed.");

	memset( (void*)pciBusTceTableParms,0,sizeof(struct TceTableManagerCB) );
	pciBusTceTableParms->busNumber      = ISERIES_BUS(DevNode);
	pciBusTceTableParms->logicalSlot    = DevNode->LogicalSlot;
	pciBusTceTableParms->virtualBusFlag = 0;

	HvCallXm_getTceTableParms( REALADDR(pciBusTceTableParms) );

        /* PciTceTableParms Bus:0x18 Slot:0x04 Start:0x000000 Offset:0x04c000 Size:0x0020 */
	printk("PciTceTableParms Bus:0x%02lx Slot:0x%02x Start:0x%06lx Offset:0x%06lx Size:0x%04lx\n",
	       pciBusTceTableParms->busNumber,
	       pciBusTceTableParms->logicalSlot,
	       pciBusTceTableParms->start,
	       pciBusTceTableParms->startOffset,
	       pciBusTceTableParms->size);

	if(pciBusTceTableParms->size == 0) {
		printk("PCI_DMA: Possible Structure mismatch, 0x%p\n",pciBusTceTableParms);
		panic( "PCI_DMA: pciBusTceTableParms->size is zero, halt here!");
	}

	newTceTable->size        = pciBusTceTableParms->size;
	newTceTable->busNumber   = pciBusTceTableParms->busNumber;
	newTceTable->startOffset = pciBusTceTableParms->startOffset;
	newTceTable->index       = pciBusTceTableParms->index;
	newTceTable->tceType     = TCE_PCI;

	kfree(pciBusTceTableParms);
}

static void getTceTableParmsPSeries(struct pci_controller *phb,
				    struct device_node *dn,
				    struct TceTable *newTceTable ) {
	phandle node;
	unsigned long i;

	node = ((struct device_node *)(phb->arch_data))->node;

	PPCDBG(PPCDBG_TCEINIT, "getTceTableParms: start\n"); 
	PPCDBG(PPCDBG_TCEINIT, "\tof_tce_table = 0x%lx\n", of_tce_table); 
	PPCDBG(PPCDBG_TCEINIT, "\tphb          = 0x%lx\n", phb); 
	PPCDBG(PPCDBG_TCEINIT, "\tdn           = 0x%lx\n", dn); 
	PPCDBG(PPCDBG_TCEINIT, "\tdn->name     = %s\n", dn->name); 
	PPCDBG(PPCDBG_TCEINIT, "\tdn->full_name= %s\n", dn->full_name); 
	PPCDBG(PPCDBG_TCEINIT, "\tnewTceTable  = 0x%lx\n", newTceTable); 
	PPCDBG(PPCDBG_TCEINIT, "\tdma_window_size = 0x%lx\n", phb->dma_window_size); 

	i = 0;
	while(of_tce_table[i].node) {
		PPCDBG(PPCDBG_TCEINIT, "\tof_tce_table[%d].node = 0x%lx\n", 
		       i, of_tce_table[i].node);
		PPCDBG(PPCDBG_TCEINIT, "\tof_tce_table[%d].base = 0x%lx\n", 
		       i, of_tce_table[i].base);
		PPCDBG(PPCDBG_TCEINIT, "\tof_tce_table[%d].size = 0x%lx\n", 
		       i, of_tce_table[i].size >> PAGE_SHIFT);
		PPCDBG(PPCDBG_TCEINIT, "\tphb->arch_data->node = 0x%lx\n", 
		       node);

		if(of_tce_table[i].node == node) {
			memset((void *)of_tce_table[i].base, 
			       0, of_tce_table[i].size);
			newTceTable->busNumber = phb->bus->number;

			/* Units of tce entries.                        */
			newTceTable->startOffset = phb->dma_window_base_cur;

			/* Adjust the current table offset to the next  */
			/* region.  Measured in TCE entries. Force an   */
			/* alignment to the size alloted per IOA. This  */
			/* makes it easier to remove the 1st 16MB.      */
			phb->dma_window_base_cur += (phb->dma_window_size>>3);
			phb->dma_window_base_cur &= 
				~((phb->dma_window_size>>3)-1);

			/* Set the tce table size - measured in units   */
			/* of pages of tce table.                       */
			newTceTable->size = ((phb->dma_window_base_cur -
					      newTceTable->startOffset) << 3)
					      >> PAGE_SHIFT;

			/* Test if we are going over 2GB of DMA space.  */
			if(phb->dma_window_base_cur > (1 << 19)) { 
				panic("PCI_DMA: Unexpected number of IOAs under this PHB.\n"); 
			}

			newTceTable->base = of_tce_table[i].base;
			newTceTable->index = 0;
			
			PPCDBG(PPCDBG_TCEINIT, 
			       "\tnewTceTable->base        = 0x%lx\n",
			       newTceTable->base);
			PPCDBG(PPCDBG_TCEINIT, 
			       "\tnewTceTable->startOffset = 0x%lx"
			       "(# tce entries)\n", 
			       newTceTable->startOffset);
			PPCDBG(PPCDBG_TCEINIT, 
			       "\tnewTceTable->size        = 0x%lx"
			       "(# pages of tce table)\n", 
			       newTceTable->size);
		}
		i++;
	}
}

/*
 * getTceTableParmsPSeriesLP
 *
 * Function: On pSeries LPAR systems, return TCE table info, given a pci bus.
 *
 * ToDo: properly interpret the ibm,dma-window property.  The definition is:
 *	logical-bus-number	(1 word)
 *	phys-address		(#address-cells words)
 *	size			(#cell-size words)
 *
 * Currently we hard code these sizes (more or less).
 */
static void getTceTableParmsPSeriesLP(struct pci_controller *phb,
				    struct device_node *dn,
				    struct TceTable *newTceTable ) {
	u32 *dma_window = (u32 *)get_property(dn, "ibm,dma-window", 0);
	if (!dma_window) {
		panic("PCI_DMA: getTceTableParmsPSeriesLP: device %s has no ibm,dma-window property!\n", dn->full_name);
	}

	newTceTable->busNumber = dn->busno;
	newTceTable->size = (((((unsigned long)dma_window[4] << 32) | (unsigned long)dma_window[5]) >> PAGE_SHIFT) << 3) >> PAGE_SHIFT;
	newTceTable->startOffset = ((((unsigned long)dma_window[2] << 32) | (unsigned long)dma_window[3]) >> 12);
	newTceTable->base = 0;
	newTceTable->index = dma_window[0];
	PPCDBG(PPCDBG_TCEINIT, "getTceTableParmsPSeriesLP for bus 0x%lx:\n", dn->busno);
	PPCDBG(PPCDBG_TCEINIT, "\tDevice = %s\n", dn->full_name);
	PPCDBG(PPCDBG_TCEINIT, "\tnewTceTable->index       = 0x%lx\n", newTceTable->index);
	PPCDBG(PPCDBG_TCEINIT, "\tnewTceTable->startOffset = 0x%lx\n", newTceTable->startOffset);
	PPCDBG(PPCDBG_TCEINIT, "\tnewTceTable->size        = 0x%lx\n", newTceTable->size);
}

/* Allocates a contiguous real buffer and creates TCEs over it.
 * Returns the virtual address of the buffer and sets dma_handle
 * to the dma address (tce) of the first page.
 */
void *pci_alloc_consistent(struct pci_dev *hwdev, size_t size,
			   dma_addr_t *dma_handle)
{
	struct TceTable * tbl;
	void *ret = NULL;
	unsigned order, nPages;
	dma_addr_t tce;

	PPCDBG(PPCDBG_TCE, "pci_alloc_consistent:\n");
	PPCDBG(PPCDBG_TCE, "\thwdev      = 0x%16.16lx\n", hwdev);
	PPCDBG(PPCDBG_TCE, "\tsize       = 0x%16.16lx\n", size);
	PPCDBG(PPCDBG_TCE, "\tdma_handle = 0x%16.16lx\n", dma_handle);	

	size = PAGE_ALIGN(size);
	order = get_order(size);
	nPages = 1 << order;

 	/* Client asked for way to much space.  This is checked later anyway */
	/* It is easier to debug here for the drivers than in the tce tables.*/
 	if (order >= NUM_TCE_LEVELS) {
 		printk("PCI_DMA: pci_alloc_consistent size too large: 0x%lx\n",
		       size);
 		return (void *)NULL;
 	}

	tbl = get_tce_table(hwdev); 

	if (tbl) {
		/* Alloc enough pages (and possibly more) */
		ret = (void *)__get_free_pages( GFP_ATOMIC, order );
		if (ret) {
			/* Page allocation succeeded */
			memset(ret, 0, nPages << PAGE_SHIFT);
			/* Set up tces to cover the allocated range */
			tce = get_tces( tbl, order, ret, nPages, PCI_DMA_BIDIRECTIONAL );
			if (tce == NO_TCE) {
				free_pages( (unsigned long)ret, order );
				ret = NULL;
			} else {
				*dma_handle = tce;
			}
		} else {
			printk("pci_alloc_consistent: __get_free_pages failed for order = %d\n", order);
		}
	} else {
		panic("pci_alloc_consistent: unable to find TCE table\n");
	}

	PPCDBG(PPCDBG_TCE, "\tpci_alloc_consistent: dma_handle = 0x%16.16lx\n", *dma_handle);	
	PPCDBG(PPCDBG_TCE, "\tpci_alloc_consistent: return     = 0x%16.16lx\n", ret);	
	return ret;
}

void pci_free_consistent(struct pci_dev *hwdev, size_t size,
			 void *vaddr, dma_addr_t dma_handle)
{
	struct TceTable * tbl;
	unsigned order, nPages;
	
	PPCDBG(PPCDBG_TCE, "pci_free_consistent:\n");
	PPCDBG(PPCDBG_TCE, "\thwdev = 0x%16.16lx, size = 0x%16.16lx, dma_handle = 0x%16.16lx, vaddr = 0x%16.16lx\n", hwdev, size, dma_handle, vaddr);	

	size = PAGE_ALIGN(size);
	order = get_order(size);
	nPages = 1 << order;

 	/* Client asked for way to much space.  This is checked later anyway */
	/* It is easier to debug here for the drivers than in the tce tables.*/
 	if(order >= NUM_TCE_LEVELS) {
 		printk("PCI_DMA: pci_free_consistent size too large: 0x%lx \n",size);
 		return;
 	}
	
	tbl = get_tce_table(hwdev); 

	if ( tbl ) {
		tce_free(tbl, dma_handle, order, nPages);
		free_pages( (unsigned long)vaddr, order );
	}
}

/* Creates TCEs for a user provided buffer.  The user buffer must be 
 * contiguous real kernel storage (not vmalloc).  The address of the buffer
 * passed here is the kernel (virtual) address of the buffer.  The buffer
 * need not be page aligned, the dma_addr_t returned will point to the same
 * byte within the page as vaddr.
 */
dma_addr_t pci_map_single(struct pci_dev *hwdev, void *vaddr, 
			  size_t size, int direction )
{
	struct TceTable * tbl;
	dma_addr_t dma_handle = NO_TCE;
	unsigned long uaddr;
	unsigned order, nPages;

	PPCDBG(PPCDBG_TCE, "pci_map_single:\n");
	PPCDBG(PPCDBG_TCE, "\thwdev = 0x%16.16lx, size = 0x%16.16lx, direction = 0x%16.16lx, vaddr = 0x%16.16lx\n", hwdev, size, direction, vaddr);	
	if (direction == PCI_DMA_NONE)
		BUG();
	
	uaddr = (unsigned long)vaddr;
	nPages = PAGE_ALIGN( uaddr + size ) - ( uaddr & PAGE_MASK );
	order = get_order( nPages & PAGE_MASK );
	nPages >>= PAGE_SHIFT;
	
 	/* Client asked for way to much space.  This is checked later anyway */
	/* It is easier to debug here for the drivers than in the tce tables.*/
 	if(order >= NUM_TCE_LEVELS) {
		panic("PCI_DMA: pci_map_single size too large: 0x%lx \n", size);
		return dma_handle;
 	}

	tbl = get_tce_table(hwdev); 

	if (tbl) {
		/* get_tces panics if there are no entries available */
		dma_handle = get_tces( tbl, order, vaddr, nPages, direction );
		dma_handle |= ( uaddr & ~PAGE_MASK );
	} else {
		panic("PCI_DMA: Unable to find TCE table.\n");
	}

	return dma_handle;
}

void pci_unmap_single( struct pci_dev *hwdev, dma_addr_t dma_handle, size_t size, int direction )
{
	struct TceTable * tbl;
	unsigned order, nPages;
	
	PPCDBG(PPCDBG_TCE, "pci_unmap_single:\n");
	PPCDBG(PPCDBG_TCE, "\thwdev = 0x%16.16lx, size = 0x%16.16lx, direction = 0x%16.16lx, dma_handle = 0x%16.16lx\n", hwdev, size, direction, dma_handle);	
	if ( direction == PCI_DMA_NONE )
		BUG();

	nPages = PAGE_ALIGN( dma_handle + size ) - ( dma_handle & PAGE_MASK );
	order = get_order( nPages & PAGE_MASK );
	nPages >>= PAGE_SHIFT;

 	/* Client asked for way to much space.  This is checked later anyway */
	/* It is easier to debug here for the drivers than in the tce tables.*/
 	if(order >= NUM_TCE_LEVELS) {
 		printk("PCI_DMA: pci_unmap_single size too large: 0x%lx \n",size);
 		return;
 	}
	
	tbl = get_tce_table(hwdev); 

	if ( tbl ) 
		tce_free(tbl, dma_handle, order, nPages);

}

/* Figure out how many TCEs are actually going to be required
 * to map this scatterlist.  This code is not optimal.  It 
 * takes into account the case where entry n ends in the same
 * page in which entry n+1 starts.  It does not handle the 
 * general case of entry n ending in the same page in which 
 * entry m starts.   
 */
static unsigned long num_tces_sg( struct scatterlist *sg, int nents )
{
	unsigned long nTces, numPages, startPage, endPage, prevEndPage;
	unsigned i;
	void *address;

	prevEndPage = 0;
	nTces = 0;

	for (i=0; i<nents; ++i) {
		/* Compute the starting page number and
		 * the ending page number for this entry
		 */
		address = sg->address ? sg->address :
			(page_address(sg->page) + sg->offset);
		startPage = (unsigned long)address >> PAGE_SHIFT;
		endPage = ((unsigned long)address + sg->length - 1) >> PAGE_SHIFT;
		numPages = endPage - startPage + 1;
		/* Simple optimization: if the previous entry ended
		 * in the same page in which this entry starts
		 * then we can reduce the required pages by one.
		 * This matches assumptions in fill_scatterlist_sg and
		 * create_tces_sg
		 */
		if ( startPage == prevEndPage )
			--numPages;
		nTces += numPages;
		prevEndPage = endPage;
		sg++;
	}
	return nTces;
}

/* Fill in the dma data in the scatterlist
 * return the number of dma sg entries created
 */
static unsigned fill_scatterlist_sg( struct scatterlist *sg, int nents, 
				 dma_addr_t dma_addr , unsigned long numTces)
{
	struct scatterlist *dma_sg;
	u32 cur_start_dma;
	unsigned long cur_len_dma, cur_end_virt, uaddr;
	unsigned num_dma_ents;
	void *address;

	dma_sg = sg;
	num_dma_ents = 1;

	/* Process the first sg entry */
	address = sg->address ? sg->address :
		(page_address(sg->page) + sg->offset);
	cur_start_dma = dma_addr + ((unsigned long)address & (~PAGE_MASK));
	cur_len_dma = sg->length;
	/* cur_end_virt holds the address of the byte immediately after the
	 * end of the current buffer.
	 */
	cur_end_virt = (unsigned long)address + cur_len_dma;
	/* Later code assumes that unused sg->dma_address and sg->dma_length
	 * fields will be zero.  Other archs seem to assume that the user
	 * (device driver) guarantees that...I don't want to depend on that
	 */
	sg->dma_address = sg->dma_length = 0;
	
	/* Process the rest of the sg entries */
	while (--nents) {
		++sg;
		/* Clear possibly unused fields. Note: sg >= dma_sg so
		 * this can't be clearing a field we've already set
		 */
		sg->dma_address = sg->dma_length = 0;

		/* Check if it is possible to make this next entry
		 * contiguous (in dma space) with the previous entry.
		 */
		
		/* The entries can be contiguous in dma space if
		 * the previous entry ends immediately before the
		 * start of the current entry (in virtual space)
		 * or if the previous entry ends at a page boundary
		 * and the current entry starts at a page boundary.
		 */
		address = sg->address ? sg->address :
			(page_address(sg->page) + sg->offset);
		uaddr = (unsigned long)address;
		if ( ( uaddr != cur_end_virt ) &&
		     ( ( ( uaddr | cur_end_virt ) & (~PAGE_MASK) ) ||
		       ( ( uaddr & PAGE_MASK ) == ( ( cur_end_virt-1 ) & PAGE_MASK ) ) ) ) {
			/* This entry can not be contiguous in dma space.
			 * save the previous dma entry and start a new one
			 */
			dma_sg->dma_address = cur_start_dma;
			dma_sg->dma_length  = cur_len_dma;

			++dma_sg;
			++num_dma_ents;
			
			cur_start_dma += cur_len_dma-1;
			/* If the previous entry ends and this entry starts
			 * in the same page then they share a tce.  In that
			 * case don't bump cur_start_dma to the next page 
			 * in dma space.  This matches assumptions made in
			 * num_tces_sg and create_tces_sg.
			 */
			if ((uaddr & PAGE_MASK) == ((cur_end_virt-1) & PAGE_MASK))
				cur_start_dma &= PAGE_MASK;
			else
				cur_start_dma = PAGE_ALIGN(cur_start_dma+1);
			cur_start_dma += ( uaddr & (~PAGE_MASK) );
			cur_len_dma = 0;
		}
		/* Accumulate the length of this entry for the next 
		 * dma entry
		 */
		cur_len_dma += sg->length;
		cur_end_virt = uaddr + sg->length;
	}
	/* Fill in the last dma entry */
	dma_sg->dma_address = cur_start_dma;
	dma_sg->dma_length  = cur_len_dma;

	if ((((cur_start_dma +cur_len_dma - 1)>> PAGE_SHIFT) - (dma_addr >> PAGE_SHIFT) + 1) != numTces)
	  {
	    PPCDBG(PPCDBG_TCE, "fill_scatterlist_sg: numTces %ld, used tces %d\n",
		   numTces,
		   (unsigned)(((cur_start_dma + cur_len_dma - 1) >> PAGE_SHIFT) - (dma_addr >> PAGE_SHIFT) + 1));
	  }
	

	return num_dma_ents;
}

/* Call the hypervisor to create the TCE entries.
 * return the number of TCEs created
 */
static dma_addr_t create_tces_sg(struct TceTable *tbl, struct scatterlist *sg, 
				 int nents, unsigned numTces, int direction)
{
	unsigned order, i, j;
	unsigned long startPage, endPage, prevEndPage, numPages, uaddr;
	long tcenum, starttcenum;
	dma_addr_t dmaAddr;
	void *address;

	dmaAddr = NO_TCE;

	order = get_order( numTces << PAGE_SHIFT );
 	/* Client asked for way to much space.  This is checked later anyway */
	/* It is easier to debug here for the drivers than in the tce tables.*/
 	if(order >= NUM_TCE_LEVELS) {
		printk("PCI_DMA: create_tces_sg size too large: 0x%llx \n",(numTces << PAGE_SHIFT));
		panic("numTces is off");
 		return NO_TCE;
 	}

	/* allocate a block of tces */
	tcenum = alloc_tce_range(tbl, order);
	if (tcenum != -1) {
		tcenum += tbl->startOffset;
		starttcenum = tcenum;
		dmaAddr = tcenum << PAGE_SHIFT;
		prevEndPage = 0;
		for (j=0; j<nents; ++j) {
			address = sg->address ? sg->address :
				(page_address(sg->page) + sg->offset);
			startPage = (unsigned long)address >> PAGE_SHIFT;
			endPage = ((unsigned long)address + sg->length - 1) >> PAGE_SHIFT;
			numPages = endPage - startPage + 1;
			
			uaddr = (unsigned long)address;

			/* If the previous entry ended in the same page that
			 * the current page starts then they share that
			 * tce and we reduce the number of tces we need
			 * by one.  This matches assumptions made in
			 * num_tces_sg and fill_scatterlist_sg
			 */
			if ( startPage == prevEndPage ) {
				--numPages;
				uaddr += PAGE_SIZE;
			}
			
			for (i=0; i<numPages; ++i) {
			  ppc_md.tce_build(tbl, tcenum, uaddr, direction); 
			  ++tcenum;
			  uaddr += PAGE_SIZE;
			}
		
			prevEndPage = endPage;
			sg++;
		}
		/* Make sure the update is visible to hardware. 
		   sync required to synchronize the update to 
		   the TCE table with the MMIO that will send
		   the bus address to the IOA */
		__asm__ __volatile__ ("sync" : : : "memory");

		if ((tcenum - starttcenum) != numTces)
	    		PPCDBG(PPCDBG_TCE, "create_tces_sg: numTces %d, tces used %d\n",
		   		numTces, (unsigned)(tcenum - starttcenum));

	} else {
		panic("PCI_DMA: TCE allocation failure in create_tces_sg. 0x%p 0x%x\n",
		      tbl, order);
	}

	return dmaAddr;
}

int pci_map_sg( struct pci_dev *hwdev, struct scatterlist *sg, int nents, int direction )
{
	struct TceTable * tbl;
	unsigned numTces;
	int num_dma = 0;
	dma_addr_t dma_handle;
	void *address;

	/* Fast path for a single entry scatterlist */
	if ( nents == 1 ) {
 		address = sg->address ? sg->address :
 			(page_address(sg->page) + sg->offset);
 		sg->dma_address = pci_map_single( hwdev, address,
 						  sg->length, direction );
		sg->dma_length = sg->length;
		return 1;
	}
	
	if (direction == PCI_DMA_NONE)
		BUG();
	
	tbl = get_tce_table(hwdev); 

	if (tbl) {
		/* Compute the number of tces required */
		numTces = num_tces_sg(sg, nents);
		/* Create the tces and get the dma address */ 
		dma_handle = create_tces_sg( tbl, sg, nents, numTces, direction );

		if(dma_handle == NO_TCE) return 0;

		/* Fill in the dma scatterlist */
		num_dma = fill_scatterlist_sg( sg, nents, dma_handle, numTces );
	} else {
		panic("pci_map_sg: unable to find TCE table\n");
	}

	return num_dma;
}

void pci_unmap_sg( struct pci_dev *hwdev, struct scatterlist *sg, int nelms, int direction )
{
	struct TceTable * tbl;
	unsigned order, numTces, i;
	dma_addr_t dma_end_page, dma_start_page;
	
	PPCDBG(PPCDBG_TCE, "pci_unmap_sg:\n");
	PPCDBG(PPCDBG_TCE, "\thwdev = 0x%16.16lx, sg = 0x%16.16lx, direction = 0x%16.16lx, nelms = 0x%16.16lx\n", hwdev, sg, direction, nelms);	

	if ( direction == PCI_DMA_NONE || nelms == 0 )
		BUG();

	dma_start_page = sg->dma_address & PAGE_MASK;
 	dma_end_page   = 0;
	for ( i=nelms; i>0; --i ) {
		unsigned k = i - 1;
		if ( sg[k].dma_length ) {
			dma_end_page = ( sg[k].dma_address +
					 sg[k].dma_length - 1 ) & PAGE_MASK;
			break;
		}
	}

	numTces = ((dma_end_page - dma_start_page ) >> PAGE_SHIFT) + 1;
	order = get_order( numTces << PAGE_SHIFT );

 	/* Client asked for way to much space.  This is checked later anyway */
	/* It is easier to debug here for the drivers than in the tce tables.*/
 	if(order >= NUM_TCE_LEVELS) {
		printk("PCI_DMA: dma_start_page:0x%lx  dma_end_page:0x%lx\n",dma_start_page,dma_end_page);
		printk("PCI_DMA: pci_unmap_sg size too large: 0x%x \n",(numTces << PAGE_SHIFT));
 		return;
 	}
	
	tbl = get_tce_table(hwdev); 

	if ( tbl ) 
		tce_free( tbl, dma_start_page, order, numTces );

}

/*
 * phb_tce_table_init
 * 
 * Function: Display TCE config registers.  Could be easily changed
 *           to initialize the hardware to use TCEs.
 */
unsigned long phb_tce_table_init(struct pci_controller *phb) {
	unsigned int r, cfg_rw, i;	
	unsigned long r64;	
	phandle node;

	PPCDBG(PPCDBG_TCE, "phb_tce_table_init: start.\n"); 

	node = ((struct device_node *)(phb->arch_data))->node;

	PPCDBG(PPCDBG_TCEINIT, "\tphb            = 0x%lx\n", phb); 
	PPCDBG(PPCDBG_TCEINIT, "\tphb->type      = 0x%lx\n", phb->type); 
	PPCDBG(PPCDBG_TCEINIT, "\tphb->phb_regs  = 0x%lx\n", phb->phb_regs); 
	PPCDBG(PPCDBG_TCEINIT, "\tphb->chip_regs = 0x%lx\n", phb->chip_regs); 
	PPCDBG(PPCDBG_TCEINIT, "\tphb: node      = 0x%lx\n", node);
	PPCDBG(PPCDBG_TCEINIT, "\tphb->arch_data = 0x%lx\n", phb->arch_data); 

	i = 0;
	while(of_tce_table[i].node) {
		if(of_tce_table[i].node == node) {
			if(phb->type == phb_type_python) {
				r = *(((unsigned int *)phb->phb_regs) + (0xf10>>2)); 
				PPCDBG(PPCDBG_TCEINIT, "\tTAR(low)    = 0x%x\n", r);
				r = *(((unsigned int *)phb->phb_regs) + (0xf00>>2)); 
				PPCDBG(PPCDBG_TCEINIT, "\tTAR(high)   = 0x%x\n", r);
				r = *(((unsigned int *)phb->phb_regs) + (0xfd0>>2)); 
				PPCDBG(PPCDBG_TCEINIT, "\tPHB cfg(rw) = 0x%x\n", r);
				break;
			} else if(phb->type == phb_type_speedwagon) {
				r64 = *(((unsigned long *)phb->chip_regs) + 
					(0x800>>3)); 
				PPCDBG(PPCDBG_TCEINIT, "\tNCFG    = 0x%lx\n", r64);
				r64 = *(((unsigned long *)phb->chip_regs) + 
					(0x580>>3)); 
				PPCDBG(PPCDBG_TCEINIT, "\tTAR0    = 0x%lx\n", r64);
				r64 = *(((unsigned long *)phb->chip_regs) + 
					(0x588>>3)); 
				PPCDBG(PPCDBG_TCEINIT, "\tTAR1    = 0x%lx\n", r64);
				r64 = *(((unsigned long *)phb->chip_regs) + 
					(0x590>>3)); 
				PPCDBG(PPCDBG_TCEINIT, "\tTAR2    = 0x%lx\n", r64);
				r64 = *(((unsigned long *)phb->chip_regs) + 
					(0x598>>3)); 
				PPCDBG(PPCDBG_TCEINIT, "\tTAR3    = 0x%lx\n", r64);
				cfg_rw = *(((unsigned int *)phb->chip_regs) + 
					   ((0x160 +
					     (((phb->local_number)+8)<<12))>>2)); 
				PPCDBG(PPCDBG_TCEINIT, "\tcfg_rw = 0x%x\n", cfg_rw);
			}
		}
		i++;
	}

	PPCDBG(PPCDBG_TCEINIT, "phb_tce_table_init: done\n"); 

	return(0); 
}

/* These are called very early. */
void tce_init_pSeries(void)
{
	ppc_md.tce_build = tce_build_pSeries;
	ppc_md.tce_free_one = tce_free_one_pSeries;
}

void tce_init_iSeries(void)
{
	ppc_md.tce_build = tce_build_iSeries;
	ppc_md.tce_free_one = tce_free_one_iSeries;
}
