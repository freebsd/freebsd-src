/*
 *  linux/kernel/printk.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 * Modified to make sys_syslog() more flexible: added commands to
 * return the last 4k of kernel messages, regardless of whether
 * they've been read or not.  Added option to suppress kernel printk's
 * to the console.  Added hook for sending the console messages
 * elsewhere, in preparation for a serial line console (someday).
 * Ted Ts'o, 2/11/93.
 * Modified for sysctl support, 1/8/97, Chris Horn.
 * Fixed SMP synchronization, 08/08/99, Manfred Spraul 
 *     manfreds@colorfullife.com
 * Rewrote bits to get rid of console_lock
 *	01Mar01 Andrew Morton <andrewm@uow.edu.au>
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/smp_lock.h>
#include <linux/console.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>			/* For in_interrupt() */
#include <linux/config.h>

#include <asm/uaccess.h>

#if !defined(CONFIG_LOG_BUF_SHIFT) || (CONFIG_LOG_BUF_SHIFT == 0)
#if defined(CONFIG_MULTIQUAD) || defined(CONFIG_IA64)
#define LOG_BUF_LEN	(65536)
#elif defined(CONFIG_ARCH_S390)
#define LOG_BUF_LEN	(131072)
#elif defined(CONFIG_SMP)
#define LOG_BUF_LEN	(32768)
#else	
#define LOG_BUF_LEN	(16384)			/* This must be a power of two */
#endif
#else /* CONFIG_LOG_BUF_SHIFT */
#define LOG_BUF_LEN (1 << CONFIG_LOG_BUF_SHIFT)
#endif

#define LOG_BUF_MASK	(LOG_BUF_LEN-1)

#ifndef arch_consoles_callable
#define arch_consoles_callable() (1)
#endif

/* printk's without a loglevel use this.. */
#define DEFAULT_MESSAGE_LOGLEVEL 4 /* KERN_WARNING */

/* We show everything that is MORE important than this.. */
#define MINIMUM_CONSOLE_LOGLEVEL 1 /* Minimum loglevel we let people use */
#define DEFAULT_CONSOLE_LOGLEVEL 7 /* anything MORE serious than KERN_DEBUG */

DECLARE_WAIT_QUEUE_HEAD(log_wait);

int console_printk[4] = {
	DEFAULT_CONSOLE_LOGLEVEL,	/* console_loglevel */
	DEFAULT_MESSAGE_LOGLEVEL,	/* default_message_loglevel */
	MINIMUM_CONSOLE_LOGLEVEL,	/* minimum_console_loglevel */
	DEFAULT_CONSOLE_LOGLEVEL,	/* default_console_loglevel */
};

int oops_in_progress;

/*
 * console_sem protects the console_drivers list, and also
 * provides serialisation for access to the entire console
 * driver system.
 */
static DECLARE_MUTEX(console_sem);
struct console *console_drivers;

/*
 * logbuf_lock protects log_buf, log_start, log_end, con_start and logged_chars
 * It is also used in interesting ways to provide interlocking in
 * release_console_sem().
 */
static spinlock_t logbuf_lock = SPIN_LOCK_UNLOCKED;

static char log_buf[LOG_BUF_LEN];
#define LOG_BUF(idx) (log_buf[(idx) & LOG_BUF_MASK])

/*
 * The indices into log_buf are not constrained to LOG_BUF_LEN - they
 * must be masked before subscripting
 */
static unsigned long log_start;			/* Index into log_buf: next char to be read by syslog() */
static unsigned long con_start;			/* Index into log_buf: next char to be sent to consoles */
static unsigned long log_end;			/* Index into log_buf: most-recently-written-char + 1 */
static unsigned long logged_chars;		/* Number of chars produced since last read+clear operation */

struct console_cmdline console_cmdline[MAX_CMDLINECONSOLES];
static int preferred_console = -1;

/* Flag: console code may call schedule() */
static int console_may_schedule;

/*
 *	Setup a list of consoles. Called from init/main.c
 */
static int __init console_setup(char *str)
{
	struct console_cmdline *c;
	char name[sizeof(c->name)];
	char *s, *options;
	int i, idx;

	/*
	 *	Decode str into name, index, options.
	 */
	if (str[0] >= '0' && str[0] <= '9') {
		strcpy(name, "ttyS");
		strncpy(name + 4, str, sizeof(name) - 5);
	} else
		strncpy(name, str, sizeof(name) - 1);
	name[sizeof(name) - 1] = 0;
	if ((options = strchr(str, ',')) != NULL)
		*(options++) = 0;
#ifdef __sparc__
	if (!strcmp(str, "ttya"))
		strcpy(name, "ttyS0");
	if (!strcmp(str, "ttyb"))
		strcpy(name, "ttyS1");
#endif
	for(s = name; *s; s++)
		if (*s >= '0' && *s <= '9')
			break;
	idx = simple_strtoul(s, NULL, 10);
	*s = 0;

	/*
	 *	See if this tty is not yet registered, and
	 *	if we have a slot free.
	 */
	for(i = 0; i < MAX_CMDLINECONSOLES && console_cmdline[i].name[0]; i++)
		if (strcmp(console_cmdline[i].name, name) == 0 &&
			  console_cmdline[i].index == idx) {
				preferred_console = i;
				return 1;
		}
	if (i == MAX_CMDLINECONSOLES)
		return 1;
	preferred_console = i;
	c = &console_cmdline[i];
	memcpy(c->name, name, sizeof(c->name));
	c->options = options;
	c->index = idx;
	return 1;
}

__setup("console=", console_setup);

/*
 * Commands to do_syslog:
 *
 * 	0 -- Close the log.  Currently a NOP.
 * 	1 -- Open the log. Currently a NOP.
 * 	2 -- Read from the log.
 * 	3 -- Read all messages remaining in the ring buffer.
 * 	4 -- Read and clear all messages remaining in the ring buffer
 * 	5 -- Clear ring buffer.
 * 	6 -- Disable printk's to console
 * 	7 -- Enable printk's to console
 *	8 -- Set level of messages printed to console
 *	9 -- Return number of unread characters in the log buffer
 */
int do_syslog(int type, char * buf, int len)
{
	unsigned long i, j, limit, count;
	int do_clear = 0;
	char c;
	int error = 0;

	switch (type) {
	case 0:		/* Close log */
		break;
	case 1:		/* Open log */
		break;
	case 2:		/* Read from log */
		error = -EINVAL;
		if (!buf || len < 0)
			goto out;
		error = 0;
		if (!len)
			goto out;
		error = verify_area(VERIFY_WRITE,buf,len);
		if (error)
			goto out;
		error = wait_event_interruptible(log_wait, (log_start - log_end));
		if (error)
			goto out;
		i = 0;
		spin_lock_irq(&logbuf_lock);
		while ((log_start != log_end) && i < len) {
			c = LOG_BUF(log_start);
			log_start++;
			spin_unlock_irq(&logbuf_lock);
			__put_user(c,buf);
			buf++;
			i++;
			spin_lock_irq(&logbuf_lock);
		}
		spin_unlock_irq(&logbuf_lock);
		error = i;
		break;
	case 4:		/* Read/clear last kernel messages */
		do_clear = 1; 
		/* FALL THRU */
	case 3:		/* Read last kernel messages */
		error = -EINVAL;
		if (!buf || len < 0)
			goto out;
		error = 0;
		if (!len)
			goto out;
		error = verify_area(VERIFY_WRITE,buf,len);
		if (error)
			goto out;
		count = len;
		if (count > LOG_BUF_LEN)
			count = LOG_BUF_LEN;
		spin_lock_irq(&logbuf_lock);
		if (count > logged_chars)
			count = logged_chars;
		if (do_clear)
			logged_chars = 0;
		limit = log_end;
		/*
		 * __put_user() could sleep, and while we sleep
		 * printk() could overwrite the messages 
		 * we try to copy to user space. Therefore
		 * the messages are copied in reverse. <manfreds>
		 */
		for(i=0;i < count;i++) {
			j = limit-1-i;
			if (j+LOG_BUF_LEN < log_end)
				break;
			c = LOG_BUF(j);
			spin_unlock_irq(&logbuf_lock);
			__put_user(c,&buf[count-1-i]);
			spin_lock_irq(&logbuf_lock);
		}
		spin_unlock_irq(&logbuf_lock);
		error = i;
		if(i != count) {
			int offset = count-error;
			/* buffer overflow during copy, correct user buffer. */
			for(i=0;i<error;i++) {
				__get_user(c,&buf[i+offset]);
				__put_user(c,&buf[i]);
			}
		}

		break;
	case 5:		/* Clear ring buffer */
		spin_lock_irq(&logbuf_lock);
		logged_chars = 0;
		spin_unlock_irq(&logbuf_lock);
		break;
	case 6:		/* Disable logging to console */
		spin_lock_irq(&logbuf_lock);
		console_loglevel = minimum_console_loglevel;
		spin_unlock_irq(&logbuf_lock);
		break;
	case 7:		/* Enable logging to console */
		spin_lock_irq(&logbuf_lock);
		console_loglevel = default_console_loglevel;
		spin_unlock_irq(&logbuf_lock);
		break;
	case 8:		/* Set level of messages printed to console */
		error = -EINVAL;
		if (len < 1 || len > 8)
			goto out;
		if (len < minimum_console_loglevel)
			len = minimum_console_loglevel;
		spin_lock_irq(&logbuf_lock);
		console_loglevel = len;
		spin_unlock_irq(&logbuf_lock);
		error = 0;
		break;
	case 9:		/* Number of chars in the log buffer */
		spin_lock_irq(&logbuf_lock);
		error = log_end - log_start;
		spin_unlock_irq(&logbuf_lock);
		break;
	default:
		error = -EINVAL;
		break;
	}
out:
	return error;
}

asmlinkage long sys_syslog(int type, char * buf, int len)
{
	if ((type != 3) && !capable(CAP_SYS_ADMIN))
		return -EPERM;
	return do_syslog(type, buf, len);
}

/*
 * Call the console drivers on a range of log_buf
 */
static void __call_console_drivers(unsigned long start, unsigned long end)
{
	struct console *con;

	for (con = console_drivers; con; con = con->next) {
		if ((con->flags & CON_ENABLED) && con->write)
			con->write(con, &LOG_BUF(start), end - start);
	}
}

/*
 * Write out chars from start to end - 1 inclusive
 */
static void _call_console_drivers(unsigned long start, unsigned long end, int msg_log_level)
{
	if (msg_log_level < console_loglevel && console_drivers && start != end) {
		if ((start & LOG_BUF_MASK) > (end & LOG_BUF_MASK)) {
			/* wrapped write */
			__call_console_drivers(start & LOG_BUF_MASK, LOG_BUF_LEN);
			__call_console_drivers(0, end & LOG_BUF_MASK);
		} else {
			__call_console_drivers(start, end);
		}
	}
}

/*
 * Call the console drivers, asking them to write out
 * log_buf[start] to log_buf[end - 1].
 * The console_sem must be held.
 */
static void call_console_drivers(unsigned long start, unsigned long end)
{
	unsigned long cur_index, start_print;
	static int msg_level = -1;

	if (((long)(start - end)) > 0)
		BUG();

	cur_index = start;
	start_print = start;
	while (cur_index != end) {
		if (	msg_level < 0 &&
			((end - cur_index) > 2) &&
			LOG_BUF(cur_index + 0) == '<' &&
			LOG_BUF(cur_index + 1) >= '0' &&
			LOG_BUF(cur_index + 1) <= '7' &&
			LOG_BUF(cur_index + 2) == '>')
		{
			msg_level = LOG_BUF(cur_index + 1) - '0';
			cur_index += 3;
			start_print = cur_index;
		}
		while (cur_index != end) {
			char c = LOG_BUF(cur_index);
			cur_index++;

			if (c == '\n') {
				if (msg_level < 0) {
					/*
					 * printk() has already given us loglevel tags in
					 * the buffer.  This code is here in case the
					 * log buffer has wrapped right round and scribbled
					 * on those tags
					 */
					msg_level = default_message_loglevel;
				}
				_call_console_drivers(start_print, cur_index, msg_level);
				msg_level = -1;
				start_print = cur_index;
				break;
			}
		}
	}
	_call_console_drivers(start_print, end, msg_level);
}

static void emit_log_char(char c)
{
	LOG_BUF(log_end) = c;
	log_end++;
	if (log_end - log_start > LOG_BUF_LEN)
		log_start = log_end - LOG_BUF_LEN;
	if (log_end - con_start > LOG_BUF_LEN)
		con_start = log_end - LOG_BUF_LEN;
	if (logged_chars < LOG_BUF_LEN)
		logged_chars++;
}

/*
 * This is printk.  It can be called from any context.  We want it to work.
 * 
 * We try to grab the console_sem.  If we succeed, it's easy - we log the output and
 * call the console drivers.  If we fail to get the semaphore we place the output
 * into the log buffer and return.  The current holder of the console_sem will
 * notice the new output in release_console_sem() and will send it to the
 * consoles before releasing the semaphore.
 *
 * One effect of this deferred printing is that code which calls printk() and
 * then changes console_loglevel may break. This is because console_loglevel
 * is inspected when the actual printing occurs.
 */
asmlinkage int printk(const char *fmt, ...)
{
	va_list args;
	unsigned long flags;
	int printed_len;
	char *p;
	static char printk_buf[1024];
	static int log_level_unknown = 1;

	if (oops_in_progress) {
		/* If a crash is occurring, make sure we can't deadlock */
		spin_lock_init(&logbuf_lock);
		/* And make sure that we print immediately */
		init_MUTEX(&console_sem);
	}

	/* This stops the holder of console_sem just where we want him */
	spin_lock_irqsave(&logbuf_lock, flags);

	/* Emit the output into the temporary buffer */
	va_start(args, fmt);
	printed_len = vsnprintf(printk_buf, sizeof(printk_buf), fmt, args);
	va_end(args);

	/*
	 * Copy the output into log_buf.  If the caller didn't provide
	 * appropriate log level tags, we insert them here
	 */
	for (p = printk_buf; *p; p++) {
		if (log_level_unknown) {
			if (p[0] != '<' || p[1] < '0' || p[1] > '7' || p[2] != '>') {
				emit_log_char('<');
				emit_log_char(default_message_loglevel + '0');
				emit_log_char('>');
			}
			log_level_unknown = 0;
		}
		emit_log_char(*p);
		if (*p == '\n')
			log_level_unknown = 1;
	}

	if (!arch_consoles_callable()) {
		/*
		 * On some architectures, the consoles are not usable
		 * on secondary CPUs early in the boot process.
		 */
		spin_unlock_irqrestore(&logbuf_lock, flags);
		goto out;
	}
	if (!down_trylock(&console_sem)) {
		/*
		 * We own the drivers.  We can drop the spinlock and let
		 * release_console_sem() print the text
		 */
		spin_unlock_irqrestore(&logbuf_lock, flags);
		console_may_schedule = 0;
		release_console_sem();
	} else {
		/*
		 * Someone else owns the drivers.  We drop the spinlock, which
		 * allows the semaphore holder to proceed and to call the
		 * console drivers with the output which we just produced.
		 */
		spin_unlock_irqrestore(&logbuf_lock, flags);
	}
out:
	return printed_len;
}
EXPORT_SYMBOL(printk);

/**
 * acquire_console_sem - lock the console system for exclusive use.
 *
 * Acquires a semaphore which guarantees that the caller has
 * exclusive access to the console system and the console_drivers list.
 *
 * Can sleep, returns nothing.
 */
void acquire_console_sem(void)
{
	if (in_interrupt())
		BUG();
	down(&console_sem);
	console_may_schedule = 1;
}
EXPORT_SYMBOL(acquire_console_sem);

/**
 * release_console_sem - unlock the console system
 *
 * Releases the semaphore which the caller holds on the console system
 * and the console driver list.
 *
 * While the semaphore was held, console output may have been buffered
 * by printk().  If this is the case, release_console_sem() emits
 * the output prior to releasing the semaphore.
 *
 * If there is output waiting for klogd, we wake it up.
 *
 * release_console_sem() may be called from any context.
 */
void release_console_sem(void)
{
	unsigned long flags;
	unsigned long _con_start, _log_end;
	unsigned long must_wake_klogd = 0;

	for ( ; ; ) {
		spin_lock_irqsave(&logbuf_lock, flags);
		must_wake_klogd |= log_start - log_end;
		if (con_start == log_end)
			break;			/* Nothing to print */
		_con_start = con_start;
		_log_end = log_end;
		con_start = log_end;		/* Flush */
		spin_unlock_irqrestore(&logbuf_lock, flags);
		call_console_drivers(_con_start, _log_end);
	}
	console_may_schedule = 0;
	up(&console_sem);
	spin_unlock_irqrestore(&logbuf_lock, flags);
	if (must_wake_klogd && !oops_in_progress)
		wake_up_interruptible(&log_wait);
}

/** console_conditional_schedule - yield the CPU if required
 *
 * If the console code is currently allowed to sleep, and
 * if this CPU should yield the CPU to another task, do
 * so here.
 *
 * Must be called within acquire_console_sem().
 */
void console_conditional_schedule(void)
{
	if (console_may_schedule && current->need_resched) {
		set_current_state(TASK_RUNNING);
		schedule();
	}
}

void console_print(const char *s)
{
	printk(KERN_EMERG "%s", s);
}
EXPORT_SYMBOL(console_print);

void console_unblank(void)
{
	struct console *c;

	/*
	 * Try to get the console semaphore. If someone else owns it
	 * we have to return without unblanking because console_unblank
	 * may be called in interrupt context.
	 */
	if (down_trylock(&console_sem) != 0)
		return;
	console_may_schedule = 0;
	for (c = console_drivers; c != NULL; c = c->next)
		if ((c->flags & CON_ENABLED) && c->unblank)
			c->unblank();
	release_console_sem();
}
EXPORT_SYMBOL(console_unblank);

/*
 * The console driver calls this routine during kernel initialization
 * to register the console printing procedure with printk() and to
 * print any messages that were printed by the kernel before the
 * console driver was initialized.
 */
void register_console(struct console * console)
{
	int     i;
	unsigned long flags;

	/*
	 *	See if we want to use this console driver. If we
	 *	didn't select a console we take the first one
	 *	that registers here.
	 */
	if (preferred_console < 0) {
		if (console->index < 0)
			console->index = 0;
		if (console->setup == NULL ||
		    console->setup(console, NULL) == 0) {
			console->flags |= CON_ENABLED | CON_CONSDEV;
			preferred_console = 0;
		}
	}

	/*
	 *	See if this console matches one we selected on
	 *	the command line.
	 */
	for(i = 0; i < MAX_CMDLINECONSOLES && console_cmdline[i].name[0]; i++) {
		if (strcmp(console_cmdline[i].name, console->name) != 0)
			continue;
		if (console->index >= 0 &&
		    console->index != console_cmdline[i].index)
			continue;
		if (console->index < 0)
			console->index = console_cmdline[i].index;
		if (console->setup &&
		    console->setup(console, console_cmdline[i].options) != 0)
			break;
		console->flags |= CON_ENABLED;
		console->index = console_cmdline[i].index;
		if (i == preferred_console)
			console->flags |= CON_CONSDEV;
		break;
	}

	if (!(console->flags & CON_ENABLED))
		return;

	/*
	 *	Put this console in the list - keep the
	 *	preferred driver at the head of the list.
	 */
	acquire_console_sem();
	if ((console->flags & CON_CONSDEV) || console_drivers == NULL) {
		console->next = console_drivers;
		console_drivers = console;
	} else {
		console->next = console_drivers->next;
		console_drivers->next = console;
	}
	if (console->flags & CON_PRINTBUFFER) {
		/*
		 * release_console_sem() will print out the buffered messages for us.
		 */
		spin_lock_irqsave(&logbuf_lock, flags);
		con_start = log_start;
		spin_unlock_irqrestore(&logbuf_lock, flags);
	}
	release_console_sem();
}
EXPORT_SYMBOL(register_console);

int unregister_console(struct console * console)
{
        struct console *a,*b;
	int res = 1;

	acquire_console_sem();
	if (console_drivers == console) {
		console_drivers=console->next;
		res = 0;
	} else {
		for (a=console_drivers->next, b=console_drivers ;
		     a; b=a, a=b->next) {
			if (a == console) {
				b->next = a->next;
				res = 0;
				break;
			}  
		}
	}
	
	/* If last console is removed, we re-enable picking the first
	 * one that gets registered. Without that, pmac early boot console
	 * would prevent fbcon from taking over.
	 */
	if (console_drivers == NULL)
		preferred_console = -1;
		

	release_console_sem();
	return res;
}
EXPORT_SYMBOL(unregister_console);
	
/**
 * tty_write_message - write a message to a certain tty, not just the console.
 *
 * This is used for messages that need to be redirected to a specific tty.
 * We don't put it into the syslog queue right now maybe in the future if
 * really needed.
 */
void tty_write_message(struct tty_struct *tty, char *msg)
{
	if (tty && tty->driver.write)
		tty->driver.write(tty, 0, msg, strlen(msg));
	return;
}
