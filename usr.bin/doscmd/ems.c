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
 * $FreeBSD$
 */

/* 
 * EMS memory emulation
 *
 * To emulate Expanded Memory we use a DOS driver (emsdriv.sys) which
 * routes calls to int 0x67 to this emulator routine. The main entry point
 * is ems_entry(..). The emulator needs to be initialized before the first
 * call. The first step of the initialization is done during program startup
 * the second part is done during DOS boot, from a call of the DOS driver.
 * The DOS driver is neccessary because DOS programs look for it to
 * determine if EMS is available.
 *
 * To emulate a configurable amount of EMS memory we use a file created
 * at startup with the size of the configured EMS memory. This file is
 * mapped into the EMS window like any DOS memory manager would do, using
 * mmap calls.
 *
 * The emulation follows the LIM EMS 4.0 standard. Not all functions of it
 * are implemented yet. The "alter page map and jump" and "alter page map
 * and call" functions are not implemented, because they are rather hard to
 * do. (It would mean a call to the emulator executes a routine in EMS 
 * memory and returns to the emulator, the emulator switches the page map
 * and then returns to the DOS program.) LINUX does not emulate this 
 * functions and I think they were very rarely used by DOS applications.
 *
 * Credits: To the writers of LINUX dosemu, I looked at their code
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <unistd.h>

#include "doscmd.h"
#include "ems.h"

/* Will be configurable */
u_long	ems_max_size = EMS_MAXSIZE * 1024;
u_long ems_frame_addr = EMS_FRAME_ADDR;

/*
 * Method for EMS: Allocate a mapfile with the size of EMS memory
 * and map the needed part into the page frame 
 */

#define EMS_MAP_PATH	"/var/tmp/"	/* Use a big file system */
#define EMS_MAP_FILE	"doscmd.XXXXXX"
static int mapfile_fd = -1;

/* Pages are always 16 kB in size. The page frame is 64 kB, there are
 * 4 positions (0..3) for a page to map in. The pages are numbered from 0 to
 * the highest 16 kB page in the mapfile, depending on the EMS size
 */

EMS_mapping_context ems_mapping_context;

/* Handle and page management (see ems.h) */

/* The handle array. If the pointer is NULL, the handle is unallocated */
static EMS_handle *ems_handle[EMS_NUM_HANDLES];
static u_long ems_alloc_handles;
/* The active handle, if any */
static short active_handle;

/* The page array. It is malloced at runtime, depending on the total
 * allocation size
 */

static EMS_page *ems_page = NULL;
static u_long ems_total_pages;
static u_long ems_alloc_pages;
static u_long ems_free_pages;

/* Local structure used for region copy and move operations */

struct copydesc {
#define SRC_EMS 1
#define DST_EMS 2
    short     copytype;		/* Type of source and destination memory */
    EMS_addr  src_addr;		/* Combined pointer for source */
    EMS_addr  dst_addr;		/* Combined pointer for destination */
    u_long  rest_len;		/* Lenght to copy */
};


/* Local prototypes */
static int init_mapfile();
static void map_page(u_long pagenum, u_char position, short handle, 
	int unmaponly);
static EMS_handle *get_new_handle(long npages);
static void context_to_handle(short handle);
static long find_next_free_handle();
static short lookup_handle(Hname *hp);
static void allocate_pages_to_handle(u_short handle, long npages);
static void allocate_handle(short handle, long npages);
static void reallocate_pages_to_handle(u_short handle, long npages);
static void free_handle(short handle);
static void free_pages_of_handle(short handle);
static void restore_context(EMS_mapping_context *emc);
static void save_context_to_dos(EMScontext *emp);
static int check_saved_context(EMScontext *emp);
static void *get_valid_pointer(u_short seg, u_short offs, u_long size);
static u_long move_ems_to_conv(short handle, u_short src_seg, 
			u_short src_offset, u_long dst_addr, u_long length);
static u_long move_conv_to_ems(u_long src_addr, u_short dst_handle, 
			u_short dst_seg, u_short dst_offset, u_long length);
static u_long move_ems_to_ems(u_short src_hande, u_short src_seg,
			u_short src_offset, u_short dst_handle, 
			u_short dst_seg, u_short dst_offset, u_long length);


/* 
 * EMS initialization routine: Return 1, if successful, return 0 if
 * init problem or EMS disabled
 */

int
ems_init()
{
    int i;

    if (ems_max_size == 0)
	return 0;
    if (init_mapfile() == 0)
	return 0;
    /* Sanity */
    bzero((void *)(&ems_handle[0]), sizeof(ems_handle));
    ems_total_pages = ems_max_size / EMS_PAGESIZE;
    ems_alloc_pages = 0;
    ems_free_pages = ems_total_pages;
    ems_alloc_handles = 0;
    active_handle = 0;
    /* Malloc the page array */
    ems_page = (EMS_page *)malloc(sizeof(EMS_page) * ems_total_pages);
    if (ems_page == NULL) {
	debug(D_ALWAYS, "Could not malloc page array, EMS disabled\n");
	ems_frame_addr = 0;
	ems_max_size = 0;
	ems_total_pages = 0;
	return 0;
    }
    for (i = 0; i < ems_total_pages; i++) {
	ems_page[i].handle = 0;
	ems_page[i].status = EMS_FREE;
    }
    debug(D_EMS, "EMS: Emulation init OK.\n");
    return 1;
}


/* Main entry point */

void
ems_entry(regcontext_t *REGS)
{
    /*
     * If EMS is not enabled, the DOS ems.exe module should not have
     * been loaded. If it is loaded anyway, report software malfunction
     */
    if (ems_max_size == 0) {
	R_AH = EMS_SW_MALFUNC;
	debug(D_EMS, "EMS emulation not enabled\n");
	return;
    }

    switch (R_AH)
    {
	case GET_MANAGER_STATUS:
	    debug(D_EMS, "EMS: Get manager status\n");
	    R_AH = EMS_SUCCESS;	    
	    break;

	case GET_PAGE_FRAME_SEGMENT:
	    debug(D_EMS, "EMS: Get page frame segment\n");
	    R_BX = ems_frame_addr >> 4;
	    R_AH = EMS_SUCCESS;	    
	    break;

	case GET_PAGE_COUNTS:
	    R_BX = ems_total_pages - ems_alloc_pages;
	    R_DX = ems_total_pages;
	    debug(D_EMS, "EMS: Get page count: Returned total=%d, free=%d\n", 
		R_DX, R_BX);
	    R_AH = EMS_SUCCESS;	    
	    break;

	case GET_HANDLE_AND_ALLOCATE:
        {
	    u_short npages;
	    short handle;

	    npages = R_BX;
	    debug(D_EMS, "EMS: Get handle and allocate %d pages: ", npages);

	    /* Enough handles? */
	    if ((handle = find_next_free_handle())  < 0) {
		debug(D_EMS,"Return error:No handles\n");
		R_AH = EMS_OUT_OF_HANDLES;
		break;
	    }
	    /* Enough memory for this request ? */
	    if (npages > ems_free_pages) {
		debug(D_EMS,"Return error:Request too big\n");
		R_AH = EMS_OUT_OF_LOG;
		break;
	    }
	    if (npages > ems_total_pages) {
		debug(D_EMS,"Return error:Request too big\n");
		R_AH = EMS_OUT_OF_PHYS;
		break;
	    }
	    /* Not allowed to allocate zero pages with this function */
	    if (npages == 0) {
		debug(D_EMS,"Return error:Cannot allocate 0 pages\n");
		R_AH = EMS_ZERO_PAGES;
		break;
	    }
	    /* Allocate the handle */
	    allocate_handle(handle, npages);

	    /* Allocate the pages */
	    allocate_pages_to_handle(handle, npages);
	    R_DX = handle;
	    R_AH = EMS_SUCCESS;	    
	    debug(D_EMS,"Return success:Handle = %d\n", handle);
	    break;
	}

	case MAP_UNMAP:
	{
	    u_char position;
	    u_short hpagenum, spagenum;
	    short handle;
	    
	    debug(D_EMS, "EMS: Map/Unmap handle=%d, pos=%d, pagenum=%d ", 
		R_DX, R_AL, R_BX);
	    handle = R_DX;
	    position = R_AL;
	    if (position > 3) {
		debug(D_EMS, "invalid position\n");
		R_AH = EMS_ILL_PHYS;
		break;
	    }
	    hpagenum = R_BX;
	    /* This succeeds without a valid handle ! */
	    if (hpagenum == 0xffff) {
		/* Unmap only */
		map_page(0, position, handle, 1);
		debug(D_EMS, "(unmap only) success\n");
		R_AH = EMS_SUCCESS;
		break;
	    }
	    if (handle > 255 || handle == 0 || ems_handle[handle] == NULL) {
		R_AH = EMS_INV_HANDLE;
		debug(D_EMS, "invalid handle\n");
		break;
	    }
	    if (hpagenum >= ems_handle[handle]->npages) {
		R_AH = EMS_LOGPAGE_TOOBIG;
		debug(D_EMS, "invalid pagenumber\n");
		break;
	    }
	    spagenum = ems_handle[handle]->pagenum[hpagenum];
	    map_page(spagenum, position, handle, 0);
	    debug(D_EMS, "success\n");
	    R_AH = EMS_SUCCESS;
	    break;
	}

	case DEALLOCATE_HANDLE:
	{
	    short handle;

	    /* Handle valid ? */
	    handle = R_DX;
	    debug(D_EMS, "EMS: Deallocate handle %d\n", handle);
	    if (handle > 255 || ems_handle[handle] == NULL) {
		R_AH = EMS_INV_HANDLE;
		break;
	    }
	    /* Mapping context saved ? */
	    if (ems_handle[handle]->mcontext != NULL) {
		R_AH = EMS_SAVED_MAP;
		break;
	    }

	    free_pages_of_handle(handle);
	    free_handle(handle);
	    R_AH = EMS_SUCCESS;	    
	    break;
	}

	case GET_EMM_VERSION:
	    debug(D_EMS, "EMS: Get version\n");
	    R_AL = EMS_VERSION;
	    R_AH = EMS_SUCCESS;	    
	    break;

	case SAVE_PAGE_MAP:
	{
	    short handle;

	    debug(D_EMS, "EMS: Save page map\n");
	    handle = R_DX;
	    if (handle > 255 || handle == 0 || ems_handle[handle] == NULL) {
		R_AH = EMS_INV_HANDLE;
		break;
	    }
	    if (ems_handle[handle]->mcontext != NULL) {
		/* There is already a context saved */
		if (memcmp((void *)ems_handle[handle]->mcontext,
		           (void *)&ems_mapping_context,
			   sizeof(EMS_mapping_context)) == 0)
		     R_AH = EMS_ALREADY_SAVED;
		else 
		     R_AH = EMS_NO_ROOM_TO_SAVE;
		break;
	    }
	    context_to_handle(handle);
	    R_AH = EMS_SUCCESS;	    
	    break;
	}

	case RESTORE_PAGE_MAP:
	{
	    short handle;

	    debug(D_EMS, "EMS: Restore page map\n");
	    handle = R_DX;
	    if (handle > 255 || handle == 0 || ems_handle[handle] == NULL) {
		R_AH = EMS_INV_HANDLE;
		break;
	    }
	    if (ems_handle[handle]->mcontext == NULL) {
		R_AH = EMS_NO_SAVED_CONTEXT;
		break;
	    }
	    restore_context(ems_handle[handle]->mcontext);
	    free((void *)ems_handle[handle]->mcontext);
	    ems_handle[handle]->mcontext = NULL;
	    R_AH = EMS_SUCCESS;	    
	    break;
	}

	case RESERVED_1:
	case RESERVED_2:
	    debug(D_ALWAYS, "Reserved function called: %02x\n", R_AH);
	    R_AH = EMS_FUNC_NOSUP;
	    break;

	case GET_HANDLE_COUNT:
	    debug(D_EMS, "EMS: Get handle count\n");
	    R_BX = ems_alloc_handles + 1;
	    R_AH = EMS_SUCCESS;	    
	    break;

	case GET_PAGES_OWNED:
	{
	    short handle;

	    debug(D_EMS, "EMS: Get pages owned\n");
	    /* Handle valid ? */
	    handle = R_DX;
	    if (handle > 255 || ems_handle[handle] == NULL) {
		R_AH = EMS_INV_HANDLE;
		break;
	    }
	    if (handle == 0)
		R_BX = 0;
	    else
		R_BX = ems_handle[handle]->npages;
	    R_AH = EMS_SUCCESS;	    
	    break;
	}

	case GET_PAGES_FOR_ALL:
	{
	    EMShandlepage *ehp;
	    int safecount;
	    int i;

	    debug(D_EMS, "EMS: Get pages for all\n");
	    /* Get the address passed from DOS app */
	    ehp = (EMShandlepage *)get_valid_pointer(R_ES, R_DI,
			sizeof(EMShandlepage) * ems_alloc_handles); 
	    if (ehp == NULL) {
		R_AH = EMS_SW_MALFUNC;
		break;
	    }

	    R_BX = ems_alloc_handles;
	    safecount = 0;
	    for (i = 0; i < 255; i++) {
		if (ems_handle[i] != NULL) {
		    if (safecount > (ems_alloc_handles+1))
		        fatal("EMS: ems_alloc_handles is wrong, cannot continue\n");
		    ehp->handle = i;
		    ehp->npages = ems_handle[i]->npages;
		    ehp++;
		    safecount++;
		}
	    }
	    R_AH = EMS_SUCCESS;	    
	    break;
        }

	case PAGE_MAP:
	/* This function is a nuisance. It was invented to save time and
         * memory, but in our case it is useless. We have to support it
         * but we use the same save memory as for the page map function.
         * It uses only 20 bytes anyway. We store/restore the entire mapping
	 */
	case PAGE_MAP_PARTIAL:
	{
	    u_long addr;
	    int subfunction;
	    EMScontext *src, *dest;

	    debug(D_EMS, "EMS: Page map ");
	    subfunction = R_AL;
	    if (R_AH == PAGE_MAP_PARTIAL) {
		debug(D_EMS, "partial ");
		/* Page map partial has slightly different subfunctions
		 * GET_SET does not exist and is GET_SIZE in this case 
		 */
		if (subfunction == GET_SET)
		    subfunction = GET_SIZE;
	    }
	    switch (subfunction)
	    {
		case GET:
		{
		    debug(D_EMS, "get\n");
		    /* Get the address passed from DOS app */
		    dest = (EMScontext *)get_valid_pointer(R_ES, R_DI, 
			sizeof(EMScontext));
		    if (dest == NULL) {
			R_AH = EMS_SW_MALFUNC;
			break;
		    }
		    save_context_to_dos(dest);
		    R_AH = EMS_SUCCESS;     
	            break;
		}
		case SET:
		{
		    debug(D_EMS, "set\n");
		    src = (EMScontext *)get_valid_pointer(R_DS, R_SI, 
			sizeof(EMScontext));
		    if (src == NULL) {
			R_AH = EMS_SW_MALFUNC;
			break;
		    }
		    if (check_saved_context(src) == 0) {
			R_AH = EMS_SAVED_CONTEXT_BAD;
			break;
		    }
		    restore_context(&src->ems_saved_context);
		    R_AH = EMS_SUCCESS;     
	            break;
		}
		case GET_SET:
		{
		    debug(D_EMS, "get/set\n");
		    dest = (EMScontext *)get_valid_pointer(R_ES, R_DI, 
			sizeof(EMScontext));
		    if (dest == NULL) {
			R_AH = EMS_SW_MALFUNC;
			break;
		    }
		    save_context_to_dos(dest);
		    src = (EMScontext *)get_valid_pointer(R_DS, R_SI, 
			sizeof(EMScontext));
		    if (src == NULL) {
			R_AH = EMS_SW_MALFUNC;
			break;
		    }
		    if (check_saved_context(src) == 0) {
			R_AH = EMS_SAVED_CONTEXT_BAD;
			break;
		    }
		    restore_context(&src->ems_saved_context);
		    R_AH = EMS_SUCCESS;     
	            break;
		}
		case GET_SIZE:
		    debug(D_EMS, "get size\n");
		    R_AL = (sizeof(EMScontext) + 1) & 0xfe;
		    R_AH = EMS_SUCCESS;     
	            break;
		default:
		    debug(D_EMS, "invalid subfunction\n");
		    R_AH = EMS_INVALID_SUB;
		    break;
	    }
	    break;
	}

	case MAP_UNMAP_MULTI_HANDLE:
	{
	    u_char position;
	    u_short hpagenum, spagenum;
	    short handle;
	    EMSmapunmap *mp;
	    int n_entry, i;
	    
	    
	    debug(D_EMS, "EMS: Map/Unmap multiple ");

	    if ((n_entry = R_CX) > 3) {
		R_AH = EMS_ILL_PHYS;
	    }

	    /* This is valid according to the LIM EMS 4.0 spec */
	    if (n_entry == 0) {
		R_AH = EMS_SUCCESS;     
	        break;
	    }

	    handle = R_DX;
	    if (handle > 255 || handle == 0 || ems_handle[handle] == NULL) {
		R_AH = EMS_INV_HANDLE;
		break;
	    }

	    mp = (EMSmapunmap *)get_valid_pointer(R_DS, R_SI,
			sizeof(EMSmapunmap) * n_entry);
	    if (mp == NULL) {
		R_AH = EMS_SW_MALFUNC;
		break;
	    }

	    R_AH = EMS_SUCCESS;
	    /* Walk through the table and map/unmap */
	    for (i = 0; i < n_entry; i++) {
		hpagenum = mp->log;
		/* Method is in R_AL */
		if (R_AL == 0) {
		    debug(D_EMS, "phys page method\n");
		    if (mp->phys <= 3) {
		    	position = mp->phys;
		    } else {
			R_AH = EMS_ILL_PHYS;
			break;
		    }
		} else if (R_AL == 1) {
		    /* Compute position from segment address */
	    	    u_short p_seg;

		    debug(D_EMS, "segment method\n");
		    p_seg = mp->phys;
		    p_seg -= ems_frame_addr;
		    p_seg /= EMS_PAGESIZE;
		    if (p_seg <= 3) {
			position = p_seg;
		    } else {
			R_AH = EMS_ILL_PHYS;
			break;
		    }
		} else {
		    debug(D_EMS, "invalid subfunction\n");
		    R_AH = EMS_INVALID_SUB;
		    break;
		}

		mp++;
	        if (hpagenum == 0xffff) {
		    /* Unmap only */
		    map_page(0, position, handle, 1);
		    continue;
	        }
	        if (hpagenum >= ems_handle[handle]->npages) {
		    R_AH = EMS_LOGPAGE_TOOBIG;
		    break;
	        }
	        spagenum = ems_handle[handle]->pagenum[hpagenum];
	        map_page(spagenum, position, handle, 0);
	    }
	    break;
	}

	case REALLOC_PAGES:
	{
	    short handle;
	    u_long newpages;

	    debug(D_EMS, "EMS: Realloc pages ");

	    handle = R_DX;
	    if (handle > 255 || handle == 0 || ems_handle[handle] == NULL) {
		R_AH = EMS_INV_HANDLE;
		debug(D_EMS, "invalid handle\n");
		break;
	    }
	    newpages = R_BX;
	    debug(D_EMS, "changed from %d to %d pages\n", 
		ems_handle[handle]->npages, newpages);

	    /* Case 1: Realloc to zero pages */
	    if (newpages == 0) {
		free_pages_of_handle(handle);
		R_AH = EMS_SUCCESS;     
	        break;
	    }
	    /* Case 2: New allocation is equal to allocated number */
	    if (newpages == ems_handle[handle]->npages) {
		R_AH = EMS_SUCCESS;     
	        break;
	    }
	    /* Case 3: Reallocate to bigger and smaller sizes */
	    if (newpages > ems_handle[handle]->npages) {
		if (newpages > ems_free_pages) {
            	    R_AH = EMS_OUT_OF_LOG;
                    break;
		}
		if (newpages > ems_total_pages) {
            	    R_AH = EMS_OUT_OF_PHYS;
                    break;
		}
	    }
	    reallocate_pages_to_handle(handle, newpages);
	    R_AH = EMS_SUCCESS;     
	    break;
	}

	/* We do not support nonvolatile pages */
	case HANDLE_ATTRIBUTES:
	    debug(D_EMS, "Handle attributes called\n");
	    switch (R_AL) {
		case GET:
		case SET:
		    R_AH = EMS_FEAT_NOSUP;
		    break;
		case HANDLE_CAPABILITY:
		    R_AL = 0; 		/* Volatile only */
		    R_AH = EMS_SUCCESS;     
		    break;
		default:
		    R_AH = EMS_FUNC_NOSUP;
		    break;
	    }
	    break;

	case HANDLE_NAME:
	{
	    short handle;
	    Hname *hp;

	    handle = R_DX;
	    if (handle > 255 || handle == 0 || ems_handle[handle] == NULL) {
		R_AH = EMS_INV_HANDLE;
		debug(D_EMS, "invalid handle\n");
		break;
	    }
	    switch (R_AL) {
		case GET:
		    if ((hp = (Hname *)get_valid_pointer(R_ES, R_DI, 8))
		     == NULL) {
			R_AH = EMS_SW_MALFUNC;
			break;
		    }
		    *hp = ems_handle[handle]->hname;
		    R_AH = EMS_SUCCESS;     
		    break;

		case SET:
		    if ((hp = (Hname *)get_valid_pointer(R_DS, R_SI, 8))
		     == NULL) {
			R_AH = EMS_SW_MALFUNC;
			break;
		    }
		    /* If the handle name is not 0, it may not exist */
		    if ((hp->ul_hn[0] | hp->ul_hn[1]) != 0) {
			if (lookup_handle(hp) == 0) {
			    ems_handle[handle]->hname = *hp;
			    R_AH = EMS_SUCCESS;     
			} else {
			    R_AH = EMS_NAME_EXISTS;     
			    break;
			}
		    } else {
		        /* Name is deleted (set to zeros) */
			ems_handle[handle]->hname = *hp;
			R_AH = EMS_SUCCESS;     
		    }
		    break;

		default:
		    R_AH = EMS_FUNC_NOSUP;
		    break;
	    }
	    break;
	}

	case HANDLE_DIRECTORY:
	{
	    int i;
	    EMShandledir *hdp;
	    Hname *hp;
	    short handle;

	    switch(R_AL) {
		case GET:
		    hdp = (EMShandledir *)get_valid_pointer(R_ES, R_DI, 
			sizeof(EMShandledir) * ems_alloc_handles);
		    if (hdp == NULL) {
			R_AH = EMS_SW_MALFUNC;
			break;
		    }
		    for (i = 0; i < EMS_NUM_HANDLES; i++) {
			if (ems_handle[i] != NULL) {
			    hdp->log = i;
			    hdp->name = ems_handle[i]->hname;
			}
		    }
		    R_AH = EMS_SUCCESS;     
		    break;

		case HANDLE_SEARCH:
		    hp = (Hname *)get_valid_pointer(R_DS, R_SI, 8);
		    if (hp == NULL) {
			R_AH = EMS_SW_MALFUNC;
			break;
		    }
		    /* Cannot search for NULL handle name */
		    if ((hp->ul_hn[0] | hp->ul_hn[1]) != 0) {
			R_AH = EMS_NAME_EXISTS;
			break;
		    }
		    if ((handle = lookup_handle(hp)) == 0) {
			R_AH = EMS_HNAME_NOT_FOUND;     
		    } else {
			R_DX = handle;
			R_AH = EMS_SUCCESS;     
		    }
		    break;

		case GET_TOTAL_HANDLES:
		    R_AH = EMS_SUCCESS;     
		    R_BX = EMS_NUM_HANDLES;	/* Includes OS handle */
		    break;

		default:
		    R_AH = EMS_FUNC_NOSUP;
		    break;
	    }
	    break;
	}


	/* I do not know if we need this. LINUX emulation leaves it out
         * so I leave it out too for now.
	 */
	case ALTER_PAGEMAP_JUMP:
	    debug(D_ALWAYS, "Alter pagemap and jump used!\n");
	    R_AH = EMS_FUNC_NOSUP;
	    break;
	case ALTER_PAGEMAP_CALL:
	    debug(D_ALWAYS, "Alter pagemap and call used!\n");
	    R_AH = EMS_FUNC_NOSUP;
	    break;


	case MOVE_MEMORY_REGION:
	{
	    EMSmovemem *emvp;
	    u_long src_addr, dst_addr;
	    u_short src_handle, dst_handle;

	    if (R_AL == EXCHANGE)
	    	debug(D_EMS, "EMS: Exchange memory region ");
	    else
	    	debug(D_EMS, "EMS: Move memory region ");

	    emvp = (EMSmovemem *)get_valid_pointer(R_DS, R_SI,
			sizeof(EMSmovemem));
	    if (emvp == NULL) {
		debug(D_EMS, "Invalid structure pointer\n");
		R_AH = EMS_SW_MALFUNC;
		break;
	    }
	    /* Zero length is not an error */
	    if (emvp->length == 0) {
		debug(D_EMS, "Zero length\n");
		R_AH = EMS_SUCCESS;     
		break;
 	    }
	    /* Some checks */
	    if (emvp->src_type == EMS_MOVE_CONV) {
		/* Conventional memory source */
		src_addr = MAKEPTR(emvp->src_seg, emvp->src_offset);
		/* May not exceed conventional memory */
		if ((src_addr + emvp->length) > 640 * 1024) {
		    R_AH = EMS_SW_MALFUNC;
		    break;
		}
	    } else {
		/* Check the handle */
		src_handle = emvp->src_handle;
	    	if (src_handle > 255 || src_handle == 0 || 
			ems_handle[src_handle] == NULL) {
		    R_AH = EMS_INV_HANDLE;
		    debug(D_EMS, "invalid source handle\n");
		    break;
		}
		/* Offset may not exceed page size */
		if (emvp->src_offset >= (16 * 1024)) {
		    R_AH = EMS_PAGEOFFSET;
		    debug(D_EMS, "source page offset too big\n");
		    break;
		}
	    }

	    if (emvp->dst_type == EMS_MOVE_CONV) {
		/* Conventional memory source */
		dst_addr = MAKEPTR(emvp->dst_seg, emvp->dst_offset);
		/* May not exceed conventional memory */
		if ((dst_addr + emvp->length) > 640 * 1024) {
		    R_AH = EMS_SW_MALFUNC;
		    break;
		}
	    } else {
		/* Check the handle */
		dst_handle = emvp->dst_handle;
	    	if (dst_handle > 255 || dst_handle == 0 || 
			ems_handle[dst_handle] == NULL) {
		    R_AH = EMS_INV_HANDLE;
		    debug(D_EMS, "invalid destination handle\n");
		    break;
		}
		/* Offset may not exceed page size */
		if (emvp->dst_offset >= (16 * 1024)) {
		    R_AH = EMS_PAGEOFFSET;
		    debug(D_EMS, "destination page offset too big\n");
		    break;
		}
	    }

	    if (R_AL == MOVE) {
		/* If it is conventional memory only, do it */
	        if (emvp->src_type == EMS_MOVE_CONV &&
		    emvp->dst_type == EMS_MOVE_CONV) {
		    memmove((void *)dst_addr, (void *)src_addr, 
			(size_t) emvp->length);
		    debug(D_EMS, "conventional to conventional memory done\n");
		    R_AH = EMS_SUCCESS;     
		    break;
	        }
	        if (emvp->src_type == EMS_MOVE_EMS &&
		    emvp->dst_type == EMS_MOVE_CONV)
		    R_AH = move_ems_to_conv(src_handle, emvp->src_seg,
			emvp->src_offset, dst_addr, emvp->length);
	        else if (emvp->src_type == EMS_MOVE_CONV &&
                    emvp->dst_type == EMS_MOVE_EMS)
		    R_AH = move_conv_to_ems(src_addr, dst_handle,
		              emvp->dst_seg, emvp->dst_offset, emvp->length);
	        else
		    R_AH = move_ems_to_ems(src_handle, emvp->src_seg,
			emvp->src_offset, dst_handle, emvp->dst_seg,
			emvp->dst_offset, emvp->length);
	        debug(D_EMS, " done\n");
	        break;
	    } else {
		/* exchange memory region */

		/* We need a scratch area for the exchange */
		void *buffer;
		if ((buffer = malloc(emvp->length)) == NULL)
		    fatal("EMS: Could not malloc scratch area for exchange");

		/* If it is conventional memory only, do it */
	        if (emvp->src_type == EMS_MOVE_CONV &&
		    emvp->dst_type == EMS_MOVE_CONV) {
		    /* destination -> buffer */
		    memmove(buffer, (void *)dst_addr, (size_t) emvp->length);
		    /* Source -> destination */
		    memmove((void *)dst_addr, (void *)src_addr, 
			(size_t) emvp->length);
		    /* Buffer -> source */
		    memmove((void *)src_addr, buffer, (size_t) emvp->length);
		    free(buffer);
		    debug(D_EMS, "conventional to conventional memory done\n");
		    R_AH = EMS_SUCCESS;     
		    break;
	        }

		/* Exchange EMS with conventional */
	        if (emvp->src_type == EMS_MOVE_EMS &&
		    emvp->dst_type == EMS_MOVE_CONV) {
		    /* Destination -> buffer */
		    memmove(buffer, (void *)dst_addr, (size_t) emvp->length);
		    /* Source -> destination */
		    R_AH = move_ems_to_conv(src_handle, emvp->src_seg,
                              emvp->src_offset, dst_addr, emvp->length);
                    if (R_AH != EMS_SUCCESS) {
			free(buffer);
			break;
		    }
		    /* Buffer -> source */
		    R_AH = move_conv_to_ems((u_long)buffer, src_handle,
		              emvp->src_seg, emvp->src_offset, emvp->length);

                /* Exchange conventional with EMS */
		} else if (emvp->src_type == EMS_MOVE_CONV &&
                           emvp->dst_type == EMS_MOVE_EMS) {
		    /* Destination -> buffer */
		    R_AH = move_ems_to_conv(dst_handle, emvp->dst_seg,
			emvp->dst_offset, (u_long)buffer, emvp->length);
                    if (R_AH != EMS_SUCCESS) {
			free(buffer);
			break;
		    }
		    /* Source -> destination */
		    R_AH = move_conv_to_ems((u_long)buffer, dst_handle,
		              emvp->dst_seg, emvp->dst_offset, emvp->length);
		    /* Buffer -> source */
		    memmove(buffer, (void *)src_addr, (size_t) emvp->length);

		/* Exchange EMS with EMS */
		} else {
		    /* Destination -> buffer */
		    R_AH = move_ems_to_conv(dst_handle, emvp->dst_seg,
			emvp->dst_offset, (u_long)buffer, emvp->length);
                    if (R_AH != EMS_SUCCESS) {
			free(buffer);
			break;
		    }
		    /* Source -> destination */
		    R_AH = move_ems_to_ems(src_handle, emvp->src_seg,
			emvp->src_offset, dst_handle, emvp->dst_seg,
			emvp->dst_offset, emvp->length);
		    if (R_AH != EMS_SUCCESS) {
			free(buffer);
			break;
		    }
		    /* Buffer -> source */
		    R_AH = move_conv_to_ems((u_long)buffer, src_handle,
		              emvp->src_seg, emvp->src_offset, emvp->length);
	        }
		free(buffer);
	    }
	    debug(D_EMS, " done\n");
	    break;
	}

	case GET_MAPPABLE_PHYS_ADDR:
	{
	    switch (R_AL) {
		case GET_ARRAY:
		{
		    EMSaddrarray *eadp;
		    int i;
		    u_short seg;

		    eadp = (EMSaddrarray *)get_valid_pointer(R_ES, R_DI, 
                        sizeof(EMSaddrarray) * 4);
	    	    if (eadp == NULL) {
			R_AH = EMS_SW_MALFUNC;
			break;
	    	    }
		    for (i = 0, seg = (ems_frame_addr >> 4); i < 4; i++) {
			eadp->segm = seg;
			eadp->phys = i;
			eadp++;
			seg += 1024;
		    }
		    R_AH = EMS_SUCCESS;     
		    break;
		}
		case GET_ARRAY_ENTRIES:
		    /* There are always 4 positions, 4*16kB = 64kB */
		    R_CX = 4;
		    R_AH = EMS_SUCCESS;
		    break;
		default:
		    R_AH = EMS_FUNC_NOSUP;
		    break;
	    }
	    break;
	}

	/* This is an OS function in the LIM EMS 4.0 standard: It is
	 * usable only by an OS and its use can be disabled for all other
	 * programs. I think we do not need to support it. It is not
	 * implemented and it reports "disabled" to any caller.
	 */
	case GET_HW_CONFIGURATION:
	    R_AH = EMS_FUNCTION_DISABLED;
	    break;

	/* This function is a little different, it was defined with
	 * LIM EMS 4.0: It is allowed to allocate zero pages and raw
	 * page size (i.e. page size != 16kB) is supported. We have 
	 * only 16kB pages, so the second difference does not matter.
	 */
	case ALLOCATE_PAGES:
        {
	    u_short npages;
	    short handle;

	    npages = R_BX;
	    debug(D_EMS, "EMS: Get handle and allocate %d pages: ", npages);

	    /* Enough handles? */
	    if ((handle = find_next_free_handle()) < 0) {
		debug(D_EMS,"Return error:No handles\n");
		R_AH = EMS_OUT_OF_HANDLES;
		break;
	    }
	    /* Enough memory for this request ? */
	    if (npages > ems_free_pages) {
		debug(D_EMS,"Return error:Request too big\n");
		R_AH = EMS_OUT_OF_LOG;
		break;
	    }
	    if (npages > ems_total_pages) {
		debug(D_EMS,"Return error:Request too big\n");
		R_AH = EMS_OUT_OF_PHYS;
		break;
	    }

	    /* Allocate the handle */
	    allocate_handle(handle, npages);

	    /* Allocate the pages */
	    allocate_pages_to_handle(handle, npages);
	    R_DX = handle;
	    R_AH = EMS_SUCCESS;	    
	    debug(D_EMS,"Return success:Handle = %d\n", handle);
	    break;
	}

	/* This is an OS function in the LIM EMS 4.0 standard: It is
	 * usable only by an OS and its use can be disabled for all other
	 * programs. I think we do not need to support it. It is not
	 * implemented and it reports "disabled" to any caller.
	 */
	case ALTERNATE_MAP_REGISTER:
	    R_AH = EMS_FUNCTION_DISABLED;
	    break;

	/* We cannot support that ! */
	case PREPARE_WARMBOOT:
	    R_AH = EMS_FUNC_NOSUP;
	    break;

	case OS_FUNCTION_SET:
	    R_AH = EMS_FUNCTION_DISABLED;
	    break;

unknown:
	default:
	    debug(D_ALWAYS, "EMS: Unknown function called: %02x\n", R_AH);
	    R_AH = EMS_FUNC_NOSUP;
	    break;
    }
}

/* Initialize the EMS memory: Return 1 on success, 0 on failure */

static int
init_mapfile()
{
    char path[256];
    int mfd;

    /* Sanity */
    if (ems_max_size == 0)
	return;
    strcpy(path, EMS_MAP_PATH);
    strcat(path, EMS_MAP_FILE);

    mfd = mkstemp(path);

    if (mfd < 0) {
        debug(D_ALWAYS, "Could not create EMS mapfile, ");
	goto fail;
    }
    unlink(path);
    mapfile_fd = squirrel_fd(mfd);

    if (lseek(mapfile_fd, (off_t)(ems_max_size - 1), 0) < 0) {
	debug(D_ALWAYS, "Could not seek into EMS mapfile, ");
	goto fail;
    }
    if (write(mapfile_fd, "", 1) < 0) {
	debug(D_ALWAYS, "Could not write to EMS mapfile, ");
	goto fail;
    }
    /* Unmap the entire page frame */
    if (munmap((caddr_t)ems_frame_addr, 64 * 1024) < 0) {
	debug(D_ALWAYS, "Could not unmap EMS page frame, ");
	goto fail;
    }
    /* DOS programs will access the page frame without allocating 
     * pages first. Microsoft diagnose MSD.EXE does this, for example
     * We need to have memory here to avoid segmentation violation
     */
    if (mmap((caddr_t)ems_frame_addr, 64 * 1024,
              PROT_EXEC | PROT_READ | PROT_WRITE,
              MAP_ANON | MAP_FIXED | MAP_INHERIT | MAP_SHARED,
	      -1, 0) < 0) {
	debug(D_ALWAYS, "Could not map EMS page frame, ");
	goto fail;
    }
    bzero((void *)&ems_mapping_context, sizeof(EMS_mapping_context));
    return (1);

fail:
    debug(D_ALWAYS, "EMS disabled\n");
    ems_max_size = 0;
    ems_frame_addr = 0;
    return (0);
}

/* Map/Unmap pages into one of four positions in the frame segment */

static void
map_page(u_long pagenum, u_char position, short handle, int unmaponly)
{
    caddr_t map_addr;
    size_t  len;
    off_t   file_offs;

    if (position > 3)
	fatal("EMS: Internal error: Mapping position\n");

    map_addr = (caddr_t)(ems_frame_addr + (1024 * 16 * (u_long)position));
    len = 1024 * 16;
    file_offs = (off_t)(pagenum * 16 * 1024);

    if (ems_mapping_context.pos_mapped[position]) {
        if (munmap(map_addr, len) < 0) {
             fatal("EMS unmapping error: %s\nCannot recover\n", 
		strerror(errno));
	}
	ems_page[ems_mapping_context.pos_pagenum[position]].status 
		&= ~EMS_MAPPED;
	ems_mapping_context.pos_mapped[position] = 0;
	ems_mapping_context.handle[position] = 0;
    }
    if (unmaponly) {
        /* DOS programs will access the page frame without allocating 
         * pages first. Microsoft diagnose MSD.EXE does this, for example
         * We need to have memory here to avoid segmentation violation
         */
    	if (mmap((caddr_t)ems_frame_addr, 64 * 1024,
              PROT_EXEC | PROT_READ | PROT_WRITE,
              MAP_ANON | MAP_FIXED | MAP_INHERIT | MAP_SHARED,
	      -1, 0) < 0)
	    fatal("Could not map EMS page frame during unmap only\n");
	return;
    }
    if (mmap(map_addr, len,
              PROT_EXEC | PROT_READ | PROT_WRITE,
              MAP_FILE | MAP_FIXED | MAP_INHERIT | MAP_SHARED,
              mapfile_fd, file_offs) < 0) {
        fatal("EMS mapping error: %s\nCannot recover\n", strerror(errno));
    }
    ems_mapping_context.pos_mapped[position] = 1;
    ems_mapping_context.pos_pagenum[position] = pagenum;
    ems_mapping_context.handle[position] = handle;
    ems_page[pagenum].status |= EMS_MAPPED;
}

/* Get a pointer from VM86 app, check it and return it. This returns NULL
 * if the pointer is not valid. We can check only for very limited
 * criteria: The pointer and the area defined by size may not point to
 * memory over 1MB and it may not may to addresses under 1kB, because there
 * is the VM86 interrupt table.
 */
static void 
*get_valid_pointer(u_short seg, u_short offs, u_long size)
{
    u_long addr;
    addr = MAKEPTR(seg, offs);
    /* Check bounds */
    if ((addr + size) >= (1024 * 1024) || addr < 1024)
	return NULL;
    else
	return (void *)addr;
}

/* Malloc a new handle */
static EMS_handle
*get_new_handle(long npages)
{
    EMS_handle *ehp;
    size_t dynsize = sizeof(EMS_handle) + sizeof(short) * npages;

    if ((ehp = calloc(1, dynsize)) == NULL)
	fatal("Cannot malloc EMS handle, cannot continue\n");
    return ehp;
}

/* Allocate a mapping context to a handle */
static void
context_to_handle(short handle)
{
    EMS_mapping_context *emc;

    if (ems_handle[handle] == NULL)
	fatal("EMS context_to_handle called with invalid handle\n");
    if ((emc = calloc(1, sizeof(EMS_mapping_context))) == NULL)
	fatal("EMS Cannot malloc mapping context, cannot continue\n");
    ems_handle[handle]->mcontext = emc;
    memmove((void *)emc, (void *)&ems_mapping_context, 
             sizeof(EMS_mapping_context));
}
   
/* Find the next free handle, returns -1 if there are no more handles */
static long
find_next_free_handle()
{
    int i;

    if (ems_alloc_handles >= 255)
	return (-1);
    /* handle 0 is OS handle */
    for (i = 1; i < EMS_NUM_HANDLES; i++) {
	if (ems_handle[i] == NULL)
	    return (i);
    }
    fatal("EMS handle count garbled, should not happen\n");
}

/* Look for a named handle, returns 0 if not found, else handle */
static short
lookup_handle(Hname *hp)
{
    int i;

    for (i = 1; i < EMS_NUM_HANDLES; i++) {
	if (ems_handle[i] != NULL) {
	    if (hp->ul_hn[0] == ems_handle[i]->hname.ul_hn[0] &&
	        hp->ul_hn[1] == ems_handle[i]->hname.ul_hn[1])
		return (i);
	}
    }
    return (0);
}    

/* Malloc a new handle struct and put into array at index handle */
static void
allocate_handle(short handle, long npages)
{
    if (ems_handle[handle] != NULL)
	fatal("EMS allocate_handle, handle was not free\n");
    ems_handle[handle] = get_new_handle(npages);
    ems_alloc_handles++;
}

/* Free a handle, return its memory. Call this *after* freeing the
 * allocated pages !
 */
static void
free_handle(short handle)
{
    if (ems_handle[handle] == NULL)
	fatal("EMS free_handle, handle was free\n");
    if (ems_handle[handle]->mcontext != NULL)
	free((void *)ems_handle[handle]->mcontext);
    free((void *)ems_handle[handle]);
    ems_handle[handle] = NULL;
    ems_alloc_handles--;
}


/* Allocates npages to handle. Call this routine only after you have
 * ensured there are enough free pages *and* the new handle is in place
 * in the handle array !
 */
static void
allocate_pages_to_handle(u_short handle, long npages)
{
    int syspagenum;
    int pages_to_alloc = npages;
    int allocpagenum = 0;

    /* sanity */
    if (handle > 255 || ems_handle[handle] == NULL)
	fatal("EMS allocate_pages_to_handle called with invalid handle\n");

    ems_handle[handle]->npages = npages;    
    for (syspagenum = 0; syspagenum < ems_total_pages; syspagenum++) {
	if (ems_page[syspagenum].status == EMS_FREE) {
	    ems_page[syspagenum].handle = handle;
	    ems_page[syspagenum].status = EMS_ALLOCED;
	    ems_handle[handle]->pagenum[allocpagenum] = syspagenum;
	    allocpagenum++;
	    pages_to_alloc--;
	    if (pages_to_alloc == 0)
		break;
	}
    }
    if (pages_to_alloc > 0)
	fatal("EMS allocate_pages_to_handle found not enough free pages\n");
    ems_alloc_pages += npages;
    ems_free_pages -= npages;
}

/* Reallocates npages to handle. Call this routine only after you have
 * ensured there are enough free pages *and* the new handle is in place
 * in the handle array !
 */
static void
reallocate_pages_to_handle(u_short handle, long npages)
{
    int syspagenum;
    int pages_to_alloc;
    int allocpagenum;
    long delta;
    size_t dynsize;
    EMS_handle *emp;

    /* sanity */
    if (handle > 255 || ems_handle[handle] == NULL)
	fatal("EMS allocate_pages_to_handle called with invalid handle\n");

    delta = npages - ems_handle[handle]->npages;
    if (delta > 0) {
	/* Grow array size and allocation */

	emp = ems_handle[handle];
	dynsize = sizeof(EMS_handle) + sizeof(short) * npages;

	/* First step: Make room in the handle pagenum array */
	if ((emp = (EMS_handle *)realloc((void *)emp, dynsize)) == NULL)
	    fatal("Cannot malloc EMS handle, cannot continue\n");
	ems_handle[handle] = emp;

	/* Second step: Add pages to the handle */ 
        pages_to_alloc = delta;
	allocpagenum = ems_handle[handle]->npages;
        ems_handle[handle]->npages = npages;    
        for (syspagenum = 0; syspagenum < ems_total_pages; syspagenum++) {
	    if (ems_page[syspagenum].status == EMS_FREE) {
	        ems_page[syspagenum].handle = handle;
	        ems_page[syspagenum].status = EMS_ALLOCED;
	        ems_handle[handle]->pagenum[allocpagenum] = syspagenum;
	        allocpagenum++;
	        pages_to_alloc--;
	        if (pages_to_alloc == 0)
		    break;
	    }
        }
    	if (pages_to_alloc > 0)
	    fatal("EMS allocate_pages_to_handle found not enough free pages\n");

    } else {
	/* Shrink array size and allocation */

	/* First step: Deallocate all pages from new size to old size */
	for (allocpagenum = npages; 
		allocpagenum < ems_handle[handle]->npages; 
		allocpagenum++) { 
	    syspagenum = ems_handle[handle]->pagenum[allocpagenum];

	    /* sanity */
            if (syspagenum > ems_total_pages)
                fatal("EMS free_pages_of_handle found invalid page number\n");
	    if (!(ems_page[syspagenum].status & EMS_ALLOCED))
	    	fatal("EMS free_pages_of_handle tried to free page already free\n");
	    ems_page[syspagenum].handle = 0;
	    ems_page[syspagenum].status = EMS_FREE;
	}

	/* Second step: Shrink the dynamic array of the handle */	
	dynsize = sizeof(EMS_handle) + sizeof(short) * npages;
	emp = ems_handle[handle];
	if ((emp = (EMS_handle *)realloc((void *)emp, dynsize)) == NULL)
	    fatal("Cannot realloc EMS handle, cannot continue\n");
	ems_handle[handle] = emp;
	ems_handle[handle]->npages = npages;
    }
    ems_alloc_pages += delta;
    ems_free_pages -= delta;
}

/* Free all pages belonging to a handle, handle must be valid */
static void
free_pages_of_handle(short handle)
{
    int allocpagenum;
    int syspagenum;
    int npages;

    /* sanity */

    if (handle > 255 || ems_handle[handle] == NULL)
	fatal("EMS free_pages_of_handle called with invalid handle\n");

    if ((npages = ems_handle[handle]->npages) == 0)
	return;

    for (allocpagenum = 0; allocpagenum < npages; allocpagenum++) {
	syspagenum = ems_handle[handle]->pagenum[allocpagenum];
	/* sanity */
	if (syspagenum > ems_total_pages)
	    fatal("EMS free_pages_of_handle found invalid page number\n");
	if (!(ems_page[syspagenum].status & EMS_ALLOCED))
	    fatal("EMS free_pages_of_handle tried to free page already free\n");
	ems_page[syspagenum].handle = 0;
	ems_page[syspagenum].status = EMS_FREE;
    }
    ems_alloc_pages -= npages;
    ems_free_pages += npages;
}

/* Restore a saved mapping context, overwrites current mapping context */
static void
restore_context(EMS_mapping_context *emc)
{
    int i;

    for (i = 0; i < 4; i++) {
	ems_mapping_context.handle[i] = emc->handle[i];
	if (emc->pos_mapped[i] != 0 &&
	    ems_mapping_context.pos_pagenum[i] != emc->pos_pagenum[i]) {
	    map_page(emc->pos_pagenum[i], (u_char) i, emc->handle[i], 0);
	} else {
	    ems_mapping_context.pos_mapped[i] = 0;
	}
    }
}

/* Prepare a special context save block for DOS and save it to
 * VM86 memory
 */
static void
save_context_to_dos(EMScontext *emp)
{
    int i, end;
    EMScontext context;
    u_short *sp;
    u_short sum;

    context.ems_saved_context = ems_mapping_context;
    context.magic = EMS_SAVEMAGIC;
    context.checksum = 0;
    sp = (u_short *)&context;
    end = sizeof(EMScontext) / sizeof(short);
    /* Generate checksum */
    for (i = 0, sum = 0; i < end; i++) {
	sum += *sp++;
	sum &= 0xffff;
    }
    context.checksum = 0x10000L - sum;
    /* Save it to VM86 memory */
    *emp = context;
}

/* Check a context returned from VM86 app for validity, return 0, if
 * not valid, else return 1
 */
static int
check_saved_context(EMScontext *emp)
{
    int i, end;
    u_short *sp;
    u_short sum;

    if (emp->magic != EMS_SAVEMAGIC)
	return 0;

    sp = (u_short *)emp;
    end = sizeof(EMScontext) / sizeof(short);
    /* Generate checksum */
    for (i = 0, sum = 0; i < end; i++) {
        sum += *sp++;
        sum &= 0xffff;
    }
    if (sum != 0)
	return 0;
    else
	return 1;
}

/* Helper routine for the move routines below: Check if length bytes
 * can be moved from/to handle pages (i.e are there enough pages)
 */
static int
check_alloc_pages(u_short handle, u_short firstpage, u_short offset, 
                  u_long length)
{
    u_long nbytes;

    if (firstpage > ems_handle[handle]->npages)
	return (0);
    nbytes = (ems_handle[handle]->npages - firstpage) * EMS_PAGESIZE - offset;
    return (ems_handle[handle]->npages >= nbytes);
}

/* Copy a block of memory up to the next 16kB boundary in the source
 * to the destination in upward direction (i.e. with ascending addresses)
 * XXX Could be an inline function.
 */
static void 
copy_block_up(struct copydesc *cdp)
{
    size_t size;
    void *srcp;
    void *dstp;

    /* If source or both memory types are EMS, source determines the
     * block lenght, else destination determines the block lenght
     */
    if (cdp->copytype & SRC_EMS)
	size = EMS_PAGESIZE - cdp->EMS_OFFS(src_addr);
    else
	size = EMS_PAGESIZE - cdp->EMS_OFFS(dst_addr);

    if (size > cdp->rest_len)
	size = cdp->rest_len;
 
    /* If src is EMS memory, it is mapped into position 0 */
    if (cdp->copytype & SRC_EMS)
	srcp = (void *)(ems_frame_addr + cdp->EMS_OFFS(src_addr));
    else
	srcp = (void *)(cdp->EMS_PTR(src_addr));

    /* If dest is EMS memory, it is mapped into position 1,2 */
    if (cdp->copytype & DST_EMS)
	dstp = (void *)(ems_frame_addr + EMS_PAGESIZE + 
					cdp->EMS_OFFS(dst_addr));
    else
	dstp = (void *)(cdp->EMS_PTR(dst_addr));

    /* Move this block */
    memmove(dstp, srcp, size);

    /* Update the copy descriptor: This updates the address of both 
     * conventional and EMS memory 
     */
    cdp->EMS_PTR(src_addr) += size;
    cdp->EMS_PTR(dst_addr) += size;

    cdp->rest_len -= size;
}


/* Move EMS memory starting with handle page src_seg and offset src_offset
 * to conventional memory dst_addr for length bytes
 * dst_addr is checked, handle is valid 
 */
static u_long 
move_ems_to_conv(short src_handle, u_short src_seg, 
			u_short src_offset, u_long dst_addr, u_long length)
{
    EMS_mapping_context ems_saved_context;
    EMS_handle *ehp;
    int pageindx = src_seg;
    struct copydesc cd;

    if (check_alloc_pages(src_handle, src_seg, src_offset, length) == 0)
	return EMS_MOVE_OVERFLOW;

    ehp = ems_handle[src_handle];

    /* Prepare the move: Save the mapping context */
    ems_saved_context = ems_mapping_context;

    /* Setup the copy descriptor struct */

    cd.copytype = SRC_EMS;
    cd.EMS_PAGE(src_addr) = ehp->pagenum[pageindx];
    cd.EMS_OFFS(src_addr) = src_offset;
    cd.EMS_PTR(dst_addr) = dst_addr;
    cd.rest_len = length;

    do {
	/* Map for the first block copy, source is mapped to position zero */
	map_page(cd.EMS_PAGE(src_addr), 0, src_handle, 0);
        copy_block_up(&cd);
    } while(cd.rest_len > 0);   

    /* Restore the original mapping */
    restore_context(&ems_saved_context);
    return EMS_SUCCESS;
}

/* Move conventional memory starting with src_addr
 * to EMS memory starting with handle page src_seg and offset src_offset
 * for length bytes
 * dst_addr is checked, handle is valid 
 */
static u_long
move_conv_to_ems(u_long src_addr, u_short dst_handle, u_short dst_seg,
                 u_short dst_offset, u_long length)
{
    EMS_mapping_context ems_saved_context;
    EMS_handle *ehp;
    int pageindx = dst_seg;
    struct copydesc cd;
    
    if (check_alloc_pages(dst_handle, dst_seg, dst_offset, length) == 0)
    	return EMS_MOVE_OVERFLOW;
    
    ehp = ems_handle[dst_handle];
    
    /* Prepare the move: Save the mapping context */
    ems_saved_context = ems_mapping_context;

    /* Setup the copy descriptor struct */

    cd.copytype = DST_EMS;
    cd.EMS_PAGE(dst_addr) = ehp->pagenum[pageindx];
    cd.EMS_OFFS(dst_addr) = dst_offset;
    cd.EMS_PTR(src_addr) = src_addr;
    cd.rest_len = length;

    do {
	map_page(cd.EMS_PAGE(dst_addr), 1, dst_handle, 0);
        copy_block_up(&cd);
    } while(cd.rest_len > 0);   

    /* Restore the original mapping */
    restore_context(&ems_saved_context);
    return EMS_SUCCESS;
}
    
static u_long
move_ems_to_ems(u_short src_handle, u_short src_seg, u_short src_offset,
                u_short dst_handle, u_short dst_seg, u_short dst_offset,
                u_long length)
{
    EMS_mapping_context ems_saved_context;
    EMS_handle *src_hp, *dst_hp;
    struct copydesc cd;
    
    if (check_alloc_pages(src_handle, src_seg, src_offset, length) == 0)
    	return EMS_MOVE_OVERFLOW;
    if (check_alloc_pages(dst_handle, dst_seg, dst_offset, length) == 0)
    	return EMS_MOVE_OVERFLOW;
    
    src_hp = ems_handle[src_handle];
    dst_hp = ems_handle[dst_handle];

    /* Prepare the move: Save the mapping context */
    ems_saved_context = ems_mapping_context;

    /* Setup the copy descriptor struct */

    cd.copytype = SRC_EMS | DST_EMS;
    cd.EMS_PAGE(src_addr) = src_hp->pagenum[src_seg];
    cd.EMS_OFFS(src_addr) = src_offset;
    cd.EMS_PAGE(dst_addr) = dst_hp->pagenum[dst_seg];
    cd.EMS_OFFS(dst_addr) = dst_offset;
    cd.rest_len = length;
    
    /* Copy */
    do {
        map_page(cd.EMS_PAGE(src_addr), 0, src_handle, 0);
	map_page(cd.EMS_PAGE(dst_addr), 1, dst_handle, 0);
        /* If there are more pages, map the next destination page to
         * position 2. This removes a compare between source and dest
         * offsets.
         */
        if (cd.EMS_PAGE(dst_addr) < dst_hp->npages)
	    map_page((cd.EMS_PAGE(dst_addr) + 1), 2, dst_handle, 0);        
        copy_block_up(&cd);
    } while(cd.rest_len > 0);   

    /* Restore the original mapping */
    restore_context(&ems_saved_context);
    return EMS_SUCCESS;
}
