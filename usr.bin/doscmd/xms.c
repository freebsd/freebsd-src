/*-
 * Copyright (c) 1997 Helmut Wirth <hfwirth@ping.at>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, witout modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.bin/doscmd/xms.c,v 1.6 1999/09/29 20:09:19 marcel Exp $
 */

/*
 * XMS memory manmagement 
 *
 * To emulate DOS extended memory (EMM) we use an implementation of
 * HIMEM.SYS driver capabitlities, according to the XMS 3.0 Spec.
 * The actual memory allocated via XMS calls from DOS is allocated
 * via malloc by the emulator. Maximum memory allocation is configureable.
 *
 * Credits to:
 *	The original author of this file, some parts are still here
 *	Linux dosemu programmers. I looked into their code.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <unistd.h>

#include "doscmd.h"
#include "xms.h"


/* Extended memory handle management */

static XMS_handle xms_hand[NUM_HANDLES];
int num_free_handle = NUM_HANDLES;

/* This is planned to be selectable from .doscmdrc */
u_long xms_maxsize = DEFAULT_EMM_SIZE;
static u_long xms_free_mem;
static u_long xms_used_mem;
static u_char vec_grabbed;

/* Address entry for zero size allocated handles */
#define XMS_NULL_ALLOC 0xffffffff

/* High memory area (HMA) management */
static u_char HMA_allocated = 0;
static short  HMA_a20 = -1;
static int    HMA_fd_off, HMA_fd_on;

/* high memory mapfiles */
static char memfile[] = "/tmp/doscmd.XXXXXX";

/* Upper memory block (UMB) management */
UMB_block *UMB_freelist = NULL;
UMB_block *UMB_alloclist = NULL;

/* Calls to emulator */
u_long xms_vector;
static u_char xms_trampoline[] = {
    0xeb,	/* JMP 5 */
    0x03,
    0x90,	/* NOP */
    0x90,	/* NOP */
    0x90,	/* NOP */
    0xf4,	/* HLT */
    0xcb,	/* RETF */
};

/* Local prototypes */
static void xms_entry(regcontext_t *REGS);
static UMB_block *create_block(u_long addr, u_long size);
static void add_block(UMB_block **listp, UMB_block *blk);
static void merge_blocks();

/* Init the entire module */
void
xms_init(void)
{
    int i;

    /* Initialize handle table: xms_handle.addr == 0 means free */
    bzero((void *)xms_hand, sizeof(XMS_handle) * NUM_HANDLES);	       
    xms_free_mem = xms_maxsize;
    xms_used_mem = 0;
    vec_grabbed = 0;
    HMA_allocated = 0;
    /* Initialize UMB blocks */
    /* 0xD0000 to 0xDffff */
    add_block(&UMB_freelist, create_block(0xd0000, 64*1024));
    /*XXX check for EMS emulation, when it is done! */
    /* 0xE0000 to 0xEffff */

/* This is used as window for EMS, will be configurable ! */
/*    add_block(&UMB_freelist, create_block(0xe0000, 64*1024)); */

    merge_blocks();

    xms_vector = insert_generic_trampoline(
	sizeof(xms_trampoline), xms_trampoline);
    register_callback(xms_vector + 5, xms_entry, "xms");
}

/*
 * UMB management routines: UMBs normally lie between 0xd0000 and
 * 0xefff0 in VM86 memory space and are accessible for all DOS applictions.
 * We could enable more space, but with the emulator we do not
 * need many drivers, so I think 2 * 64kB will suffice. If EMS emulation
 * exists, a 64kB segment (0xe0000 - 0xeffff for example) is needed for
 * the EMS mapping, in this case we have 64kB UMB space. This is more than 
 * many PCs are able to do.
 * This emulation does only the management for the memory, the memory
 * is present and read/write/excutable for VM86 applications.
 */

/* Add a block to a list, maintain ascending start address order */

static void
add_block(UMB_block **listp, UMB_block *blk)
{
    UMB_block *bp, *obp;

    /* No blocks there, attach the new block to the head */
    if (*listp == NULL) {
	*listp = blk;
        blk->next = NULL;
    } else {
	/* Insert at the start */
	bp = obp = *listp;
	if (blk->addr < bp->addr) {
	    blk->next = *listp; 
	    *listp = blk;
	    return;
	}
	/* Not at the start, insert into the list */
	for (; bp != NULL; bp = bp->next) {
	    if (blk->addr > bp->addr) {
		obp = bp;
		continue;
	    } else {
		obp->next = blk;
		blk->next = bp;
		return;
	    }
	}
	/* Append to the end of the list */
	obp->next = blk;
	blk->next = NULL;
    }
    return;
}

/* Find a block with address addr in the alloc list */
static UMB_block *
find_allocated_block(u_long addr)
{
    UMB_block *bp;

    if (UMB_alloclist == NULL)
	return NULL;

    for (bp = UMB_alloclist; bp != NULL; bp = bp->next)
	if (bp->addr == addr)
	    return bp;
    return NULL;
}

/* Remove a block blk from a list, the block must exist on the list */
static void 
remove_block(UMB_block **listp, UMB_block *blk)
{
    UMB_block *bp;

    if (*listp == NULL)
	goto faterr;

    if (*listp == blk) {
	*listp = (*listp)->next;
	return;
    }
    bp = *listp;
    do {
	if (bp->next == blk) {
	    bp->next = bp->next->next;
	    return;
	}
	bp = bp->next;
    } while(bp != NULL);
faterr:
    fatal("XMS: UMB remove_block did not find block\n");
}

/* Try to merge neighbouring blocks in the free list */
static void
merge_blocks()
{
    UMB_block *bp;
    u_long endaddr;

    if (UMB_freelist == NULL)
	return;
    bp = UMB_freelist;
    do {
	endaddr = bp->addr + bp->size;
	if (bp->next != NULL && endaddr == bp->next->addr) {
	    /* Merge the current and the next block */
	    UMB_block *mergebp = bp->next;
	    bp->size += mergebp->size;
	    bp->next = mergebp->next;
	    free(mergebp);
	} else {
	    /* Goto next block */
	    bp = bp->next; 
        }
    } while (bp != NULL);
}

/* Try to find a free block of size exactly siz */ 
static UMB_block *
find_exact_block(u_long siz)
{
    UMB_block *bp;

    if (UMB_freelist == NULL)
	return NULL;

    for (bp = UMB_freelist; bp != NULL; bp = bp->next)
	if (bp->size == siz)
	    return bp;
    return NULL;
}

/* Try to find a block with a size bigger than requested. If there is
 * no such block, return the block with the biggest size. If there is
 * no free block at all, return NULL
 */
static UMB_block *
find_block(u_long siz)
{
    UMB_block *bp;
    UMB_block *biggest = NULL;

    if (UMB_freelist == NULL)
	return NULL;

    for (bp = UMB_freelist; bp != NULL; bp = bp->next) {
	if (bp->size > siz)
	    return bp;
	if (biggest == NULL) {
	    biggest = bp;
	    continue;
	}
	if (biggest->size < bp->size)
	    biggest = bp;
    }
    return biggest;
}

/* Create a block structure, memory is allocated. The structure lives
 * until the block is merged into another block, then it is freed */
static UMB_block *
create_block(u_long addr, u_long size)
{
    UMB_block *blk;

    if ((blk = malloc(sizeof(UMB_block))) == NULL)
	fatal ("XMS: Cannot allocate UMB structure\n");
    blk->addr = addr;
    blk->size = size;
    blk->next = NULL;
    return blk;
} 


/*
 * initHMA(): The first 64kB of memory are mapped from 1MB (0x100000)
 * again to emulate the address wrap around of the 808x. The HMA area
 * is a cheap trick, usable only with 80386 and higher. The 80[345..]86
 * does not have this address wrap around. If more than 1MB is installed
 * the processor can address more than 1MB: load 0xFFFF to the segment
 * register and using the full offset of 0xffff the resulting highest
 * address is (0xffff << 4) + 0xffff = 0x10ffef. Nearly 64kB are accessible 
 * from real or VM86 mode. The mmap calls emulate the address wrap by
 * mapping the lowest 64kB the the first 64kB after the 1MB limit.
 * In hardware this is achieved by setting and resetting the a20 bit,
 * an ugly compatibility hack: The hardware simlpy clamps the address 20
 * line of the processor to low and hence the wrap around is forced.
 * This is switchable via the BIOS or via HIMEM.SYS and therefore the
 * first 64kB over 1MB can be enabled or disabled at will by software.
 * DOS uses this trick to load itself high, if the memory is present.
 * We emulate this behaviour by mapping and unmapping the HMA area.
 * (Linux has implemented this feature using shared memory (SHM) calls.)
 *
 * This routine is called from doscmd.c at startup. A20 is disabled after
 * startup.
 */

void initHMA()
{
    caddr_t add;
    int mfd;

    /*
     * We need two files, one for the wrap around mapping and one 
     * for the HMA contents
     */

    mfd = mkstemp(memfile);

    if (mfd < 0) {
        fprintf(stderr, "memfile: %s\n", strerror(errno));
        fprintf(stderr, "High memory will not be mapped\n");

        /* We need this for XMS services. If it fails, turn HMA off */
	HMA_a20 = -1;
        return;
    }
    unlink(memfile);
    HMA_fd_off = squirrel_fd(mfd);

    lseek(HMA_fd_off, 64 * 1024 - 1, 0);
    write(HMA_fd_off, "", 1);

    mfd = mkstemp(memfile);

    if (mfd < 0) {
        fprintf(stderr, "memfile: %s\n", strerror(errno));
        fprintf(stderr, "High memory will not be mapped\n");

        /* We need this for XMS services. If it fails, turn HMA off */
	HMA_a20 = -1;
        return;
    }
    unlink(memfile);
    HMA_fd_on = squirrel_fd(mfd);

    lseek(HMA_fd_on, 64 * 1024 - 1, 0);
    write(HMA_fd_on, "", 1);

    if (mmap((caddr_t)0x000000, 0x100000,
                   PROT_EXEC | PROT_READ | PROT_WRITE,
                   MAP_ANON | MAP_FIXED | MAP_INHERIT | MAP_SHARED,
                   -1, 0) < 0) {
	perror("Error mapping HMA, HMA disabled: ");
        HMA_a20 = -1;
	close(HMA_fd_off);
	close(HMA_fd_on);
	return;
    }
    if (mmap((caddr_t)0x000000, 64 * 1024,
                   PROT_EXEC | PROT_READ | PROT_WRITE,
                   MAP_FILE | MAP_FIXED | MAP_INHERIT | MAP_SHARED,
                   HMA_fd_off, 0) < 0) {
	perror("Error mapping HMA, HMA disabled: ");
        HMA_a20 = -1;
	close(HMA_fd_off);
	close(HMA_fd_on);
	return;
    }
    if (mmap((caddr_t)0x100000, 64 * 1024,
                   PROT_EXEC | PROT_READ | PROT_WRITE,
                   MAP_FILE | MAP_FIXED | MAP_INHERIT | MAP_SHARED,
                   HMA_fd_off, 0) < 0) {
	perror("Error mapping HMA, HMA disabled: ");
        HMA_a20 = -1;
	close(HMA_fd_off);
	close(HMA_fd_on);
	return;
    }
    HMA_a20 = 0;
}


/* Enable the a20 "address line" by unmapping the 64kB over 1MB */
static void enable_a20()
{
    if (HMA_a20 < 0)
	return;

    /* Unmap the wrap around portion (fd = HMA_fd_off) */
    /* XXX Not sure about this: Should I unmap first, then map new or
     * does it suffice to map new "over' the existing mapping ? Both
     * works (define to #if 0 next line and some lines below to try.
     */
#if 1
    if (munmap((caddr_t)0x100000, 64 * 1024) < 0) {
	fatal("HMA unmapping error: %s\nCannot recover\n", strerror(errno));
    }
#endif
    /* Map memory for the HMA with fd = HMA_fd_on */
    if (mmap((caddr_t)0x100000, 64 * 1024,
		PROT_EXEC | PROT_READ | PROT_WRITE,
		MAP_FILE | MAP_FIXED | MAP_INHERIT | MAP_SHARED,
		HMA_fd_on, 0) < 0) {
	fatal("HMA mapping error: %s\nCannot recover\n", strerror(errno));
    }
}

/* Disable the a20 "address line" by mapping the 64kB over 1MB again */
static void disable_a20()
{
    if (HMA_a20 < 0)
	return;
#if 1
    /* Unmap the HMA (fd = HMA_fd_on) */
    if (munmap((caddr_t)0x100000, 64 * 1024) < 0) {
	fatal("HMA unmapping error: %s\nCannot recover\n", strerror(errno));
    }
#endif
    /* Remap the wrap around area */
    if (mmap((caddr_t)0x100000, 64 * 1024,
                   PROT_EXEC | PROT_READ | PROT_WRITE,
                   MAP_FILE | MAP_FIXED | MAP_INHERIT | MAP_SHARED,
                   HMA_fd_off, 0) < 0) {
	fatal("HMA mapping error: %s\nCannot recover\n", strerror(errno));
    }
}


/*
 * This handles calls to int15 function 88: BIOS extended memory
 * request. XMS spec says: "In order to maintain compatibility with existing 
 * device drivers, DOS XMS drivers must not hook INT 15h until the first 
 * non-Version Number call to the control function is made."
 */

void
get_raw_extmemory_info(regcontext_t *REGS)
{
    if (vec_grabbed)
	R_AX = 0x0;
    else 
	R_AX = xms_maxsize / 1024;
    return;
}


/* Handle management routine: Find next free handle */

static int
get_free_handle()
{
    int i;
    /* Linear search, there are only a few handles */
    for (i = 0; i < NUM_HANDLES; i++) {
	if (xms_hand[i].addr == 0)
	    return i + 1;
    }
    return 0;
}


/* Installation check */
int
int2f_43(regcontext_t *REGS)
{               

    switch (R_AL) {
    case 0x00:			/* installation check */
	R_AL = 0x80;
	break;

    case 0x10:			/* get handler address */
	PUTVEC(R_ES, R_BX, xms_vector);
	break;

    default:
	return (0);
    }
    return (1);
}

/* Main call entry point for the XMS handler from DOS */
static void
xms_entry(regcontext_t *REGS)
{

    if (R_AH != 0)
	vec_grabbed = 1;

    /* If the HMA feature is disabled these calls are "not managed" */
    if (HMA_a20 < 0) {
       if (R_AH == XMS_ALLOCATE_HIGH_MEMORY || R_AH == XMS_FREE_HIGH_MEMORY ||
	   R_AH == XMS_GLOBAL_ENABLE_A20 || R_AH == XMS_GLOBAL_DISABLE_A20 ||
	   R_AH == XMS_LOCAL_ENABLE_A20 || R_AH == XMS_LOCAL_DISABLE_A20 ||
	   R_AH == XMS_QUERY_A20) {
	      R_AX = 0x0;
	      R_BL = XMS_HMA_NOT_MANAGED;
	      return;
	}
    }

    switch (R_AH) {
    case XMS_GET_VERSION:
	debug(D_XMS, "XMS: Get Version\n");
	R_AX = XMS_VERSION;	/* 3.0 */
	R_BX = XMS_REVISION;	/* internal revision 0 */
	R_DX = (HMA_a20 < 0) ? 0x0000 : 0x0001;
	break;

    /*
     * XXX Not exact! Spec says compare size to a HMAMIN parameter and
     * refuse HMA, if space is too small. With MSDOS 5.0 and higher DOS
     * itself uses the HMA (DOS=HIGH), so I think we can safely ignore
     * that.
     */
    case XMS_ALLOCATE_HIGH_MEMORY:
	debug(D_XMS, "XMS: Allocate HMA\n");
	if (HMA_allocated) {
	    R_AX = 0x0;
	    R_BL = XMS_HMA_ALREADY_USED;
	} else {
	    HMA_allocated = 1;
	    R_AX = 0x1;
	    R_BL = XMS_SUCCESS;
	}
	break;

    case XMS_FREE_HIGH_MEMORY:
	debug(D_XMS, "XMS: Free HMA\n");
	if (HMA_allocated) {
	    HMA_allocated = 0;
	    R_AX = 0x1;
	    R_BL = XMS_SUCCESS;
	} else {
	    R_AX = 0x0;
	    R_BL = XMS_HMA_NOT_ALLOCATED;
	}
	break;

    case XMS_GLOBAL_ENABLE_A20:
	debug(D_XMS, "XMS: Global enable A20\n");
	if (HMA_a20 == 0)
	    enable_a20();
	HMA_a20 = 1;
	R_AX = 0x1;
	R_BL = XMS_SUCCESS;
	break;

    case XMS_GLOBAL_DISABLE_A20:
	debug(D_XMS, "XMS: Global disable A20\n");
	if (HMA_a20 != 0)
	    disable_a20();
	HMA_a20 = 0;
	R_AX = 0x1;
	R_BL = XMS_SUCCESS;
	break;

   /* 
    * This is an accumulating call. Every call increments HMA_a20.
    * Caller must use LOCAL_DISBALE_A20 once for each previous call
    * to LOCAL_ENABLE_A20.
    */
    case XMS_LOCAL_ENABLE_A20:
	debug(D_XMS, "XMS: Local enable A20\n");
	HMA_a20++;
	if (HMA_a20 == 1)
	    enable_a20();
	R_AX = 0x1;
	R_BL = XMS_SUCCESS;
	break;

    case XMS_LOCAL_DISABLE_A20:
	debug(D_XMS, "XMS: Local disable A20\n");
	if (HMA_a20 > 0)
	    HMA_a20--;
	if (HMA_a20 == 0)
	    disable_a20();
	R_AX = 0x1;
	R_BL = XMS_SUCCESS;
	break;

    case XMS_QUERY_A20:
    /*
     * Disabled because DOS permanently scans this, to avoid endless output.
     */
#if 0
 	debug(D_XMS, "XMS: Query A20\n"); */
#endif
	R_AX = (HMA_a20 > 0) ? 0x1 : 0x0;
	R_BL = XMS_SUCCESS;
	break;

    case XMS_QUERY_FREE_EXTENDED_MEMORY:
	/* DOS MEM.EXE chokes, if the HMA is enabled and the reported
	 * free space includes the HMA. So we subtract 64kB from the
	 * space reported, if the HMA is enabled.
	 */
	if (HMA_a20 < 0)
	    R_EAX = R_EDX = xms_free_mem / 1024;
	else
	    R_EAX = R_EDX = (xms_free_mem / 1024) - 64;

	if (xms_free_mem == 0)
	    R_BL = XMS_FULL;
	else
	    R_BL = XMS_SUCCESS;
	debug(D_XMS, "XMS: Query free EMM: Returned %dkB\n", R_AX);
	break;

    case XMS_ALLOCATE_EXTENDED_MEMORY:
	{
	    size_t req_siz;
	    int hindx, hnum;
	    void *mem;

	    debug(D_XMS, "XMS: Allocate EMM: ");
	    /* Enough handles ? */
	    if ((hnum = get_free_handle()) == 0) {
		R_AX = 0x00;
		R_BL = XMS_OUT_OF_HANDLES;
		debug(D_XMS, " Out of handles\n");
		break;
	    }
	    hindx = hnum - 1;
	    req_siz = R_DX * 1024;

	    /* Enough memory ? */
	    if (req_siz > xms_free_mem) {
		R_AX = 0x00;
		R_BL = XMS_FULL;
		debug(D_XMS, " No memory left\n");
		break;
	    }

	    xms_hand[hindx].size = req_siz;
	    xms_hand[hindx].num_locks = 0;

	    /* XXX
	     * Not sure about that: Is it possible to reserve a handle
 	     * but with no memory attached ? XMS specs are unclear on
	     * that point. Linux implementation does it this way.
	     */
	    if (req_siz == 0) {
		/* This handle is reserved, but has size 0 and no address */
		xms_hand[hindx].addr = XMS_NULL_ALLOC;
	    } else {
		if ((mem = malloc(req_siz)) == NULL)
		    fatal("XMS: Cannot malloc !");
		xms_hand[hindx].addr = (u_long)mem;
	    }
	    xms_free_mem -= req_siz;
	    xms_used_mem += req_siz;
	    num_free_handle--;
	    R_AX = 0x1;
	    R_DX = hnum;
	    R_BL = XMS_SUCCESS;
	    debug(D_XMS, " Allocated %d kB, handle %d\n", 
			req_siz / 1024, hnum);
	    break;
	}	

    case XMS_FREE_EXTENDED_MEMORY:
	{
	    int hnum, hindx;

	    debug(D_XMS, "XMS: Free EMM: ");
	    hnum = R_DX;
	    if (hnum > NUM_HANDLES || hnum == 0) {
		R_AX = 0x0;
		R_BL = XMS_INVALID_HANDLE;
		debug(D_XMS, " Invalid handle\n");
		break;
	    }
	    hindx = hnum - 1;

	    if (xms_hand[hindx].addr == 0) {
		R_AX = 0x0;
		R_BL = XMS_INVALID_HANDLE;
		debug(D_XMS, " Invalid handle\n");

	    } else if (xms_hand[hindx].num_locks > 0) {
		R_AX = 0x0;
		R_BL = XMS_BLOCK_IS_LOCKED;
		debug(D_XMS, " Is locked\n");

	    } else {
		if (xms_hand[hindx].addr != XMS_NULL_ALLOC) {
		    free((void *)xms_hand[hindx].addr);
		    xms_free_mem += xms_hand[hindx].size;
		    xms_used_mem -= xms_hand[hindx].size;
		}
		xms_hand[hindx].addr = 0;
		xms_hand[hindx].size = 0;
		xms_hand[hindx].num_locks = 0;
		num_free_handle++;
		debug(D_XMS, " Success for handle %d\n", hnum);
		R_AX = 0x1;
		R_BL = XMS_SUCCESS;
	    }
	    break;
	}

    case XMS_MOVE_EXTENDED_MEMORY_BLOCK:
	{
	    u_long srcptr, dstptr;
	    u_long srcoffs, dstoffs;
	    int srcidx, dstidx;
	    const struct EMM *eptr;
	    int n;

	    debug(D_XMS, "XMS: Move EMM block: ");
	    eptr = (struct EMM *)MAKEPTR(R_DS, R_SI);

	    /* Sanity check: Don't allow eptr pointing to emulator data */
	    if (((u_long)eptr + sizeof(struct EMM)) >= 0x100000) {
		R_AX = 0x0;
		R_BL = XMS_GENERAL_ERROR;
		debug(D_XMS, " Offset to EMM structure wrong\n");
		break;
	    }

	    /* Validate handles and offsets */

	    if (eptr->src_handle > NUM_HANDLES) {
		    R_AX = 0x0;
		    R_BL = XMS_INVALID_SOURCE_HANDLE;
		    debug(D_XMS, " Invalid handle\n");
		    break;
	    }
	    if (eptr->dst_handle > NUM_HANDLES) {
		    R_AX = 0x0;
		    R_BL = XMS_INVALID_DESTINATION_HANDLE;
		    debug(D_XMS, " Invalid handle\n");
		    break;
	    }
	    srcidx = eptr->src_handle - 1;
	    dstidx = eptr->dst_handle - 1;
	    srcoffs = eptr->src_offset;
	    dstoffs = eptr->dst_offset;
	    n = eptr->nbytes;
	    /* Length must be even, see XMS spec */
	    if (n & 1) {
		R_AX = 0x0;
		R_BL = XMS_INVALID_LENGTH;
		debug(D_XMS, " Length not even\n");
		break;
	    }
	    if (eptr->src_handle != 0) {
		srcptr = xms_hand[srcidx].addr;
		if (srcptr == 0 || srcptr == XMS_NULL_ALLOC) {
		    R_AX = 0x0;
		    R_BL = XMS_INVALID_SOURCE_HANDLE;
		    debug(D_XMS, " Invalid source handle\n");
		    break;
 		}
		if ((srcoffs + n) > xms_hand[srcidx].size) {
		    R_AX = 0x0;
		    R_BL = XMS_INVALID_SOURCE_OFFSET;
		    debug(D_XMS, " Invalid source offset\n");
		    break;
 		}
		srcptr += srcoffs;
	    } else {
		srcptr = VECPTR(srcoffs);
	    	/* Sanity check: Don't allow srcptr pointing to 
		 * emulator data above 1M
		 */
	        if ((srcptr + n) >= 0x100000) {
		    R_AX = 0x0;
		    R_BL = XMS_GENERAL_ERROR;
		    debug(D_XMS, " Source segment invalid\n");
		    break;
		}
	    }

	    if (eptr->dst_handle != 0) {
		dstptr = xms_hand[dstidx].addr;
		if (dstptr == NULL || dstptr == XMS_NULL_ALLOC) {
		    R_AX = 0x0;
		    R_BL = XMS_INVALID_DESTINATION_HANDLE;
		    debug(D_XMS, " Invalid dest handle\n");
		    break;
 		}
		if ((dstoffs + n) > xms_hand[dstidx].size) {
		    R_AX = 0x0;
		    R_BL = XMS_INVALID_DESTINATION_OFFSET;
		    debug(D_XMS, " Invalid dest offset\n");
		    break;
 		}
		dstptr += dstoffs;
	    } else {
		dstptr = VECPTR(dstoffs);
	    	/* Sanity check: Don't allow dstptr pointing to 
		 * emulator data above 1M
		 */
	        if ((dstptr + n) >= 0x100000) {
		    R_AX = 0x0;
		    R_BL = XMS_GENERAL_ERROR;
		    debug(D_XMS, " Dest segment invalid\n");
		    break;
		}
	    }
	    memmove((void *)dstptr, (void *)srcptr, n);
	    debug(D_XMS, "Moved from %08x to %08x, %04x bytes\n",
			srcptr, dstptr, n);
	    R_AX = 0x1;
	    R_BL = XMS_SUCCESS;
	    break;
	}

    case XMS_LOCK_EXTENDED_MEMORY_BLOCK:
	{
	    int hnum,hindx;

	    debug(D_XMS, "XMS: Lock EMM block\n");
	    hnum = R_DX;
	    if (hnum > NUM_HANDLES || hnum == 0) {
		R_AX = 0x0;
		R_BL = XMS_INVALID_HANDLE;
		break;
	    }
	    hindx = hnum - 1;
	    if (xms_hand[hindx].addr == 0) {
		R_AX = 0x0;
		R_BL = XMS_INVALID_HANDLE;
		break;
	    }
	    if (xms_hand[hindx].num_locks == 255) {
		R_AX = 0x0;
		R_BL = XMS_BLOCK_LOCKCOUNT_OVERFLOW;
		break;
	    }
	    xms_hand[hindx].num_locks++;
	    R_AX = 0x1;
	    /* 
	     * The 32 bit "physical" address is returned here. I hope
	     * the solution to simply return the linear address of the
	     * malloced area is good enough. Most DOS programs won't
	     * need this anyway. It could be important for future DPMI.
	     */
	    R_BX = xms_hand[hindx].addr & 0xffff;
	    R_DX = (xms_hand[hindx].addr & 0xffff0000) >> 16;
	    break;
	}

    case XMS_UNLOCK_EXTENDED_MEMORY_BLOCK:
	{
	    int hnum,hindx;

	    debug(D_XMS, "XMS: Unlock EMM block\n");
	    hnum = R_DX;
	    if (hnum > NUM_HANDLES || hnum == 0) {
		R_AX = 0x0;
		R_BL = XMS_INVALID_HANDLE;
		break;
	    }
	    hindx = hnum - 1;
	    if (xms_hand[hindx].addr == 0) {
		R_AX = 0x0;
		R_BL = XMS_INVALID_HANDLE;
		break;
	    }
	    if (xms_hand[hindx].num_locks == 0) {
		R_AX = 0x0;
		R_BL = XMS_BLOCK_NOT_LOCKED;
		break;
	    }
	    xms_hand[hindx].num_locks--;
	    R_AX = 0x1;
	    R_BL = XMS_SUCCESS;
	    break;
	}

    case XMS_GET_EMB_HANDLE_INFORMATION:
	{
	    int hnum,hindx;

	    debug(D_XMS, "XMS: Get handle information: DX=%04x\n", R_DX);
	    hnum = R_DX;
	    if (hnum > NUM_HANDLES || hnum == 0) {
		R_AX = 0x0;
		R_BL = XMS_INVALID_HANDLE;
		break;
	    }
	    hindx = hnum - 1;
	    if (xms_hand[hindx].addr == 0) {
		R_AX = 0x0;
		R_BL = XMS_INVALID_HANDLE;
		break;
	    }
	    R_AX = 0x1;
	    R_BH = xms_hand[hindx].num_locks;
	    R_BL = num_free_handle;
	    R_DX = xms_hand[hindx].size / 1024;
	    break;
	}

    case XMS_RESIZE_EXTENDED_MEMORY_BLOCK:
	{
	    int hnum,hindx;
	    size_t req_siz;
	    long sizediff;
	    void *mem;

	    debug(D_XMS, "XMS: Resize EMM block\n");
	    hnum = R_DX;
	    req_siz = R_BX * 1024;
	    if (hnum > NUM_HANDLES || hnum == 0) {
		R_AX = 0x0;
		R_BL = XMS_INVALID_HANDLE;
		break;
	    }
	    hindx = hnum - 1;
	    if (xms_hand[hindx].addr == 0) {
		R_AX = 0x0;
		R_BL = XMS_INVALID_HANDLE;
		break;
	    }
	    if (xms_hand[hindx].num_locks > 0) {
		R_AX = 0x0;
		R_BL = XMS_BLOCK_IS_LOCKED;
		break;
	    }
	    sizediff = req_siz - xms_hand[hindx].size;

	    if (sizediff > 0) {
		if ((sizediff + xms_used_mem) > xms_maxsize) {
		    R_AX = 0x0;
		    R_BL = XMS_FULL;
		    break;
	        }
	    }

	    if (sizediff == 0) {	/* Never trust DOS programs */
		R_AX = 0x1;
		R_BL = XMS_SUCCESS;
		break;
	    }

	    xms_used_mem += sizediff;
	    xms_free_mem -= sizediff;

	    if (xms_hand[hindx].addr == XMS_NULL_ALLOC) {
		if ((mem = malloc(req_siz)) == NULL)
		    fatal("XMS: Cannot malloc !");
		xms_hand[hindx].addr = (u_long)mem;
		xms_hand[hindx].size = req_siz;
	    } else {
		if ((mem = realloc((void *)xms_hand[hindx].addr,req_siz)) 
			        == NULL)
		    fatal("XMS: Cannot realloc !");
		xms_hand[hindx].addr = (u_long)mem;
		xms_hand[hindx].size = req_siz;
	    }

	    R_AX = 0x1;
	    R_BL = XMS_SUCCESS;
	    break;
	}

    case XMS_ALLOCATE_UMB:
	{
	    u_long req_siz;
	    UMB_block *bp;

	    debug(D_XMS, "XMS: Allocate UMB: DX=%04x\n", R_DX);
	    req_siz = R_DX * 16;
	    /* Some programs try to allocate 0 bytes. XMS spec says
	     * nothing about this. So the driver grants the request
	     * but it rounds up to the next paragraph size (1) and
	     * returns this amount of memory
	     */
	    if (req_siz == 0)
		req_siz = 0x10;

	    /* First try to find an exact fit */
	    if ((bp = find_exact_block(req_siz)) != NULL) {
		/* Found ! Move block from free list to alloc list */
		remove_block(&UMB_freelist, bp);
		add_block(&UMB_alloclist, bp);		
		R_AX = 0x1;
		R_DX = req_siz >> 4;
		R_BX = bp->addr >> 4;
		break;
	    }
	    /* Try to find a block big enough */
	    bp = find_block(req_siz);
	    if (bp == NULL) {
		R_AX = 0x0;
		R_BL = XMS_NO_UMBS_AVAILABLE;
		R_DX = 0x0;

	    } else if (bp->size < req_siz) {
		R_AX = 0x0;
		R_BL = XMS_REQUESTED_UMB_TOO_BIG;
		R_DX = bp->size / 16;

	    } else {
		UMB_block *newbp;
		/* Found a block large enough. Split it into the size
		 * we need, rest remains on the free list. New block
		 * goes to the alloc list
		 */
		newbp = create_block(bp->addr, req_siz);
		bp->addr += req_siz;
		bp->size -= req_siz;
		add_block(&UMB_alloclist, newbp);
		R_AX = 0x1;
		R_BX = newbp->addr >> 4;
		R_DX = req_siz / 16;
	    }
	    break;
	}

    case XMS_DEALLOCATE_UMB:
	{
	    u_long req_addr;
	    UMB_block *blk;

	    debug(D_XMS, "XMS: Deallocate UMB: DX=%04x\n", R_DX);
	    req_addr = R_DX << 4;
	    if ((blk = find_allocated_block(req_addr)) == NULL) {
		R_AX = 0x0;
		R_BL = XMS_INVALID_UMB_SEGMENT;
	    } else {
		/* Move the block from the alloc list to the free list
		 * and try to do garbage collection
		 */
		remove_block(&UMB_alloclist, blk);
		add_block(&UMB_freelist, blk);
		merge_blocks();
		R_AX = 0x1;
		R_BL = XMS_SUCCESS;
	    }
	    break;
	}


    /* 
     * If the option DOS=UMB is enabled, DOS grabs the entire UMB
     * at boot time. In any other case this is used to load resident
     * utilities. I don't think this function is neccesary here.
     */
    case XMS_REALLOCATE_UMB:
	debug(D_XMS, "XMS: Reallocate UMB\n");
	R_AX = 0x0;
	R_BL = XMS_NOT_IMPLEMENTED;
	break;

    /* Some test programs use this call */
    case XMS_QUERY_FREE_EXTENDED_MEMORY_LARGE:
	/* DOS MEM.EXE chokes, if the HMA is enabled and the reported
	 * free space includes the HMA. So we subtract 64kB from the
	 * space reported, if the HMA is enabled.
	 */
	if (HMA_a20 < 0)
	    R_EAX = R_EDX = xms_free_mem / 1024;
	else
	    R_EAX = R_EDX = (xms_free_mem / 1024) - 64;
	/* ECX should return the highest address of any memory block
	 * We return 1MB + size of extended memory 
	 */
	R_ECX = 1024 * 1024 + xms_maxsize -1;
	if (xms_free_mem == 0)
	    R_BL = XMS_FULL;
	else
	    R_BL = XMS_SUCCESS;
	debug(D_XMS, "XMS: Query free EMM(large): Returned %dkB\n", R_AX);
	break;

    /* These are the same as the above functions, but they use 32 bit
     * registers (i.e. EDX instead of DX). This is for allocations of 
     * more than 64MB. I think this will hardly be used in the emulator
     * It seems to work without them, but the functions are in the XMS 3.0
     * spec. If something breaks because they are not here, I can implement
     * them
     */
    case XMS_ALLOCATE_EXTENDED_MEMORY_LARGE:
    case XMS_FREE_EXTENDED_MEMORY_LARGE:
	debug(D_XMS, "XMS: %02x function called, not implemented\n", R_AH);
	R_AX = 0x0;
	R_BL = XMS_NOT_IMPLEMENTED;
	break;

    default:
	debug(D_ALWAYS, "XMS: Unimplemented function %02x, \n", R_AH);
	R_AX = 0;
	R_BL = XMS_NOT_IMPLEMENTED;
	break;
    }
}
