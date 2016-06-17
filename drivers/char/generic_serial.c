/*
 *  generic_serial.c
 *
 *  Copyright (C) 1998/1999 R.E.Wolff@BitWizard.nl
 *
 *  written for the SX serial driver.
 *     Contains the code that should be shared over all the serial drivers.
 *
 *  Credit for the idea to do it this way might go to Alan Cox. 
 *
 *
 *  Version 0.1 -- December, 1998. Initial version.
 *  Version 0.2 -- March, 1999.    Some more routines. Bugfixes. Etc.
 *  Version 0.5 -- August, 1999.   Some more fixes. Reformat for Linus.
 *
 *  BitWizard is actively maintaining this file. We sometimes find
 *  that someone submitted changes to this file. We really appreciate
 *  your help, but please submit changes through us. We're doing our
 *  best to be responsive.  -- REW
 * */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/serial.h>
#include <linux/mm.h>
#include <linux/generic_serial.h>
#include <asm/semaphore.h>
#include <asm/uaccess.h>

#define DEBUG 

static char *                  tmp_buf; 
static DECLARE_MUTEX(tmp_buf_sem);

static int gs_debug;

#ifdef DEBUG
#define gs_dprintk(f, str...) if (gs_debug & f) printk (str)
#else
#define gs_dprintk(f, str...) /* nothing */
#endif

#define func_enter() gs_dprintk (GS_DEBUG_FLOW, "gs: enter %s\n", __FUNCTION__)
#define func_exit()  gs_dprintk (GS_DEBUG_FLOW, "gs: exit  %s\n", __FUNCTION__)

#if NEW_WRITE_LOCKING
#define DECL      /* Nothing */
#define LOCKIT    down (& port->port_write_sem);
#define RELEASEIT up (&port->port_write_sem);
#else
#define DECL      unsigned long flags;
#define LOCKIT    save_flags (flags);cli ()
#define RELEASEIT restore_flags (flags)
#endif

#define RS_EVENT_WRITE_WAKEUP	1

MODULE_PARM(gs_debug, "i");


void gs_put_char(struct tty_struct * tty, unsigned char ch)
{
	struct gs_port *port;
	DECL

	func_enter (); 

	if (!tty) return;

	port = tty->driver_data;

	if (!port) return;

	if (! (port->flags & ASYNC_INITIALIZED)) return;

	/* Take a lock on the serial tranmit buffer! */
	LOCKIT;

	if (port->xmit_cnt >= SERIAL_XMIT_SIZE - 1) {
		/* Sorry, buffer is full, drop character. Update statistics???? -- REW */
		RELEASEIT;
		return;
	}

	port->xmit_buf[port->xmit_head++] = ch;
	port->xmit_head &= SERIAL_XMIT_SIZE - 1;
	port->xmit_cnt++;  /* Characters in buffer */

	RELEASEIT;
	func_exit ();
}


#ifdef NEW_WRITE_LOCKING

/*
> Problems to take into account are:
>       -1- Interrupts that empty part of the buffer.
>       -2- page faults on the access to userspace. 
>       -3- Other processes that are also trying to do a "write". 
*/

int gs_write(struct tty_struct * tty, int from_user, 
                    const unsigned char *buf, int count)
{
	struct gs_port *port;
	int c, total = 0;
	int t;

	func_enter ();

	if (!tty) return 0;

	port = tty->driver;

	if (!port) return 0;

	if (! (port->flags & ASYNC_INITIALIZED))
		return 0;

	/* get exclusive "write" access to this port (problem 3) */
	/* This is not a spinlock because we can have a disk access (page 
		 fault) in copy_from_user */
	down (& port->port_write_sem);

	while (1) {

		c = count;
 
		/* This is safe because we "OWN" the "head". Noone else can 
		   change the "head": we own the port_write_sem. */
		/* Don't overrun the end of the buffer */
		t = SERIAL_XMIT_SIZE - port->xmit_head;
		if (t < c) c = t;
 
		/* This is safe because the xmit_cnt can only decrease. This 
		   would increase "t", so we might copy too little chars. */
		/* Don't copy past the "head" of the buffer */
		t = SERIAL_XMIT_SIZE - 1 - port->xmit_cnt;
		if (t < c) c = t;
 
		/* Can't copy more? break out! */
		if (c <= 0) break;
		if (from_user)
                       if (copy_from_user (port->xmit_buf + port->xmit_head, 
                                           buf, c)) {
                               up (& port->port_write_sem);
                               return -EFAULT;
                       }

		else
			memcpy         (port->xmit_buf + port->xmit_head, buf, c);

		port -> xmit_cnt += c;
		port -> xmit_head = (port->xmit_head + c) & (SERIAL_XMIT_SIZE -1);
		buf += c;
		count -= c;
		total += c;
	}
	up (& port->port_write_sem);

	gs_dprintk (GS_DEBUG_WRITE, "write: interrupts are %s\n", 
	            (port->flags & GS_TX_INTEN)?"enabled": "disabled"); 

	if (port->xmit_cnt && 
	    !tty->stopped && 
	    !tty->hw_stopped &&
	    !(port->flags & GS_TX_INTEN)) {
		port->flags |= GS_TX_INTEN;
		port->rd->enable_tx_interrupts (port);
	}
	func_exit ();
	return total;
}
#else
/*
> Problems to take into account are:
>       -1- Interrupts that empty part of the buffer.
>       -2- page faults on the access to userspace. 
>       -3- Other processes that are also trying to do a "write". 
*/

int gs_write(struct tty_struct * tty, int from_user, 
                    const unsigned char *buf, int count)
{
	struct gs_port *port;
	int c, total = 0;
	int t;
	unsigned long flags;

	func_enter ();

	/* The standard serial driver returns 0 in this case. 
	   That sounds to me as "No error, I just didn't get to writing any
	   bytes. Feel free to try again." 
	   The "official" way to write n bytes from buf is:

		 for (nwritten = 0;nwritten < n;nwritten += rv) {
			 rv = write (fd, buf+nwritten, n-nwritten);
			 if (rv < 0) break; // Error: bail out. //
		 } 

	   which will loop endlessly in this case. The manual page for write
	   agrees with me. In practise almost everybody writes 
	   "write (fd, buf,n);" but some people might have had to deal with 
	   incomplete writes in the past and correctly implemented it by now... 
	 */

	if (!tty) return -EIO;

	port = tty->driver_data;
	if (!port || !port->xmit_buf || !tmp_buf)
		return -EIO;

	save_flags(flags);
	if (from_user) {
		down(&tmp_buf_sem);
		while (1) {
			c = count;

			/* Note: This part can be done without
			 * interrupt routine protection since
			 * the interrupt routines may only modify
			 * shared variables in safe ways, in the worst
			 * case causing us to loop twice in the code
			 * below. See comments below. */ 

			/* Don't overrun the end of the buffer */
			t = SERIAL_XMIT_SIZE - port->xmit_head;
			if (t < c) c = t;
 
			/* This is safe because the xmit_cnt can only decrease. This 
			   would increase "t", so we might copy too little chars. */
			/* Don't copy past the "head" of the buffer */
			t = SERIAL_XMIT_SIZE - 1 - port->xmit_cnt;
			if (t < c) c = t;	 

			/* Can't copy more? break out! */
			if (c <= 0) break;

			c -= copy_from_user(tmp_buf, buf, c);
			if (!c) {
				if (!total)
					total = -EFAULT;
				break;
			}
			cli();
			t = SERIAL_XMIT_SIZE - port->xmit_head;
			if (t < c) c = t;
			t = SERIAL_XMIT_SIZE - 1 - port->xmit_cnt;
			if (t < c) c = t;

			memcpy(port->xmit_buf + port->xmit_head, tmp_buf, c);
			port->xmit_head = ((port->xmit_head + c) &
			                   (SERIAL_XMIT_SIZE-1));
			port->xmit_cnt += c;
			restore_flags(flags);
			buf += c;
			count -= c;
			total += c;
		}
		up(&tmp_buf_sem);
	} else {
		while (1) {
			cli();
			c = count;

			/* This is safe because we "OWN" the "head". Noone else can 
			   change the "head": we own the port_write_sem. */
			/* Don't overrun the end of the buffer */
			t = SERIAL_XMIT_SIZE - port->xmit_head;
			if (t < c) c = t;
 
			/* This is safe because the xmit_cnt can only decrease. This 
			   would increase "t", so we might copy too little chars. */
			/* Don't copy past the "head" of the buffer */
			t = SERIAL_XMIT_SIZE - 1 - port->xmit_cnt;
			if (t < c) c = t;
 
			/* Can't copy more? break out! */
			if (c <= 0) {
				restore_flags(flags);
				break;
			}
			memcpy(port->xmit_buf + port->xmit_head, buf, c);
			port->xmit_head = ((port->xmit_head + c) &
			                   (SERIAL_XMIT_SIZE-1));
			port->xmit_cnt += c;
			restore_flags(flags);
			buf += c;
			count -= c;
			total += c;
		}
	}

	if (port->xmit_cnt && 
	    !tty->stopped && 
	    !tty->hw_stopped &&
	    !(port->flags & GS_TX_INTEN)) {
		port->flags |= GS_TX_INTEN;
		port->rd->enable_tx_interrupts (port);
	}
	func_exit ();
	return total;
}

#endif



int gs_write_room(struct tty_struct * tty)
{
	struct gs_port *port = tty->driver_data;
	int ret;

	func_enter ();
	ret = SERIAL_XMIT_SIZE - port->xmit_cnt - 1;
	if (ret < 0)
		ret = 0;
	func_exit ();
	return ret;
}


int gs_chars_in_buffer(struct tty_struct *tty)
{
	struct gs_port *port = tty->driver_data;
	func_enter ();

	func_exit ();
	return port->xmit_cnt;
}


int gs_real_chars_in_buffer(struct tty_struct *tty)
{
	struct gs_port *port;
	func_enter ();

	if (!tty) return 0;
	port = tty->driver_data;

	if (!port->rd) return 0;
	if (!port->rd->chars_in_buffer) return 0;

	func_exit ();
	return port->xmit_cnt + port->rd->chars_in_buffer (port);
}


static int gs_wait_tx_flushed (void * ptr, int timeout) 
{
	struct gs_port *port = ptr;
	long end_jiffies;
	int jiffies_to_transmit, charsleft = 0, rv = 0;
	int rcib;

	func_enter();

	gs_dprintk (GS_DEBUG_FLUSH, "port=%p.\n", port);
	if (port) {
		gs_dprintk (GS_DEBUG_FLUSH, "xmit_cnt=%x, xmit_buf=%p, tty=%p.\n", 
		port->xmit_cnt, port->xmit_buf, port->tty);
	}

	if (!port || port->xmit_cnt < 0 || !port->xmit_buf) {
		gs_dprintk (GS_DEBUG_FLUSH, "ERROR: !port, !port->xmit_buf or prot->xmit_cnt < 0.\n");
		func_exit();
		return -EINVAL;  /* This is an error which we don't know how to handle. */
	}

	rcib = gs_real_chars_in_buffer(port->tty);

	if(rcib <= 0) {
		gs_dprintk (GS_DEBUG_FLUSH, "nothing to wait for.\n");
		func_exit();
		return rv;
	}
	/* stop trying: now + twice the time it would normally take +  seconds */
	if (timeout == 0) timeout = MAX_SCHEDULE_TIMEOUT;
	end_jiffies  = jiffies; 
	if (timeout !=  MAX_SCHEDULE_TIMEOUT)
		end_jiffies += port->baud?(2 * rcib * 10 * HZ / port->baud):0;
	end_jiffies += timeout;

	gs_dprintk (GS_DEBUG_FLUSH, "now=%lx, end=%lx (%ld).\n", 
		    jiffies, end_jiffies, end_jiffies-jiffies); 

	/* the expression is actually jiffies < end_jiffies, but that won't
	   work around the wraparound. Tricky eh? */
	while ((charsleft = gs_real_chars_in_buffer (port->tty)) &&
	        time_after (end_jiffies, jiffies)) {
		/* Units check: 
		   chars * (bits/char) * (jiffies /sec) / (bits/sec) = jiffies!
		   check! */

		charsleft += 16; /* Allow 16 chars more to be transmitted ... */
		jiffies_to_transmit = port->baud?(1 + charsleft * 10 * HZ / port->baud):0;
		/*                                ^^^ Round up.... */
		if (jiffies_to_transmit <= 0) jiffies_to_transmit = 1;

		gs_dprintk (GS_DEBUG_FLUSH, "Expect to finish in %d jiffies "
			    "(%d chars).\n", jiffies_to_transmit, charsleft); 

		set_current_state (TASK_INTERRUPTIBLE);
		schedule_timeout(jiffies_to_transmit);
		if (signal_pending (current)) {
			gs_dprintk (GS_DEBUG_FLUSH, "Signal pending. Bombing out: "); 
			rv = -EINTR;
			break;
		}
	}

	gs_dprintk (GS_DEBUG_FLUSH, "charsleft = %d.\n", charsleft); 
	set_current_state (TASK_RUNNING);

	func_exit();
	return rv;
}



void gs_flush_buffer(struct tty_struct *tty)
{
	struct gs_port *port;
	unsigned long flags;

	func_enter ();

	if (!tty) return;

	port = tty->driver_data;

	if (!port) return;

	/* XXX Would the write semaphore do? */
	save_flags(flags); cli();
	port->xmit_cnt = port->xmit_head = port->xmit_tail = 0;
	restore_flags(flags);

	wake_up_interruptible(&tty->write_wait);
	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
	    tty->ldisc.write_wakeup)
		(tty->ldisc.write_wakeup)(tty);
	func_exit ();
}


void gs_flush_chars(struct tty_struct * tty)
{
	struct gs_port *port;

	func_enter ();

	if (!tty) return;

	port = tty->driver_data;

	if (!port) return;

	if (port->xmit_cnt <= 0 || tty->stopped || tty->hw_stopped ||
	    !port->xmit_buf) {
		func_exit ();
		return;
	}

	/* Beats me -- REW */
	port->flags |= GS_TX_INTEN;
	port->rd->enable_tx_interrupts (port);
	func_exit ();
}


void gs_stop(struct tty_struct * tty)
{
	struct gs_port *port;

	func_enter ();

	if (!tty) return;

	port = tty->driver_data;

	if (!port) return;

	if (port->xmit_cnt && 
	    port->xmit_buf && 
	    (port->flags & GS_TX_INTEN) ) {
		port->flags &= ~GS_TX_INTEN;
		port->rd->disable_tx_interrupts (port);
	}
	func_exit ();
}


void gs_start(struct tty_struct * tty)
{
	struct gs_port *port;

	if (!tty) return;

	port = tty->driver_data;

	if (!port) return;

	if (port->xmit_cnt && 
	    port->xmit_buf && 
	    !(port->flags & GS_TX_INTEN) ) {
		port->flags |= GS_TX_INTEN;
		port->rd->enable_tx_interrupts (port);
	}
	func_exit ();
}


void gs_shutdown_port (struct gs_port *port)
{
	unsigned long flags;

	func_enter();
	
	if (!port) return;
	
	if (!(port->flags & ASYNC_INITIALIZED))
		return;

	save_flags (flags);
	cli ();

	if (port->xmit_buf) {
		free_page((unsigned long) port->xmit_buf);
		port->xmit_buf = 0;
	}

	if (port->tty)
		set_bit(TTY_IO_ERROR, &port->tty->flags);

	port->rd->shutdown_port (port);

	port->flags &= ~ASYNC_INITIALIZED;
	restore_flags (flags);

	func_exit();
}


void gs_hangup(struct tty_struct *tty)
{
	struct gs_port   *port;

	func_enter ();

	if (!tty) return;

	port = tty->driver_data;
	tty = port->tty;
	if (!tty) 
		return;

	gs_shutdown_port (port);
	port->flags &= ~(ASYNC_NORMAL_ACTIVE|ASYNC_CALLOUT_ACTIVE |GS_ACTIVE);
	port->tty = NULL;
	port->count = 0;

	wake_up_interruptible(&port->open_wait);
	func_exit ();
}


void gs_do_softint(void *private_)
{
	struct gs_port *port = private_;
	struct tty_struct *tty;

	func_enter ();

	if (!port) return;

	tty = port->tty;

	if (!tty) return;

	if (test_and_clear_bit(RS_EVENT_WRITE_WAKEUP, &port->event)) {
		if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
		    tty->ldisc.write_wakeup)
			(tty->ldisc.write_wakeup)(tty);
		wake_up_interruptible(&tty->write_wait);
	}
	func_exit ();
}


int gs_block_til_ready(void *port_, struct file * filp)
{
	struct gs_port *port = port_;
	DECLARE_WAITQUEUE(wait, current);
	int    retval;
	int    do_clocal = 0;
	int    CD;
	struct tty_struct *tty;
	unsigned long flags;

	func_enter ();

	if (!port) return 0;

	tty = port->tty;

	if (!tty) return 0;

	gs_dprintk (GS_DEBUG_BTR, "Entering gs_block_till_ready.\n"); 
	/*
	 * If the device is in the middle of being closed, then block
	 * until it's done, and then try again.
	 */
	if (tty_hung_up_p(filp) || port->flags & ASYNC_CLOSING) {
		interruptible_sleep_on(&port->close_wait);
		if (port->flags & ASYNC_HUP_NOTIFY)
			return -EAGAIN;
		else
			return -ERESTARTSYS;
	}

	gs_dprintk (GS_DEBUG_BTR, "after hung up\n"); 

	/*
	 * If this is a callout device, then just make sure the normal
	 * device isn't being used.
	 */
	if (tty->driver.subtype == GS_TYPE_CALLOUT) {
		if (port->flags & ASYNC_NORMAL_ACTIVE)
			return -EBUSY;
		if ((port->flags & ASYNC_CALLOUT_ACTIVE) &&
		    (port->flags & ASYNC_SESSION_LOCKOUT) &&
		    (port->session != current->session))
			return -EBUSY;
		if ((port->flags & ASYNC_CALLOUT_ACTIVE) &&
		    (port->flags & ASYNC_PGRP_LOCKOUT) &&
		    (port->pgrp != current->pgrp))
			return -EBUSY;
		port->flags |= ASYNC_CALLOUT_ACTIVE;
		return 0;
	}

	gs_dprintk (GS_DEBUG_BTR, "after subtype\n");

	/*
	 * If non-blocking mode is set, or the port is not enabled,
	 * then make the check up front and then exit.
	 */
	if ((filp->f_flags & O_NONBLOCK) ||
	    (tty->flags & (1 << TTY_IO_ERROR))) {
		if (port->flags & ASYNC_CALLOUT_ACTIVE)
			return -EBUSY;
		port->flags |= ASYNC_NORMAL_ACTIVE;
		return 0;
	}

	gs_dprintk (GS_DEBUG_BTR, "after nonblock\n"); 
 
	if (port->flags & ASYNC_CALLOUT_ACTIVE) {
		if (port->normal_termios.c_cflag & CLOCAL) 
			do_clocal = 1;
	} else {
		if (C_CLOCAL(tty))
			do_clocal = 1;
	}

	/*
	 * Block waiting for the carrier detect and the line to become
	 * free (i.e., not in use by the callout).  While we are in
	 * this loop, port->count is dropped by one, so that
	 * rs_close() knows when to free things.  We restore it upon
	 * exit, either normal or abnormal.
	 */
	retval = 0;

	add_wait_queue(&port->open_wait, &wait);

	gs_dprintk (GS_DEBUG_BTR, "after add waitq.\n"); 
	save_flags(flags);
	cli();
	if (!tty_hung_up_p(filp))
		port->count--;
	restore_flags(flags);
	port->blocked_open++;
	while (1) {
		CD = port->rd->get_CD (port);
		gs_dprintk (GS_DEBUG_BTR, "CD is now %d.\n", CD);
		set_current_state (TASK_INTERRUPTIBLE);
		if (tty_hung_up_p(filp) ||
		    !(port->flags & ASYNC_INITIALIZED)) {
			if (port->flags & ASYNC_HUP_NOTIFY)
				retval = -EAGAIN;
			else
				retval = -ERESTARTSYS;
			break;
		}
		if (!(port->flags & ASYNC_CALLOUT_ACTIVE) &&
		    !(port->flags & ASYNC_CLOSING) &&
		    (do_clocal || CD))
			break;
		gs_dprintk (GS_DEBUG_BTR, "signal_pending is now: %d (%lx)\n", 
		(int)signal_pending (current), *(long*)(&current->blocked)); 
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}
		schedule();
	}
	gs_dprintk (GS_DEBUG_BTR, "Got out of the loop. (%d)\n",
		    port->blocked_open);
	set_current_state (TASK_RUNNING);
	remove_wait_queue(&port->open_wait, &wait);
	if (!tty_hung_up_p(filp))
		port->count++;
	port->blocked_open--;
	if (retval)
		return retval;

	port->flags |= ASYNC_NORMAL_ACTIVE;
	func_exit ();
	return 0;
}			 


void gs_close(struct tty_struct * tty, struct file * filp)
{
	unsigned long flags;
	struct gs_port *port;

	func_enter ();

	if (!tty) return;

	port = (struct gs_port *) tty->driver_data;

	if (!port) return;

	if (!port->tty) {
		/* This seems to happen when this is called from vhangup. */
		gs_dprintk (GS_DEBUG_CLOSE, "gs: Odd: port->tty is NULL\n");
		port->tty = tty;
	}

	save_flags(flags); cli();

	if (tty_hung_up_p(filp)) {
		restore_flags(flags);
		port->rd->hungup (port);
		func_exit ();
		return;
	}

	if ((tty->count == 1) && (port->count != 1)) {
		printk(KERN_ERR "gs: gs_close: bad port count;"
		       " tty->count is 1, port count is %d\n", port->count);
		port->count = 1;
	}
	if (--port->count < 0) {
		printk(KERN_ERR "gs: gs_close: bad port count: %d\n", port->count);
		port->count = 0;
	}
	if (port->count) {
		gs_dprintk(GS_DEBUG_CLOSE, "gs_close: count: %d\n", port->count);
		restore_flags(flags);
		func_exit ();
		return;
	}
	port->flags |= ASYNC_CLOSING;

	/*
	 * Save the termios structure, since this port may have
	 * separate termios for callout and dialin.
	 */
	if (port->flags & ASYNC_NORMAL_ACTIVE)
		port->normal_termios = *tty->termios;
	if (port->flags & ASYNC_CALLOUT_ACTIVE)
		port->callout_termios = *tty->termios;
	/*
	 * Now we wait for the transmit buffer to clear; and we notify 
	 * the line discipline to only process XON/XOFF characters.
	 */
	tty->closing = 1;
	/* if (port->closing_wait != ASYNC_CLOSING_WAIT_NONE)
	   tty_wait_until_sent(tty, port->closing_wait); */

	/*
	 * At this point we stop accepting input.  To do this, we
	 * disable the receive line status interrupts, and tell the
	 * interrupt driver to stop checking the data ready bit in the
	 * line status register.
	 */

	port->rd->disable_rx_interrupts (port);

	/* close has no way of returning "EINTR", so discard return value */
	if (port->closing_wait != ASYNC_CLOSING_WAIT_NONE)
		gs_wait_tx_flushed (port, port->closing_wait); 

	port->flags &= ~GS_ACTIVE;

	if (tty->driver.flush_buffer)
		tty->driver.flush_buffer(tty);
	if (tty->ldisc.flush_buffer)
		tty->ldisc.flush_buffer(tty);
	tty->closing = 0;

	port->event = 0;
	port->rd->close (port);
	port->rd->shutdown_port (port);
	port->tty = 0;

	if (port->blocked_open) {
		if (port->close_delay) {
			set_current_state (TASK_INTERRUPTIBLE);
			schedule_timeout(port->close_delay);
		}
		wake_up_interruptible(&port->open_wait);
	}
	port->flags &= ~(ASYNC_NORMAL_ACTIVE|ASYNC_CALLOUT_ACTIVE|
	                 ASYNC_CLOSING | ASYNC_INITIALIZED);
	wake_up_interruptible(&port->close_wait);

	restore_flags(flags);
	func_exit ();
}


static unsigned int     gs_baudrates[] = {
  0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800,
  9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600
};


void gs_set_termios (struct tty_struct * tty, 
                     struct termios * old_termios)
{
	struct gs_port *port;
	int baudrate, tmp, rv;
	struct termios *tiosp;

	func_enter();

	if (!tty) return;

	port = tty->driver_data;

	if (!port) return;

	tiosp = tty->termios;

	if (gs_debug & GS_DEBUG_TERMIOS) {
		gs_dprintk (GS_DEBUG_TERMIOS, "termios structure (%p):\n", tiosp);
	}

#if 0
	/* This is an optimization that is only allowed for dumb cards */
	/* Smart cards require knowledge of iflags and oflags too: that 
	   might change hardware cooking mode.... */
#endif
	if (old_termios) {
		if(   (tiosp->c_iflag == old_termios->c_iflag)
		   && (tiosp->c_oflag == old_termios->c_oflag)
		   && (tiosp->c_cflag == old_termios->c_cflag)
		   && (tiosp->c_lflag == old_termios->c_lflag)
		   && (tiosp->c_line  == old_termios->c_line)
		   && (memcmp(tiosp->c_cc, old_termios->c_cc, NCC) == 0)) {
			gs_dprintk(GS_DEBUG_TERMIOS, "gs_set_termios: optimized away\n");
			return /* 0 */;
		}
	} else 
		gs_dprintk(GS_DEBUG_TERMIOS, "gs_set_termios: no old_termios: "
		           "no optimization\n");

	if(old_termios && (gs_debug & GS_DEBUG_TERMIOS)) {
		if(tiosp->c_iflag != old_termios->c_iflag)  printk("c_iflag changed\n");
		if(tiosp->c_oflag != old_termios->c_oflag)  printk("c_oflag changed\n");
		if(tiosp->c_cflag != old_termios->c_cflag)  printk("c_cflag changed\n");
		if(tiosp->c_lflag != old_termios->c_lflag)  printk("c_lflag changed\n");
		if(tiosp->c_line  != old_termios->c_line)   printk("c_line changed\n");
		if(!memcmp(tiosp->c_cc, old_termios->c_cc, NCC)) printk("c_cc changed\n");
	}

	baudrate = tiosp->c_cflag & CBAUD;
	if (baudrate & CBAUDEX) {
		baudrate &= ~CBAUDEX;
		if ((baudrate < 1) || (baudrate > 4))
			tiosp->c_cflag &= ~CBAUDEX;
		else
			baudrate += 15;
	}

	baudrate = gs_baudrates[baudrate];
	if ((tiosp->c_cflag & CBAUD) == B38400) {
		if (     (port->flags & ASYNC_SPD_MASK) == ASYNC_SPD_HI)
			baudrate = 57600;
		else if ((port->flags & ASYNC_SPD_MASK) == ASYNC_SPD_VHI)
			baudrate = 115200;
		else if ((port->flags & ASYNC_SPD_MASK) == ASYNC_SPD_SHI)
			baudrate = 230400;
		else if ((port->flags & ASYNC_SPD_MASK) == ASYNC_SPD_WARP)
			baudrate = 460800;
		else if ((port->flags & ASYNC_SPD_MASK) == ASYNC_SPD_CUST)
			baudrate = (port->baud_base / port->custom_divisor);
	}

	/* I recommend using THIS instead of the mess in termios (and
	   duplicating the above code). Next we should create a clean
	   interface towards this variable. If your card supports arbitrary
	   baud rates, (e.g. CD1400 or 16550 based cards) then everything
	   will be very easy..... */
	port->baud = baudrate;

	/* Two timer ticks seems enough to wakeup something like SLIP driver */
	/* Baudrate/10 is cps. Divide by HZ to get chars per tick. */
	tmp = (baudrate / 10 / HZ) * 2;			 

	if (tmp <                 0) tmp = 0;
	if (tmp >= SERIAL_XMIT_SIZE) tmp = SERIAL_XMIT_SIZE-1;

	port->wakeup_chars = tmp;

	/* We should really wait for the characters to be all sent before
	   changing the settings. -- CAL */
	rv = gs_wait_tx_flushed (port, MAX_SCHEDULE_TIMEOUT);
	if (rv < 0) return /* rv */;

	rv = port->rd->set_real_termios(port);
	if (rv < 0) return /* rv */;

	if ((!old_termios || 
	     (old_termios->c_cflag & CRTSCTS)) &&
	    !(      tiosp->c_cflag & CRTSCTS)) {
		tty->stopped = 0;
		gs_start(tty);
	}

#ifdef tytso_patch_94Nov25_1726
	/* This "makes sense", Why is it commented out? */

	if (!(old_termios->c_cflag & CLOCAL) &&
	    (tty->termios->c_cflag & CLOCAL))
		wake_up_interruptible(&info->open_wait);
#endif

	func_exit();
	return /* 0 */;
}



/* Must be called with interrupts enabled */
int gs_init_port(struct gs_port *port)
{
	unsigned long flags;
	unsigned long page;

	save_flags (flags);
	if (!tmp_buf) {
		page = get_free_page(GFP_KERNEL);

		cli (); /* Don't expect this to make a difference. */ 
		if (tmp_buf)
			free_page(page);
		else
			tmp_buf = (unsigned char *) page;
		restore_flags (flags);

		if (!tmp_buf) {
			return -ENOMEM;
		}
	}

	if (port->flags & ASYNC_INITIALIZED)
		return 0;

	if (!port->xmit_buf) {
		/* We may sleep in get_free_page() */
		unsigned long tmp;

		tmp = get_free_page(GFP_KERNEL);

		/* Spinlock? */
		cli ();
		if (port->xmit_buf) 
			free_page (tmp);
		else
			port->xmit_buf = (unsigned char *) tmp;
		restore_flags (flags);

		if (!port->xmit_buf)
			return -ENOMEM;
	}

	cli();

	if (port->tty) 
		clear_bit(TTY_IO_ERROR, &port->tty->flags);

	port->xmit_cnt = port->xmit_head = port->xmit_tail = 0;

	gs_set_termios(port->tty, NULL);

	port->flags |= ASYNC_INITIALIZED;
	port->flags &= ~GS_TX_INTEN;

	restore_flags(flags);
	return 0;
}


int gs_setserial(struct gs_port *port, struct serial_struct *sp)
{
	struct serial_struct sio;

	if (copy_from_user(&sio, sp, sizeof(struct serial_struct)))
		return(-EFAULT);

	if (!capable(CAP_SYS_ADMIN)) {
		if ((sio.baud_base != port->baud_base) ||
		    (sio.close_delay != port->close_delay) ||
		    ((sio.flags & ~ASYNC_USR_MASK) !=
		     (port->flags & ~ASYNC_USR_MASK)))
			return(-EPERM);
	} 

	port->flags = (port->flags & ~ASYNC_USR_MASK) |
		(sio.flags & ASYNC_USR_MASK);
  
	port->baud_base = sio.baud_base;
	port->close_delay = sio.close_delay;
	port->closing_wait = sio.closing_wait;
	port->custom_divisor = sio.custom_divisor;

	gs_set_termios (port->tty, NULL);

	return 0;
}


/*****************************************************************************/

/*
 *      Generate the serial struct info.
 */

int gs_getserial(struct gs_port *port, struct serial_struct *sp)
{
	struct serial_struct    sio;

	memset(&sio, 0, sizeof(struct serial_struct));
	sio.flags = port->flags;
	sio.baud_base = port->baud_base;
	sio.close_delay = port->close_delay;
	sio.closing_wait = port->closing_wait;
	sio.custom_divisor = port->custom_divisor;
	sio.hub6 = 0;

	/* If you want you can override these. */
	sio.type = PORT_UNKNOWN;
	sio.xmit_fifo_size = -1;
	sio.line = -1;
	sio.port = -1;
	sio.irq = -1;

	if (port->rd->getserial)
		port->rd->getserial (port, &sio);

	if (copy_to_user(sp, &sio, sizeof(struct serial_struct)))
		return -EFAULT;
	return 0;

}


void gs_got_break(struct gs_port *port)
{
	if (port->flags & ASYNC_SAK) {
		do_SAK (port->tty);
	}
	*(port->tty->flip.flag_buf_ptr) = TTY_BREAK;
	port->tty->flip.flag_buf_ptr++;
	port->tty->flip.char_buf_ptr++;
	port->tty->flip.count++;
}


EXPORT_SYMBOL(gs_put_char);
EXPORT_SYMBOL(gs_write);
EXPORT_SYMBOL(gs_write_room);
EXPORT_SYMBOL(gs_chars_in_buffer);
EXPORT_SYMBOL(gs_flush_buffer);
EXPORT_SYMBOL(gs_flush_chars);
EXPORT_SYMBOL(gs_stop);
EXPORT_SYMBOL(gs_start);
EXPORT_SYMBOL(gs_hangup);
EXPORT_SYMBOL(gs_do_softint);
EXPORT_SYMBOL(gs_block_til_ready);
EXPORT_SYMBOL(gs_close);
EXPORT_SYMBOL(gs_set_termios);
EXPORT_SYMBOL(gs_init_port);
EXPORT_SYMBOL(gs_setserial);
EXPORT_SYMBOL(gs_getserial);
EXPORT_SYMBOL(gs_got_break);

MODULE_LICENSE("GPL");
