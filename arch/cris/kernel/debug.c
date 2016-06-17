/*
 * arch/cris/kernel/debug.c
 * Various debug routines:
 * o Logging of interrupt enabling/disabling. /proc/debug_interrupt
 *   gives result and enables logging when read.
 *
 * Copyright (C) 2003 Axis Communications AB
 */

#include <linux/config.h>
#include <asm/system.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <asm/svinto.h>

#ifdef CONFIG_ETRAX_DEBUG_INTERRUPT
#define LOG_INT_SIZE 8192
#define DEBUG_INT_PROC_FILE "debug_interrupt"
#define LOG_INT_SHOW_MIN_USEC 45000

/* These are global and can be used to trig certain events. */
int log_int_pos = 0;
int log_int_size = LOG_INT_SIZE;
int log_int_trig0_pos = 0;
int log_int_trig1_pos = 0;

int log_int_enable = 0; /* Enabled every read of /proc/debug_interrupt */

struct log_int_struct 
{
  unsigned long pc;
  unsigned long ev_timer_data;
};

static struct log_int_struct log_ints[LOG_INT_SIZE];

//static unsigned long prev_log_int_t = 0;
static unsigned long prev_logged_ccr = 0;
static unsigned long prev_ei_timer_data = 0;
static unsigned long prev_di_timer_data = 0;

#define CCR_EI_BIT 5
enum {
	INT_OLD_MASK = 0x01, INT_OLD_BIT = 0,
	INT_NEW_MASK = 0x02, INT_NEW_BIT = 1,
	INT_EI_CHANGE_MASK = 0x04, INT_EI_CHANGE_BIT = 2,
	INT_ACTION_RESTORE = 0x08, INT_ACTION_RESTORE_BIT = 3,
	INT_ACTION_MISSED = 0x10, INT_ACTION_MISSED_BIT = 4,
	INT_ACTION_LONG = 0x20,  INT_ACTION_LONG_BIT = 5,

	INT_OLD_EI = INT_OLD_MASK, INT_OLD_EI_BIT = INT_OLD_BIT,
	INT_NEW_EI = INT_NEW_MASK, INT_NEW_EI_BIT = INT_NEW_BIT,
	INT_EV_DI = INT_OLD_EI | INT_EI_CHANGE_MASK,
	INT_EV_NOP_DI = 0,
	INT_EV_EI = INT_NEW_EI | INT_EI_CHANGE_MASK,
	INT_EV_NOP_EI = INT_OLD_EI | INT_NEW_EI, 
	INT_EV_RESTORE_DI= INT_EV_DI | INT_EI_CHANGE_MASK | INT_ACTION_RESTORE,
	INT_EV_RESTORE_EI= INT_EV_EI | INT_EI_CHANGE_MASK | INT_ACTION_RESTORE,
	INT_EV_RESTORE_NOP_DI = INT_EV_NOP_DI | INT_ACTION_RESTORE,
	INT_EV_RESTORE_NOP_EI = INT_EV_NOP_EI | INT_ACTION_RESTORE,
};

void log_int(unsigned long pc, unsigned long curr_ccr, unsigned long next_ccr)
{
	unsigned long t;
	int ev;
	static int no_change_cnt = 0;
	
	/* Just disable interrupts without logging here,
	 * the caller will either do ei, di or restore
	 */
	__asm__ __volatile__ ("di" : : :"memory");
	t = *R_TIMER_DATA;

	if (curr_ccr & CCR_EI_MASK) {
		prev_ei_timer_data = t;		
		ev = INT_OLD_EI;
	} else {
		prev_di_timer_data = t;
		if ( (((prev_ei_timer_data >> 8) & 0x000000FF) -
		      ((prev_di_timer_data >> 8) & 0x000000FF)) & ~0x03)
					    ev = INT_ACTION_LONG;
		else
			ev = 0;
	}
	if ((curr_ccr ^ prev_logged_ccr) & CCR_EI_MASK)
		ev |= INT_ACTION_MISSED;
	if (next_ccr & CCR_EI_MASK)
		ev |= INT_NEW_EI;
	if ((curr_ccr ^ next_ccr) & CCR_EI_MASK) {
		ev |= INT_EI_CHANGE_MASK;
		no_change_cnt = 0;
	} else
		no_change_cnt++;

	prev_logged_ccr = next_ccr;
	
	if (log_int_enable &&
	    ((ev & (INT_EI_CHANGE_MASK | INT_ACTION_MISSED | INT_ACTION_LONG))
	     ||	(no_change_cnt < 40)) &&
	    log_int_pos < LOG_INT_SIZE) {
		int i;
		i = log_int_pos;
		log_int_pos++;
		log_ints[i].pc = pc;
		log_ints[i].ev_timer_data = (t & 0x00FFFFFF) |
			((ev & 0xFF) << 24);

	}
//	__asm__ __volatile__ ("move %0,$ccr" : : "rm" (curr_ccr) : "memory");
}
void log_int_di(void)
{
  unsigned long pc;
  unsigned long flags;
  __asm__ __volatile__ ("move $srp,%0" : "=rm" (pc) : : "memory");
  __asm__ __volatile__ ("move $ccr,%0" : "=rm" (flags) : : "memory");
  log_int(pc, flags, 0);
}

void log_int_ei(void)
{
  unsigned long pc;
  unsigned long flags;
  __asm__ __volatile__ ("move $srp,%0" : "=rm" (pc) : : "memory");
  __asm__ __volatile__ ("move $ccr,%0" : "=rm" (flags) : : "memory");
  log_int(pc, flags, CCR_EI_MASK);
}


static int log_int_read_proc(char *page, char **start, off_t off, int count,
			     int *eof, void *data)
{
	int i, len = 0;
	off_t	begin = 0;	
	int j, t0, t1, t, thigh, tlow, tdi0;
	int di0, ei1;
	len += sprintf(page + len, "trig0: %i trig1: %i\n", log_int_trig0_pos, log_int_trig1_pos);
	for (i = 0; i < log_int_pos-1; i++) {

		if (((log_ints[i].ev_timer_data >> 24) & INT_NEW_EI)) {
			di0 = i;
			while (di0 < log_int_pos-1 && (((log_ints[di0].ev_timer_data >> 24) & INT_NEW_EI)))
				di0++;
			ei1 = di0+1;
			while (ei1 < log_int_pos-1 && (((log_ints[ei1].ev_timer_data >> 24) & INT_NEW_EI) == 0))
				ei1++;
			thigh = timer_data_to_ns(log_ints[ei1].ev_timer_data);
			tlow = timer_data_to_ns(log_ints[di0].ev_timer_data);
			tdi0 = timer_data_to_ns(log_ints[di0].ev_timer_data);
			
			t = thigh-tlow;
			j = di0-1;		
			if ((t > LOG_INT_SHOW_MIN_USEC) || (log_int_trig0_pos-30 < j && j < log_int_trig1_pos+30)) {

			for (; j <= ei1; j++) {
				t0 = ((log_ints[j+1].ev_timer_data & 0xFF) -
				      (log_ints[j].ev_timer_data & 0xFF));
				t1 = (((log_ints[j+1].ev_timer_data >> 8) & 0xFF) -
				      ((log_ints[j].ev_timer_data >> 8) & 0xFF));
				if (t1 == 0 || t1 == 1) {
					if (t0 < 0)
						t0 += 256;
				} else {
					if (t1 < 0)
						t1 += 256;
				}
				thigh = timer_data_to_ns(log_ints[j+1].ev_timer_data);
				tlow = timer_data_to_ns(log_ints[j].ev_timer_data);
				t = thigh-tlow;
				len += sprintf(page + len, "%4i PC %08lX-%08lX %08lX-%08lX %s high %i in %-6i ns %-7i to %-7i = %-5i ns, from first di %i ns %s\n",
					       j, log_ints[j].pc, log_ints[j+1].pc,
					       log_ints[j].ev_timer_data, log_ints[j+1].ev_timer_data,
					       ((log_ints[j].ev_timer_data >> 24) & INT_NEW_EI)!= 0?"ei":"di", t1, t,
					       tlow, thigh, thigh-tlow, thigh-tdi0,
					       j==log_int_trig0_pos?"TRIG0":(j==log_int_trig1_pos?"TRIG1":""));
				if (len+begin > off+count)
					goto done;
				if (len+begin < off) {
					begin += len;
					len = 0;
				}
			}
			
			len += sprintf(page + len,"\n");
			i = ei1-2;
			}
			
		}
	
	}
	log_int_enable = 1;
	log_int_pos = 0;
	log_int_trig0_pos = 0;
	log_int_trig1_pos = 0;
	*eof = 1;
done:
	if (off >= len+begin)
		return 0;
	*start = page + (off-begin);
	return ((count < begin+len-off) ? count : begin+len-off);	
}

static int __init
log_int_init(void)
{
	create_proc_read_entry (DEBUG_INT_PROC_FILE, 0, 0, log_int_read_proc, NULL);
	printk(KERN_INFO "/proc/" DEBUG_INT_PROC_FILE " size %i.\r\n",
	       LOG_INT_SIZE);
	return 0;
}
module_init(log_int_init);
#endif /* CONFIG_ETRAX_DEBUG_INTERRUPT */
