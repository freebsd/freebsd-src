  /* 
   * This header tries to allow you to write 2.3-compatible drivers, 
   * but (using this header) still allows you to run them on 2.2 and 
   * 2.0 kernels. 
   *
   * Sometimes, a #define replaces a "construct" that older kernels
   * had. For example, 
   *
   *       DECLARE_MUTEX(name);
   *
   * replaces the older 
   *
   *       struct semaphore name = MUTEX;
   *
   * This file then declares the DECLARE_MUTEX macro to compile into the 
   * older version. 
   * 
   * In some cases, a macro or function changes the number of arguments.
   * In that case, there is nothing we can do except define an access 
   * macro that provides the same functionality on both versions of Linux. 
   * 
   * This is the case for example with the "get_user" macro 2.0 kernels use:
   *
   *          a = get_user (b);
   *  
   * while newer kernels use 
   * 
   *          get_user (a,b);
   *
   * This is unfortunate. We therefore define "Get_user (a,b)" which looks
   * almost the same as the 2.2+ construct, and translates into the 
   * appropriate sequence for earlier constructs. 
   * 
   * Supported by this file are the 2.0 kernels, 2.2 kernels, and the 
   * most recent 2.3 kernel. 2.3 support will be dropped as soon when 2.4
   * comes out. 2.0 support may someday be dropped. But then again, maybe 
   * not. 
   *
   * I'll try to maintain this, provided that Linus agrees with the setup. 
   * Feel free to mail updates or suggestions. 
   *
   * -- R.E.Wolff@BitWizard.nl
   *
   */

#ifndef COMPATMAC_H
#define COMPATMAC_H

#include <linux/version.h>

#if LINUX_VERSION_CODE < 0x020100    /* Less than 2.1.0 */
#define TWO_ZERO
#else
#if LINUX_VERSION_CODE < 0x020200   /* less than 2.2.x */
#warning "Please use a 2.2.x kernel. "
#else
#if LINUX_VERSION_CODE < 0x020300   /* less than 2.3.x */
#define TWO_TWO
#else
#define TWO_THREE
#endif
#endif
#endif

#ifdef TWO_ZERO

/* Here is the section that makes the 2.2 compatible driver source 
   work for 2.0 too! We mostly try to adopt the "new thingies" from 2.2, 
   and provide for compatibility stuff here if possible. */

/* Some 200 days (on intel) */
#define MAX_SCHEDULE_TIMEOUT     ((long)(~0UL>>1))

#include <linux/bios32.h>

#define Get_user(a,b)                a = get_user(b)
#define Put_user(a,b)                0,put_user(a,b)
#define copy_to_user(a,b,c)          memcpy_tofs(a,b,c)

static inline int copy_from_user(void *to,const void *from, int c) 
{
  memcpy_fromfs(to, from, c);
  return 0;
}

#define pci_present                  pcibios_present
#define pci_read_config_word         pcibios_read_config_word
#define pci_read_config_dword        pcibios_read_config_dword

static inline unsigned char get_irq (unsigned char bus, unsigned char fn)
{
	unsigned char t; 
	pcibios_read_config_byte (bus, fn, PCI_INTERRUPT_LINE, &t);
	return t;
}

static inline void *ioremap(unsigned long base, long length)
{
	if (base < 0x100000) return (void *)base;
	return vremap (base, length);
}

#define my_iounmap(x, b)             (((long)x<0x100000)?0:vfree ((void*)x))

#define capable(x)                   suser()

#define tty_flip_buffer_push(tty)    queue_task(&tty->flip.tqueue, &tq_timer)
#define signal_pending(current)      (current->signal & ~current->blocked)
#define schedule_timeout(to)         do {current->timeout = jiffies + (to);schedule ();} while (0)
#define time_after(t1,t2)            (((long)t1-t2) > 0)


#define test_and_set_bit(nr, addr)   set_bit(nr, addr)
#define test_and_clear_bit(nr, addr) clear_bit(nr, addr)

/* Not yet implemented on 2.0 */
#define ASYNC_SPD_SHI  -1
#define ASYNC_SPD_WARP -1


/* Ugly hack: the driver_name doesn't exist in 2.0.x . So we define it
   to the "name" field that does exist. As long as the assignments are
   done in the right order, there is nothing to worry about. */
#define driver_name           name 

/* Should be in a header somewhere. They are in tty.h on 2.2 */
#define TTY_HW_COOK_OUT       14 /* Flag to tell ntty what we can handle */
#define TTY_HW_COOK_IN        15 /* in hardware - output and input       */

/* The return type of a "close" routine. */
#define INT                   void
#define NO_ERROR              /* Nothing */

#else

/* The 2.2.x compatibility section. */
#include <asm/uaccess.h>


#define Get_user(a,b)         get_user(a,b)
#define Put_user(a,b)         put_user(a,b)
#define get_irq(pdev)         pdev->irq

#define INT                   int
#define NO_ERROR              0

#define my_iounmap(x,b)       (iounmap((char *)(b)))

#endif

#ifndef TWO_THREE
/* These are new in 2.3. The source now uses 2.3 syntax, and here is 
   the compatibility define... */
#define wait_queue_head_t     struct wait_queue *
#define DECLARE_MUTEX(name)   struct semaphore name = MUTEX
#define DECLARE_WAITQUEUE(wait, current) \
                              struct wait_queue wait = { current, NULL }

#endif


#endif
