
/*
 * mtd/include/compatmac.h
 *
 * $Id: compatmac.h,v 1.45 2003/01/24 15:50:57 dwmw2 Exp $
 *
 * Extensions and omissions from the normal 'linux/compatmac.h'
 * files. hopefully this will end up empty as the 'real' one 
 * becomes fully-featured.
 */


/* First, include the parts which the kernel is good enough to provide 
 * to us 
 */
   
#ifndef __LINUX_MTD_COMPATMAC_H__
#define __LINUX_MTD_COMPATMAC_H__

#include <linux/config.h>
#include <linux/module.h>
#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#endif

#ifndef VERSION_CODE
#  define VERSION_CODE(vers,rel,seq) ( ((vers)<<16) | ((rel)<<8) | (seq) )
#endif
#ifndef KERNEL_VERSION
#  define KERNEL_VERSION(a,b,c) VERSION_CODE(a,b,c)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,0,0)
#  error "This kernel is too old: not supported by this file"
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,1,0)
#include <linux/types.h> /* used later in this header */

#define memcpy_fromio(a,b,c)    memcpy((a),(void *)(b),(c))
#define memcpy_toio(a,b,c)      memcpy((void *)(a),(b),(c))

typedef struct wait_queue * wait_queue_head_t;

#define DECLARE_WAITQUEUE(x,y) struct wait_queue x = {y,NULL}
#define DECLARE_WAIT_QUEUE_HEAD(x) struct wait_queue *x = NULL
#define init_waitqueue_head init_waitqueue
#define DECLARE_MUTEX(x) struct semaphore x = MUTEX
#define DECLARE_MUTEX_LOCKED(x) struct semaphore x = MUTEX_LOCKED

/* from sysdep-2.1.h */
#  include <asm/segment.h>
#  define access_ok(t,a,sz)           (verify_area((t),(a),(sz)) ? 0 : 1)
#  define verify_area_20              verify_area
#  define copy_to_user(t,f,n)         (memcpy_tofs(t,f,n), 0)
#  define __copy_to_user(t,f,n)       copy_to_user((t),(f),(n))
#  define copy_to_user_ret(t,f,n,r)   copy_to_user((t),(f),(n))
#  define copy_from_user(t,f,n)       (memcpy_fromfs((t),(f),(n)), 0)
#  define __copy_from_user(t,f,n)     copy_from_user((t),(f),(n))
#  define copy_from_user_ret(t,f,n,r) copy_from_user((t),(f),(n))
//xxx #  define PUT_USER(val,add)           (put_user((val),(add)), 0)
#  define Put_user(val,add)           (put_user((val),(add)), 0)
#  define __PUT_USER(val,add)         PUT_USER((val),(add))
#  define PUT_USER_RET(val,add,ret)   PUT_USER((val),(add))
#  define GET_USER(dest,add)          ((dest)=get_user((add)), 0)
#  define __GET_USER(dest,add)        GET_USER((dest),(add))
#  define GET_USER_RET(dest,add,ret)  GET_USER((dest),(add))

#define ioremap(offset,size) vremap(offset,size)
#define iounmap(adr)	/* */

#define EXPORT_SYMBOL(s) /* */
#define EXPORT_SYMBOL_NOVERS(s) /* */

/* 2.1.10 and 2.1.43 introduced new functions. They are worth using */

#if LINUX_VERSION_CODE < VERSION_CODE(2,1,10)

#  include <asm/byteorder.h>
#  ifdef __LITTLE_ENDIAN
#    define cpu_to_le16(x) (x)
#    define cpu_to_le32(x) (x)
#    define cpu_to_be16(x) htons((x))
#    define cpu_to_be32(x) htonl((x))
#  else
#    define cpu_to_be16(x) (x)
#    define cpu_to_be32(x) (x)
     extern inline __u16 cpu_to_le16(__u16 x) { return (x<<8) | (x>>8);}
     extern inline __u32 cpu_to_le32(__u32 x) { return((x>>24) |
             ((x>>8)&0xff00) | ((x<<8)&0xff0000) | (x<<24));}
#  endif

#  define le16_to_cpu(x)  cpu_to_le16(x)
#  define le32_to_cpu(x)  cpu_to_le32(x)
#  define be16_to_cpu(x)  cpu_to_be16(x)
#  define be32_to_cpu(x)  cpu_to_be32(x)

#endif

#if LINUX_VERSION_CODE < VERSION_CODE(2,1,43)
#  define cpu_to_le16p(addr) (cpu_to_le16(*(addr)))
#  define cpu_to_le32p(addr) (cpu_to_le32(*(addr)))
#  define cpu_to_be16p(addr) (cpu_to_be16(*(addr)))
#  define cpu_to_be32p(addr) (cpu_to_be32(*(addr)))

   extern inline void cpu_to_le16s(__u16 *a) {*a = cpu_to_le16(*a);}
   extern inline void cpu_to_le32s(__u16 *a) {*a = cpu_to_le32(*a);}
   extern inline void cpu_to_be16s(__u16 *a) {*a = cpu_to_be16(*a);}
   extern inline void cpu_to_be32s(__u16 *a) {*a = cpu_to_be32(*a);}

#  define le16_to_cpup(x) cpu_to_le16p(x)
#  define le32_to_cpup(x) cpu_to_le32p(x)
#  define be16_to_cpup(x) cpu_to_be16p(x)
#  define be32_to_cpup(x) cpu_to_be32p(x)

#  define le16_to_cpus(x) cpu_to_le16s(x)
#  define le32_to_cpus(x) cpu_to_le32s(x)
#  define be16_to_cpus(x) cpu_to_be16s(x)
#  define be32_to_cpus(x) cpu_to_be32s(x)
#endif

// from 2.2, linux/types.h
#ifndef __BIT_TYPES_DEFINED__
#define __BIT_TYPES_DEFINED__

typedef         __u8            u_int8_t;
typedef         __s8            int8_t;
typedef         __u16           u_int16_t;
typedef         __s16           int16_t;
typedef         __u32           u_int32_t;
typedef         __s32           int32_t;

#endif /* !(__BIT_TYPES_DEFINED__) */

#if (__GNUC__ > 2) || (__GNUC__ == 2 && __GNUC_MINOR__ >= 8)
  typedef struct { } spinlock_t;
  #define SPIN_LOCK_UNLOCKED (spinlock_t) { }
#else
  typedef struct { int gcc_is_buggy; } spinlock_t;
  #define SPIN_LOCK_UNLOCKED (spinlock_t) { 0 }
#endif

#define spin_lock_init(lock)    do { } while(0)
#define spin_lock(lock)         (void)(lock) /* Not "unused variable". */
#define spin_trylock(lock)      (1)
#define spin_unlock_wait(lock)  do { } while(0)
#define spin_unlock(lock)       do { } while(0)
#define spin_lock_irq(lock)     cli()
#define spin_unlock_irq(lock)   sti()

#define spin_lock_irqsave(lock, flags) \
        do { save_flags(flags); cli(); } while (0)
#define spin_unlock_irqrestore(lock, flags) \
        restore_flags(flags)

// Doesn't work when tqueue.h is included. 
// #define queue_task                   queue_task_irq_off
#define tty_flip_buffer_push(tty)    queue_task_irq_off(&tty->flip.tqueue, &tq_timer)
#define signal_pending(current)      (current->signal & ~current->blocked)
#define schedule_timeout(to)         do {current->timeout = jiffies + (to);schedule ();} while (0)
#define time_after(t1,t2)            (((long)t1-t2) > 0)

#else
  #include <linux/compatmac.h>
#endif  // LINUX_VERSION_CODE < 0x020100


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,0)
#include <linux/vmalloc.h>
#endif

/* Modularization issues */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,1,18)
#  define __USE_OLD_SYMTAB__
#  define EXPORT_NO_SYMBOLS register_symtab(NULL);
#  define REGISTER_SYMTAB(tab) register_symtab(tab)
#else
#  define REGISTER_SYMTAB(tab) /* nothing */
#endif

#ifdef __USE_OLD_SYMTAB__
#  define __MODULE_STRING(s)         /* nothing */
#  define MODULE_PARM(v,t)           /* nothing */
#  define MODULE_PARM_DESC(v,t)      /* nothing */
#  define MODULE_AUTHOR(n)           /* nothing */
#  define MODULE_DESCRIPTION(d)      /* nothing */
#  define MODULE_SUPPORTED_DEVICE(n) /* nothing */
#endif

/*
 * "select" changed in 2.1.23. The implementation is twin, but this
 * header is new
 */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,1,22)
#  include <linux/poll.h>
#else
#  define __USE_OLD_SELECT__
#endif

/* Other change in the fops are solved using pseudo-types */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,0)
#  define lseek_t      long long
#  define lseek_off_t  long long
#else
#  define lseek_t      int
#  define lseek_off_t  off_t
#endif

/* changed the prototype of read/write */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,0) || defined(__alpha__)
# define count_t unsigned long
# define read_write_t long
#else
# define count_t int
# define read_write_t int
#endif


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,1,31)
# define release_t void
#  define release_return(x) return
#else
#  define release_t int
#  define release_return(x) return (x)
#endif

#if LINUX_VERSION_CODE < 0x20300
#define __exit
#endif
#if LINUX_VERSION_CODE < 0x20200
#define __init
#else
#include <linux/init.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,18)
#define init_MUTEX(x) do {*(x) = MUTEX;} while (0)
#define init_MUTEX_LOCKED(x) do {*(x) = MUTEX_LOCKED;} while (0)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0)
#define RQFUNC_ARG void
#define blkdev_dequeue_request(req) do {CURRENT = req->next;} while (0)
#else
#define RQFUNC_ARG request_queue_t *q
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,32)
#define blk_cleanup_queue(nr) do {blk_dev[nr].request_fn = 0;} while(0)
#define BLK_DEFAULT_QUEUE(nr) (blk_dev[nr].request_fn)
#define blk_init_queue(q, rq) do {q = rq;} while(0)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,0)
#ifdef CONFIG_MODULES
#define __MOD_INC_USE_COUNT(mod)                                        \
        (atomic_inc(&(mod)->uc.usecount), (mod)->flags |= MOD_VISITED|MOD_USED_ONCE)
#define __MOD_DEC_USE_COUNT(mod)                                        \
        (atomic_dec(&(mod)->uc.usecount), (mod)->flags |= MOD_VISITED)
#else
#define __MOD_INC_USE_COUNT(mod)
#define __MOD_DEC_USE_COUNT(mod)
#endif
#endif


#ifndef HAVE_INTER_MODULE
static inline void *inter_module_get(char *x) {return NULL;}
static inline void *inter_module_get_request(char *x, char *y) {return NULL;}
static inline void inter_module_put(const char *x) {}
static inline void inter_module_register(const char *x, struct module *y, const void *z) {}
static inline void inter_module_unregister(const char *x) {}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,18)

#define DECLARE_WAIT_QUEUE_HEAD(x) struct wait_queue *x = NULL
#define init_waitqueue_head init_waitqueue

#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0)

static inline int try_inc_mod_count(struct module *mod)
{
#ifdef CONFIG_MODULES
	if (mod)
		__MOD_INC_USE_COUNT(mod);
#endif
	return 1;
}
#endif


/* Yes, I'm aware that it's a fairly ugly hack.
   Until the __constant_* macros appear in Linus' own kernels, this is
   the way it has to be done.
 DW 19/1/00
 */

#include <asm/byteorder.h>

#ifndef __constant_cpu_to_le16

#ifdef __BIG_ENDIAN
#define __constant_cpu_to_le64(x) ___swab64((x))
#define __constant_le64_to_cpu(x) ___swab64((x))
#define __constant_cpu_to_le32(x) ___swab32((x))
#define __constant_le32_to_cpu(x) ___swab32((x))
#define __constant_cpu_to_le16(x) ___swab16((x))
#define __constant_le16_to_cpu(x) ___swab16((x))
#define __constant_cpu_to_be64(x) ((__u64)(x))
#define __constant_be64_to_cpu(x) ((__u64)(x))
#define __constant_cpu_to_be32(x) ((__u32)(x))
#define __constant_be32_to_cpu(x) ((__u32)(x))
#define __constant_cpu_to_be16(x) ((__u16)(x))
#define __constant_be16_to_cpu(x) ((__u16)(x))
#else
#ifdef __LITTLE_ENDIAN
#define __constant_cpu_to_le64(x) ((__u64)(x))
#define __constant_le64_to_cpu(x) ((__u64)(x))
#define __constant_cpu_to_le32(x) ((__u32)(x))
#define __constant_le32_to_cpu(x) ((__u32)(x))
#define __constant_cpu_to_le16(x) ((__u16)(x))
#define __constant_le16_to_cpu(x) ((__u16)(x))
#define __constant_cpu_to_be64(x) ___swab64((x))
#define __constant_be64_to_cpu(x) ___swab64((x))
#define __constant_cpu_to_be32(x) ___swab32((x))
#define __constant_be32_to_cpu(x) ___swab32((x))
#define __constant_cpu_to_be16(x) ___swab16((x))
#define __constant_be16_to_cpu(x) ___swab16((x))
#else
#error No (recognised) endianness defined (unless it,s PDP)
#endif /* __LITTLE_ENDIAN */
#endif /* __BIG_ENDIAN */

#endif /* ifndef __constant_cpu_to_le16 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0)
  #define mod_init_t int  __init
  #define mod_exit_t void  
#else
  #define mod_init_t static int __init
  #define mod_exit_t static void __exit
#endif

#ifndef THIS_MODULE
#ifdef MODULE
#define THIS_MODULE (&__this_module)
#else
#define THIS_MODULE (NULL)
#endif
#endif

#if LINUX_VERSION_CODE < 0x20300
#include <linux/interrupt.h>
#define spin_lock_bh(lock) do {start_bh_atomic();spin_lock(lock);}while(0)
#define spin_unlock_bh(lock) do {spin_unlock(lock);end_bh_atomic();}while(0)
#else
#include <asm/softirq.h>
#include <linux/spinlock.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,18)
#define set_current_state(state_value)                        \
        do { current->state = (state_value); } while (0)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,0) 
static inline int invalidate_device(kdev_t dev, int do_sync) {

	if (do_sync)
		fsync_dev(dev);
	
	invalidate_buffers(dev);
	return 0;
}
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,4,5)
static inline int invalidate_device(kdev_t dev, int do_sync) {
	struct super_block *sb = get_super(dev);
	int res = 0;

	if (do_sync)
		fsync_dev(dev);
	
	if (sb)
		res = invalidate_inodes(sb);

	invalidate_buffers(dev);
	return res;
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,10)
#undef min
#undef max
#undef min_t
#undef max_t
/*
 * min()/max() macros that also do
 * strict type-checking.. See the
 * "unnecessary" pointer comparison.
 */
#define min(x,y) ({ \
        const typeof(x) _x = (x);       \
        const typeof(y) _y = (y);       \
        (void) (&_x == &_y);            \
        _x < _y ? _x : _y; })

#define max(x,y) ({ \
        const typeof(x) _x = (x);       \
        const typeof(y) _y = (y);       \
        (void) (&_x == &_y);            \
        _x > _y ? _x : _y; })

/*
 * ..and if you can't take the strict
 * types, you can specify one yourself.
 *
 * Or not use min/max at all, of course.
 */
#define min_t(type,x,y) \
        ({ type __x = (x); type __y = (y); __x < __y ? __x: __y; })
#define max_t(type,x,y) \
        ({ type __x = (x); type __y = (y); __x > __y ? __x: __y; })
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,7)
struct completion {
	struct semaphore s;
};

#define complete(c) up(&(c)->s)
#define wait_for_completion(c) down(&(c)->s)
#define init_completion(c) init_MUTEX_LOCKED(&(c)->s);

#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,9)
/* This came later */
#define complete_and_exit(c, r) do { complete(c); do_exit(r); } while(0)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,9) || \
    (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,10) && !defined(__rh_config_h__))

#include <linux/genhd.h>

static inline void add_gendisk(struct gendisk *gp)
{
	gp->next = gendisk_head;
	gendisk_head = gp;
}

static inline void del_gendisk(struct gendisk *gp)
{
	struct gendisk *gd, **gdp;

	for (gdp = &gendisk_head; *gdp; gdp = &((*gdp)->next))
		if (*gdp == gp) {
			gd = *gdp; *gdp = gd->next;
			break;
		}
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,18) && defined(MODULE)

#define module_init(func)		\
mod_init_t init_module(void) {		\
	return func();			\
}

#define module_exit(func)		\
mod_exit_t cleanup_module(void) {	\
	return func();			\
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,9) || \
    (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,10) && !defined(__rh_config_h__))
#define MODULE_LICENSE(x) /* */
#endif

/* Removed for 2.4.21 kernel. This really should have been renamed
   when it was changed -- this is a PITA */
#if 0 && LINUX_VERSION_CODE < KERNEL_VERSION(2,5,5)
#include <linux/sched.h>
static inline void __recalc_sigpending(void)
{
	recalc_sigpending(current);
}
#undef recalc_sigpending
#define recalc_sigpending() __recalc_sigpending ()
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,5)
#define parent_ino(d) ((d)->d_parent->d_inode->i_ino)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,3)
#define need_resched() (current->need_resched)
#define cond_resched() do { if need_resched() schedule(); } while(0)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,19)
#ifndef yield
#define yield() do { set_current_state(TASK_RUNNING); schedule(); } while(0)
#endif
#ifndef minor
#define major(d) (MAJOR(to_kdev_t(d)))
#define minor(d) (MINOR(to_kdev_t(d)))
#endif
#ifndef mk_kdev
#define mk_kdev(ma,mi) MKDEV(ma,mi)
#define kdev_t_to_nr(x)	(x)
#endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
	/* Is this right? */
#define set_user_nice(tsk, n) do { (tsk)->priority = 20-(n); } while(0) 
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,4,21) && !defined(RED_HAT_LINUX_KERNEL)
#define set_user_nice(tsk, n) do { (tsk)->nice = n; } while(0)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,21)
#define rq_data_dir(x)	((x)->cmd)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)

#define IS_REQ_CMD(req) (1)

#define QUEUE_LOCK(q) (&io_request_lock)

#define BLK_INIT_QUEUE(q, req, lock) blk_init_queue((q), (req)) 

#else /* > 2.5.0 */

#define IS_REQ_CMD(req) ((req)->flags & REQ_CMD)

#define QUEUE_LOCK(q) ((q)->queue_lock)

#define BLK_INIT_QUEUE(q, req, lock) blk_init_queue((q), (req), (lock)) 

#endif

/* Removed cos it broke stuff. Where is this required anyway? 
 * #ifndef QUEUE_EMPTY
 * #define QUEUE_EMPTY  (!CURRENT)
 * #endif
 */
#if LINUX_VERSION_CODE < 0x20300
#define QUEUE_PLUGGED (blk_dev[MAJOR_NR].plug_tq.sync)
#elif LINUX_VERSION_CODE < 0x20500 //FIXME (Si)
#define QUEUE_PLUGGED (blk_dev[MAJOR_NR].request_queue.plugged)
#else
#define QUEUE_PLUGGED (blk_queue_plugged(QUEUE))
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,14)
#define BLK_INC_USE_COUNT MOD_INC_USE_COUNT
#define BLK_DEC_USE_COUNT MOD_DEC_USE_COUNT
#else
#define BLK_INC_USE_COUNT do {} while(0)
#define BLK_DEC_USE_COUNT do {} while(0)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,12)
#define PageUptodate(x) Page_Uptodate(x)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,48)
#define get_seconds() CURRENT_TIME
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,53)
#define generic_file_readonly_mmap generic_file_mmap
#endif

#endif /* __LINUX_MTD_COMPATMAC_H__ */
