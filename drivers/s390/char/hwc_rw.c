/*
 *  drivers/s390/char/hwc_rw.c
 *     driver: reading from and writing to system console on S/390 via HWC
 *
 *  S390 version
 *    Copyright (C) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Martin Peschke <mpeschke@de.ibm.com>
 *
 * 
 *
 * 
 * 
 * 
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/ctype.h>
#include <linux/mm.h>
#include <linux/timer.h>
#include <linux/bootmem.h>
#include <linux/module.h>

#include <asm/ebcdic.h>
#include <asm/uaccess.h>
#include <asm/types.h>
#include <asm/bitops.h>
#include <asm/setup.h>
#include <asm/page.h>
#include <asm/s390_ext.h>
#include <asm/irq.h>

#ifndef MIN
#define MIN(a,b) (((a<b) ? a : b))
#endif

extern void ctrl_alt_del (void);

#define HWC_RW_PRINT_HEADER "hwc low level driver: "

#define  USE_VM_DETECTION

#define  DEFAULT_CASE_DELIMITER '%'

#undef DUMP_HWC_INIT_ERROR

#undef DUMP_HWC_WRITE_ERROR

#undef DUMP_HWC_WRITE_LIST_ERROR

#undef DUMP_HWC_READ_ERROR

#undef DUMP_HWCB_INPUT

#undef BUFFER_STRESS_TEST

typedef struct {
	unsigned char *next;
	unsigned short int mto_char_sum;
	unsigned char mto_number;
	unsigned char times_lost;
	unsigned short int mto_number_lost;
	unsigned long int mto_char_sum_lost;
} __attribute__ ((packed)) 

hwcb_list_t;

#define MAX_HWCB_ROOM (PAGE_SIZE - sizeof(hwcb_list_t))

#define MAX_MESSAGE_SIZE (MAX_HWCB_ROOM - sizeof(write_hwcb_t))

#define BUF_HWCB hwc_data.hwcb_list_tail
#define OUT_HWCB hwc_data.hwcb_list_head
#define ALL_HWCB_MTO hwc_data.mto_number
#define ALL_HWCB_CHAR hwc_data.mto_char_sum

#define _LIST(hwcb) ((hwcb_list_t*)(&(hwcb)[PAGE_SIZE-sizeof(hwcb_list_t)]))

#define _HWCB_CHAR(hwcb) (_LIST(hwcb)->mto_char_sum)

#define _HWCB_MTO(hwcb) (_LIST(hwcb)->mto_number)

#define _HWCB_CHAR_LOST(hwcb) (_LIST(hwcb)->mto_char_sum_lost)

#define _HWCB_MTO_LOST(hwcb) (_LIST(hwcb)->mto_number_lost)

#define _HWCB_TIMES_LOST(hwcb) (_LIST(hwcb)->times_lost)

#define _HWCB_NEXT(hwcb) (_LIST(hwcb)->next)

#define BUF_HWCB_CHAR _HWCB_CHAR(BUF_HWCB)

#define BUF_HWCB_MTO _HWCB_MTO(BUF_HWCB)

#define BUF_HWCB_NEXT _HWCB_NEXT(BUF_HWCB)

#define OUT_HWCB_CHAR _HWCB_CHAR(OUT_HWCB)

#define OUT_HWCB_MTO _HWCB_MTO(OUT_HWCB)

#define OUT_HWCB_NEXT _HWCB_NEXT(OUT_HWCB)

#define BUF_HWCB_CHAR_LOST _HWCB_CHAR_LOST(BUF_HWCB)

#define BUF_HWCB_MTO_LOST _HWCB_MTO_LOST(BUF_HWCB)

#define OUT_HWCB_CHAR_LOST _HWCB_CHAR_LOST(OUT_HWCB)

#define OUT_HWCB_MTO_LOST _HWCB_MTO_LOST(OUT_HWCB)

#define BUF_HWCB_TIMES_LOST _HWCB_TIMES_LOST(BUF_HWCB)

#include  "hwc.h"

#define __HWC_RW_C__
#include "hwc_rw.h"
#undef __HWC_RW_C__

static unsigned char _obuf[MAX_HWCB_ROOM];

static unsigned char
 _page[PAGE_SIZE] __attribute__ ((aligned (PAGE_SIZE)));

typedef unsigned long kmem_pages_t;

#define MAX_KMEM_PAGES (sizeof(kmem_pages_t) << 3)

#define HWC_WTIMER_RUNS	1
#define HWC_FLUSH		2
#define HWC_INIT		4
#define HWC_BROKEN		8
#define HWC_INTERRUPT		16
#define HWC_PTIMER_RUNS	32

static struct {

	hwc_ioctls_t ioctls;

	hwc_ioctls_t init_ioctls;

	unsigned char *hwcb_list_head;

	unsigned char *hwcb_list_tail;

	unsigned short int mto_number;

	unsigned int mto_char_sum;

	unsigned char hwcb_count;

	unsigned long kmem_start;

	unsigned long kmem_end;

	kmem_pages_t kmem_pages;

	unsigned char *obuf;

	unsigned short int obuf_cursor;

	unsigned short int obuf_count;

	unsigned short int obuf_start;

	unsigned char *page;

	u32 current_servc;

	unsigned char *current_hwcb;

	unsigned char write_nonprio:1;
	unsigned char write_prio:1;
	unsigned char read_nonprio:1;
	unsigned char read_prio:1;
	unsigned char read_statechange:1;
	unsigned char sig_quiesce:1;

	unsigned char flags;

	hwc_high_level_calls_t *calls;

	hwc_request_t *request;

	spinlock_t lock;

	struct timer_list write_timer;

	struct timer_list poll_timer;
} hwc_data =
{
	{
	},
	{
		8,
		    0,
		    80,
		    1,
		    MAX_KMEM_PAGES,
		    MAX_KMEM_PAGES,

		    0,

		    0x6c

	},
	    NULL,
	    NULL,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    _obuf,
	    0,
	    0,
	    0,
	    _page,
	    0,
	    NULL,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    NULL,
	    NULL

};

static unsigned long cr0 __attribute__ ((aligned (8)));
static unsigned long cr0_save __attribute__ ((aligned (8)));
static unsigned char psw_mask __attribute__ ((aligned (8)));

static ext_int_info_t ext_int_info_hwc;

#define DELAYED_WRITE 0
#define IMMEDIATE_WRITE 1

static signed int do_hwc_write (int from_user, unsigned char *,
				unsigned int,
				unsigned char);

unsigned char hwc_ip_buf[512];

static asmlinkage int 
internal_print (char write_time, char *fmt,...)
{
	va_list args;
	int i;

	va_start (args, fmt);
	i = vsprintf (hwc_ip_buf, fmt, args);
	va_end (args);
	return do_hwc_write (0, hwc_ip_buf, i, write_time);
}

int 
hwc_printk (const char *fmt,...)
{
	va_list args;
	int i;
	unsigned long flags;
	int retval;

	spin_lock_irqsave (&hwc_data.lock, flags);

	i = vsprintf (hwc_ip_buf, fmt, args);
	va_end (args);
	retval = do_hwc_write (0, hwc_ip_buf, i, IMMEDIATE_WRITE);

	spin_unlock_irqrestore (&hwc_data.lock, flags);

	return retval;
}

#ifdef DUMP_HWCB_INPUT

static void 
dump_storage_area (unsigned char *area, unsigned short int count)
{
	unsigned short int index;
	ioctl_nl_t old_final_nl;

	if (!area || !count)
		return;

	old_final_nl = hwc_data.ioctls.final_nl;
	hwc_data.ioctls.final_nl = 1;

	internal_print (DELAYED_WRITE, "\n%8x   ", area);

	for (index = 0; index < count; index++) {

		if (area[index] <= 0xF)
			internal_print (DELAYED_WRITE, "0%x", area[index]);
		else
			internal_print (DELAYED_WRITE, "%x", area[index]);

		if ((index & 0xF) == 0xF)
			internal_print (DELAYED_WRITE, "\n%8x   ",
					&area[index + 1]);
		else if ((index & 3) == 3)
			internal_print (DELAYED_WRITE, " ");
	}

	internal_print (IMMEDIATE_WRITE, "\n");

	hwc_data.ioctls.final_nl = old_final_nl;
}
#endif

static inline u32 
service_call (
		     u32 hwc_command_word,
		     unsigned char hwcb[])
{
	unsigned int condition_code = 1;

	__asm__ __volatile__ ("L 1, 0(%0) \n\t"
			      "LRA 2, 0(%1) \n\t"
			      ".long 0xB2200012 \n\t"
			      :
			      :"a" (&hwc_command_word), "a" (hwcb)
			      :"1", "2", "memory");

	__asm__ __volatile__ ("IPM %0 \n\t"
			      "SRL %0, 28 \n\t"
			      :"=r" (condition_code));

	return condition_code;
}

static inline unsigned long 
hwc_ext_int_param (void)
{
	u32 param;

	__asm__ __volatile__ ("L %0,128\n\t"
			      :"=r" (param));

	return (unsigned long) param;
}

static int 
prepare_write_hwcb (void)
{
	write_hwcb_t *hwcb;

	if (!BUF_HWCB)
		return -ENOMEM;

	BUF_HWCB_MTO = 0;
	BUF_HWCB_CHAR = 0;

	hwcb = (write_hwcb_t *) BUF_HWCB;

	memcpy (hwcb, &write_hwcb_template, sizeof (write_hwcb_t));

	return 0;
}

static int 
sane_write_hwcb (void)
{
	unsigned short int lost_msg;
	unsigned int lost_char;
	unsigned char lost_hwcb;
	unsigned char *bad_addr;
	unsigned long page;
	int page_nr;

	if (!OUT_HWCB)
		return -ENOMEM;

	if ((unsigned long) OUT_HWCB & 0xFFF) {

		bad_addr = OUT_HWCB;

#ifdef DUMP_HWC_WRITE_LIST_ERROR
		__asm__ ("LHI 1,0xe30\n\t"
			 "LRA 2,0(%0) \n\t"
			 "J .+0 \n\t"
	      :
	      :	 "a" (bad_addr)
	      :	 "1", "2");
#endif

		hwc_data.kmem_pages = 0;
		if ((unsigned long) BUF_HWCB & 0xFFF) {

			lost_hwcb = hwc_data.hwcb_count;
			lost_msg = ALL_HWCB_MTO;
			lost_char = ALL_HWCB_CHAR;

			OUT_HWCB = NULL;
			BUF_HWCB = NULL;
			ALL_HWCB_MTO = 0;
			ALL_HWCB_CHAR = 0;
			hwc_data.hwcb_count = 0;
		} else {

			lost_hwcb = hwc_data.hwcb_count - 1;
			lost_msg = ALL_HWCB_MTO - BUF_HWCB_MTO;
			lost_char = ALL_HWCB_CHAR - BUF_HWCB_CHAR;
			OUT_HWCB = BUF_HWCB;
			ALL_HWCB_MTO = BUF_HWCB_MTO;
			ALL_HWCB_CHAR = BUF_HWCB_CHAR;
			hwc_data.hwcb_count = 1;
			page = (unsigned long) BUF_HWCB;

			if (page >= hwc_data.kmem_start &&
			    page <= hwc_data.kmem_end) {

				page_nr = (int)
				    ((page - hwc_data.kmem_start) >> 12);
				set_bit (page_nr, &hwc_data.kmem_pages);
			}
		}

		internal_print (
				       DELAYED_WRITE,
				       HWC_RW_PRINT_HEADER
		       "found invalid HWCB at address 0x%lx. List corrupted. "
			   "Lost %i HWCBs with %i characters within up to %i "
			   "messages. Saved %i HWCB with last %i characters i"
				       "within up to %i messages.\n",
				       (unsigned long) bad_addr,
				       lost_hwcb, lost_char, lost_msg,
				       hwc_data.hwcb_count,
				       ALL_HWCB_CHAR, ALL_HWCB_MTO);
	}
	return 0;
}

static int 
reuse_write_hwcb (void)
{
	int retval;

	if (hwc_data.hwcb_count < 2)
#ifdef DUMP_HWC_WRITE_LIST_ERROR
		__asm__ ("LHI 1,0xe31\n\t"
			 "LRA 2,0(%0)\n\t"
			 "LRA 3,0(%1)\n\t"
			 "J .+0 \n\t"
	      :
	      :	 "a" (BUF_HWCB), "a" (OUT_HWCB)
	      :	 "1", "2", "3");
#else
		return -EPERM;
#endif

	if (hwc_data.current_hwcb == OUT_HWCB) {

		if (hwc_data.hwcb_count > 2) {

			BUF_HWCB_NEXT = OUT_HWCB_NEXT;

			BUF_HWCB = OUT_HWCB_NEXT;

			OUT_HWCB_NEXT = BUF_HWCB_NEXT;

			BUF_HWCB_NEXT = NULL;
		}
	} else {

		BUF_HWCB_NEXT = OUT_HWCB;

		BUF_HWCB = OUT_HWCB;

		OUT_HWCB = OUT_HWCB_NEXT;

		BUF_HWCB_NEXT = NULL;
	}

	BUF_HWCB_TIMES_LOST += 1;
	BUF_HWCB_CHAR_LOST += BUF_HWCB_CHAR;
	BUF_HWCB_MTO_LOST += BUF_HWCB_MTO;
	ALL_HWCB_MTO -= BUF_HWCB_MTO;
	ALL_HWCB_CHAR -= BUF_HWCB_CHAR;

	retval = prepare_write_hwcb ();

	if (hwc_data.hwcb_count == hwc_data.ioctls.max_hwcb)
		internal_print (
				       DELAYED_WRITE,
				       HWC_RW_PRINT_HEADER
				       "reached my own limit of "
			    "allowed buffer space for output (%i HWCBs = %li "
			  "bytes), skipped content of oldest HWCB %i time(s) "
				       "(%i lines = %i characters)\n",
				       hwc_data.ioctls.max_hwcb,
				       hwc_data.ioctls.max_hwcb * PAGE_SIZE,
				       BUF_HWCB_TIMES_LOST,
				       BUF_HWCB_MTO_LOST,
				       BUF_HWCB_CHAR_LOST);
	else
		internal_print (
				       DELAYED_WRITE,
				       HWC_RW_PRINT_HEADER
				       "page allocation failed, "
			   "could not expand buffer for output (currently in "
			     "use: %i HWCBs = %li bytes), skipped content of "
			"oldest HWCB %i time(s) (%i lines = %i characters)\n",
				       hwc_data.hwcb_count,
				       hwc_data.hwcb_count * PAGE_SIZE,
				       BUF_HWCB_TIMES_LOST,
				       BUF_HWCB_MTO_LOST,
				       BUF_HWCB_CHAR_LOST);

	return retval;
}

static int 
allocate_write_hwcb (void)
{
	unsigned char *page;
	int page_nr;

	if (hwc_data.hwcb_count == hwc_data.ioctls.max_hwcb)
		return -ENOMEM;

	page_nr = find_first_zero_bit (&hwc_data.kmem_pages, MAX_KMEM_PAGES);
	if (page_nr < hwc_data.ioctls.kmem_hwcb) {

		page = (unsigned char *)
		    (hwc_data.kmem_start + (page_nr << 12));
		set_bit (page_nr, &hwc_data.kmem_pages);
	} else
		page = (unsigned char *) __get_free_page (GFP_ATOMIC | GFP_DMA);

	if (!page)
		return -ENOMEM;

	if (!OUT_HWCB)
		OUT_HWCB = page;
	else
		BUF_HWCB_NEXT = page;

	BUF_HWCB = page;

	BUF_HWCB_NEXT = NULL;

	hwc_data.hwcb_count++;

	prepare_write_hwcb ();

	BUF_HWCB_TIMES_LOST = 0;
	BUF_HWCB_MTO_LOST = 0;
	BUF_HWCB_CHAR_LOST = 0;

#ifdef BUFFER_STRESS_TEST

	internal_print (
			       DELAYED_WRITE,
			       "*** " HWC_RW_PRINT_HEADER
			    "page #%i at 0x%x for buffering allocated. ***\n",
			       hwc_data.hwcb_count, page);

#endif

	return 0;
}

static int 
release_write_hwcb (void)
{
	unsigned long page;
	int page_nr;

	if (!hwc_data.hwcb_count)
		return -ENODATA;

	if (hwc_data.hwcb_count == 1) {

		prepare_write_hwcb ();

		ALL_HWCB_CHAR = 0;
		ALL_HWCB_MTO = 0;
		BUF_HWCB_TIMES_LOST = 0;
		BUF_HWCB_MTO_LOST = 0;
		BUF_HWCB_CHAR_LOST = 0;
	} else {
		page = (unsigned long) OUT_HWCB;

		ALL_HWCB_MTO -= OUT_HWCB_MTO;
		ALL_HWCB_CHAR -= OUT_HWCB_CHAR;
		hwc_data.hwcb_count--;

		OUT_HWCB = OUT_HWCB_NEXT;

		if (page >= hwc_data.kmem_start &&
		    page <= hwc_data.kmem_end) {
			/*memset((void *) page, 0, PAGE_SIZE); */

			page_nr = (int) ((page - hwc_data.kmem_start) >> 12);
			clear_bit (page_nr, &hwc_data.kmem_pages);
		} else
			free_page (page);
#ifdef BUFFER_STRESS_TEST

		internal_print (
				       DELAYED_WRITE,
				       "*** " HWC_RW_PRINT_HEADER
			 "page at 0x%x released, %i pages still in use ***\n",
				       page, hwc_data.hwcb_count);

#endif
	}
	return 0;
}

static int 
add_mto (
		unsigned char *message,
		unsigned short int count)
{
	unsigned short int mto_size;
	write_hwcb_t *hwcb;
	mto_t *mto;
	void *dest;

	if (!BUF_HWCB)
		return -ENOMEM;

	if (BUF_HWCB == hwc_data.current_hwcb)
		return -ENOMEM;

	mto_size = sizeof (mto_t) + count;

	hwcb = (write_hwcb_t *) BUF_HWCB;

	if ((MAX_HWCB_ROOM - hwcb->length) < mto_size)
		return -ENOMEM;

	mto = (mto_t *) (((unsigned long) hwcb) + hwcb->length);

	memcpy (mto, &mto_template, sizeof (mto_t));

	dest = (void *) (((unsigned long) mto) + sizeof (mto_t));

	memcpy (dest, message, count);

	mto->length += count;

	hwcb->length += mto_size;
	hwcb->msgbuf.length += mto_size;
	hwcb->msgbuf.mdb.length += mto_size;

	BUF_HWCB_MTO++;
	ALL_HWCB_MTO++;
	BUF_HWCB_CHAR += count;
	ALL_HWCB_CHAR += count;

	return count;
}

static int write_event_data_1 (void);

static void 
do_poll_hwc (unsigned long data)
{
	unsigned long flags;

	spin_lock_irqsave (&hwc_data.lock, flags);

	write_event_data_1 ();

	spin_unlock_irqrestore (&hwc_data.lock, flags);
}

void 
start_poll_hwc (void)
{
	init_timer (&hwc_data.poll_timer);
	hwc_data.poll_timer.function = do_poll_hwc;
	hwc_data.poll_timer.data = (unsigned long) NULL;
	hwc_data.poll_timer.expires = jiffies + 2 * HZ;
	add_timer (&hwc_data.poll_timer);
	hwc_data.flags |= HWC_PTIMER_RUNS;
}

static int 
write_event_data_1 (void)
{
	unsigned short int condition_code;
	int retval;
	write_hwcb_t *hwcb = (write_hwcb_t *) OUT_HWCB;

	if ((!hwc_data.write_prio) &&
	    (!hwc_data.write_nonprio) &&
	    hwc_data.read_statechange)
		return -EOPNOTSUPP;

	if (hwc_data.current_servc)
		return -EBUSY;

	retval = sane_write_hwcb ();
	if (retval < 0)
		return -EIO;

	if (!OUT_HWCB_MTO)
		return -ENODATA;

	if (!hwc_data.write_nonprio && hwc_data.write_prio)
		hwcb->msgbuf.type = ET_PMsgCmd;
	else
		hwcb->msgbuf.type = ET_Msg;

	condition_code = service_call (HWC_CMDW_WRITEDATA, OUT_HWCB);

#ifdef DUMP_HWC_WRITE_ERROR
	if (condition_code != HWC_COMMAND_INITIATED)
		__asm__ ("LHI 1,0xe20\n\t"
			 "L 2,0(%0)\n\t"
			 "LRA 3,0(%1)\n\t"
			 "J .+0 \n\t"
	      :
	      :	 "a" (&condition_code), "a" (OUT_HWCB)
	      :	 "1", "2", "3");
#endif

	switch (condition_code) {
	case HWC_COMMAND_INITIATED:
		hwc_data.current_servc = HWC_CMDW_WRITEDATA;
		hwc_data.current_hwcb = OUT_HWCB;
		retval = condition_code;
		break;
	case HWC_BUSY:
		retval = -EBUSY;
		break;
	case HWC_NOT_OPERATIONAL:
		start_poll_hwc ();
	default:
		retval = -EIO;
	}

	return retval;
}

static void 
flush_hwcbs (void)
{
	while (hwc_data.hwcb_count > 1)
		release_write_hwcb ();

	release_write_hwcb ();

	hwc_data.flags &= ~HWC_FLUSH;
}

static int 
write_event_data_2 (u32 ext_int_param)
{
	write_hwcb_t *hwcb;
	int retval = 0;

#ifdef DUMP_HWC_WRITE_ERROR
	if ((ext_int_param & HWC_EXT_INT_PARAM_ADDR)
	    != (unsigned long) hwc_data.current_hwcb) {
		internal_print (
				       DELAYED_WRITE,
				       HWC_RW_PRINT_HEADER
				       "write_event_data_2 : "
				       "HWCB address does not fit "
				       "(expected: 0x%lx, got: 0x%lx).\n",
				       (unsigned long) hwc_data.current_hwcb,
				       ext_int_param);
		return -EINVAL;
	}
#endif

	hwcb = (write_hwcb_t *) OUT_HWCB;

#ifdef DUMP_HWC_WRITE_LIST_ERROR
	if (((unsigned char *) hwcb) != hwc_data.current_hwcb) {
		__asm__ ("LHI 1,0xe22\n\t"
			 "LRA 2,0(%0)\n\t"
			 "LRA 3,0(%1)\n\t"
			 "LRA 4,0(%2)\n\t"
			 "LRA 5,0(%3)\n\t"
			 "J .+0 \n\t"
	      :
	      :	 "a" (OUT_HWCB),
			 "a" (hwc_data.current_hwcb),
			 "a" (BUF_HWCB),
			 "a" (hwcb)
	      :	 "1", "2", "3", "4", "5");
	}
#endif

#ifdef DUMP_HWC_WRITE_ERROR
	if (hwcb->response_code != 0x0020) {
		__asm__ ("LHI 1,0xe21\n\t"
			 "LRA 2,0(%0)\n\t"
			 "LRA 3,0(%1)\n\t"
			 "LRA 4,0(%2)\n\t"
			 "LH 5,0(%3)\n\t"
			 "SRL 5,8\n\t"
			 "J .+0 \n\t"
	      :
	      :	 "a" (OUT_HWCB), "a" (hwc_data.current_hwcb),
			 "a" (BUF_HWCB),
			 "a" (&(hwc_data.hwcb_count))
	      :	 "1", "2", "3", "4", "5");
	}
#endif

	switch (hwcb->response_code) {
	case 0x0020:

		retval = OUT_HWCB_CHAR;
		release_write_hwcb ();
		break;
	case 0x0040:
	case 0x0340:
	case 0x40F0:
		if (!hwc_data.read_statechange) {
			hwcb->response_code = 0;
			start_poll_hwc ();
		}
		retval = -EIO;
		break;
	default:
		internal_print (
				       DELAYED_WRITE,
				       HWC_RW_PRINT_HEADER
				       "write_event_data_2 : "
				       "failed operation "
				       "(response code: 0x%x "
				       "HWCB address: 0x%x).\n",
				       hwcb->response_code,
				       hwcb);
		retval = -EIO;
	}

	if (retval == -EIO) {

		hwcb->control_mask[0] = 0;
		hwcb->control_mask[1] = 0;
		hwcb->control_mask[2] = 0;
		hwcb->response_code = 0;
	}
	hwc_data.current_servc = 0;
	hwc_data.current_hwcb = NULL;

	if (hwc_data.flags & HWC_FLUSH)
		flush_hwcbs ();

	return retval;
}

static void 
do_put_line (
		    unsigned char *message,
		    unsigned short count)
{

	if (add_mto (message, count) != count) {

		if (allocate_write_hwcb () < 0)
			reuse_write_hwcb ();

#ifdef DUMP_HWC_WRITE_LIST_ERROR
		if (add_mto (message, count) != count)
			__asm__ ("LHI 1,0xe32\n\t"
				 "LRA 2,0(%0)\n\t"
				 "L 3,0(%1)\n\t"
				 "LRA 4,0(%2)\n\t"
				 "LRA 5,0(%3)\n\t"
				 "J .+0 \n\t"
		      :
		      :	 "a" (message), "a" (&hwc_data.kmem_pages),
				 "a" (BUF_HWCB), "a" (OUT_HWCB)
		      :	 "1", "2", "3", "4", "5");
#else
		add_mto (message, count);
#endif
	}
}

static void 
put_line (
		 unsigned char *message,
		 unsigned short count)
{

	if ((!hwc_data.obuf_start) && (hwc_data.flags & HWC_WTIMER_RUNS)) {
		del_timer (&hwc_data.write_timer);
		hwc_data.flags &= ~HWC_WTIMER_RUNS;
	}
	hwc_data.obuf_start += count;

	do_put_line (message, count);

	hwc_data.obuf_start -= count;
}

static void 
set_alarm (void)
{
	write_hwcb_t *hwcb;

	if ((!BUF_HWCB) || (BUF_HWCB == hwc_data.current_hwcb))
		allocate_write_hwcb ();

	hwcb = (write_hwcb_t *) BUF_HWCB;
	hwcb->msgbuf.mdb.mdb_body.go.general_msg_flags |= GMF_SndAlrm;
}

static void 
hwc_write_timeout (unsigned long data)
{
	unsigned long flags;

	spin_lock_irqsave (&hwc_data.lock, flags);

	hwc_data.obuf_start = hwc_data.obuf_count;
	if (hwc_data.obuf_count)
		put_line (hwc_data.obuf, hwc_data.obuf_count);
	hwc_data.obuf_start = 0;

	hwc_data.obuf_cursor = 0;
	hwc_data.obuf_count = 0;

	write_event_data_1 ();

	spin_unlock_irqrestore (&hwc_data.lock, flags);
}

static int 
do_hwc_write (
		     int from_user,
		     unsigned char *msg,
		     unsigned int count,
		     unsigned char write_time)
{
	unsigned int i_msg = 0;
	unsigned short int spaces = 0;
	unsigned int processed_characters = 0;
	unsigned char ch;
	unsigned short int obuf_count;
	unsigned short int obuf_cursor;
	unsigned short int obuf_columns;

	if (hwc_data.obuf_start) {
		obuf_cursor = 0;
		obuf_count = 0;
		obuf_columns = MIN (hwc_data.ioctls.columns,
				    MAX_MESSAGE_SIZE - hwc_data.obuf_start);
	} else {
		obuf_cursor = hwc_data.obuf_cursor;
		obuf_count = hwc_data.obuf_count;
		obuf_columns = hwc_data.ioctls.columns;
	}

	for (i_msg = 0; i_msg < count; i_msg++) {
		if (from_user)
			get_user (ch, msg + i_msg);
		else
			ch = msg[i_msg];

		processed_characters++;

		if ((obuf_cursor == obuf_columns) &&

		    (ch != '\n') &&

		    (ch != '\t')) {
			put_line (&hwc_data.obuf[hwc_data.obuf_start],
				  obuf_columns);
			obuf_cursor = 0;
			obuf_count = 0;
		}
		switch (ch) {

		case '\n':

			put_line (&hwc_data.obuf[hwc_data.obuf_start],
				  obuf_count);
			obuf_cursor = 0;
			obuf_count = 0;
			break;

		case '\a':

			hwc_data.obuf_start += obuf_count;
			set_alarm ();
			hwc_data.obuf_start -= obuf_count;

			break;

		case '\t':

			do {
				if (obuf_cursor < obuf_columns) {
					hwc_data.obuf[hwc_data.obuf_start +
						      obuf_cursor]
					    = HWC_ASCEBC (' ');
					obuf_cursor++;
				} else
					break;
			} while (obuf_cursor % hwc_data.ioctls.width_htab);

			break;

		case '\f':
		case '\v':

			spaces = obuf_cursor;
			put_line (&hwc_data.obuf[hwc_data.obuf_start],
				  obuf_count);
			obuf_count = obuf_cursor;
			while (spaces) {
				hwc_data.obuf[hwc_data.obuf_start +
					      obuf_cursor - spaces]
				    = HWC_ASCEBC (' ');
				spaces--;
			}

			break;

		case '\b':

			if (obuf_cursor)
				obuf_cursor--;
			break;

		case '\r':

			obuf_cursor = 0;
			break;

		case 0x00:

			put_line (&hwc_data.obuf[hwc_data.obuf_start],
				  obuf_count);
			obuf_cursor = 0;
			obuf_count = 0;
			goto out;

		default:

			if (isprint (ch))
				hwc_data.obuf[hwc_data.obuf_start +
					      obuf_cursor++]
				    = HWC_ASCEBC (ch);
		}
		if (obuf_cursor > obuf_count)
			obuf_count = obuf_cursor;
	}

	if (obuf_cursor) {

		if (hwc_data.obuf_start ||
		    (hwc_data.ioctls.final_nl == 0)) {

			put_line (&hwc_data.obuf[hwc_data.obuf_start],
				  obuf_count);
			obuf_cursor = 0;
			obuf_count = 0;
		} else {

			if (hwc_data.ioctls.final_nl > 0) {

				if (hwc_data.flags & HWC_WTIMER_RUNS) {

					mod_timer (&hwc_data.write_timer,
						   jiffies + hwc_data.ioctls.final_nl * HZ / 10);
				} else {

					init_timer (&hwc_data.write_timer);
					hwc_data.write_timer.function =
					    hwc_write_timeout;
					hwc_data.write_timer.data =
					    (unsigned long) NULL;
					hwc_data.write_timer.expires =
					    jiffies +
					    hwc_data.ioctls.final_nl * HZ / 10;
					add_timer (&hwc_data.write_timer);
					hwc_data.flags |= HWC_WTIMER_RUNS;
				}
			} else;

		}
	} else;

      out:

	if (!hwc_data.obuf_start) {
		hwc_data.obuf_cursor = obuf_cursor;
		hwc_data.obuf_count = obuf_count;
	}
	if (write_time == IMMEDIATE_WRITE)
		write_event_data_1 ();

	return processed_characters;
}

signed int 
hwc_write (int from_user, const unsigned char *msg, unsigned int count)
{
	unsigned long flags;
	int retval;

	spin_lock_irqsave (&hwc_data.lock, flags);

	retval = do_hwc_write (from_user, (unsigned char *) msg,
			       count, IMMEDIATE_WRITE);

	spin_unlock_irqrestore (&hwc_data.lock, flags);

	return retval;
}

unsigned int 
hwc_chars_in_buffer (unsigned char flag)
{
	unsigned short int number = 0;
	unsigned long flags;

	spin_lock_irqsave (&hwc_data.lock, flags);

	if (flag & IN_HWCB)
		number += ALL_HWCB_CHAR;

	if (flag & IN_WRITE_BUF)
		number += hwc_data.obuf_cursor;

	spin_unlock_irqrestore (&hwc_data.lock, flags);

	return number;
}

static inline int 
nr_setbits (kmem_pages_t arg)
{
	int i;
	int nr = 0;

	for (i = 0; i < (sizeof (arg) << 3); i++) {
		if (arg & 1)
			nr++;
		arg >>= 1;
	}

	return nr;
}

unsigned int 
hwc_write_room (unsigned char flag)
{
	unsigned int number = 0;
	unsigned long flags;
	write_hwcb_t *hwcb;

	spin_lock_irqsave (&hwc_data.lock, flags);

	if (flag & IN_HWCB) {

		if (BUF_HWCB) {
			hwcb = (write_hwcb_t *) BUF_HWCB;
			number += MAX_HWCB_ROOM - hwcb->length;
		}
		number += (hwc_data.ioctls.kmem_hwcb -
			   nr_setbits (hwc_data.kmem_pages)) *
		    (MAX_HWCB_ROOM -
		     (sizeof (write_hwcb_t) + sizeof (mto_t)));
	}
	if (flag & IN_WRITE_BUF)
		number += MAX_HWCB_ROOM - hwc_data.obuf_cursor;

	spin_unlock_irqrestore (&hwc_data.lock, flags);

	return number;
}

void 
hwc_flush_buffer (unsigned char flag)
{
	unsigned long flags;

	spin_lock_irqsave (&hwc_data.lock, flags);

	if (flag & IN_HWCB) {
		if (hwc_data.current_servc != HWC_CMDW_WRITEDATA)
			flush_hwcbs ();
		else
			hwc_data.flags |= HWC_FLUSH;
	}
	if (flag & IN_WRITE_BUF) {
		hwc_data.obuf_cursor = 0;
		hwc_data.obuf_count = 0;
	}
	spin_unlock_irqrestore (&hwc_data.lock, flags);
}

unsigned short int 
seperate_cases (unsigned char *buf, unsigned short int count)
{

	unsigned short int i_in;

	unsigned short int i_out = 0;

	unsigned char _case = 0;

	for (i_in = 0; i_in < count; i_in++) {

		if (buf[i_in] == hwc_data.ioctls.delim) {

			if ((i_in + 1 < count) &&
			    (buf[i_in + 1] == hwc_data.ioctls.delim)) {

				buf[i_out] = hwc_data.ioctls.delim;

				i_out++;

				i_in++;

			} else
				_case = ~_case;

		} else {

			if (_case) {

				if (hwc_data.ioctls.tolower)
					buf[i_out] = _ebc_toupper[buf[i_in]];

				else
					buf[i_out] = _ebc_tolower[buf[i_in]];

			} else
				buf[i_out] = buf[i_in];

			i_out++;
		}
	}

	return i_out;
}

#ifdef DUMP_HWCB_INPUT

static int 
gds_vector_name (u16 id, unsigned char name[])
{
	int retval = 0;

	switch (id) {
	case GDS_ID_MDSMU:
		name = "Multiple Domain Support Message Unit";
		break;
	case GDS_ID_MDSRouteInfo:
		name = "MDS Routing Information";
		break;
	case GDS_ID_AgUnWrkCorr:
		name = "Agent Unit of Work Correlator";
		break;
	case GDS_ID_SNACondReport:
		name = "SNA Condition Report";
		break;
	case GDS_ID_CPMSU:
		name = "CP Management Services Unit";
		break;
	case GDS_ID_RoutTargInstr:
		name = "Routing and Targeting Instructions";
		break;
	case GDS_ID_OpReq:
		name = "Operate Request";
		break;
	case GDS_ID_TextCmd:
		name = "Text Command";
		break;

	default:
		name = "unknown GDS variable";
		retval = -EINVAL;
	}

	return retval;
}
#endif

inline static gds_vector_t *
find_gds_vector (
			gds_vector_t * start, void *end, u16 id)
{
	gds_vector_t *vec;
	gds_vector_t *retval = NULL;

	vec = start;

	while (((void *) vec) < end) {
		if (vec->gds_id == id) {

#ifdef DUMP_HWCB_INPUT
			int retval_name;
			unsigned char name[64];

			retval_name = gds_vector_name (id, name);
			internal_print (
					       DELAYED_WRITE,
					       HWC_RW_PRINT_HEADER
					  "%s at 0x%x up to 0x%x, length: %d",
					       name,
					       (unsigned long) vec,
				      ((unsigned long) vec) + vec->length - 1,
					       vec->length);
			if (retval_name < 0)
				internal_print (
						       IMMEDIATE_WRITE,
						       ", id: 0x%x\n",
						       vec->gds_id);
			else
				internal_print (
						       IMMEDIATE_WRITE,
						       "\n");
#endif

			retval = vec;
			break;
		}
		vec = (gds_vector_t *) (((unsigned long) vec) + vec->length);
	}

	return retval;
}

inline static gds_subvector_t *
find_gds_subvector (
			   gds_subvector_t * start, void *end, u8 key)
{
	gds_subvector_t *subvec;
	gds_subvector_t *retval = NULL;

	subvec = start;

	while (((void *) subvec) < end) {
		if (subvec->key == key) {
			retval = subvec;
			break;
		}
		subvec = (gds_subvector_t *)
		    (((unsigned long) subvec) + subvec->length);
	}

	return retval;
}

inline static int 
get_input (void *start, void *end)
{
	int count;

	count = ((unsigned long) end) - ((unsigned long) start);

	if (hwc_data.ioctls.tolower)
		EBC_TOLOWER (start, count);

	if (hwc_data.ioctls.delim)
		count = seperate_cases (start, count);

	HWC_EBCASC_STR (start, count);

	if (hwc_data.ioctls.echo)
		do_hwc_write (0, start, count, IMMEDIATE_WRITE);

	if (hwc_data.calls != NULL)
		if (hwc_data.calls->move_input != NULL)
			(hwc_data.calls->move_input) (start, count);

	return count;
}

inline static int 
eval_selfdeftextmsg (gds_subvector_t * start, void *end)
{
	gds_subvector_t *subvec;
	void *subvec_data;
	void *subvec_end;
	int retval = 0;

	subvec = start;

	while (((void *) subvec) < end) {
		subvec = find_gds_subvector (subvec, end, 0x30);
		if (!subvec)
			break;
		subvec_data = (void *)
		    (((unsigned long) subvec) +
		     sizeof (gds_subvector_t));
		subvec_end = (void *)
		    (((unsigned long) subvec) + subvec->length);
		retval += get_input (subvec_data, subvec_end);
		subvec = (gds_subvector_t *) subvec_end;
	}

	return retval;
}

inline static int 
eval_textcmd (gds_subvector_t * start, void *end)
{
	gds_subvector_t *subvec;
	gds_subvector_t *subvec_data;
	void *subvec_end;
	int retval = 0;

	subvec = start;

	while (((void *) subvec) < end) {
		subvec = find_gds_subvector (
					 subvec, end, GDS_KEY_SelfDefTextMsg);
		if (!subvec)
			break;
		subvec_data = (gds_subvector_t *)
		    (((unsigned long) subvec) +
		     sizeof (gds_subvector_t));
		subvec_end = (void *)
		    (((unsigned long) subvec) + subvec->length);
		retval += eval_selfdeftextmsg (subvec_data, subvec_end);
		subvec = (gds_subvector_t *) subvec_end;
	}

	return retval;
}

inline static int 
eval_cpmsu (gds_vector_t * start, void *end)
{
	gds_vector_t *vec;
	gds_subvector_t *vec_data;
	void *vec_end;
	int retval = 0;

	vec = start;

	while (((void *) vec) < end) {
		vec = find_gds_vector (vec, end, GDS_ID_TextCmd);
		if (!vec)
			break;
		vec_data = (gds_subvector_t *)
		    (((unsigned long) vec) + sizeof (gds_vector_t));
		vec_end = (void *) (((unsigned long) vec) + vec->length);
		retval += eval_textcmd (vec_data, vec_end);
		vec = (gds_vector_t *) vec_end;
	}

	return retval;
}

inline static int 
eval_mdsmu (gds_vector_t * start, void *end)
{
	gds_vector_t *vec;
	gds_vector_t *vec_data;
	void *vec_end;
	int retval = 0;

	vec = find_gds_vector (start, end, GDS_ID_CPMSU);
	if (vec) {
		vec_data = (gds_vector_t *)
		    (((unsigned long) vec) + sizeof (gds_vector_t));
		vec_end = (void *) (((unsigned long) vec) + vec->length);
		retval = eval_cpmsu (vec_data, vec_end);
	}
	return retval;
}

static int 
eval_evbuf (gds_vector_t * start, void *end)
{
	gds_vector_t *vec;
	gds_vector_t *vec_data;
	void *vec_end;
	int retval = 0;

	vec = find_gds_vector (start, end, GDS_ID_MDSMU);
	if (vec) {
		vec_data = (gds_vector_t *)
		    (((unsigned long) vec) + sizeof (gds_vector_t));
		vec_end = (void *) (((unsigned long) vec) + vec->length);
		retval = eval_mdsmu (vec_data, vec_end);
	}
	return retval;
}

static inline int 
eval_hwc_receive_mask (_hwcb_mask_t mask)
{

	hwc_data.write_nonprio
	    = ((mask & ET_Msg_Mask) == ET_Msg_Mask);

	hwc_data.write_prio
	    = ((mask & ET_PMsgCmd_Mask) == ET_PMsgCmd_Mask);

	if (hwc_data.write_prio || hwc_data.write_nonprio) {
		internal_print (
				       DELAYED_WRITE,
				       HWC_RW_PRINT_HEADER
				       "can write messages\n");
		return 0;
	} else {
		internal_print (
				       DELAYED_WRITE,
				       HWC_RW_PRINT_HEADER
				       "can not write messages\n");
		return -1;
	}
}

static inline int 
eval_hwc_send_mask (_hwcb_mask_t mask)
{

	hwc_data.read_statechange
	    = ((mask & ET_StateChange_Mask) == ET_StateChange_Mask);
	if (hwc_data.read_statechange)
		internal_print (
				       DELAYED_WRITE,
				       HWC_RW_PRINT_HEADER
				     "can read state change notifications\n");
	else
		internal_print (
				       DELAYED_WRITE,
				       HWC_RW_PRINT_HEADER
				 "can not read state change notifications\n");

	hwc_data.sig_quiesce
	    = ((mask & ET_SigQuiesce_Mask) == ET_SigQuiesce_Mask);
	if (hwc_data.sig_quiesce)
		internal_print (
				       DELAYED_WRITE,
				       HWC_RW_PRINT_HEADER
				       "can receive signal quiesce\n");
	else
		internal_print (
				       DELAYED_WRITE,
				       HWC_RW_PRINT_HEADER
				       "can not receive signal quiesce\n");

	hwc_data.read_nonprio
	    = ((mask & ET_OpCmd_Mask) == ET_OpCmd_Mask);
	if (hwc_data.read_nonprio)
		internal_print (
				       DELAYED_WRITE,
				       HWC_RW_PRINT_HEADER
				       "can read commands\n");

	hwc_data.read_prio
	    = ((mask & ET_PMsgCmd_Mask) == ET_PMsgCmd_Mask);
	if (hwc_data.read_prio)
		internal_print (
				       DELAYED_WRITE,
				       HWC_RW_PRINT_HEADER
				       "can read priority commands\n");

	if (hwc_data.read_prio || hwc_data.read_nonprio) {
		return 0;
	} else {
		internal_print (
				       DELAYED_WRITE,
				       HWC_RW_PRINT_HEADER
				     "can not read commands from operator\n");
		return -1;
	}
}

static int 
eval_statechangebuf (statechangebuf_t * scbuf)
{
	int retval = 0;

	internal_print (
			       DELAYED_WRITE,
			       HWC_RW_PRINT_HEADER
			       "HWC state change detected\n");

	if (scbuf->validity_hwc_active_facility_mask) {

	}
	if (scbuf->validity_hwc_receive_mask) {

		if (scbuf->mask_length != 4) {
#ifdef DUMP_HWC_INIT_ERROR
			__asm__ ("LHI 1,0xe50\n\t"
				 "LRA 2,0(%0)\n\t"
				 "J .+0 \n\t"
		      :
		      :	 "a" (scbuf)
		      :	 "1", "2");
#endif
		} else {

			retval += eval_hwc_receive_mask
			    (scbuf->hwc_receive_mask);
		}
	}
	if (scbuf->validity_hwc_send_mask) {

		if (scbuf->mask_length != 4) {
#ifdef DUMP_HWC_INIT_ERROR
			__asm__ ("LHI 1,0xe51\n\t"
				 "LRA 2,0(%0)\n\t"
				 "J .+0 \n\t"
		      :
		      :	 "a" (scbuf)
		      :	 "1", "2");
#endif
		} else {

			retval += eval_hwc_send_mask
			    (scbuf->hwc_send_mask);
		}
	}
	if (scbuf->validity_read_data_function_mask) {

	}
	return retval;
}

#ifdef CONFIG_SMP
extern unsigned long cpu_online_map;
static volatile unsigned long cpu_quiesce_map;

static void 
do_load_quiesce_psw (void)
{
	psw_t quiesce_psw;

	clear_bit (smp_processor_id (), &cpu_quiesce_map);
	if (smp_processor_id () == 0) {

		while (cpu_quiesce_map != 0) ;

		quiesce_psw.mask = _DW_PSW_MASK;
		quiesce_psw.addr = 0xfff;
		__load_psw (quiesce_psw);
	}
	signal_processor (smp_processor_id (), sigp_stop);
}

static void 
do_machine_quiesce (void)
{
	cpu_quiesce_map = cpu_online_map;
	smp_call_function (do_load_quiesce_psw, NULL, 0, 0);
	do_load_quiesce_psw ();
}

#else
static void 
do_machine_quiesce (void)
{
	psw_t quiesce_psw;

	quiesce_psw.mask = _DW_PSW_MASK;
	queisce_psw.addr = 0xfff;
	__load_psw (quiesce_psw);
}

#endif

static int 
process_evbufs (void *start, void *end)
{
	int retval = 0;
	evbuf_t *evbuf;
	void *evbuf_end;
	gds_vector_t *evbuf_data;

	evbuf = (evbuf_t *) start;
	while (((void *) evbuf) < end) {
		evbuf_data = (gds_vector_t *)
		    (((unsigned long) evbuf) + sizeof (evbuf_t));
		evbuf_end = (void *) (((unsigned long) evbuf) + evbuf->length);
		switch (evbuf->type) {
		case ET_OpCmd:
		case ET_CntlProgOpCmd:
		case ET_PMsgCmd:
#ifdef DUMP_HWCB_INPUT

			internal_print (
					       DELAYED_WRITE,
					       HWC_RW_PRINT_HEADER
					       "event buffer "
					   "at 0x%x up to 0x%x, length: %d\n",
					       (unsigned long) evbuf,
					       (unsigned long) (evbuf_end - 1),
					       evbuf->length);
			dump_storage_area ((void *) evbuf, evbuf->length);
#endif
			retval += eval_evbuf (evbuf_data, evbuf_end);
			break;
		case ET_StateChange:
			retval += eval_statechangebuf
			    ((statechangebuf_t *) evbuf);
			break;
		case ET_SigQuiesce:

			_machine_restart = do_machine_quiesce;
			_machine_halt = do_machine_quiesce;
			_machine_power_off = do_machine_quiesce;
			ctrl_alt_del ();
			break;
		default:
			internal_print (
					       DELAYED_WRITE,
					       HWC_RW_PRINT_HEADER
					       "unconditional read: "
					       "unknown event buffer found, "
					       "type 0x%x",
					       evbuf->type);
			retval = -ENOSYS;
		}
		evbuf = (evbuf_t *) evbuf_end;
	}
	return retval;
}

static int 
unconditional_read_1 (void)
{
	unsigned short int condition_code;
	read_hwcb_t *hwcb = (read_hwcb_t *) hwc_data.page;
	int retval;

#if 0

	if ((!hwc_data.read_prio) && (!hwc_data.read_nonprio))
		return -EOPNOTSUPP;

	if (hwc_data.current_servc)
		return -EBUSY;
#endif

	memset (hwcb, 0x00, PAGE_SIZE);
	memcpy (hwcb, &read_hwcb_template, sizeof (read_hwcb_t));

	condition_code = service_call (HWC_CMDW_READDATA, hwc_data.page);

#ifdef DUMP_HWC_READ_ERROR
	if (condition_code == HWC_NOT_OPERATIONAL)
		__asm__ ("LHI 1,0xe40\n\t"
			 "L 2,0(%0)\n\t"
			 "LRA 3,0(%1)\n\t"
			 "J .+0 \n\t"
	      :
	      :	 "a" (&condition_code), "a" (hwc_data.page)
	      :	 "1", "2", "3");
#endif

	switch (condition_code) {
	case HWC_COMMAND_INITIATED:
		hwc_data.current_servc = HWC_CMDW_READDATA;
		hwc_data.current_hwcb = hwc_data.page;
		retval = condition_code;
		break;
	case HWC_BUSY:
		retval = -EBUSY;
		break;
	default:
		retval = -EIO;
	}

	return retval;
}

static int 
unconditional_read_2 (u32 ext_int_param)
{
	read_hwcb_t *hwcb = (read_hwcb_t *) hwc_data.page;

#ifdef DUMP_HWC_READ_ERROR
	if ((hwcb->response_code != 0x0020) &&
	    (hwcb->response_code != 0x0220) &&
	    (hwcb->response_code != 0x60F0) &&
	    (hwcb->response_code != 0x62F0))
		__asm__ ("LHI 1,0xe41\n\t"
			 "LRA 2,0(%0)\n\t"
			 "L 3,0(%1)\n\t"
			 "J .+0\n\t"
	      :
	      :	 "a" (hwc_data.page), "a" (&(hwcb->response_code))
	      :	 "1", "2", "3");
#endif

	hwc_data.current_servc = 0;
	hwc_data.current_hwcb = NULL;

	switch (hwcb->response_code) {

	case 0x0020:
	case 0x0220:
		return process_evbufs (
		     (void *) (((unsigned long) hwcb) + sizeof (read_hwcb_t)),
			    (void *) (((unsigned long) hwcb) + hwcb->length));

	case 0x60F0:
	case 0x62F0:
		internal_print (
				       IMMEDIATE_WRITE,
				       HWC_RW_PRINT_HEADER
				       "unconditional read: "
				     "got interrupt and tried to read input, "
				  "but nothing found (response code=0x%x).\n",
				       hwcb->response_code);
		return 0;

	case 0x0100:
		internal_print (
				       IMMEDIATE_WRITE,
				       HWC_RW_PRINT_HEADER
			 "unconditional read: HWCB boundary violation - this "
			 "must not occur in a correct driver, please contact "
				       "author\n");
		return -EIO;

	case 0x0300:
		internal_print (
				       IMMEDIATE_WRITE,
				       HWC_RW_PRINT_HEADER
				       "unconditional read: "
			"insufficient HWCB length - this must not occur in a "
				   "correct driver, please contact author\n");
		return -EIO;

	case 0x01F0:
		internal_print (
				       IMMEDIATE_WRITE,
				       HWC_RW_PRINT_HEADER
				       "unconditional read: "
			 "invalid command - this must not occur in a correct "
				       "driver, please contact author\n");
		return -EIO;

	case 0x40F0:
		internal_print (
				       IMMEDIATE_WRITE,
				       HWC_RW_PRINT_HEADER
			       "unconditional read: invalid function code\n");
		return -EIO;

	case 0x70F0:
		internal_print (
				       IMMEDIATE_WRITE,
				       HWC_RW_PRINT_HEADER
			      "unconditional read: invalid selection mask\n");
		return -EIO;

	case 0x0040:
		internal_print (
				       IMMEDIATE_WRITE,
				       HWC_RW_PRINT_HEADER
				 "unconditional read: HWC equipment check\n");
		return -EIO;

	default:
		internal_print (
				       IMMEDIATE_WRITE,
				       HWC_RW_PRINT_HEADER
			"unconditional read: invalid response code %x - this "
			 "must not occur in a correct driver, please contact "
				       "author\n",
				       hwcb->response_code);
		return -EIO;
	}
}

static int 
write_event_mask_1 (void)
{
	unsigned int condition_code;
	int retval;

	condition_code = service_call (HWC_CMDW_WRITEMASK, hwc_data.page);

#ifdef DUMP_HWC_INIT_ERROR

	if (condition_code == HWC_NOT_OPERATIONAL)
		__asm__ ("LHI 1,0xe10\n\t"
			 "L 2,0(%0)\n\t"
			 "LRA 3,0(%1)\n\t"
			 "J .+0\n\t"
	      :
	      :	 "a" (&condition_code), "a" (hwc_data.page)
	      :	 "1", "2", "3");
#endif

	switch (condition_code) {
	case HWC_COMMAND_INITIATED:
		hwc_data.current_servc = HWC_CMDW_WRITEMASK;
		hwc_data.current_hwcb = hwc_data.page;
		retval = condition_code;
		break;
	case HWC_BUSY:
		retval = -EBUSY;
		break;
	default:
		retval = -EIO;
	}

	return retval;
}

static int 
write_event_mask_2 (u32 ext_int_param)
{
	init_hwcb_t *hwcb = (init_hwcb_t *) hwc_data.page;
	int retval = 0;

	if (hwcb->response_code != 0x0020) {
#ifdef DUMP_HWC_INIT_ERROR
		__asm__ ("LHI 1,0xe11\n\t"
			 "LRA 2,0(%0)\n\t"
			 "L 3,0(%1)\n\t"
			 "J .+0\n\t"
	      :
	      :	 "a" (hwcb), "a" (&(hwcb->response_code))
	      :	 "1", "2", "3");
#else
		retval = -1;
#endif
	} else {
		if (hwcb->mask_length != 4) {
#ifdef DUMP_HWC_INIT_ERROR
			__asm__ ("LHI 1,0xe52\n\t"
				 "LRA 2,0(%0)\n\t"
				 "J .+0 \n\t"
		      :
		      :	 "a" (hwcb)
		      :	 "1", "2");
#endif
		} else {
			retval += eval_hwc_receive_mask
			    (hwcb->hwc_receive_mask);
			retval += eval_hwc_send_mask (hwcb->hwc_send_mask);
		}
	}

	hwc_data.current_servc = 0;
	hwc_data.current_hwcb = NULL;

	return retval;
}

static int 
set_hwc_ioctls (hwc_ioctls_t * ioctls, char correct)
{
	int retval = 0;
	hwc_ioctls_t tmp;

	if (ioctls->width_htab > MAX_MESSAGE_SIZE) {
		if (correct)
			tmp.width_htab = MAX_MESSAGE_SIZE;
		else
			retval = -EINVAL;
	} else
		tmp.width_htab = ioctls->width_htab;

	tmp.echo = ioctls->echo;

	if (ioctls->columns > MAX_MESSAGE_SIZE) {
		if (correct)
			tmp.columns = MAX_MESSAGE_SIZE;
		else
			retval = -EINVAL;
	} else
		tmp.columns = ioctls->columns;

	tmp.final_nl = ioctls->final_nl;

	if (ioctls->max_hwcb < 2) {
		if (correct)
			tmp.max_hwcb = 2;
		else
			retval = -EINVAL;
	} else
		tmp.max_hwcb = ioctls->max_hwcb;

	tmp.tolower = ioctls->tolower;

	if (ioctls->kmem_hwcb > ioctls->max_hwcb) {
		if (correct)
			tmp.kmem_hwcb = ioctls->max_hwcb;
		else
			retval = -EINVAL;
	} else
		tmp.kmem_hwcb = ioctls->kmem_hwcb;

	if (ioctls->kmem_hwcb > MAX_KMEM_PAGES) {
		if (correct)
			ioctls->kmem_hwcb = MAX_KMEM_PAGES;
		else
			retval = -EINVAL;
	}
	if (ioctls->kmem_hwcb < 2) {
		if (correct)
			ioctls->kmem_hwcb = 2;
		else
			retval = -EINVAL;
	}
	tmp.delim = ioctls->delim;

	if (!(retval < 0))
		hwc_data.ioctls = tmp;

	return retval;
}

int 
do_hwc_init (void)
{
	int retval;

	memcpy (hwc_data.page, &init_hwcb_template, sizeof (init_hwcb_t));

	do {

		retval = write_event_mask_1 ();

		if (retval == -EBUSY) {

			hwc_data.flags |= HWC_INIT;

			__ctl_store (cr0, 0, 0);
			cr0_save = cr0;
			cr0 |= 0x00000200;
			cr0 &= 0xFFFFF3AC;
			__ctl_load (cr0, 0, 0);

			asm volatile ("STOSM %0,0x01"
				      :"=m" (psw_mask)::"memory");

			while (!(hwc_data.flags & HWC_INTERRUPT))
				barrier ();

			asm volatile ("STNSM %0,0xFE"
				      :"=m" (psw_mask)::"memory");

			__ctl_load (cr0_save, 0, 0);

			hwc_data.flags &= ~HWC_INIT;
		}
	} while (retval == -EBUSY);

	if (retval == -EIO) {
		hwc_data.flags |= HWC_BROKEN;
		printk (HWC_RW_PRINT_HEADER "HWC not operational\n");
	}
	return retval;
}

void hwc_interrupt_handler (struct pt_regs *regs, __u16 code);

int 
hwc_init (void)
{
	int retval;

#ifdef BUFFER_STRESS_TEST

	init_hwcb_t *hwcb;
	int i;

#endif

	if (register_early_external_interrupt (0x2401, hwc_interrupt_handler,
					       &ext_int_info_hwc) != 0)
		panic ("Couldn't request external interrupts 0x2401");

	spin_lock_init (&hwc_data.lock);

#ifdef USE_VM_DETECTION

	if (MACHINE_IS_VM) {

		if (hwc_data.init_ioctls.columns > 76)
			hwc_data.init_ioctls.columns = 76;
		hwc_data.init_ioctls.tolower = 1;
		if (!hwc_data.init_ioctls.delim)
			hwc_data.init_ioctls.delim = DEFAULT_CASE_DELIMITER;
	} else {
		hwc_data.init_ioctls.tolower = 0;
		hwc_data.init_ioctls.delim = 0;
	}
#endif
	retval = set_hwc_ioctls (&hwc_data.init_ioctls, 1);

	hwc_data.kmem_start = (unsigned long)
	    alloc_bootmem_low_pages (hwc_data.ioctls.kmem_hwcb * PAGE_SIZE);
	hwc_data.kmem_end = hwc_data.kmem_start +
	    hwc_data.ioctls.kmem_hwcb * PAGE_SIZE - 1;

	retval = do_hwc_init ();

	ctl_set_bit (0, 9);

#ifdef BUFFER_STRESS_TEST

	internal_print (
			       DELAYED_WRITE,
			       HWC_RW_PRINT_HEADER
			       "use %i bytes for buffering.\n",
			       hwc_data.ioctls.kmem_hwcb * PAGE_SIZE);
	for (i = 0; i < 500; i++) {
		hwcb = (init_hwcb_t *) BUF_HWCB;
		internal_print (
				       DELAYED_WRITE,
				       HWC_RW_PRINT_HEADER
			  "This is stress test message #%i, free: %i bytes\n",
				       i,
			     MAX_HWCB_ROOM - (hwcb->length + sizeof (mto_t)));
	}

#endif

	return /*retval */ 0;
}

signed int 
hwc_register_calls (hwc_high_level_calls_t * calls)
{
	if (calls == NULL)
		return -EINVAL;

	if (hwc_data.calls != NULL)
		return -EBUSY;

	hwc_data.calls = calls;
	return 0;
}

signed int 
hwc_unregister_calls (hwc_high_level_calls_t * calls)
{
	if (hwc_data.calls == NULL)
		return -EINVAL;

	if (calls != hwc_data.calls)
		return -EINVAL;

	hwc_data.calls = NULL;
	return 0;
}

int 
hwc_send (hwc_request_t * req)
{
	unsigned long flags;
	int retval;
	int cc;

	spin_lock_irqsave (&hwc_data.lock, flags);
	if (!req || !req->callback || !req->block) {
		retval = -EINVAL;
		goto unlock;
	}
	if (hwc_data.request) {
		retval = -ENOTSUPP;
		goto unlock;
	}
	cc = service_call (req->word, req->block);
	switch (cc) {
	case 0:
		hwc_data.request = req;
		hwc_data.current_servc = req->word;
		hwc_data.current_hwcb = req->block;
		retval = 0;
		break;
	case 2:
		retval = -EBUSY;
		break;
	default:
		retval = -ENOSYS;

	}
      unlock:
	spin_unlock_irqrestore (&hwc_data.lock, flags);
	return retval;
}

EXPORT_SYMBOL (hwc_send);

void 
do_hwc_callback (u32 ext_int_param)
{
	if (!hwc_data.request || !hwc_data.request->callback)
		return;
	if ((ext_int_param & HWC_EXT_INT_PARAM_ADDR)
	    != (unsigned long) hwc_data.request->block)
		return;
	hwc_data.request->callback (hwc_data.request);
	hwc_data.request = NULL;
	hwc_data.current_hwcb = NULL;
	hwc_data.current_servc = 0;
}

void 
hwc_do_interrupt (u32 ext_int_param)
{
	u32 finished_hwcb = ext_int_param & HWC_EXT_INT_PARAM_ADDR;
	u32 evbuf_pending = ext_int_param & HWC_EXT_INT_PARAM_PEND;

	if (hwc_data.flags & HWC_PTIMER_RUNS) {
		del_timer (&hwc_data.poll_timer);
		hwc_data.flags &= ~HWC_PTIMER_RUNS;
	}
	if (finished_hwcb) {

		if ((unsigned long) hwc_data.current_hwcb != finished_hwcb) {
			internal_print (
					       DELAYED_WRITE,
					       HWC_RW_PRINT_HEADER
					       "interrupt: mismatch: "
					       "ext. int param. (0x%x) vs. "
					       "current HWCB (0x%x)\n",
					       ext_int_param,
					       hwc_data.current_hwcb);
		} else {
			if (hwc_data.request) {

				do_hwc_callback (ext_int_param);
			} else {

				switch (hwc_data.current_servc) {

				case HWC_CMDW_WRITEMASK:

					write_event_mask_2 (ext_int_param);
					break;

				case HWC_CMDW_WRITEDATA:

					write_event_data_2 (ext_int_param);
					break;

				case HWC_CMDW_READDATA:

					unconditional_read_2 (ext_int_param);
					break;
				default:
				}
			}
		}
	} else {

		if (hwc_data.current_hwcb) {
			internal_print (
					       DELAYED_WRITE,
					       HWC_RW_PRINT_HEADER
					       "interrupt: mismatch: "
					       "ext. int. param. (0x%x) vs. "
					       "current HWCB (0x%x)\n",
					       ext_int_param,
					       hwc_data.current_hwcb);
		}
	}

	if (evbuf_pending) {

		unconditional_read_1 ();
	} else {

		write_event_data_1 ();
	}

	if (!hwc_data.calls || !hwc_data.calls->wake_up)
		return;
	(hwc_data.calls->wake_up) ();
}

void 
hwc_interrupt_handler (struct pt_regs *regs, __u16 code)
{
	int cpu = smp_processor_id ();

	u32 ext_int_param = hwc_ext_int_param ();

	irq_enter (cpu, 0x2401);

	if (hwc_data.flags & HWC_INIT) {

		hwc_data.flags |= HWC_INTERRUPT;
	} else if (hwc_data.flags & HWC_BROKEN) {

		if (!do_hwc_init ()) {
			hwc_data.flags &= ~HWC_BROKEN;
			internal_print (DELAYED_WRITE,
					HWC_RW_PRINT_HEADER
					"delayed HWC setup after"
					" temporary breakdown"
					" (ext. int. parameter=0x%x)\n",
					ext_int_param);
		}
	} else {
		spin_lock (&hwc_data.lock);
		hwc_do_interrupt (ext_int_param);
		spin_unlock (&hwc_data.lock);
	}
	irq_exit (cpu, 0x2401);
}

void 
hwc_unblank (void)
{

	spin_lock (&hwc_data.lock);
	spin_unlock (&hwc_data.lock);

	__ctl_store (cr0, 0, 0);
	cr0_save = cr0;
	cr0 |= 0x00000200;
	cr0 &= 0xFFFFF3AC;
	__ctl_load (cr0, 0, 0);

	asm volatile ("STOSM %0,0x01":"=m" (psw_mask)::"memory");

	while (ALL_HWCB_CHAR)
		barrier ();

	asm volatile ("STNSM %0,0xFE":"=m" (psw_mask)::"memory");

	__ctl_load (cr0_save, 0, 0);
}

int 
hwc_ioctl (unsigned int cmd, unsigned long arg)
{
	hwc_ioctls_t tmp = hwc_data.ioctls;
	int retval = 0;
	unsigned long flags;
	unsigned int obuf;

	spin_lock_irqsave (&hwc_data.lock, flags);

	switch (cmd) {

	case TIOCHWCSHTAB:
		if (get_user (tmp.width_htab, (ioctl_htab_t *) arg))
			goto fault;
		break;

	case TIOCHWCSECHO:
		if (get_user (tmp.echo, (ioctl_echo_t *) arg))
			goto fault;
		break;

	case TIOCHWCSCOLS:
		if (get_user (tmp.columns, (ioctl_cols_t *) arg))
			goto fault;
		break;

	case TIOCHWCSNL:
		if (get_user (tmp.final_nl, (ioctl_nl_t *) arg))
			goto fault;
		break;

	case TIOCHWCSOBUF:
		if (get_user (obuf, (unsigned int *) arg))
			goto fault;
		if (obuf & 0xFFF)
			tmp.max_hwcb = (((obuf | 0xFFF) + 1) >> 12);
		else
			tmp.max_hwcb = (obuf >> 12);
		break;

	case TIOCHWCSCASE:
		if (get_user (tmp.tolower, (ioctl_case_t *) arg))
			goto fault;
		break;

	case TIOCHWCSDELIM:
		if (get_user (tmp.delim, (ioctl_delim_t *) arg))
			goto fault;
		break;

	case TIOCHWCSINIT:
		retval = set_hwc_ioctls (&hwc_data.init_ioctls, 1);
		break;

	case TIOCHWCGHTAB:
		if (put_user (tmp.width_htab, (ioctl_htab_t *) arg))
			goto fault;
		break;

	case TIOCHWCGECHO:
		if (put_user (tmp.echo, (ioctl_echo_t *) arg))
			goto fault;
		break;

	case TIOCHWCGCOLS:
		if (put_user (tmp.columns, (ioctl_cols_t *) arg))
			goto fault;
		break;

	case TIOCHWCGNL:
		if (put_user (tmp.final_nl, (ioctl_nl_t *) arg))
			goto fault;
		break;

	case TIOCHWCGOBUF:
		if (put_user (tmp.max_hwcb, (ioctl_obuf_t *) arg))
			goto fault;
		break;

	case TIOCHWCGKBUF:
		if (put_user (tmp.kmem_hwcb, (ioctl_obuf_t *) arg))
			goto fault;
		break;

	case TIOCHWCGCASE:
		if (put_user (tmp.tolower, (ioctl_case_t *) arg))
			goto fault;
		break;

	case TIOCHWCGDELIM:
		if (put_user (tmp.delim, (ioctl_delim_t *) arg))
			goto fault;
		break;
#if 0

	case TIOCHWCGINIT:
		if (put_user (&hwc_data.init_ioctls, (hwc_ioctls_t *) arg))
			goto fault;
		break;

	case TIOCHWCGCURR:
		if (put_user (&hwc_data.ioctls, (hwc_ioctls_t *) arg))
			goto fault;
		break;
#endif

	default:
		goto noioctlcmd;
	}

	if (_IOC_DIR (cmd) == _IOC_WRITE)
		retval = set_hwc_ioctls (&tmp, 0);

	goto out;

      fault:
	retval = -EFAULT;
	goto out;
      noioctlcmd:
	retval = -ENOIOCTLCMD;
      out:
	spin_unlock_irqrestore (&hwc_data.lock, flags);
	return retval;
}
