/*
 * n_tty.c --- implements the N_TTY line discipline.
 * 
 * This code used to be in tty_io.c, but things are getting hairy
 * enough that it made sense to split things off.  (The N_TTY
 * processing has changed so much that it's hardly recognizable,
 * anyway...)
 *
 * Note that the open routine for N_TTY is guaranteed never to return
 * an error.  This is because Linux will fall back to setting a line
 * to N_TTY if it can not switch to any other line discipline.  
 *
 * Written by Theodore Ts'o, Copyright 1994.
 * 
 * This file also contains code originally written by Linus Torvalds,
 * Copyright 1991, 1992, 1993, and by Julian Cowley, Copyright 1994.
 * 
 * This file may be redistributed under the terms of the GNU General Public
 * License.
 *
 * Reduced memory usage for older ARM systems  - Russell King.
 *
 * 2000/01/20   Fixed SMP locking on put_tty_queue using bits of 
 *		the patch by Andrew J. Kroll <ag784@freenet.buffalo.edu>
 *		who actually finally proved there really was a race.
 *
 * 2002/03/18   Implemented n_tty_wakeup to send SIGIO POLL_OUTs to
 *		waiting writing processes-Sapan Bhatia <sapan@corewars.org>.
 *		Also fixed a bug in BLOCKING mode where write_chan returns
 *		EAGAIN
 */

#include <linux/types.h>
#include <linux/major.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/fcntl.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/timer.h>
#include <linux/ctype.h>
#include <linux/kd.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/poll.h>

#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/bitops.h>

#define CONSOLE_DEV MKDEV(TTY_MAJOR,0)
#define SYSCONS_DEV  MKDEV(TTYAUX_MAJOR,1)

#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif

/* number of characters left in xmit buffer before select has we have room */
#define WAKEUP_CHARS 256

/*
 * This defines the low- and high-watermarks for throttling and
 * unthrottling the TTY driver.  These watermarks are used for
 * controlling the space in the read buffer.
 */
#define TTY_THRESHOLD_THROTTLE		128 /* now based on remaining room */
#define TTY_THRESHOLD_UNTHROTTLE 	128

static inline unsigned char *alloc_buf(void)
{
	unsigned char *p;
	int prio = in_interrupt() ? GFP_ATOMIC : GFP_KERNEL;

	if (PAGE_SIZE != N_TTY_BUF_SIZE) {
		p = kmalloc(N_TTY_BUF_SIZE, prio);
		if (p)
			memset(p, 0, N_TTY_BUF_SIZE);
	} else
		p = (unsigned char *)get_zeroed_page(prio);

	return p;
}

static inline void free_buf(unsigned char *buf)
{
	if (PAGE_SIZE != N_TTY_BUF_SIZE)
		kfree(buf);
	else
		free_page((unsigned long) buf);
}

static inline void put_tty_queue_nolock(unsigned char c, struct tty_struct *tty)
{
	if (tty->read_cnt < N_TTY_BUF_SIZE) {
		tty->read_buf[tty->read_head] = c;
		tty->read_head = (tty->read_head + 1) & (N_TTY_BUF_SIZE-1);
		tty->read_cnt++;
	}
}

static inline void put_tty_queue(unsigned char c, struct tty_struct *tty)
{
	unsigned long flags;
	/*
	 *	The problem of stomping on the buffers ends here.
	 *	Why didn't anyone see this one coming? --AJK
	*/
	spin_lock_irqsave(&tty->read_lock, flags);
	put_tty_queue_nolock(c, tty);
	spin_unlock_irqrestore(&tty->read_lock, flags);
}

/* 
 * Check whether to call the driver.unthrottle function.
 * We test the TTY_THROTTLED bit first so that it always
 * indicates the current state.
 */
static void check_unthrottle(struct tty_struct * tty)
{
	if (tty->count &&
	    test_and_clear_bit(TTY_THROTTLED, &tty->flags) && 
	    tty->driver.unthrottle)
		tty->driver.unthrottle(tty);
}

/*
 * Reset the read buffer counters, clear the flags, 
 * and make sure the driver is unthrottled. Called
 * from n_tty_open() and n_tty_flush_buffer().
 */
static void reset_buffer_flags(struct tty_struct *tty)
{
	unsigned long flags;

	spin_lock_irqsave(&tty->read_lock, flags);
	tty->read_head = tty->read_tail = tty->read_cnt = 0;
	spin_unlock_irqrestore(&tty->read_lock, flags);
	tty->canon_head = tty->canon_data = tty->erasing = 0;
	memset(&tty->read_flags, 0, sizeof tty->read_flags);
	check_unthrottle(tty);
}

/*
 * Flush the input buffer
 */
void n_tty_flush_buffer(struct tty_struct * tty)
{
	/* clear everything and unthrottle the driver */
	reset_buffer_flags(tty);
	
	if (!tty->link)
		return;

	if (tty->link->packet) {
		tty->ctrl_status |= TIOCPKT_FLUSHREAD;
		wake_up_interruptible(&tty->link->read_wait);
	}
}

/*
 * Return number of characters buffered to be delivered to user
 */
ssize_t n_tty_chars_in_buffer(struct tty_struct *tty)
{
	unsigned long flags;
	ssize_t n = 0;

	spin_lock_irqsave(&tty->read_lock, flags);
	if (!tty->icanon) {
		n = tty->read_cnt;
	} else if (tty->canon_data) {
		n = (tty->canon_head > tty->read_tail) ?
			tty->canon_head - tty->read_tail :
			tty->canon_head + (N_TTY_BUF_SIZE - tty->read_tail);
	}
	spin_unlock_irqrestore(&tty->read_lock, flags);
	return n;
}

/*
 * Perform OPOST processing.  Returns -1 when the output device is
 * full and the character must be retried.
 */
static int opost(unsigned char c, struct tty_struct *tty)
{
	int	space, spaces;

	space = tty->driver.write_room(tty);
	if (!space)
		return -1;

	if (O_OPOST(tty)) {
		switch (c) {
		case '\n':
			if (O_ONLRET(tty))
				tty->column = 0;
			if (O_ONLCR(tty)) {
				if (space < 2)
					return -1;
				tty->driver.put_char(tty, '\r');
				tty->column = 0;
			}
			tty->canon_column = tty->column;
			break;
		case '\r':
			if (O_ONOCR(tty) && tty->column == 0)
				return 0;
			if (O_OCRNL(tty)) {
				c = '\n';
				if (O_ONLRET(tty))
					tty->canon_column = tty->column = 0;
				break;
			}
			tty->canon_column = tty->column = 0;
			break;
		case '\t':
			spaces = 8 - (tty->column & 7);
			if (O_TABDLY(tty) == XTABS) {
				if (space < spaces)
					return -1;
				tty->column += spaces;
				tty->driver.write(tty, 0, "        ", spaces);
				return 0;
			}
			tty->column += spaces;
			break;
		case '\b':
			if (tty->column > 0)
				tty->column--;
			break;
		default:
			if (O_OLCUC(tty))
				c = toupper(c);
			if (!iscntrl(c))
				tty->column++;
			break;
		}
	}
	tty->driver.put_char(tty, c);
	return 0;
}

/*
 * opost_block --- to speed up block console writes, among other
 * things.
 */
static ssize_t opost_block(struct tty_struct * tty,
		       const unsigned char * inbuf, unsigned int nr)
{
	char	buf[80];
	int	space;
	int 	i;
	char	*cp;

	space = tty->driver.write_room(tty);
	if (!space)
		return 0;
	if (nr > space)
		nr = space;
	if (nr > sizeof(buf))
	    nr = sizeof(buf);

	if (copy_from_user(buf, inbuf, nr))
		return -EFAULT;

	for (i = 0, cp = buf; i < nr; i++, cp++) {
		switch (*cp) {
		case '\n':
			if (O_ONLRET(tty))
				tty->column = 0;
			if (O_ONLCR(tty))
				goto break_out;
			tty->canon_column = tty->column;
			break;
		case '\r':
			if (O_ONOCR(tty) && tty->column == 0)
				goto break_out;
			if (O_OCRNL(tty)) {
				*cp = '\n';
				if (O_ONLRET(tty))
					tty->canon_column = tty->column = 0;
				break;
			}
			tty->canon_column = tty->column = 0;
			break;
		case '\t':
			goto break_out;
		case '\b':
			if (tty->column > 0)
				tty->column--;
			break;
		default:
			if (O_OLCUC(tty))
				*cp = toupper(*cp);
			if (!iscntrl(*cp))
				tty->column++;
			break;
		}
	}
break_out:
	if (tty->driver.flush_chars)
		tty->driver.flush_chars(tty);
	i = tty->driver.write(tty, 0, buf, i);	
	return i;
}



static inline void put_char(unsigned char c, struct tty_struct *tty)
{
	tty->driver.put_char(tty, c);
}

/* Must be called only when L_ECHO(tty) is true. */

static void echo_char(unsigned char c, struct tty_struct *tty)
{
	if (L_ECHOCTL(tty) && iscntrl(c) && c != '\t') {
		put_char('^', tty);
		put_char(c ^ 0100, tty);
		tty->column += 2;
	} else
		opost(c, tty);
}

static inline void finish_erasing(struct tty_struct *tty)
{
	if (tty->erasing) {
		put_char('/', tty);
		tty->column += 2;
		tty->erasing = 0;
	}
}

static void eraser(unsigned char c, struct tty_struct *tty)
{
	enum { ERASE, WERASE, KILL } kill_type;
	int head, seen_alnums;
	unsigned long flags;

	if (tty->read_head == tty->canon_head) {
		/* opost('\a', tty); */		/* what do you think? */
		return;
	}
	if (c == ERASE_CHAR(tty))
		kill_type = ERASE;
	else if (c == WERASE_CHAR(tty))
		kill_type = WERASE;
	else {
		if (!L_ECHO(tty)) {
			spin_lock_irqsave(&tty->read_lock, flags);
			tty->read_cnt -= ((tty->read_head - tty->canon_head) &
					  (N_TTY_BUF_SIZE - 1));
			tty->read_head = tty->canon_head;
			spin_unlock_irqrestore(&tty->read_lock, flags);
			return;
		}
		if (!L_ECHOK(tty) || !L_ECHOKE(tty) || !L_ECHOE(tty)) {
			spin_lock_irqsave(&tty->read_lock, flags);
			tty->read_cnt -= ((tty->read_head - tty->canon_head) &
					  (N_TTY_BUF_SIZE - 1));
			tty->read_head = tty->canon_head;
			spin_unlock_irqrestore(&tty->read_lock, flags);
			finish_erasing(tty);
			echo_char(KILL_CHAR(tty), tty);
			/* Add a newline if ECHOK is on and ECHOKE is off. */
			if (L_ECHOK(tty))
				opost('\n', tty);
			return;
		}
		kill_type = KILL;
	}

	seen_alnums = 0;
	while (tty->read_head != tty->canon_head) {
		head = (tty->read_head - 1) & (N_TTY_BUF_SIZE-1);
		c = tty->read_buf[head];
		if (kill_type == WERASE) {
			/* Equivalent to BSD's ALTWERASE. */
			if (isalnum(c) || c == '_')
				seen_alnums++;
			else if (seen_alnums)
				break;
		}
		spin_lock_irqsave(&tty->read_lock, flags);
		tty->read_head = head;
		tty->read_cnt--;
		spin_unlock_irqrestore(&tty->read_lock, flags);
		if (L_ECHO(tty)) {
			if (L_ECHOPRT(tty)) {
				if (!tty->erasing) {
					put_char('\\', tty);
					tty->column++;
					tty->erasing = 1;
				}
				echo_char(c, tty);
			} else if (kill_type == ERASE && !L_ECHOE(tty)) {
				echo_char(ERASE_CHAR(tty), tty);
			} else if (c == '\t') {
				unsigned int col = tty->canon_column;
				unsigned long tail = tty->canon_head;

				/* Find the column of the last char. */
				while (tail != tty->read_head) {
					c = tty->read_buf[tail];
					if (c == '\t')
						col = (col | 7) + 1;
					else if (iscntrl(c)) {
						if (L_ECHOCTL(tty))
							col += 2;
					} else
						col++;
					tail = (tail+1) & (N_TTY_BUF_SIZE-1);
				}

				/* should never happen */
				if (tty->column > 0x80000000)
					tty->column = 0; 

				/* Now backup to that column. */
				while (tty->column > col) {
					/* Can't use opost here. */
					put_char('\b', tty);
					if (tty->column > 0)
						tty->column--;
				}
			} else {
				if (iscntrl(c) && L_ECHOCTL(tty)) {
					put_char('\b', tty);
					put_char(' ', tty);
					put_char('\b', tty);
					if (tty->column > 0)
						tty->column--;
				}
				if (!iscntrl(c) || L_ECHOCTL(tty)) {
					put_char('\b', tty);
					put_char(' ', tty);
					put_char('\b', tty);
					if (tty->column > 0)
						tty->column--;
				}
			}
		}
		if (kill_type == ERASE)
			break;
	}
	if (tty->read_head == tty->canon_head)
		finish_erasing(tty);
}

static inline void isig(int sig, struct tty_struct *tty, int flush)
{
	if (tty->pgrp > 0)
		kill_pg(tty->pgrp, sig, 1);
	if (flush || !L_NOFLSH(tty)) {
		n_tty_flush_buffer(tty);
		if (tty->driver.flush_buffer)
			tty->driver.flush_buffer(tty);
	}
}

static inline void n_tty_receive_break(struct tty_struct *tty)
{
	if (I_IGNBRK(tty))
		return;
	if (I_BRKINT(tty)) {
		isig(SIGINT, tty, 1);
		return;
	}
	if (I_PARMRK(tty)) {
		put_tty_queue('\377', tty);
		put_tty_queue('\0', tty);
	}
	put_tty_queue('\0', tty);
	wake_up_interruptible(&tty->read_wait);
}

static inline void n_tty_receive_overrun(struct tty_struct *tty)
{
	char buf[64];

	tty->num_overrun++;
	if (time_before(tty->overrun_time, jiffies - HZ)) {
		printk("%s: %d input overrun(s)\n", tty_name(tty, buf),
		       tty->num_overrun);
		tty->overrun_time = jiffies;
		tty->num_overrun = 0;
	}
}

static inline void n_tty_receive_parity_error(struct tty_struct *tty,
					      unsigned char c)
{
	if (I_IGNPAR(tty)) {
		return;
	}
	if (I_PARMRK(tty)) {
		put_tty_queue('\377', tty);
		put_tty_queue('\0', tty);
		put_tty_queue(c, tty);
	} else	if (I_INPCK(tty))
		put_tty_queue('\0', tty);
	else
		put_tty_queue(c, tty);
	wake_up_interruptible(&tty->read_wait);
}

static inline void n_tty_receive_char(struct tty_struct *tty, unsigned char c)
{
	unsigned long flags;

	if (tty->raw) {
		put_tty_queue(c, tty);
		return;
	}
	
	if (tty->stopped && !tty->flow_stopped &&
	    I_IXON(tty) && I_IXANY(tty)) {
		start_tty(tty);
		return;
	}
	
	if (I_ISTRIP(tty))
		c &= 0x7f;
	if (I_IUCLC(tty) && L_IEXTEN(tty))
		c=tolower(c);

	if (tty->closing) {
		if (I_IXON(tty)) {
			if (c == START_CHAR(tty))
				start_tty(tty);
			else if (c == STOP_CHAR(tty))
				stop_tty(tty);
		}
		return;
	}

	/*
	 * If the previous character was LNEXT, or we know that this
	 * character is not one of the characters that we'll have to
	 * handle specially, do shortcut processing to speed things
	 * up.
	 */
	if (!test_bit(c, &tty->process_char_map) || tty->lnext) {
		finish_erasing(tty);
		tty->lnext = 0;
		if (L_ECHO(tty)) {
			if (tty->read_cnt >= N_TTY_BUF_SIZE-1) {
				put_char('\a', tty); /* beep if no space */
				return;
			}
			/* Record the column of first canon char. */
			if (tty->canon_head == tty->read_head)
				tty->canon_column = tty->column;
			echo_char(c, tty);
		}
		if (I_PARMRK(tty) && c == (unsigned char) '\377')
			put_tty_queue(c, tty);
		put_tty_queue(c, tty);
		return;
	}
		
	if (c == '\r') {
		if (I_IGNCR(tty))
			return;
		if (I_ICRNL(tty))
			c = '\n';
	} else if (c == '\n' && I_INLCR(tty))
		c = '\r';
	if (I_IXON(tty)) {
		if (c == START_CHAR(tty)) {
			start_tty(tty);
			return;
		}
		if (c == STOP_CHAR(tty)) {
			stop_tty(tty);
			return;
		}
	}
	if (L_ISIG(tty)) {
		int signal;
		signal = SIGINT;
		if (c == INTR_CHAR(tty))
			goto send_signal;
		signal = SIGQUIT;
		if (c == QUIT_CHAR(tty))
			goto send_signal;
		signal = SIGTSTP;
		if (c == SUSP_CHAR(tty)) {
send_signal:
			isig(signal, tty, 0);
			return;
		}
	}
	if (tty->icanon) {
		if (c == ERASE_CHAR(tty) || c == KILL_CHAR(tty) ||
		    (c == WERASE_CHAR(tty) && L_IEXTEN(tty))) {
			eraser(c, tty);
			return;
		}
		if (c == LNEXT_CHAR(tty) && L_IEXTEN(tty)) {
			tty->lnext = 1;
			if (L_ECHO(tty)) {
				finish_erasing(tty);
				if (L_ECHOCTL(tty)) {
					put_char('^', tty);
					put_char('\b', tty);
				}
			}
			return;
		}
		if (c == REPRINT_CHAR(tty) && L_ECHO(tty) &&
		    L_IEXTEN(tty)) {
			unsigned long tail = tty->canon_head;

			finish_erasing(tty);
			echo_char(c, tty);
			opost('\n', tty);
			while (tail != tty->read_head) {
				echo_char(tty->read_buf[tail], tty);
				tail = (tail+1) & (N_TTY_BUF_SIZE-1);
			}
			return;
		}
		if (c == '\n') {
			if (L_ECHO(tty) || L_ECHONL(tty)) {
				if (tty->read_cnt >= N_TTY_BUF_SIZE-1) {
					put_char('\a', tty);
					return;
				}
				opost('\n', tty);
			}
			goto handle_newline;
		}
		if (c == EOF_CHAR(tty)) {
		        if (tty->canon_head != tty->read_head)
			        set_bit(TTY_PUSH, &tty->flags);
			c = __DISABLED_CHAR;
			goto handle_newline;
		}
		if ((c == EOL_CHAR(tty)) ||
		    (c == EOL2_CHAR(tty) && L_IEXTEN(tty))) {
			/*
			 * XXX are EOL_CHAR and EOL2_CHAR echoed?!?
			 */
			if (L_ECHO(tty)) {
				if (tty->read_cnt >= N_TTY_BUF_SIZE-1) {
					put_char('\a', tty);
					return;
				}
				/* Record the column of first canon char. */
				if (tty->canon_head == tty->read_head)
					tty->canon_column = tty->column;
				echo_char(c, tty);
			}
			/*
			 * XXX does PARMRK doubling happen for
			 * EOL_CHAR and EOL2_CHAR?
			 */
			if (I_PARMRK(tty) && c == (unsigned char) '\377')
				put_tty_queue(c, tty);

		handle_newline:
			spin_lock_irqsave(&tty->read_lock, flags);
			set_bit(tty->read_head, &tty->read_flags);
			put_tty_queue_nolock(c, tty);
			tty->canon_head = tty->read_head;
			tty->canon_data++;
			spin_unlock_irqrestore(&tty->read_lock, flags);
			kill_fasync(&tty->fasync, SIGIO, POLL_IN);
			if (waitqueue_active(&tty->read_wait))
				wake_up_interruptible(&tty->read_wait);
			return;
		}
	}
	
	finish_erasing(tty);
	if (L_ECHO(tty)) {
		if (tty->read_cnt >= N_TTY_BUF_SIZE-1) {
			put_char('\a', tty); /* beep if no space */
			return;
		}
		if (c == '\n')
			opost('\n', tty);
		else {
			/* Record the column of first canon char. */
			if (tty->canon_head == tty->read_head)
				tty->canon_column = tty->column;
			echo_char(c, tty);
		}
	}

	if (I_PARMRK(tty) && c == (unsigned char) '\377')
		put_tty_queue(c, tty);

	put_tty_queue(c, tty);
}	

static int n_tty_receive_room(struct tty_struct *tty)
{
	int	left = N_TTY_BUF_SIZE - tty->read_cnt - 1;

	/*
	 * If we are doing input canonicalization, and there are no
	 * pending newlines, let characters through without limit, so
	 * that erase characters will be handled.  Other excess
	 * characters will be beeped.
	 */
	if (tty->icanon && !tty->canon_data)
		return N_TTY_BUF_SIZE;

	if (left > 0)
		return left;
	return 0;
}

/*
 * Required for the ptys, serial driver etc. since processes
 * that attach themselves to the master and rely on ASYNC
 * IO must be woken up
 */

static void n_tty_write_wakeup(struct tty_struct *tty)
{
	if (tty->fasync)
	{
 		set_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);
		kill_fasync(&tty->fasync, SIGIO, POLL_OUT);
	}
	return;
}

static void n_tty_receive_buf(struct tty_struct *tty, const unsigned char *cp,
			      char *fp, int count)
{
	const unsigned char *p;
	char *f, flags = TTY_NORMAL;
	int	i;
	char	buf[64];
	unsigned long cpuflags;

	if (!tty->read_buf)
		return;

	if (tty->real_raw) {
		spin_lock_irqsave(&tty->read_lock, cpuflags);
		i = MIN(count, MIN(N_TTY_BUF_SIZE - tty->read_cnt,
				   N_TTY_BUF_SIZE - tty->read_head));
		memcpy(tty->read_buf + tty->read_head, cp, i);
		tty->read_head = (tty->read_head + i) & (N_TTY_BUF_SIZE-1);
		tty->read_cnt += i;
		cp += i;
		count -= i;

		i = MIN(count, MIN(N_TTY_BUF_SIZE - tty->read_cnt,
			       N_TTY_BUF_SIZE - tty->read_head));
		memcpy(tty->read_buf + tty->read_head, cp, i);
		tty->read_head = (tty->read_head + i) & (N_TTY_BUF_SIZE-1);
		tty->read_cnt += i;
		spin_unlock_irqrestore(&tty->read_lock, cpuflags);
	} else {
		for (i=count, p = cp, f = fp; i; i--, p++) {
			if (f)
				flags = *f++;
			switch (flags) {
			case TTY_NORMAL:
				n_tty_receive_char(tty, *p);
				break;
			case TTY_BREAK:
				n_tty_receive_break(tty);
				break;
			case TTY_PARITY:
			case TTY_FRAME:
				n_tty_receive_parity_error(tty, *p);
				break;
			case TTY_OVERRUN:
				n_tty_receive_overrun(tty);
				break;
			default:
				printk("%s: unknown flag %d\n",
				       tty_name(tty, buf), flags);
				break;
			}
		}
		if (tty->driver.flush_chars)
			tty->driver.flush_chars(tty);
	}

	if (!tty->icanon && (tty->read_cnt >= tty->minimum_to_wake)) {
		kill_fasync(&tty->fasync, SIGIO, POLL_IN);
		if (waitqueue_active(&tty->read_wait))
			wake_up_interruptible(&tty->read_wait);
	}

	/*
	 * Check the remaining room for the input canonicalization
	 * mode.  We don't want to throttle the driver if we're in
	 * canonical mode and don't have a newline yet!
	 */
	if (n_tty_receive_room(tty) < TTY_THRESHOLD_THROTTLE) {
		/* check TTY_THROTTLED first so it indicates our state */
		if (!test_and_set_bit(TTY_THROTTLED, &tty->flags) &&
		    tty->driver.throttle)
			tty->driver.throttle(tty);
	}
}

int is_ignored(int sig)
{
	return (sigismember(&current->blocked, sig) ||
	        current->sig->action[sig-1].sa.sa_handler == SIG_IGN);
}

static void n_tty_set_termios(struct tty_struct *tty, struct termios * old)
{
	if (!tty)
		return;
	
	tty->icanon = (L_ICANON(tty) != 0);
	if (test_bit(TTY_HW_COOK_IN, &tty->flags)) {
		tty->raw = 1;
		tty->real_raw = 1;
		return;
	}
	if (I_ISTRIP(tty) || I_IUCLC(tty) || I_IGNCR(tty) ||
	    I_ICRNL(tty) || I_INLCR(tty) || L_ICANON(tty) ||
	    I_IXON(tty) || L_ISIG(tty) || L_ECHO(tty) ||
	    I_PARMRK(tty)) {
		cli();
		memset(tty->process_char_map, 0, 256/8);

		if (I_IGNCR(tty) || I_ICRNL(tty))
			set_bit('\r', &tty->process_char_map);
		if (I_INLCR(tty))
			set_bit('\n', &tty->process_char_map);

		if (L_ICANON(tty)) {
			set_bit(ERASE_CHAR(tty), &tty->process_char_map);
			set_bit(KILL_CHAR(tty), &tty->process_char_map);
			set_bit(EOF_CHAR(tty), &tty->process_char_map);
			set_bit('\n', &tty->process_char_map);
			set_bit(EOL_CHAR(tty), &tty->process_char_map);
			if (L_IEXTEN(tty)) {
				set_bit(WERASE_CHAR(tty),
					&tty->process_char_map);
				set_bit(LNEXT_CHAR(tty),
					&tty->process_char_map);
				set_bit(EOL2_CHAR(tty),
					&tty->process_char_map);
				if (L_ECHO(tty))
					set_bit(REPRINT_CHAR(tty),
						&tty->process_char_map);
			}
		}
		if (I_IXON(tty)) {
			set_bit(START_CHAR(tty), &tty->process_char_map);
			set_bit(STOP_CHAR(tty), &tty->process_char_map);
		}
		if (L_ISIG(tty)) {
			set_bit(INTR_CHAR(tty), &tty->process_char_map);
			set_bit(QUIT_CHAR(tty), &tty->process_char_map);
			set_bit(SUSP_CHAR(tty), &tty->process_char_map);
		}
		clear_bit(__DISABLED_CHAR, &tty->process_char_map);
		sti();
		tty->raw = 0;
		tty->real_raw = 0;
	} else {
		tty->raw = 1;
		if ((I_IGNBRK(tty) || (!I_BRKINT(tty) && !I_PARMRK(tty))) &&
		    (I_IGNPAR(tty) || !I_INPCK(tty)) &&
		    (tty->driver.flags & TTY_DRIVER_REAL_RAW))
			tty->real_raw = 1;
		else
			tty->real_raw = 0;
	}
}

static void n_tty_close(struct tty_struct *tty)
{
	n_tty_flush_buffer(tty);
	if (tty->read_buf) {
		free_buf(tty->read_buf);
		tty->read_buf = 0;
	}
}

static int n_tty_open(struct tty_struct *tty)
{
	if (!tty)
		return -EINVAL;

	if (!tty->read_buf) {
		tty->read_buf = alloc_buf();
		if (!tty->read_buf)
			return -ENOMEM;
	}
	memset(tty->read_buf, 0, N_TTY_BUF_SIZE);
	reset_buffer_flags(tty);
	tty->column = 0;
	n_tty_set_termios(tty, 0);
	tty->minimum_to_wake = 1;
	tty->closing = 0;
	return 0;
}

static inline int input_available_p(struct tty_struct *tty, int amt)
{
	if (tty->icanon) {
		if (tty->canon_data)
			return 1;
	} else if (tty->read_cnt >= (amt ? amt : 1))
		return 1;

	return 0;
}

/*
 * Helper function to speed up read_chan.  It is only called when
 * ICANON is off; it copies characters straight from the tty queue to
 * user space directly.  It can be profitably called twice; once to
 * drain the space from the tail pointer to the (physical) end of the
 * buffer, and once to drain the space from the (physical) beginning of
 * the buffer to head pointer.
 */
static inline int copy_from_read_buf(struct tty_struct *tty,
				      unsigned char **b,
				      size_t *nr)

{
	int retval;
	ssize_t n;
	unsigned long flags;

	retval = 0;
	spin_lock_irqsave(&tty->read_lock, flags);
	n = MIN(*nr, MIN(tty->read_cnt, N_TTY_BUF_SIZE - tty->read_tail));
	spin_unlock_irqrestore(&tty->read_lock, flags);
	if (n) {
		mb();
		retval = copy_to_user(*b, &tty->read_buf[tty->read_tail], n);
		n -= retval;
		spin_lock_irqsave(&tty->read_lock, flags);
		tty->read_tail = (tty->read_tail + n) & (N_TTY_BUF_SIZE-1);
		tty->read_cnt -= n;
		spin_unlock_irqrestore(&tty->read_lock, flags);
		*b += n;
		*nr -= n;
	}
	return retval;
}

static ssize_t read_chan(struct tty_struct *tty, struct file *file,
			 unsigned char *buf, size_t nr)
{
	unsigned char *b = buf;
	DECLARE_WAITQUEUE(wait, current);
	int c;
	int minimum, time;
	ssize_t retval = 0;
	ssize_t size;
	long timeout;
	unsigned long flags;

do_it_again:

	if (!tty->read_buf) {
		printk("n_tty_read_chan: called with read_buf == NULL?!?\n");
		return -EIO;
	}

	/* Job control check -- must be done at start and after
	   every sleep (POSIX.1 7.1.1.4). */
	/* NOTE: not yet done after every sleep pending a thorough
	   check of the logic of this change. -- jlc */
	/* don't stop on /dev/console */
	if (file->f_dentry->d_inode->i_rdev != CONSOLE_DEV &&
	    file->f_dentry->d_inode->i_rdev != SYSCONS_DEV &&
	    current->tty == tty) {
		if (tty->pgrp <= 0)
			printk("read_chan: tty->pgrp <= 0!\n");
		else if (current->pgrp != tty->pgrp) {
			if (is_ignored(SIGTTIN) ||
			    is_orphaned_pgrp(current->pgrp))
				return -EIO;
			kill_pg(current->pgrp, SIGTTIN, 1);
			return -ERESTARTSYS;
		}
	}

	minimum = time = 0;
	timeout = MAX_SCHEDULE_TIMEOUT;
	if (!tty->icanon) {
		time = (HZ / 10) * TIME_CHAR(tty);
		minimum = MIN_CHAR(tty);
		if (minimum) {
			if (time)
				tty->minimum_to_wake = 1;
			else if (!waitqueue_active(&tty->read_wait) ||
				 (tty->minimum_to_wake > minimum))
				tty->minimum_to_wake = minimum;
		} else {
			timeout = 0;
			if (time) {
				timeout = time;
				time = 0;
			}
			tty->minimum_to_wake = minimum = 1;
		}
	}

	if (file->f_flags & O_NONBLOCK) {
		if (down_trylock(&tty->atomic_read))
			return -EAGAIN;
	}
	else {
		if (down_interruptible(&tty->atomic_read))
			return -ERESTARTSYS;
	}

	add_wait_queue(&tty->read_wait, &wait);
	set_bit(TTY_DONT_FLIP, &tty->flags);
	while (nr) {
		/* First test for status change. */
		if (tty->packet && tty->link->ctrl_status) {
			unsigned char cs;
			if (b != buf)
				break;
			cs = tty->link->ctrl_status;
			tty->link->ctrl_status = 0;
			put_user(cs, b++);
			nr--;
			break;
		}
		/* This statement must be first before checking for input
		   so that any interrupt will set the state back to
		   TASK_RUNNING. */
		set_current_state(TASK_INTERRUPTIBLE);
		
		if (((minimum - (b - buf)) < tty->minimum_to_wake) &&
		    ((minimum - (b - buf)) >= 1))
			tty->minimum_to_wake = (minimum - (b - buf));
		
		if (!input_available_p(tty, 0)) {
			if (test_bit(TTY_OTHER_CLOSED, &tty->flags)) {
				retval = -EIO;
				break;
			}
			if (tty_hung_up_p(file))
				break;
			if (!timeout)
				break;
			if (file->f_flags & O_NONBLOCK) {
				retval = -EAGAIN;
				break;
			}
			if (signal_pending(current)) {
				retval = -ERESTARTSYS;
				break;
			}
			clear_bit(TTY_DONT_FLIP, &tty->flags);
			timeout = schedule_timeout(timeout);
			set_bit(TTY_DONT_FLIP, &tty->flags);
			continue;
		}
		current->state = TASK_RUNNING;

		/* Deal with packet mode. */
		if (tty->packet && b == buf) {
			put_user(TIOCPKT_DATA, b++);
			nr--;
		}

		if (tty->icanon) {
			/* N.B. avoid overrun if nr == 0 */
			while (nr && tty->read_cnt) {
 				int eol;

				eol = test_and_clear_bit(tty->read_tail,
						&tty->read_flags);
				c = tty->read_buf[tty->read_tail];
				spin_lock_irqsave(&tty->read_lock, flags);
				tty->read_tail = ((tty->read_tail+1) &
						  (N_TTY_BUF_SIZE-1));
				tty->read_cnt--;
				if (eol) {
					/* this test should be redundant:
					 * we shouldn't be reading data if
					 * canon_data is 0
					 */
					if (--tty->canon_data < 0)
						tty->canon_data = 0;
				}
				spin_unlock_irqrestore(&tty->read_lock, flags);

				if (!eol || (c != __DISABLED_CHAR)) {
					put_user(c, b++);
					nr--;
				}
				if (eol)
					break;
			}
		} else {
			int uncopied;
			uncopied = copy_from_read_buf(tty, &b, &nr);
			uncopied += copy_from_read_buf(tty, &b, &nr);
			if (uncopied) {
				retval = -EFAULT;
				break;
			}
		}

		/* If there is enough space in the read buffer now, let the
		 * low-level driver know. We use n_tty_chars_in_buffer() to
		 * check the buffer, as it now knows about canonical mode.
		 * Otherwise, if the driver is throttled and the line is
		 * longer than TTY_THRESHOLD_UNTHROTTLE in canonical mode,
		 * we won't get any more characters.
		 */
		if (n_tty_chars_in_buffer(tty) <= TTY_THRESHOLD_UNTHROTTLE)
			check_unthrottle(tty);

		if (b - buf >= minimum)
			break;
		if (time)
			timeout = time;
	}
	clear_bit(TTY_DONT_FLIP, &tty->flags);
	up(&tty->atomic_read);
	remove_wait_queue(&tty->read_wait, &wait);

	if (!waitqueue_active(&tty->read_wait))
		tty->minimum_to_wake = minimum;

	current->state = TASK_RUNNING;
	size = b - buf;
	if (size) {
		retval = size;
		if (nr)
	       		clear_bit(TTY_PUSH, &tty->flags);
	} else if (test_and_clear_bit(TTY_PUSH, &tty->flags))
		 goto do_it_again;

	return retval;
}

static ssize_t write_chan(struct tty_struct * tty, struct file * file,
			  const unsigned char * buf, size_t nr)
{
	const unsigned char *b = buf;
	DECLARE_WAITQUEUE(wait, current);
	int c;
	ssize_t retval = 0;

	/* Job control check -- must be done at start (POSIX.1 7.1.1.4). */
	if (L_TOSTOP(tty) && 
	    file->f_dentry->d_inode->i_rdev != CONSOLE_DEV &&
	    file->f_dentry->d_inode->i_rdev != SYSCONS_DEV) {
		retval = tty_check_change(tty);
		if (retval)
			return retval;
	}

	add_wait_queue(&tty->write_wait, &wait);
	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}
		if (tty_hung_up_p(file) || (tty->link && !tty->link->count)) {
			retval = -EIO;
			break;
		}
		if (O_OPOST(tty) && !(test_bit(TTY_HW_COOK_OUT, &tty->flags))) {
			while (nr > 0) {
				ssize_t num = opost_block(tty, b, nr);
				if (num < 0) {
					if (num == -EAGAIN)
						break;
					retval = num;
					goto break_out;
				}
				b += num;
				nr -= num;
				if (nr == 0)
					break;
				get_user(c, b);
				if (opost(c, tty) < 0)
					break;
				b++; nr--;
			}
			if (tty->driver.flush_chars)
				tty->driver.flush_chars(tty);
		} else {
			c = tty->driver.write(tty, 1, b, nr);
			if (c < 0) {
				retval = c;
				goto break_out;
			}
			b += c;
			nr -= c;
		}
		if (!nr)
			break;
		if (file->f_flags & O_NONBLOCK) {
			retval = -EAGAIN;
			break;
		}
		schedule();
	}
break_out:
	current->state = TASK_RUNNING;
	remove_wait_queue(&tty->write_wait, &wait);
	return (b - buf) ? b - buf : retval;
}

/* Called without the kernel lock held - fine */
static unsigned int normal_poll(struct tty_struct * tty, struct file * file, poll_table *wait)
{
	unsigned int mask = 0;

	poll_wait(file, &tty->read_wait, wait);
	poll_wait(file, &tty->write_wait, wait);
	if (input_available_p(tty, TIME_CHAR(tty) ? 0 : MIN_CHAR(tty)))
		mask |= POLLIN | POLLRDNORM;
	if (tty->packet && tty->link->ctrl_status)
		mask |= POLLPRI | POLLIN | POLLRDNORM;
	if (test_bit(TTY_OTHER_CLOSED, &tty->flags))
		mask |= POLLHUP;
	if (tty_hung_up_p(file))
		mask |= POLLHUP;
	if (!(mask & (POLLHUP | POLLIN | POLLRDNORM))) {
		if (MIN_CHAR(tty) && !TIME_CHAR(tty))
			tty->minimum_to_wake = MIN_CHAR(tty);
		else
			tty->minimum_to_wake = 1;
	}
	if (tty->driver.chars_in_buffer(tty) < WAKEUP_CHARS &&
			tty->driver.write_room(tty) > 0)
		mask |= POLLOUT | POLLWRNORM;
	return mask;
}

struct tty_ldisc tty_ldisc_N_TTY = {
	TTY_LDISC_MAGIC,	/* magic */
	"n_tty",		/* name */
	0,			/* num */
	0,			/* flags */
	n_tty_open,		/* open */
	n_tty_close,		/* close */
	n_tty_flush_buffer,	/* flush_buffer */
	n_tty_chars_in_buffer,	/* chars_in_buffer */
	read_chan,		/* read */
	write_chan,		/* write */
	n_tty_ioctl,		/* ioctl */
	n_tty_set_termios,	/* set_termios */
	normal_poll,		/* poll */
	n_tty_receive_buf,	/* receive_buf */
	n_tty_receive_room,	/* receive_room */
	n_tty_write_wakeup	/* write_wakeup */
};

