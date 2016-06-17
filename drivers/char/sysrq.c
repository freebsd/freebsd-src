/* -*- linux-c -*-
 *
 *	$Id: sysrq.c,v 1.15 1998/08/23 14:56:41 mj Exp $
 *
 *	Linux Magic System Request Key Hacks
 *
 *	(c) 1997 Martin Mares <mj@atrey.karlin.mff.cuni.cz>
 *	based on ideas by Pavel Machek <pavel@atrey.karlin.mff.cuni.cz>
 *
 *	(c) 2000 Crutcher Dunnavant <crutcher+kernel@datastacks.com>
 *	overhauled to use key registration
 *	based upon discusions in irc://irc.openprojects.net/#kernelnewbies
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/tty.h>
#include <linux/mount.h>
#include <linux/kdev_t.h>
#include <linux/major.h>
#include <linux/reboot.h>
#include <linux/sysrq.h>
#include <linux/kbd_kern.h>
#include <linux/quotaops.h>
#include <linux/smp_lock.h>
#include <linux/module.h>

#include <linux/spinlock.h>

#include <asm/ptrace.h>

extern void reset_vc(unsigned int);
extern struct list_head super_blocks;

/* Whether we react on sysrq keys or just ignore them */
int sysrq_enabled = 1;

/* Machine specific power off function */
void (*sysrq_power_off)(void);

/* Loglevel sysrq handler */
static void sysrq_handle_loglevel(int key, struct pt_regs *pt_regs,
		struct kbd_struct *kbd, struct tty_struct *tty) {
	int i;
	i = key - '0';
	console_loglevel = 7;
	printk("Loglevel set to %d\n", i);
	console_loglevel = i;
}	
static struct sysrq_key_op sysrq_loglevel_op = {
	handler:	sysrq_handle_loglevel,
	help_msg:	"loglevel0-8",
	action_msg:	"Changing Loglevel",
};


/* SAK sysrq handler */
#ifdef CONFIG_VT
static void sysrq_handle_SAK(int key, struct pt_regs *pt_regs,
		struct kbd_struct *kbd, struct tty_struct *tty) {
	if (tty)
		do_SAK(tty);
	reset_vc(fg_console);
}
static struct sysrq_key_op sysrq_SAK_op = {
	handler:	sysrq_handle_SAK,
	help_msg:	"saK",
	action_msg:	"SAK",
};
#endif


/* unraw sysrq handler */
static void sysrq_handle_unraw(int key, struct pt_regs *pt_regs,
		struct kbd_struct *kbd, struct tty_struct *tty) {
	if (kbd)
		kbd->kbdmode = VC_XLATE;
}
static struct sysrq_key_op sysrq_unraw_op = {
	handler:	sysrq_handle_unraw,
	help_msg:	"unRaw",
	action_msg:	"Keyboard mode set to XLATE",
};


/* reboot sysrq handler */
static void sysrq_handle_reboot(int key, struct pt_regs *pt_regs,
		struct kbd_struct *kbd, struct tty_struct *tty) {
	machine_restart(NULL);
}
static struct sysrq_key_op sysrq_reboot_op = {
	handler:	sysrq_handle_reboot,
	help_msg:	"reBoot",
	action_msg:	"Resetting",
};



/* SYNC SYSRQ HANDLERS BLOCK */

/* do_emergency_sync helper function */
/* Guesses if the device is a local hard drive */
static int is_local_disk(kdev_t dev) {
	unsigned int major;
	major = MAJOR(dev);

	switch (major) {
	case IDE0_MAJOR:
	case IDE1_MAJOR:
	case IDE2_MAJOR:
	case IDE3_MAJOR:
	case IDE4_MAJOR:
	case IDE5_MAJOR:
	case IDE6_MAJOR:
	case IDE7_MAJOR:
	case IDE8_MAJOR:
	case IDE9_MAJOR:
	case SCSI_DISK0_MAJOR:
	case SCSI_DISK1_MAJOR:
	case SCSI_DISK2_MAJOR:
	case SCSI_DISK3_MAJOR:
	case SCSI_DISK4_MAJOR:
	case SCSI_DISK5_MAJOR:
	case SCSI_DISK6_MAJOR:
	case SCSI_DISK7_MAJOR:
	case XT_DISK_MAJOR:
		return 1;
	default:
		return 0;
	}
}

/* do_emergency_sync helper function */
static void go_sync(struct super_block *sb, int remount_flag)
{
	int orig_loglevel;
	orig_loglevel = console_loglevel;
	console_loglevel = 7;
	printk(KERN_INFO "%sing device %s ... ",
	       remount_flag ? "Remount" : "Sync",
	       kdevname(sb->s_dev));

	if (remount_flag) { /* Remount R/O */
		int ret, flags;
		struct list_head *p;

		if (sb->s_flags & MS_RDONLY) {
			printk("R/O\n");
			return;
		}

		file_list_lock();
		for (p = sb->s_files.next; p != &sb->s_files; p = p->next) {
			struct file *file = list_entry(p, struct file, f_list);
			if (file->f_dentry && file_count(file)
				&& S_ISREG(file->f_dentry->d_inode->i_mode))
				file->f_mode &= ~2;
		}
		file_list_unlock();
		DQUOT_OFF(sb);
		fsync_dev(sb->s_dev);
		flags = MS_RDONLY;
		if (sb->s_op && sb->s_op->remount_fs) {
			ret = sb->s_op->remount_fs(sb, &flags, NULL);
			if (ret)
				printk("error %d\n", ret);
			else {
				sb->s_flags = (sb->s_flags & ~MS_RMT_MASK) | (flags & MS_RMT_MASK);
				printk("OK\n");
			}
		} else
			printk("nothing to do\n");
	} else { /* Sync only */
		fsync_dev(sb->s_dev);
		printk("OK\n");
	}
	console_loglevel = orig_loglevel;
}
/*
 * Emergency Sync or Unmount. We cannot do it directly, so we set a special
 * flag and wake up the bdflush kernel thread which immediately calls this function.
 * We process all mounted hard drives first to recover from crashed experimental
 * block devices and malfunctional network filesystems.
 */

volatile int emergency_sync_scheduled;

void do_emergency_sync(void) {
	struct super_block *sb;
	int remount_flag;
	int orig_loglevel;

	lock_kernel();
	remount_flag = (emergency_sync_scheduled == EMERG_REMOUNT);
	emergency_sync_scheduled = 0;

	for (sb = sb_entry(super_blocks.next);
	     sb != sb_entry(&super_blocks); 
	     sb = sb_entry(sb->s_list.next))
		if (is_local_disk(sb->s_dev))
			go_sync(sb, remount_flag);

	for (sb = sb_entry(super_blocks.next);
	     sb != sb_entry(&super_blocks); 
	     sb = sb_entry(sb->s_list.next))
		if (!is_local_disk(sb->s_dev) && MAJOR(sb->s_dev))
			go_sync(sb, remount_flag);

	unlock_kernel();

	orig_loglevel = console_loglevel;
	console_loglevel = 7;
	printk(KERN_INFO "Done.\n");
	console_loglevel = orig_loglevel;
}

static void sysrq_handle_sync(int key, struct pt_regs *pt_regs,
		struct kbd_struct *kbd, struct tty_struct *tty) {
	emergency_sync_scheduled = EMERG_SYNC;
	wakeup_bdflush();
}
static struct sysrq_key_op sysrq_sync_op = {
	handler:	sysrq_handle_sync,
	help_msg:	"Sync",
	action_msg:	"Emergency Sync",
};

static void sysrq_handle_mountro(int key, struct pt_regs *pt_regs,
		struct kbd_struct *kbd, struct tty_struct *tty) {
	emergency_sync_scheduled = EMERG_REMOUNT;
	wakeup_bdflush();
}
static struct sysrq_key_op sysrq_mountro_op = {
	handler:	sysrq_handle_mountro,
	help_msg:	"Unmount",
	action_msg:	"Emergency Remount R/O",
};

/* END SYNC SYSRQ HANDLERS BLOCK */


/* SHOW SYSRQ HANDLERS BLOCK */

static void sysrq_handle_showregs(int key, struct pt_regs *pt_regs,
		struct kbd_struct *kbd, struct tty_struct *tty) {
	if (pt_regs)
		show_regs(pt_regs);
}
static struct sysrq_key_op sysrq_showregs_op = {
	handler:	sysrq_handle_showregs,
	help_msg:	"showPc",
	action_msg:	"Show Regs",
};


static void sysrq_handle_showstate(int key, struct pt_regs *pt_regs,
		struct kbd_struct *kbd, struct tty_struct *tty) {
	show_state();
}
static struct sysrq_key_op sysrq_showstate_op = {
	handler:	sysrq_handle_showstate,
	help_msg:	"showTasks",
	action_msg:	"Show State",
};


static void sysrq_handle_showmem(int key, struct pt_regs *pt_regs,
		struct kbd_struct *kbd, struct tty_struct *tty) {
	show_mem();
}
static struct sysrq_key_op sysrq_showmem_op = {
	handler:	sysrq_handle_showmem,
	help_msg:	"showMem",
	action_msg:	"Show Memory",
};

/* SHOW SYSRQ HANDLERS BLOCK */


/* SIGNAL SYSRQ HANDLERS BLOCK */

/* signal sysrq helper function
 * Sends a signal to all user processes */
static void send_sig_all(int sig)
{
	struct task_struct *p;

	for_each_task(p) {
		if (p->mm && p->pid != 1)
			/* Not swapper, init nor kernel thread */
			force_sig(sig, p);
	}
}

static void sysrq_handle_term(int key, struct pt_regs *pt_regs,
		struct kbd_struct *kbd, struct tty_struct *tty) {
	send_sig_all(SIGTERM);
	console_loglevel = 8;
}
static struct sysrq_key_op sysrq_term_op = {
	handler:	sysrq_handle_term,
	help_msg:	"tErm",
	action_msg:	"Terminate All Tasks",
};

static void sysrq_handle_kill(int key, struct pt_regs *pt_regs,
		struct kbd_struct *kbd, struct tty_struct *tty) {
	send_sig_all(SIGKILL);
	console_loglevel = 8;
}
static struct sysrq_key_op sysrq_kill_op = {
	handler:	sysrq_handle_kill,
	help_msg:	"kIll",
	action_msg:	"Kill All Tasks",
};

/* END SIGNAL SYSRQ HANDLERS BLOCK */


/* Key Operations table and lock */
static spinlock_t sysrq_key_table_lock = SPIN_LOCK_UNLOCKED;
#define SYSRQ_KEY_TABLE_LENGTH 36
static struct sysrq_key_op *sysrq_key_table[SYSRQ_KEY_TABLE_LENGTH] = {
/* 0 */	&sysrq_loglevel_op,
/* 1 */	&sysrq_loglevel_op,
/* 2 */	&sysrq_loglevel_op,
/* 3 */	&sysrq_loglevel_op,
/* 4 */	&sysrq_loglevel_op,
/* 5 */	&sysrq_loglevel_op,
/* 6 */	&sysrq_loglevel_op,
/* 7 */	&sysrq_loglevel_op,
/* 8 */	&sysrq_loglevel_op,
/* 9 */	&sysrq_loglevel_op,
/* a */	NULL, /* Don't use for system provided sysrqs,
		 it is handled specially on the spark
		 and will never arive */
/* b */	&sysrq_reboot_op,
/* c */	NULL,
/* d */	NULL,
/* e */	&sysrq_term_op,
/* f */	NULL,
/* g */	NULL,
/* h */	NULL,
/* i */	&sysrq_kill_op,
/* j */	NULL,
#ifdef CONFIG_VT
/* k */	&sysrq_SAK_op,
#else
/* k */	NULL,
#endif
/* l */	NULL,
/* m */	&sysrq_showmem_op,
/* n */	NULL,
/* o */	NULL, /* This will often be registered
		 as 'Off' at init time */
/* p */	&sysrq_showregs_op,
/* q */	NULL,
/* r */	&sysrq_unraw_op,
/* s */	&sysrq_sync_op,
/* t */	&sysrq_showstate_op,
/* u */	&sysrq_mountro_op,
/* v */	NULL,
/* w */	NULL,
/* x */	NULL,
/* w */	NULL,
/* z */	NULL
};

/* key2index calculation, -1 on invalid index */
static __inline__ int sysrq_key_table_key2index(int key) {
	int retval;
	if ((key >= '0') && (key <= '9')) {
		retval = key - '0';
	} else if ((key >= 'a') && (key <= 'z')) {
		retval = key + 10 - 'a';
	} else {
		retval = -1;
	}
	return retval;
}

/*
 * table lock and unlocking functions, exposed to modules
 */

void __sysrq_lock_table (void) { spin_lock(&sysrq_key_table_lock); }

void __sysrq_unlock_table (void) { spin_unlock(&sysrq_key_table_lock); }

/*
 * get and put functions for the table, exposed to modules.
 */

struct sysrq_key_op *__sysrq_get_key_op (int key) {
        struct sysrq_key_op *op_p;
        int i;
	
	i = sysrq_key_table_key2index(key);
        op_p = (i == -1) ? NULL : sysrq_key_table[i];
        return op_p;
}

void __sysrq_put_key_op (int key, struct sysrq_key_op *op_p) {
        int i;

	i = sysrq_key_table_key2index(key);
        if (i != -1)
                sysrq_key_table[i] = op_p;
}

/*
 * This function is called by the keyboard handler when SysRq is pressed
 * and any other keycode arrives.
 */

void handle_sysrq(int key, struct pt_regs *pt_regs,
		  struct kbd_struct *kbd, struct tty_struct *tty) {
	if (!sysrq_enabled)
		return;

	__sysrq_lock_table();
	__handle_sysrq_nolock(key, pt_regs, kbd, tty);
	__sysrq_unlock_table();
}

/*
 * This is the non-locking version of handle_sysrq
 * It must/can only be called by sysrq key handlers,
 * as they are inside of the lock
 */

void __handle_sysrq_nolock(int key, struct pt_regs *pt_regs,
		  struct kbd_struct *kbd, struct tty_struct *tty) {
	struct sysrq_key_op *op_p;
	int orig_log_level;
	int i, j;
	
	if (!sysrq_enabled)
		return;

	orig_log_level = console_loglevel;
	console_loglevel = 7;
	printk(KERN_INFO "SysRq : ");

        op_p = __sysrq_get_key_op(key);
        if (op_p) {
		printk ("%s\n", op_p->action_msg);
		console_loglevel = orig_log_level;
		op_p->handler(key, pt_regs, kbd, tty);
	} else {
		printk("HELP : ");
		/* Only print the help msg once per handler */
		for (i=0; i<SYSRQ_KEY_TABLE_LENGTH; i++) 
		if (sysrq_key_table[i]) {
			for (j=0; sysrq_key_table[i] != sysrq_key_table[j]; j++);
			if (j == i)
				printk ("%s ", sysrq_key_table[i]->help_msg);
		}
		printk ("\n");
		console_loglevel = orig_log_level;
	}
}

EXPORT_SYMBOL(handle_sysrq);
EXPORT_SYMBOL(__handle_sysrq_nolock);
EXPORT_SYMBOL(__sysrq_lock_table);
EXPORT_SYMBOL(__sysrq_unlock_table);
EXPORT_SYMBOL(__sysrq_get_key_op);
EXPORT_SYMBOL(__sysrq_put_key_op);
