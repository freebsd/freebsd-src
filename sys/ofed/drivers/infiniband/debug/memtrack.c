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

#define C_MEMTRACK_C

#ifdef kmalloc
        #undef kmalloc
#endif
#ifdef kfree
        #undef kfree
#endif
#ifdef vmalloc
        #undef vmalloc
#endif
#ifdef vfree
        #undef vfree
#endif
#ifdef kmem_cache_alloc
        #undef kmem_cache_alloc
#endif
#ifdef kmem_cache_free
        #undef kmem_cache_free
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <memtrack.h>

#include <linux/moduleparam.h>


MODULE_AUTHOR("Mellanox Technologies LTD.");
MODULE_DESCRIPTION("Memory allocations tracking");
MODULE_LICENSE("GPL");

#define MEMTRACK_HASH_SZ ((1<<15)-19)   /* prime: http://www.utm.edu/research/primes/lists/2small/0bit.html */
#define MAX_FILENAME_LEN 31

#define memtrack_spin_lock(spl, flags)     spin_lock_irqsave(spl, flags)
#define memtrack_spin_unlock(spl, flags)   spin_unlock_irqrestore(spl, flags)

/* if a bit is set then the corresponding allocation is tracked.
   bit0 corresponds to MEMTRACK_KMALLOC, bit1 corresponds to MEMTRACK_VMALLOC etc. */
static unsigned long track_mask = -1;   /* effectively everything */
module_param(track_mask, ulong, 0444);
MODULE_PARM_DESC(track_mask, "bitmask definenig what is tracked");

/* if a bit is set then the corresponding allocation is strictly tracked.
   That is, before inserting the whole range is checked to not overlap any
   of the allocations already in the database */
static unsigned long strict_track_mask = 0;     /* no strict tracking */
module_param(strict_track_mask, ulong, 0444);
MODULE_PARM_DESC(strict_track_mask, "bitmask which allocation requires strict tracking");

typedef struct memtrack_meminfo_st {
        unsigned long addr;
        unsigned long size;
        unsigned long line_num;
        struct memtrack_meminfo_st *next;
        struct list_head list;  /* used to link all items from a certain type together */
        char filename[MAX_FILENAME_LEN + 1];    /* putting the char array last is better for struct. packing */
} memtrack_meminfo_t;

static struct kmem_cache *meminfo_cache;

typedef struct {
        memtrack_meminfo_t *mem_hash[MEMTRACK_HASH_SZ];
        spinlock_t hash_lock;
        unsigned long count; /* size of memory tracked (*malloc) or number of objects tracked */
        struct list_head tracked_objs_head;     /* head of list of all objects */
        int strict_track;       /* if 1 then for each object inserted check if it overlaps any of the objects already in the list */
} tracked_obj_desc_t;

static tracked_obj_desc_t *tracked_objs_arr[MEMTRACK_NUM_OF_MEMTYPES];

static const char *rsc_names[MEMTRACK_NUM_OF_MEMTYPES] = {
        "kmalloc",
        "vmalloc",
        "kmem_cache_alloc"
};


static const char *rsc_free_names[MEMTRACK_NUM_OF_MEMTYPES] = {
        "kfree",
        "vfree",
        "kmem_cache_free"
};


static inline const char *memtype_alloc_str(memtrack_memtype_t memtype)
{
        switch (memtype) {
                case MEMTRACK_KMALLOC:
                case MEMTRACK_VMALLOC:
                case MEMTRACK_KMEM_OBJ:
                        return rsc_names[memtype];
                default:
                        return "(Unknown allocation type)";
        }
}

static inline const char *memtype_free_str(memtrack_memtype_t memtype)
{
        switch (memtype) {
                case MEMTRACK_KMALLOC:
                case MEMTRACK_VMALLOC:
                case MEMTRACK_KMEM_OBJ:
                        return rsc_free_names[memtype];
                default:
                        return "(Unknown allocation type)";
        }
}

/*
 *  overlap_a_b
 */
static int overlap_a_b(unsigned long a_start, unsigned long a_end,
                       unsigned long b_start, unsigned long b_end)
{
        if ((b_start > a_end) || (a_start > b_end)) {
                return 0;
        }
        return 1;
}

/*
 *  check_overlap
 */
static void check_overlap(memtrack_memtype_t memtype,
                          memtrack_meminfo_t * mem_info_p,
                          tracked_obj_desc_t * obj_desc_p)
{
        struct list_head *pos, *next;
        memtrack_meminfo_t *cur;
        unsigned long start_a, end_a, start_b, end_b;

        list_for_each_safe(pos, next, &obj_desc_p->tracked_objs_head) {
                cur = list_entry(pos, memtrack_meminfo_t, list);

                start_a = mem_info_p->addr;
                end_a = mem_info_p->addr + mem_info_p->size - 1;
                start_b = cur->addr;
                end_b = cur->addr + cur->size - 1;

                if (overlap_a_b(start_a, end_a, start_b, end_b)) {
                        printk
                            ("%s overlaps! new_start=0x%lx, new_end=0x%lx, item_start=0x%lx, item_end=0x%lx\n",
                             memtype_alloc_str(memtype), mem_info_p->addr,
                             mem_info_p->addr + mem_info_p->size - 1, cur->addr,
                             cur->addr + cur->size - 1);
                }
        }
}

/* Invoke on memory allocation */
void memtrack_alloc(memtrack_memtype_t memtype, unsigned long addr,
                    unsigned long size, const char *filename,
                    const unsigned long line_num, int alloc_flags)
{
        unsigned long hash_val;
        memtrack_meminfo_t *cur_mem_info_p, *new_mem_info_p;
        tracked_obj_desc_t *obj_desc_p;
        unsigned long flags;

        if (memtype >= MEMTRACK_NUM_OF_MEMTYPES) {
                printk("%s: Invalid memory type (%d)\n", __func__, memtype);
                return;
        }

        if (!tracked_objs_arr[memtype]) {
                /* object is not tracked */
                return;
        }
        obj_desc_p = tracked_objs_arr[memtype];

        hash_val = addr % MEMTRACK_HASH_SZ;

        new_mem_info_p = (memtrack_meminfo_t *)
            kmem_cache_alloc(meminfo_cache, alloc_flags);
        if (new_mem_info_p == NULL) {
                printk
                    ("%s: Failed allocating kmem_cache item for new mem_info. "
                     "Lost tracking on allocation at %s:%lu...\n", __func__,
                     filename, line_num);
                return;
        }
        /* save allocation properties */
        new_mem_info_p->addr = addr;
        new_mem_info_p->size = size;
        new_mem_info_p->line_num = line_num;
        /* Make sure that we will print out the path tail if the given filename is longer
         * than MAX_FILENAME_LEN. (otherwise, we will not see the name of the actual file
         * in the printout -- only the path head!
         */
        if (strlen(filename) > MAX_FILENAME_LEN) {
          strncpy(new_mem_info_p->filename, filename + strlen(filename) - MAX_FILENAME_LEN, MAX_FILENAME_LEN);
        } else {
          strncpy(new_mem_info_p->filename, filename, MAX_FILENAME_LEN);
        }
        new_mem_info_p->filename[MAX_FILENAME_LEN] = 0; /* NULL terminate anyway */

        memtrack_spin_lock(&obj_desc_p->hash_lock, flags);
        /* make sure given memory location is not already allocated */
        cur_mem_info_p = obj_desc_p->mem_hash[hash_val];
        while (cur_mem_info_p != NULL) {
                if (cur_mem_info_p->addr == addr) {
                        /* Found given address in the database */
                        printk
                            ("mtl rsc inconsistency: %s: %s::%lu: %s @ addr=0x%lX which is already known from %s:%lu\n",
                             __func__, filename, line_num,
                             memtype_alloc_str(memtype), addr,
                             cur_mem_info_p->filename,
                             cur_mem_info_p->line_num);
                        memtrack_spin_unlock(&obj_desc_p->hash_lock, flags);
                        kmem_cache_free(meminfo_cache, new_mem_info_p);
                        return;
                }
                cur_mem_info_p = cur_mem_info_p->next;
        }
        /* not found - we can put in the hash bucket */
        /* link as first */
        new_mem_info_p->next = obj_desc_p->mem_hash[hash_val];
        obj_desc_p->mem_hash[hash_val] = new_mem_info_p;
        if (obj_desc_p->strict_track) {
                check_overlap(memtype, new_mem_info_p, obj_desc_p);
        }
        obj_desc_p->count += size;
        list_add(&new_mem_info_p->list, &obj_desc_p->tracked_objs_head);

        memtrack_spin_unlock(&obj_desc_p->hash_lock, flags);
        return;
}

/* Invoke on memory free */
void memtrack_free(memtrack_memtype_t memtype, unsigned long addr,
                   const char *filename, const unsigned long line_num)
{
        unsigned long hash_val;
        memtrack_meminfo_t *cur_mem_info_p, *prev_mem_info_p;
        tracked_obj_desc_t *obj_desc_p;
        unsigned long flags;

        if (memtype >= MEMTRACK_NUM_OF_MEMTYPES) {
                printk("%s: Invalid memory type (%d)\n", __func__, memtype);
                return;
        }

        if (!tracked_objs_arr[memtype]) {
                /* object is not tracked */
                return;
        }
        obj_desc_p = tracked_objs_arr[memtype];

        hash_val = addr % MEMTRACK_HASH_SZ;

        memtrack_spin_lock(&obj_desc_p->hash_lock, flags);
        /* find  mem_info of given memory location */
        prev_mem_info_p = NULL;
        cur_mem_info_p = obj_desc_p->mem_hash[hash_val];
        while (cur_mem_info_p != NULL) {
                if (cur_mem_info_p->addr == addr) {
                        /* Found given address in the database - remove from the bucket/list */
                        if (prev_mem_info_p == NULL) {
                                obj_desc_p->mem_hash[hash_val] = cur_mem_info_p->next;  /* removing first */
                        } else {
                                prev_mem_info_p->next = cur_mem_info_p->next;   /* "crossover" */
                        }
                        list_del(&cur_mem_info_p->list);

                        obj_desc_p->count -= cur_mem_info_p->size;
                        memtrack_spin_unlock(&obj_desc_p->hash_lock, flags);
                        kmem_cache_free(meminfo_cache, cur_mem_info_p);
                        return;
                }
                prev_mem_info_p = cur_mem_info_p;
                cur_mem_info_p = cur_mem_info_p->next;
        }

        /* not found */
        printk
            ("mtl rsc inconsistency: %s: %s::%lu: %s for unknown address=0x%lX\n",
             __func__, filename, line_num, memtype_free_str(memtype), addr);
        memtrack_spin_unlock(&obj_desc_p->hash_lock, flags);
        return;
}

/* Report current allocations status (for all memory types) */
static void memtrack_report(void)
{
        memtrack_memtype_t memtype;
        unsigned long cur_bucket;
        memtrack_meminfo_t *cur_mem_info_p;
        int serial = 1;
        tracked_obj_desc_t *obj_desc_p;
        unsigned long flags;

        printk("%s: Currently known allocations:\n", __func__);
        for (memtype = 0; memtype < MEMTRACK_NUM_OF_MEMTYPES; memtype++) {
                if (tracked_objs_arr[memtype]) {
                        printk("%d) %s:\n", serial, memtype_alloc_str(memtype));
                        obj_desc_p = tracked_objs_arr[memtype];
                        /* Scan all buckets to find existing allocations */
                        /* TBD: this may be optimized by holding a linked list of all hash items */
                        for (cur_bucket = 0; cur_bucket < MEMTRACK_HASH_SZ;
                             cur_bucket++) {
                                memtrack_spin_lock(&obj_desc_p->hash_lock, flags);      /* protect per bucket/list */
                                cur_mem_info_p =
                                    obj_desc_p->mem_hash[cur_bucket];
                                while (cur_mem_info_p != NULL) {        /* scan bucket */
                                        printk("%s::%lu: %s(%lu)==%lX\n",
                                               cur_mem_info_p->filename,
                                               cur_mem_info_p->line_num,
                                               memtype_alloc_str(memtype),
                                               cur_mem_info_p->size,
                                               cur_mem_info_p->addr);
                                        cur_mem_info_p = cur_mem_info_p->next;
                                }       /* while cur_mem_info_p */
                                memtrack_spin_unlock(&obj_desc_p->hash_lock, flags);
                        }       /* for cur_bucket */
                        serial++;
                }
        }                       /* for memtype */
}



static struct proc_dir_entry *memtrack_tree;

static memtrack_memtype_t get_rsc_by_name(const char *name)
{
        memtrack_memtype_t i;

        for (i=0; i<MEMTRACK_NUM_OF_MEMTYPES; ++i) {
                if (strcmp(name, rsc_names[i]) == 0) {
                        return i;
                }
        }

        return i;
}


static ssize_t memtrack_read(struct file *filp,
                                                 char __user *buf,
                                                         size_t size,
                                                         loff_t *offset)
{
        unsigned long cur, flags;
        loff_t pos = *offset;
        static char kbuf[20];
        static int file_len;
        int _read, to_ret, left;
        const char *fname;
        memtrack_memtype_t memtype;

        if (pos < 0)
                return -EINVAL;

        fname= filp->f_dentry->d_name.name;

        memtype= get_rsc_by_name(fname);
        if (memtype >= MEMTRACK_NUM_OF_MEMTYPES) {
                printk("invalid file name\n");
                return -EINVAL;
        }

        if ( pos == 0 ) {
                memtrack_spin_lock(&tracked_objs_arr[memtype]->hash_lock, flags);
                cur= tracked_objs_arr[memtype]->count;
                memtrack_spin_unlock(&tracked_objs_arr[memtype]->hash_lock, flags);
                _read = sprintf(kbuf, "%lu\n", cur);
                if ( _read < 0 ) {
                        return _read;
                }
                else {
                        file_len = _read;
                }
        }

        left = file_len - pos;
        to_ret = (left < size) ? left : size;
        if ( copy_to_user(buf, kbuf+pos, to_ret) ) {
                return -EFAULT;
        }
        else {
                *offset = pos + to_ret;
                return to_ret;
        }
}

static struct file_operations memtrack_proc_fops = {
        .read = memtrack_read,
};

static const char *memtrack_proc_entry_name = "mt_memtrack";

static int create_procfs_tree(void)
{
        struct proc_dir_entry *dir_ent;
        struct proc_dir_entry *proc_ent;
        int i, j;
        unsigned long bit_mask;

        dir_ent = proc_mkdir(memtrack_proc_entry_name, NULL);
        if ( !dir_ent ) {
                return -1;
        }

        memtrack_tree = dir_ent;

        for (i=0, bit_mask=1; i<MEMTRACK_NUM_OF_MEMTYPES; ++i, bit_mask<<=1) {
                if (bit_mask & track_mask) {
                        proc_ent = create_proc_entry(rsc_names[i], S_IRUGO, memtrack_tree);
                        if ( !proc_ent )
                                goto undo_create_root;

			proc_ent->proc_fops = &memtrack_proc_fops;
                }
        }

        goto exit_ok;

undo_create_root:
        for (j=0, bit_mask=1; j<i; ++j, bit_mask<<=1) {
                if (bit_mask & track_mask) {
                        remove_proc_entry(rsc_names[j], memtrack_tree);
                }
        }
        remove_proc_entry(memtrack_proc_entry_name, NULL);
        return -1;

exit_ok:
        return 0;
}


static void destroy_procfs_tree(void)
{
        int i;
        unsigned long bit_mask;

        for (i=0, bit_mask=1; i<MEMTRACK_NUM_OF_MEMTYPES; ++i, bit_mask<<=1) {
                if (bit_mask & track_mask) {
                        remove_proc_entry(rsc_names[i], memtrack_tree);
                }
        }
        remove_proc_entry(memtrack_proc_entry_name, NULL);
}


/* module entry points */

int init_module(void)
{
        memtrack_memtype_t i;
        int j;
        unsigned long bit_mask;


        /* create a cache for the memtrack_meminfo_t strcutures */
        meminfo_cache = kmem_cache_create("memtrack_meminfo_t",
                                          sizeof(memtrack_meminfo_t), 0,
                                          SLAB_HWCACHE_ALIGN, NULL);
        if (!meminfo_cache) {
                printk("memtrack::%s: failed to allocate meminfo cache\n", __func__);
                return -1;
        }

        /* initialize array of descriptors */
        memset(tracked_objs_arr, 0, sizeof(tracked_objs_arr));

        /* create a tracking object descriptor for all required objects */
        for (i = 0, bit_mask = 1; i < MEMTRACK_NUM_OF_MEMTYPES;
             ++i, bit_mask <<= 1) {
                if (bit_mask & track_mask) {
                        tracked_objs_arr[i] =
                            vmalloc(sizeof(tracked_obj_desc_t));
                        if (!tracked_objs_arr[i]) {
                                printk("memtrack: failed to allocate tracking object\n");
                                goto undo_cache_create;
                        }

                        memset(tracked_objs_arr[i], 0, sizeof(tracked_obj_desc_t));
                        spin_lock_init(&tracked_objs_arr[i]->hash_lock);
                        INIT_LIST_HEAD(&tracked_objs_arr[i]->tracked_objs_head);
                        if (bit_mask & strict_track_mask) {
                                tracked_objs_arr[i]->strict_track = 1;
                        } else {
                                tracked_objs_arr[i]->strict_track = 0;
                        }
                }
        }


        if ( create_procfs_tree() ) {
                  printk("%s: create_procfs_tree() failed\n", __FILE__);
                  goto undo_cache_create;
        }


        printk("memtrack::%s done.\n", __func__);

        return 0;

undo_cache_create:
        for (j=0; j<i; ++j) {
                if (tracked_objs_arr[j]) {
                        vfree(tracked_objs_arr[j]);
                }
        }

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
        if (kmem_cache_destroy(meminfo_cache) != 0) {
                printk("Failed on kmem_cache_destroy !\n");
        }
#else
        kmem_cache_destroy(meminfo_cache);
#endif
        return -1;
}


void cleanup_module(void)
{
        memtrack_memtype_t memtype;
        unsigned long cur_bucket;
        memtrack_meminfo_t *cur_mem_info_p, *next_mem_info_p;
        tracked_obj_desc_t *obj_desc_p;
        unsigned long flags;


        memtrack_report();


        destroy_procfs_tree();

        /* clean up any hash table left-overs */
        for (memtype = 0; memtype < MEMTRACK_NUM_OF_MEMTYPES; memtype++) {
                /* Scan all buckets to find existing allocations */
                /* TBD: this may be optimized by holding a linked list of all hash items */
                if (tracked_objs_arr[memtype]) {
                        obj_desc_p = tracked_objs_arr[memtype];
                        for (cur_bucket = 0; cur_bucket < MEMTRACK_HASH_SZ;
                             cur_bucket++) {
                                memtrack_spin_lock(&obj_desc_p->hash_lock, flags);      /* protect per bucket/list */
                                cur_mem_info_p =
                                    obj_desc_p->mem_hash[cur_bucket];
                                while (cur_mem_info_p != NULL) {        /* scan bucket */
                                        next_mem_info_p = cur_mem_info_p->next; /* save "next" pointer before the "free" */
                                        kmem_cache_free(meminfo_cache,
                                                        cur_mem_info_p);
                                        cur_mem_info_p = next_mem_info_p;
                                }       /* while cur_mem_info_p */
                                memtrack_spin_unlock(&obj_desc_p->hash_lock, flags);
                        }       /* for cur_bucket */
                        vfree(obj_desc_p);
                }
        }                       /* for memtype */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
        if (kmem_cache_destroy(meminfo_cache) != 0) {
                printk
                    ("memtrack::cleanup_module: Failed on kmem_cache_destroy !\n");
        }
#else
        kmem_cache_destroy(meminfo_cache);
#endif
        printk("memtrack::cleanup_module done.\n");
}

EXPORT_SYMBOL(memtrack_alloc);
EXPORT_SYMBOL(memtrack_free);

//module_init(memtrack_init)
//module_exit(memtrack_exit)

