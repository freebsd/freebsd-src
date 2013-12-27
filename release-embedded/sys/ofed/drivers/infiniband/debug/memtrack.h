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

typedef enum {
        MEMTRACK_KMALLOC,
        MEMTRACK_VMALLOC,
        MEMTRACK_KMEM_OBJ,
        MEMTRACK_NUM_OF_MEMTYPES
} memtrack_memtype_t;

/* Invoke on memory allocation */
void memtrack_alloc(memtrack_memtype_t memtype, unsigned long addr,
                    unsigned long size, const char *filename,
                    const unsigned long line_num, int alloc_flags);

/* Invoke on memory free */
void memtrack_free(memtrack_memtype_t memtype, unsigned long addr,
                   const char *filename, const unsigned long line_num);

/* Report current allocations status (for all memory types) */
/* we do not export this function since it is used by cleanup_module only */
/* void memtrack_report(void); */

#endif
