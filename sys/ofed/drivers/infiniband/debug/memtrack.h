/*
  This software is available to you under a choice of one of two
  licenses.  You may choose to be licensed under the terms of the GNU
  General Public License (GPL) Version 2, available at
  <http://www.fsf.org/copyleft/gpl.html>, or the OpenIB.org BSD
  license, available in the LICENSE.TXT file accompanying this
  software.  These details are also available at
  <http://openib.org/license.html>.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
  ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

  Copyright (c) 2004 Mellanox Technologies Ltd.  All rights reserved.
*/

#ifndef H_MEMTRACK_H
#define H_MEMTRACK_H

enum memtrack_memtype_t {
        MEMTRACK_KMALLOC,
        MEMTRACK_VMALLOC,
        MEMTRACK_KMEM_OBJ,
        MEMTRACK_IOREMAP,       /* IO-RE/UN-MAP */
        MEMTRACK_WORK_QUEUE,    /* Handle work-queue create & destroy */
        MEMTRACK_PAGE_ALLOC,    /* Handle page allocation and free */
        MEMTRACK_DMA_MAP_SINGLE,/* Handle ib_dma_single map and unmap */
        MEMTRACK_DMA_MAP_PAGE,  /* Handle ib_dma_page map and unmap */
        MEMTRACK_DMA_MAP_SG,    /* Handle ib_dma_sg map and unmap with and without attributes */
        MEMTRACK_NUM_OF_MEMTYPES
};

/* Invoke on memory allocation */
void memtrack_alloc(enum memtrack_memtype_t memtype, unsigned long dev,
                    unsigned long addr, unsigned long size, unsigned long addr2,
                    int direction, const char *filename,
                    const unsigned long line_num, int alloc_flags);

/* Invoke on memory free */
void memtrack_free(enum memtrack_memtype_t memtype, unsigned long dev,
                   unsigned long addr, unsigned long size, int direction,
                   const char *filename, const unsigned long line_num);

/*
 * This function recognizes allocations which
 * may be released by kernel (e.g. skb & vnic) and
 * therefore not trackable by memtrack.
 * The allocations are recognized by the name
 * of their calling function.
 */
int is_non_trackable_alloc_func(const char *func_name);
/*
 * In some cases we need to free a memory
 * we defined as "non trackable" (see
 * is_non_trackable_alloc_func).
 * This function recognizes such releases
 * by the name of their calling function.
 */
int is_non_trackable_free_func(const char *func_name);

/* WA - In this function handles confirm
   the function name is
   '__ib_umem_release' or 'ib_umem_get'
   In this case we won't track the
   memory there because the kernel
   was the one who allocated it.
   Return value:
     1 - if the function name is match, else 0    */
int is_umem_put_page(const char *func_name);

/* Check page order size
   When Freeing a page allocation it checks whether
   we are trying to free the same amount of pages
   we ask to allocate (In log2(order)).
   In case an error if found it will print
   an error msg                                    */
int memtrack_check_size(enum memtrack_memtype_t memtype, unsigned long addr,
                         unsigned long size, const char *filename,
                         const unsigned long line_num);

/* Search for a specific addr whether it exist in the
   current data-base.
   If not it will print an error msg,
   Return value: 0 - if addr exist, else 1 */
int memtrack_is_new_addr(enum memtrack_memtype_t memtype, unsigned long addr, int expect_exist,
                   const char *filename, const unsigned long line_num);

/* Return current page reference counter */
int memtrack_get_page_ref_count(unsigned long addr);

/* Report current allocations status (for all memory types) */
/* we do not export this function since it is used by cleanup_module only */
/* void memtrack_report(void); */

/* Allow support of error injections */
int memtrack_inject_error(void);

/* randomize allocated memory */
int memtrack_randomize_mem(void);

#endif
