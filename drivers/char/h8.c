/*
 * Hitachi H8/337 Microcontroller driver
 *
 * The H8 is used to deal with the power and thermal environment
 * of a system.
 *
 * Fixes:
 *	June 1999, AV	added releasing /proc/driver/h8
 *	Feb  2000, Borislav Deianov
 *			changed queues to use list.h instead of lists.h
 */

#include <linux/config.h>
#include <linux/module.h>

#include <asm/system.h>
#include <asm/segment.h>
#include <asm/io.h>

#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/timer.h>
#include <linux/fcntl.h>
#include <linux/linkage.h>
#include <linux/stat.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/list.h>
#include <linux/ioport.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/slab.h>

#define __KERNEL_SYSCALLS__
#include <asm/unistd.h>

#include "h8.h"

#define DEBUG_H8

#ifdef DEBUG_H8
#define Dprintk		printk
#else
#define Dprintk
#endif

#define XDprintk if(h8_debug==-1)printk

/*
 * The h8 device is one of the misc char devices.
 */
#define H8_MINOR_DEV   140

/*
 * Forward declarations.
 */
static int  h8_init(void);
static int  h8_display_blank(void);
static int  h8_display_unblank(void);

static void  h8_intr(int irq, void *dev_id, struct pt_regs *regs);

static int   h8_get_info(char *, char **, off_t, int);

/*
 * Support Routines.
 */
static void h8_hw_init(void);
static void h8_start_new_cmd(void);
static void h8_send_next_cmd_byte(void);
static void h8_read_event_status(void);
static void h8_sync(void);
static void h8_q_cmd(u_char *, int, int);
static void h8_cmd_done(h8_cmd_q_t *qp);
static int  h8_alloc_queues(void);

static u_long h8_get_cpu_speed(void);
static int h8_get_curr_temp(u_char curr_temp[]);
static void h8_get_max_temp(void);
static void h8_get_upper_therm_thold(void);
static void h8_set_upper_therm_thold(int);
static int h8_get_ext_status(u_char stat_word[]);

static int h8_monitor_thread(void *);

static int h8_manage_therm(void);
static void h8_set_cpu_speed(int speed_divisor);

static void h8_start_monitor_timer(unsigned long secs);
static void h8_activate_monitor(unsigned long unused);

/* in arch/alpha/kernel/lca.c */
extern void lca_clock_print(void);
extern int  lca_get_clock(void);
extern void lca_clock_fiddle(int);

static void h8_set_event_mask(int);
static void h8_clear_event_mask(int);

/*
 * Driver structures
 */

static struct timer_list h8_monitor_timer;
static int h8_monitor_timer_active = 0;

static char  driver_version[] = "X0.0";/* no spaces */

static union	intr_buf intrbuf;
static int	intr_buf_ptr;
static union   intr_buf xx;	
static u_char  last_temp;

/*
 * I/O Macros for register reads and writes.
 */
#define H8_READ(a) 	inb((a))
#define H8_WRITE(d,a)	outb((d),(a))

#define	H8_GET_STATUS	H8_READ((h8_base) + H8_STATUS_REG_OFF)
#define H8_READ_DATA	H8_READ((h8_base) + H8_DATA_REG_OFF)
#define WRITE_DATA(d)	H8_WRITE((d), h8_base + H8_DATA_REG_OFF)
#define WRITE_CMD(d)	H8_WRITE((d), h8_base + H8_CMD_REG_OFF)

static unsigned int h8_base = H8_BASE_ADDR;
static unsigned int h8_irq = H8_IRQ;
static unsigned int h8_state = H8_IDLE;
static unsigned int h8_index = -1;
static unsigned int h8_enabled = 0;

static LIST_HEAD(h8_actq);
static LIST_HEAD(h8_cmdq);
static LIST_HEAD(h8_freeq);

/* 
 * Globals used in thermal control of Alphabook1.
 */
static int cpu_speed_divisor = -1;			
static int h8_event_mask = 0;			
static DECLARE_WAIT_QUEUE_HEAD(h8_monitor_wait);
static unsigned int h8_command_mask = 0;
static int h8_uthermal_threshold = DEFAULT_UTHERMAL_THRESHOLD;
static int h8_uthermal_window = UTH_HYSTERESIS;		      
static int h8_debug = 0xfffffdfc;
static int h8_ldamp = MHZ_115;
static int h8_udamp = MHZ_57;
static u_char h8_current_temp = 0;
static u_char h8_system_temp = 0;
static int h8_sync_channel = 0;
static DECLARE_WAIT_QUEUE_HEAD(h8_sync_wait);
static int h8_init_performed;

/* CPU speeds and clock divisor values */
static int speed_tab[6] = {230, 153, 115, 57, 28, 14};
  
/*
 * H8 interrupt handler
  */
static void h8_intr(int irq, void *dev_id, struct pt_regs *regs)
{
	u_char	stat_reg, data_reg;
	h8_cmd_q_t *qp = list_entry(h8_actq.next, h8_cmd_q_t, link);

	stat_reg = H8_GET_STATUS;
	data_reg = H8_READ_DATA;

	XDprintk("h8_intr: state %d status 0x%x data 0x%x\n", h8_state, stat_reg, data_reg);

	switch (h8_state) {
	  /* Response to an asynchronous event. */
	case H8_IDLE: { /* H8_IDLE */
	    if (stat_reg & H8_OFULL) {
	        if (data_reg == H8_INTR) {
		    h8_state = H8_INTR_MODE;
		    /* Executing a command to determine what happened. */
		    WRITE_CMD(H8_RD_EVENT_STATUS);
		    intr_buf_ptr = 1;
		    WRITE_CMD(H8_RD_EVENT_STATUS);
		} else {
		    Dprintk("h8_intr: idle stat 0x%x data 0x%x\n",
			    stat_reg, data_reg);
		}
	    } else {
	        Dprintk("h8_intr: bogus interrupt\n");
	    }
	    break;
	}
	case H8_INTR_MODE: { /* H8_INTR_MODE */
	    XDprintk("H8 intr/intr_mode\n");
	    if (data_reg == H8_BYTE_LEVEL_ACK) {
	        return;
	    } else if (data_reg == H8_CMD_ACK) {
	        return;
	    } else {
	        intrbuf.byte[intr_buf_ptr] = data_reg;
		if(!intr_buf_ptr) {
		    h8_state = H8_IDLE;
		    h8_read_event_status();
		}
		intr_buf_ptr--;
	    }
	    break;
	}
	/* Placed in this state by h8_start_new_cmd(). */
	case H8_XMIT: { /* H8_XMIT */
	    XDprintk("H8 intr/xmit\n");
	    /* If a byte level acknowledgement has been received */
	    if (data_reg == H8_BYTE_LEVEL_ACK) {
	        XDprintk("H8 intr/xmit BYTE ACK\n");
		qp->nacks++;
		if (qp->nacks > qp->ncmd)
		    if(h8_debug & 0x1)
		        Dprintk("h8intr: bogus # of acks!\n");
		/* 
		 * If the number of bytes sent is less than the total 
		 * number of bytes in the command.
		 */ 
		if (qp->cnt < qp->ncmd) {
		    h8_send_next_cmd_byte();
		}
		return;
		/* If the complete command has produced an acknowledgement. */
	    } else if (data_reg == H8_CMD_ACK) {
	        XDprintk("H8 intr/xmit CMD ACK\n");
		/* If there are response bytes */
		if (qp->nrsp)
		    h8_state = H8_RCV;
		else
		    h8_state = H8_IDLE;
		qp->cnt = 0;
		return;
		/* Error, need to start over with a clean slate. */
	    } else if (data_reg == H8_NACK) {
	        XDprintk("h8_intr: NACK received restarting command\n");
		qp->nacks = 0;
		qp->cnt = 0;
		h8_state = H8_IDLE;
		WRITE_CMD(H8_SYNC);
		return;
	    } else {
	        Dprintk ("h8intr: xmit unknown data 0x%x \n", data_reg);
		return;
	    }
	    break;
	}
	case H8_RESYNC: { /* H8_RESYNC */
	    XDprintk("H8 intr/resync\n");
	    if (data_reg == H8_BYTE_LEVEL_ACK) {
	        return;
	    } else if (data_reg == H8_SYNC_BYTE) {
	        h8_state = H8_IDLE;
		if (!list_empty(&h8_actq))
		    h8_send_next_cmd_byte();
	    } else {
	        Dprintk ("h8_intr: resync unknown data 0x%x \n", data_reg);
		return;
	    }
	    break;
	} 
	case H8_RCV: { /* H8_RCV */
	    XDprintk("H8 intr/rcv\n");
	    if (qp->cnt < qp->nrsp) {
	        qp->rcvbuf[qp->cnt] = data_reg;
		qp->cnt++;
		/* If command reception finished. */
		if (qp->cnt == qp->nrsp) {
		    h8_state = H8_IDLE;
		    list_del(&qp->link);
		    h8_cmd_done (qp);
		    /* More commands to send over? */
		    if (!list_empty(&h8_cmdq))
		        h8_start_new_cmd();
		}
		return;
	    } else {
	        Dprintk ("h8intr: rcv overflow cmd 0x%x\n", qp->cmdbuf[0]);
	    }
	    break;
	}
	default: /* default */
	    Dprintk("H8 intr/unknown\n");
	    break;
	}
	return;
}

static void __exit h8_cleanup (void)
{
	remove_proc_entry("driver/h8", NULL);
        release_region(h8_base, 8);
        free_irq(h8_irq, NULL);
}

static int __init h8_init(void)
{
        if(request_irq(h8_irq, h8_intr, SA_INTERRUPT, "h8", NULL))
        {
                printk(KERN_ERR "H8: error: IRQ %d is not free\n", h8_irq);
                return -EIO;
        }
        printk(KERN_INFO "H8 at 0x%x IRQ %d\n", h8_base, h8_irq);

        create_proc_info_entry("driver/h8", 0, NULL, h8_get_info);

        request_region(h8_base, 8, "h8");

	h8_alloc_queues();

	h8_hw_init();

	kernel_thread(h8_monitor_thread, NULL, 0);

        return 0;
}

module_init(h8_init);
module_exit(h8_cleanup);

static void __init h8_hw_init(void)
{
	u_char	buf[H8_MAX_CMD_SIZE];

	/* set CPU speed to max for booting */
	h8_set_cpu_speed(MHZ_230);

	/*
	 * Initialize the H8
	 */
	h8_sync();  /* activate interrupts */

	/* To clear conditions left by console */
	h8_read_event_status(); 

	/* Perform a conditioning read */
	buf[0] = H8_DEVICE_CONTROL;
	buf[1] = 0xff;
	buf[2] = 0x0;
	h8_q_cmd(buf, 3, 1);

	/* Turn on built-in and external mice, capture power switch */
	buf[0] = H8_DEVICE_CONTROL;
	buf[1] = 0x0;
	buf[2] = H8_ENAB_INT_PTR | H8_ENAB_EXT_PTR |
	       /*H8_DISAB_PWR_OFF_SW |*/ H8_ENAB_LOW_SPD_IND;
	h8_q_cmd(buf, 3, 1);

        h8_enabled = 1;
	return;
}

static int h8_get_info(char *buf, char **start, off_t fpos, int length)
{
#ifdef CONFIG_PROC_FS
        char *p;

        if (!h8_enabled)
                return 0;
        p = buf;


        /*
           0) Linux driver version (this will change if format changes)
           1) 
           2) 
           3)
           4)
	*/
            
        p += sprintf(p, "%s \n",
                     driver_version
		     );

        return p - buf;
#else
	return 0;
#endif
}

/* Called from console driver -- must make sure h8_enabled. */
static int h8_display_blank(void)
{
#ifdef CONFIG_H8_DISPLAY_BLANK
        int     error;

        if (!h8_enabled)
                return 0;
        error = h8_set_display_power_state(H8_STATE_STANDBY);
        if (error == H8_SUCCESS)
                return 1;
        h8_error("set display standby", error);
#endif
        return 0;
}

/* Called from console driver -- must make sure h8_enabled. */
static int h8_display_unblank(void)
{
#ifdef CONFIG_H8_DISPLAY_BLANK
        int error;

        if (!h8_enabled)
                return 0;
        error = h8_set_display_power_state(H8_STATE_READY);
        if (error == H8_SUCCESS)
                return 1;
        h8_error("set display ready", error);
#endif
        return 0;
}

static int h8_alloc_queues(void)
{
        h8_cmd_q_t *qp;
	unsigned long flags;
        int i;

        qp = (h8_cmd_q_t *)kmalloc((sizeof (h8_cmd_q_t) * H8_Q_ALLOC_AMOUNT),
				   GFP_KERNEL);

        if (!qp) {
                printk(KERN_ERR "H8: could not allocate memory for command queue\n");
                return(0);
        }
        /* add to the free queue */
        save_flags(flags); cli();
        for (i = 0; i < H8_Q_ALLOC_AMOUNT; i++) {
                /* place each at front of freeq */
                list_add(&qp[i].link, &h8_freeq);
        }
        restore_flags(flags);
        return (1);
}

/* 
 * Basic means by which commands are sent to the H8.
 */
void
h8_q_cmd(u_char *cmd, int cmd_size, int resp_size)
{
        h8_cmd_q_t      *qp;
	unsigned long flags;
        int             i;

        /* get cmd buf */
	save_flags(flags); cli();
        while (list_empty(&h8_freeq)) {
                Dprintk("H8: need to allocate more cmd buffers\n");
                restore_flags(flags);
                h8_alloc_queues();
                save_flags(flags); cli();
        }
        /* get first element from queue */
        qp = list_entry(h8_freeq.next, h8_cmd_q_t, link);
        list_del(&qp->link);

        restore_flags(flags);

        /* fill it in */
        for (i = 0; i < cmd_size; i++)
            qp->cmdbuf[i] = cmd[i];
        qp->ncmd = cmd_size;
        qp->nrsp = resp_size;

        /* queue it at the end of the cmd queue */
        save_flags(flags); cli();

        /* XXX this actually puts it at the start of cmd queue, bug? */
        list_add(&qp->link, &h8_cmdq);

        restore_flags(flags);

        h8_start_new_cmd();
}

void
h8_start_new_cmd(void)
{
        unsigned long flags;
        h8_cmd_q_t *qp;

	save_flags(flags); cli();
        if (h8_state != H8_IDLE) {
                if (h8_debug & 0x1)
                        Dprintk("h8_start_new_cmd: not idle\n");
                restore_flags(flags);
                return;
        }

        if (!list_empty(&h8_actq)) {
                Dprintk("h8_start_new_cmd: inconsistency: IDLE with non-empty active queue!\n");
                restore_flags(flags);
                return;
        }

        if (list_empty(&h8_cmdq)) {
                Dprintk("h8_start_new_cmd: no command to dequeue\n");
                restore_flags(flags);
                return;
        }
        /*
         * Take first command off of the command queue and put
         * it on the active queue.
         */
        qp = list_entry(h8_cmdq.next, h8_cmd_q_t, link);
        list_del(&qp->link);
        /* XXX should this go to the end of the active queue? */
        list_add(&qp->link, &h8_actq);
        h8_state = H8_XMIT;
        if (h8_debug & 0x1)
                Dprintk("h8_start_new_cmd: Starting a command\n");

        qp->cnt = 1;
        WRITE_CMD(qp->cmdbuf[0]);               /* Kick it off */

        restore_flags(flags);
        return;
}

void
h8_send_next_cmd_byte(void)
{
        h8_cmd_q_t      *qp = list_entry(h8_actq.next, h8_cmd_q_t, link);
        int cnt;

        cnt = qp->cnt;
        qp->cnt++;

        if (h8_debug & 0x1)
                Dprintk("h8 sending next cmd byte 0x%x (0x%x)\n",
			cnt, qp->cmdbuf[cnt]);

        if (cnt) {
                WRITE_DATA(qp->cmdbuf[cnt]);
        } else {
                WRITE_CMD(qp->cmdbuf[cnt]);
        }
        return;
}

/*
 * Synchronize H8 communications channel for command transmission.
 */
void
h8_sync(void)
{
        u_char  buf[H8_MAX_CMD_SIZE];

        buf[0] = H8_SYNC;
        buf[1] = H8_SYNC_BYTE;
        h8_q_cmd(buf, 2, 1);
}

/*
 * Responds to external interrupt. Reads event status word and 
 * decodes type of interrupt. 
 */
void
h8_read_event_status(void)
{

        if(h8_debug & 0x200)
                printk(KERN_DEBUG "h8_read_event_status: value 0x%x\n", intrbuf.word);

        /*
         * Power related items
         */
        if (intrbuf.word & H8_DC_CHANGE) {
		if(h8_debug & 0x4)
		    printk(KERN_DEBUG "h8_read_event_status: DC_CHANGE\n");
                /* see if dc added or removed, set batt/dc flag, send event */

                h8_set_event_mask(H8_MANAGE_BATTERY);
                wake_up(&h8_monitor_wait);
        }

        if (intrbuf.word & H8_POWER_BUTTON) {
                printk(KERN_CRIT "Power switch pressed - please wait - preparing to power 
off\n");
                h8_set_event_mask(H8_POWER_BUTTON);
                wake_up(&h8_monitor_wait);
        }

        /*
         * Thermal related items
         */
        if (intrbuf.word & H8_THERMAL_THRESHOLD) {
		if(h8_debug & 0x4)
		    printk(KERN_DEBUG "h8_read_event_status: THERMAL_THRESHOLD\n");
                h8_set_event_mask(H8_MANAGE_UTHERM);
                wake_up(&h8_monitor_wait);
        }

        /*
         * nops -for now
         */
        if (intrbuf.word & H8_DOCKING_STATION_STATUS) {
		if(h8_debug & 0x4)
		    printk(KERN_DEBUG "h8_read_event_status: DOCKING_STATION_STATUS\n");
                /* read_ext_status */
        }
        if (intrbuf.word & H8_EXT_BATT_STATUS) {
		if(h8_debug & 0x4)
		    printk(KERN_DEBUG "h8_read_event_status: EXT_BATT_STATUS\n");

        }
        if (intrbuf.word & H8_EXT_BATT_CHARGE_STATE) {
		if(h8_debug & 0x4)
		    printk(KERN_DEBUG "h8_read_event_status: EXT_BATT_CHARGE_STATE\n");

        }
        if (intrbuf.word & H8_BATT_CHANGE_OVER) {
		if(h8_debug & 0x4)
		    printk(KERN_DEBUG "h8_read_event_status: BATT_CHANGE_OVER\n");

        }
        if (intrbuf.word & H8_WATCHDOG) {
		if(h8_debug & 0x4)
		    printk(KERN_DEBUG "h8_read_event_status: WATCHDOG\n");
                /* nop */
        }
        if (intrbuf.word & H8_SHUTDOWN) {
		if(h8_debug & 0x4)
		    printk(KERN_DEBUG "h8_read_event_status: SHUTDOWN\n");
                /* nop */
        }
        if (intrbuf.word & H8_KEYBOARD) {
		if(h8_debug & 0x4)
		    printk(KERN_DEBUG "h8_read_event_status: KEYBOARD\n");
                /* nop */
        }
        if (intrbuf.word & H8_EXT_MOUSE_OR_CASE_SWITCH) {
		if(h8_debug & 0x4)
		    printk(KERN_DEBUG "h8_read_event_status: EXT_MOUSE_OR_CASE_SWITCH\n");
                /* read_ext_status*/
        }
        if (intrbuf.word & H8_INT_BATT_LOW) {
		if(h8_debug & 0x4)
		    printk(KERN_DEBUG "h8_read_event_status: INT_BATT_LOW\n"); post
                /* event, warn user */
        }
        if (intrbuf.word & H8_INT_BATT_CHARGE_STATE) {
		if(h8_debug & 0x4)
		    printk(KERN_DEBUG "h8_read_event_status: INT_BATT_CHARGE_STATE\n");
                /* nop - happens often */
        }
        if (intrbuf.word & H8_INT_BATT_STATUS) {
		if(h8_debug & 0x4)
		    printk(KERN_DEBUG "h8_read_event_status: INT_BATT_STATUS\n");

        }
        if (intrbuf.word & H8_INT_BATT_CHARGE_THRESHOLD) {
		if(h8_debug & 0x4)
		    printk(KERN_DEBUG "h8_read_event_status: INT_BATT_CHARGE_THRESHOLD\n");
                /* nop - happens often */
        }
        if (intrbuf.word & H8_EXT_BATT_LOW) {
		if(h8_debug & 0x4)
		    printk(KERN_DEBUG "h8_read_event_status: EXT_BATT_LOW\n");
                /*if no internal, post event, warn user */
                /* else nop */
        }

        return;
}

/*
 * Function called when H8 has performed requested command.
 */
static void
h8_cmd_done(h8_cmd_q_t *qp)
{

        /* what to do */
        switch (qp->cmdbuf[0]) {
	case H8_SYNC:
	    if (h8_debug & 0x40000) 
	        printk(KERN_DEBUG "H8: Sync command done - byte returned was 0x%x\n", 
		       qp->rcvbuf[0]);
	    list_add(&qp->link, &h8_freeq);
	    break;

	case H8_RD_SN:
	case H8_RD_ENET_ADDR:
	    printk(KERN_DEBUG "H8: read Ethernet address: command done - address: %x - %x - %x - %x - %x - %x \n", 
		   qp->rcvbuf[0], qp->rcvbuf[1], qp->rcvbuf[2],
		   qp->rcvbuf[3], qp->rcvbuf[4], qp->rcvbuf[5]);
	    list_add(&qp->link, &h8_freeq);
	    break;

	case H8_RD_HW_VER:
	case H8_RD_MIC_VER:
	case H8_RD_MAX_TEMP:
	    printk(KERN_DEBUG "H8: Max recorded CPU temp %d, Sys temp %d\n",
		   qp->rcvbuf[0], qp->rcvbuf[1]);
	    list_add(&qp->link, &h8_freeq);
	    break;

	case H8_RD_MIN_TEMP:
	    printk(KERN_DEBUG "H8: Min recorded CPU temp %d, Sys temp %d\n",
		   qp->rcvbuf[0], qp->rcvbuf[1]);
	    list_add(&qp->link, &h8_freeq);
	    break;

	case H8_RD_CURR_TEMP:
	    h8_sync_channel |= H8_RD_CURR_TEMP;
	    xx.byte[0] = qp->rcvbuf[0];
	    xx.byte[1] = qp->rcvbuf[1];
	    wake_up(&h8_sync_wait); 
	    list_add(&qp->link, &h8_freeq);
	    break;

	case H8_RD_SYS_VARIENT:
	case H8_RD_PWR_ON_CYCLES:
	    printk(KERN_DEBUG " H8: RD_PWR_ON_CYCLES command done\n");
	    break;

	case H8_RD_PWR_ON_SECS:
	    printk(KERN_DEBUG "H8: RD_PWR_ON_SECS command done\n");
	    break;

	case H8_RD_RESET_STATUS:
	case H8_RD_PWR_DN_STATUS:
	case H8_RD_EVENT_STATUS:
	case H8_RD_ROM_CKSM:
	case H8_RD_EXT_STATUS:
	    xx.byte[1] = qp->rcvbuf[0];
	    xx.byte[0] = qp->rcvbuf[1];
	    h8_sync_channel |= H8_GET_EXT_STATUS;
	    wake_up(&h8_sync_wait); 
	    list_add(&qp->link, &h8_freeq);
	    break;

	case H8_RD_USER_CFG:
	case H8_RD_INT_BATT_VOLT:
	case H8_RD_DC_INPUT_VOLT:
	case H8_RD_HORIZ_PTR_VOLT:
	case H8_RD_VERT_PTR_VOLT:
	case H8_RD_EEPROM_STATUS:
	case H8_RD_ERR_STATUS:
	case H8_RD_NEW_BUSY_SPEED:
	case H8_RD_CONFIG_INTERFACE:
	case H8_RD_INT_BATT_STATUS:
	    printk(KERN_DEBUG "H8: Read int batt status cmd done - returned was %x %x %x\n",
		   qp->rcvbuf[0], qp->rcvbuf[1], qp->rcvbuf[2]);
	    list_add(&qp->link, &h8_freeq);
	    break;

	case H8_RD_EXT_BATT_STATUS:
	case H8_RD_PWR_UP_STATUS:
	case H8_RD_EVENT_STATUS_MASK:
	case H8_CTL_EMU_BITPORT:
	case H8_DEVICE_CONTROL:
	    if(h8_debug & 0x20000) {
	        printk(KERN_DEBUG "H8: Device control cmd done - byte returned was 0x%x\n",
		       qp->rcvbuf[0]);
	    }
	    list_add(&qp->link, &h8_freeq);
	    break;

	case H8_CTL_TFT_BRT_DC:
	case H8_CTL_WATCHDOG:
	case H8_CTL_MIC_PROT:
	case H8_CTL_INT_BATT_CHG:
	case H8_CTL_EXT_BATT_CHG:
	case H8_CTL_MARK_SPACE:
	case H8_CTL_MOUSE_SENSITIVITY:
	case H8_CTL_DIAG_MODE:
	case H8_CTL_IDLE_AND_BUSY_SPDS:
	    printk(KERN_DEBUG "H8: Idle and busy speed command done\n");
	    break;

	case H8_CTL_TFT_BRT_BATT:
	case H8_CTL_UPPER_TEMP:
	    if(h8_debug & 0x10) {
	        XDprintk("H8: ctl upper thermal thresh cmd done - returned was %d\n",
		       qp->rcvbuf[0]);
	    }
	    list_add(&qp->link, &h8_freeq);
	    break;

	case H8_CTL_LOWER_TEMP:
	case H8_CTL_TEMP_CUTOUT:
	case H8_CTL_WAKEUP:
	case H8_CTL_CHG_THRESHOLD:
	case H8_CTL_TURBO_MODE:
	case H8_SET_DIAG_STATUS:
	case H8_SOFTWARE_RESET:
	case H8_RECAL_PTR:
	case H8_SET_INT_BATT_PERCENT:
	case H8_WRT_CFG_INTERFACE_REG:
	case H8_WRT_EVENT_STATUS_MASK:
	case H8_ENTER_POST_MODE:
	case H8_EXIT_POST_MODE:
	case H8_RD_EEPROM:
	case H8_WRT_EEPROM:
	case H8_WRT_TO_STATUS_DISP:
	    printk("H8: Write IO status display command done\n");
	    break;

	case H8_DEFINE_SPC_CHAR:
	case H8_DEFINE_TABLE_STRING_ENTRY:
	case H8_PERFORM_EMU_CMD:
	case H8_EMU_RD_REG:
	case H8_EMU_WRT_REG:
	case H8_EMU_RD_RAM:
	case H8_EMU_WRT_RAM:
	case H8_BQ_RD_REG:
	case H8_BQ_WRT_REG:
	case H8_PWR_OFF:
	    printk (KERN_DEBUG "H8: misc command completed\n");
	    break;
        }
        return;
}

/*
 * Retrieve the current CPU temperature and case temperature.  Provides
 * the feedback for the thermal control algorithm.  Synchcronized via 
 * sleep() for priority so that no other actions in the process will take
 * place before the data becomes available.
 */
int
h8_get_curr_temp(u_char curr_temp[])
{
        u_char  buf[H8_MAX_CMD_SIZE];
        unsigned long flags;

        memset(buf, 0, H8_MAX_CMD_SIZE); 
        buf[0] = H8_RD_CURR_TEMP;

        h8_q_cmd(buf, 1, 2);

	save_flags(flags); cli();

        while((h8_sync_channel & H8_RD_CURR_TEMP) == 0)
                sleep_on(&h8_sync_wait); 

        restore_flags(flags);

        h8_sync_channel &= ~H8_RD_CURR_TEMP;
        curr_temp[0] = xx.byte[0];
        curr_temp[1] = xx.byte[1];
        xx.word = 0;

        if(h8_debug & 0x8) 
                printk("H8: curr CPU temp %d, Sys temp %d\n",
		       curr_temp[0], curr_temp[1]);
        return 0;
}

static void
h8_get_max_temp(void)
{
        u_char  buf[H8_MAX_CMD_SIZE];

        buf[0] = H8_RD_MAX_TEMP;
        h8_q_cmd(buf, 1, 2);
}

/*
 * Assigns an upper limit to the value of the H8 thermal interrupt.
 * As an example setting a value of 115 F here will cause the 
 * interrupt to trigger when the CPU temperature reaches 115 F.
 */
static void
h8_set_upper_therm_thold(int thold)
{
        u_char  buf[H8_MAX_CMD_SIZE];

        /* write 0 to reinitialize interrupt */
        buf[0] = H8_CTL_UPPER_TEMP;
        buf[1] = 0x0;
        buf[2] = 0x0;
        h8_q_cmd(buf, 3, 1); 

        /* Do it for real */
        buf[0] = H8_CTL_UPPER_TEMP;
        buf[1] = 0x0;
        buf[2] = thold;
        h8_q_cmd(buf, 3, 1); 
}

static void
h8_get_upper_therm_thold(void)
{
        u_char  buf[H8_MAX_CMD_SIZE];

        buf[0] = H8_CTL_UPPER_TEMP;
        buf[1] = 0xff;
        buf[2] = 0;
        h8_q_cmd(buf, 3, 1); 
}

/*
 * The external status word contains information on keyboard controller,
 * power button, changes in external batt status, change in DC state,
 * docking station, etc. General purpose querying use.
 */
int
h8_get_ext_status(u_char stat_word[])
{
        u_char  buf[H8_MAX_CMD_SIZE];
	unsigned long flags;

        memset(buf, 0, H8_MAX_CMD_SIZE); 
        buf[0] = H8_RD_EXT_STATUS;

        h8_q_cmd(buf, 1, 2);

	save_flags(flags); cli();

        while((h8_sync_channel & H8_GET_EXT_STATUS) == 0)
                sleep_on(&h8_sync_wait); 

        restore_flags(flags);

        h8_sync_channel &= ~H8_GET_EXT_STATUS;
        stat_word[0] = xx.byte[0];
        stat_word[1] = xx.byte[1];
        xx.word = 0;

        if(h8_debug & 0x8) 
                printk("H8: curr ext status %x,  %x\n",
		       stat_word[0], stat_word[1]);

        return 0;
}

/*
 * Thread attached to task 0 manages thermal/physcial state of Alphabook. 
 * When a condition is detected by the interrupt service routine, the
 * isr does a wakeup() on h8_monitor_wait.  The mask value is then
 * screened for the appropriate action.
 */

int
h8_monitor_thread(void * unused)
{
        u_char curr_temp[2];

        /*
         * Need a logic based safety valve here. During boot when this thread is
         * started and the thermal interrupt is not yet initialized this logic 
         * checks the temperature and acts accordingly.  When this path is acted
         * upon system boot is painfully slow, however, the priority associated 
         * with overheating is high enough to warrant this action.
         */
        h8_get_curr_temp(curr_temp);

        printk(KERN_INFO "H8: Initial CPU temp: %d\n", curr_temp[0]);

        if(curr_temp[0] >= h8_uthermal_threshold) {
                h8_set_event_mask(H8_MANAGE_UTHERM);
                h8_manage_therm();
        } else {
                /*
                 * Arm the upper thermal limit of the H8 so that any temp in
                 * excess will trigger the thermal control mechanism.
                 */
                h8_set_upper_therm_thold(h8_uthermal_threshold);
        }

        for(;;) {
		sleep_on(&h8_monitor_wait);

                if(h8_debug & 0x2)
                        printk(KERN_DEBUG "h8_monitor_thread awakened, mask:%x\n",
                                h8_event_mask);

                if (h8_event_mask & (H8_MANAGE_UTHERM|H8_MANAGE_LTHERM)) {
                        h8_manage_therm();
                }

#if 0
                if (h8_event_mask & H8_POWER_BUTTON) {
                        h8_system_down();
                }

		/*
		 * If an external DC supply is removed or added make 
		 * appropriate CPU speed adjustments.
		 */
                if (h8_event_mask & H8_MANAGE_BATTERY) {
                          h8_run_level_3_manage(H8_RUN); 
                          h8_clear_event_mask(H8_MANAGE_BATTERY);
                }
#endif
        }
}

/* 
 * Function implements the following policy. When the machine is booted
 * the system is set to run at full clock speed. When the upper thermal
 * threshold is reached as a result of full clock a damping factor is 
 * applied to cool off the cpu.  The default value is one quarter clock
 * (57 Mhz).  When as a result of this cooling a temperature lower by
 * hmc_uthermal_window is reached, the machine is reset to a higher 
 * speed, one half clock (115 Mhz).  One half clock is maintained until
 * the upper thermal threshold is again reached restarting the cycle.
 */

int
h8_manage_therm(void)
{
        u_char curr_temp[2];

        if(h8_event_mask & H8_MANAGE_UTHERM) {
		/* Upper thermal interrupt received, need to cool down. */
		if(h8_debug & 0x10)
                        printk(KERN_WARNING "H8: Thermal threshold %d F reached\n",
			       h8_uthermal_threshold);
		h8_set_cpu_speed(h8_udamp); 
                h8_clear_event_mask(H8_MANAGE_UTHERM);
                h8_set_event_mask(H8_MANAGE_LTHERM);
                /* Check again in 30 seconds for CPU temperature */
                h8_start_monitor_timer(H8_TIMEOUT_INTERVAL); 
        } else if (h8_event_mask & H8_MANAGE_LTHERM) {
		/* See how cool the system has become as a result
		   of the reduction in speed. */
                h8_get_curr_temp(curr_temp);
                last_temp = curr_temp[0];
                if (curr_temp[0] < (h8_uthermal_threshold - h8_uthermal_window))
		{
			/* System cooling has progressed to a point
			   that the CPU may be sped up. */
                        h8_set_upper_therm_thold(h8_uthermal_threshold);
                        h8_set_cpu_speed(h8_ldamp); /* adjustable */ 
                        if(h8_debug & 0x10)
                            printk(KERN_WARNING "H8: CPU cool, applying cpu_divisor: %d \n",
				   h8_ldamp);
                        h8_clear_event_mask(H8_MANAGE_LTHERM);
                }
		else /* Not cool enough yet, check again in 30 seconds. */
                        h8_start_monitor_timer(H8_TIMEOUT_INTERVAL);
        } else {
                
        }
	return 0;
}

/* 
 * Function conditions the value of global_rpb_counter before
 * calling the primitive which causes the actual speed change.
 */
void
h8_set_cpu_speed(int speed_divisor)
{

#ifdef NOT_YET
/*
 * global_rpb_counter is consumed by alpha_delay() in determining just
 * how much time to delay.  It is necessary that the number of microseconds
 * in DELAY(n) be kept consistent over a variety of CPU clock speeds.
 * To that end global_rpb_counter is here adjusted.
 */ 
        
        switch (speed_divisor) {
                case 0:
                        global_rpb_counter = rpb->rpb_counter * 2L;
                        break;
                case 1:
                        global_rpb_counter = rpb->rpb_counter * 4L / 3L ;
                        break;
                case 3:
                        global_rpb_counter = rpb->rpb_counter / 2L;
                        break;
                case 4:
                        global_rpb_counter = rpb->rpb_counter / 4L;
                        break;
                case 5:
                        global_rpb_counter = rpb->rpb_counter / 8L;
                        break;
                /* 
                 * This case most commonly needed for cpu_speed_divisor 
                 * of 2 which is the value assigned by the firmware. 
                 */
                default:
                        global_rpb_counter = rpb->rpb_counter;
                break;
        }
#endif /* NOT_YET */

        if(h8_debug & 0x8)
                printk(KERN_DEBUG "H8: Setting CPU speed to %d MHz\n",
		       speed_tab[speed_divisor]); 

         /* Make the actual speed change */
        lca_clock_fiddle(speed_divisor);
}

/*
 * Gets value stored in rpb representing CPU clock speed and adjusts this
 * value based on the current clock speed divisor.
 */
u_long
h8_get_cpu_speed(void)
{
        u_long speed = 0;
        u_long counter;

#ifdef NOT_YET
        counter = rpb->rpb_counter / 1000000L;

        switch (alphabook_get_clock()) {
                case 0:
                        speed = counter * 2L;
                        break;
                case 1:
                        speed = counter * 4L / 3L ;
                        break;
                case 2:
                        speed = counter;
                        break;
                case 3:
                        speed = counter / 2L;
                        break;
                case 4:
                        speed = counter / 4L;
                        break;
                case 5:
                        speed = counter / 8L;
                        break;
                default:
                break;
        }
        if(h8_debug & 0x8)
                printk(KERN_DEBUG "H8: CPU speed current setting: %d MHz\n", speed); 
#endif  /* NOT_YET */
	return speed;
}

static void
h8_activate_monitor(unsigned long unused)
{
	unsigned long flags;

	save_flags(flags); cli();
	h8_monitor_timer_active = 0;
	restore_flags(flags);

	wake_up(&h8_monitor_wait);
}

static void
h8_start_monitor_timer(unsigned long secs)
{
	unsigned long flags;

	if (h8_monitor_timer_active)
	    return;

	save_flags(flags); cli();
	h8_monitor_timer_active = 1;
	restore_flags(flags);

        init_timer(&h8_monitor_timer);
        h8_monitor_timer.function = h8_activate_monitor;
        h8_monitor_timer.expires = secs * HZ + jiffies;
        add_timer(&h8_monitor_timer);
}

static void h8_set_event_mask(int mask)
{
	unsigned long flags;

	save_flags(flags); cli();
	h8_event_mask |= mask;
	restore_flags(flags);
}

static void h8_clear_event_mask(int mask)
{
	unsigned long flags;

	save_flags(flags); cli();
	h8_event_mask &= (~mask);
	restore_flags(flags);
}

MODULE_LICENSE("GPL");
EXPORT_NO_SYMBOLS;
