/*
 * linux/drivers/char/selection.c
 *
 * This module exports the functions:
 *
 *     'int set_selection(const unsigned long arg)'
 *     'void clear_selection(void)'
 *     'int paste_selection(struct tty_struct *tty)'
 *     'int sel_loadlut(const unsigned long arg)'
 *
 * Now that /dev/vcs exists, most of this can disappear again.
 */

#include <linux/module.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/types.h>

#include <asm/uaccess.h>

#include <linux/vt_kern.h>
#include <linux/consolemap.h>
#include <linux/console_struct.h>
#include <linux/selection.h>

#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif

/* Don't take this from <ctype.h>: 011-015 on the screen aren't spaces */
#define isspace(c)	((c) == ' ')

extern void poke_blanked_console(void);

/* Variables for selection control. */
/* Use a dynamic buffer, instead of static (Dec 1994) */
       int sel_cons;		/* must not be disallocated */
static volatile int sel_start = -1; 	/* cleared by clear_selection */
static int sel_end;
static int sel_buffer_lth;
static char *sel_buffer;

/* clear_selection, highlight and highlight_pointer can be called
   from interrupt (via scrollback/front) */

/* set reverse video on characters s-e of console with selection. */
inline static void
highlight(const int s, const int e) {
	invert_screen(sel_cons, s, e-s+2, 1);
}

/* use complementary color to show the pointer */
inline static void
highlight_pointer(const int where) {
	complement_pos(sel_cons, where);
}

static unsigned char
sel_pos(int n)
{
	return inverse_translate(vc_cons[sel_cons].d, screen_glyph(sel_cons, n));
}

/* remove the current selection highlight, if any,
   from the console holding the selection. */
void
clear_selection(void) {
	highlight_pointer(-1); /* hide the pointer */
	if (sel_start != -1) {
		highlight(sel_start, sel_end);
		sel_start = -1;
	}
}

/*
 * User settable table: what characters are to be considered alphabetic?
 * 256 bits
 */
static u32 inwordLut[8]={
  0x00000000, /* control chars     */
  0x03FF0000, /* digits            */
  0x87FFFFFE, /* uppercase and '_' */
  0x07FFFFFE, /* lowercase         */
  0x00000000,
  0x00000000,
  0xFF7FFFFF, /* latin-1 accented letters, not multiplication sign */
  0xFF7FFFFF  /* latin-1 accented letters, not division sign */
};

static inline int inword(const unsigned char c) {
	return ( inwordLut[c>>5] >> (c & 0x1F) ) & 1;
}

/* set inwordLut contents. Invoked by ioctl(). */
int sel_loadlut(const unsigned long arg)
{
	return copy_from_user(inwordLut, (u32 *)(arg+4), 32) ? -EFAULT : 0;
}

/* does screen address p correspond to character at LH/RH edge of screen? */
static inline int atedge(const int p, int size_row)
{
	return (!(p % size_row)	|| !((p + 2) % size_row));
}

/* constrain v such that v <= u */
static inline unsigned short limit(const unsigned short v, const unsigned short u)
{
	return (v > u) ? u : v;
}

/* set the current selection. Invoked by ioctl() or by kernel code. */
int set_selection(const unsigned long arg, struct tty_struct *tty, int user)
{
	int sel_mode, new_sel_start, new_sel_end, spc;
	char *bp, *obp;
	int i, ps, pe;
	unsigned int currcons = fg_console;

	unblank_screen();
	poke_blanked_console();

	{ unsigned short *args, xs, ys, xe, ye;

	  args = (unsigned short *)(arg + 1);
	  if (user) {
		  if (verify_area(VERIFY_READ, args, sizeof(short) * 5))
		  	return -EFAULT;
		  __get_user(xs, args++);
		  __get_user(ys, args++);
		  __get_user(xe, args++);
		  __get_user(ye, args++);
		  __get_user(sel_mode, args);
	  } else {
		  xs = *(args++); /* set selection from kernel */
		  ys = *(args++);
		  xe = *(args++);
		  ye = *(args++);
		  sel_mode = *args;
	  }
	  xs--; ys--; xe--; ye--;
	  xs = limit(xs, video_num_columns - 1);
	  ys = limit(ys, video_num_lines - 1);
	  xe = limit(xe, video_num_columns - 1);
	  ye = limit(ye, video_num_lines - 1);
	  ps = ys * video_size_row + (xs << 1);
	  pe = ye * video_size_row + (xe << 1);

	  if (sel_mode == 4) {
	      /* useful for screendump without selection highlights */
	      clear_selection();
	      return 0;
	  }

	  if (mouse_reporting() && (sel_mode & 16)) {
	      mouse_report(tty, sel_mode & 15, xs, ys);
	      return 0;
	  }
        }

	if (ps > pe)	/* make sel_start <= sel_end */
	{
		int tmp = ps;
		ps = pe;
		pe = tmp;
	}

	if (sel_cons != fg_console) {
		clear_selection();
		sel_cons = fg_console;
	}

	switch (sel_mode)
	{
		case 0:	/* character-by-character selection */
			new_sel_start = ps;
			new_sel_end = pe;
			break;
		case 1:	/* word-by-word selection */
			spc = isspace(sel_pos(ps));
			for (new_sel_start = ps; ; ps -= 2)
			{
				if ((spc && !isspace(sel_pos(ps))) ||
				    (!spc && !inword(sel_pos(ps))))
					break;
				new_sel_start = ps;
				if (!(ps % video_size_row))
					break;
			}
			spc = isspace(sel_pos(pe));
			for (new_sel_end = pe; ; pe += 2)
			{
				if ((spc && !isspace(sel_pos(pe))) ||
				    (!spc && !inword(sel_pos(pe))))
					break;
				new_sel_end = pe;
				if (!((pe + 2) % video_size_row))
					break;
			}
			break;
		case 2:	/* line-by-line selection */
			new_sel_start = ps - ps % video_size_row;
			new_sel_end = pe + video_size_row
				    - pe % video_size_row - 2;
			break;
		case 3:
			highlight_pointer(pe);
			return 0;
		default:
			return -EINVAL;
	}

	/* remove the pointer */
	highlight_pointer(-1);

	/* select to end of line if on trailing space */
	if (new_sel_end > new_sel_start &&
		!atedge(new_sel_end, video_size_row) &&
		isspace(sel_pos(new_sel_end))) {
		for (pe = new_sel_end + 2; ; pe += 2)
			if (!isspace(sel_pos(pe)) ||
			    atedge(pe, video_size_row))
				break;
		if (isspace(sel_pos(pe)))
			new_sel_end = pe;
	}
	if (sel_start == -1)	/* no current selection */
		highlight(new_sel_start, new_sel_end);
	else if (new_sel_start == sel_start)
	{
		if (new_sel_end == sel_end)	/* no action required */
			return 0;
		else if (new_sel_end > sel_end)	/* extend to right */
			highlight(sel_end + 2, new_sel_end);
		else				/* contract from right */
			highlight(new_sel_end + 2, sel_end);
	}
	else if (new_sel_end == sel_end)
	{
		if (new_sel_start < sel_start)	/* extend to left */
			highlight(new_sel_start, sel_start - 2);
		else				/* contract from left */
			highlight(sel_start, new_sel_start - 2);
	}
	else	/* some other case; start selection from scratch */
	{
		clear_selection();
		highlight(new_sel_start, new_sel_end);
	}
	sel_start = new_sel_start;
	sel_end = new_sel_end;

	/* Allocate a new buffer before freeing the old one ... */
	bp = kmalloc((sel_end-sel_start)/2+1, GFP_KERNEL);
	if (!bp) {
		printk(KERN_WARNING "selection: kmalloc() failed\n");
		clear_selection();
		return -ENOMEM;
	}
	if (sel_buffer)
		kfree(sel_buffer);
	sel_buffer = bp;

	obp = bp;
	for (i = sel_start; i <= sel_end; i += 2) {
		*bp = sel_pos(i);
		if (!isspace(*bp++))
			obp = bp;
		if (! ((i + 2) % video_size_row)) {
			/* strip trailing blanks from line and add newline,
			   unless non-space at end of line. */
			if (obp != bp) {
				bp = obp;
				*bp++ = '\r';
			}
			obp = bp;
		}
	}
	sel_buffer_lth = bp - sel_buffer;
	return 0;
}

/* Insert the contents of the selection buffer into the
 * queue of the tty associated with the current console.
 * Invoked by ioctl().
 */
int paste_selection(struct tty_struct *tty)
{
	struct vt_struct *vt = (struct vt_struct *) tty->driver_data;
	int	pasted = 0, count;
	DECLARE_WAITQUEUE(wait, current);

	poke_blanked_console();
	add_wait_queue(&vt->paste_wait, &wait);
	while (sel_buffer && sel_buffer_lth > pasted) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (test_bit(TTY_THROTTLED, &tty->flags)) {
			schedule();
			continue;
		}
		count = sel_buffer_lth - pasted;
		count = MIN(count, tty->ldisc.receive_room(tty));
		tty->ldisc.receive_buf(tty, sel_buffer + pasted, 0, count);
		pasted += count;
	}
	remove_wait_queue(&vt->paste_wait, &wait);
	current->state = TASK_RUNNING;
	return 0;
}

EXPORT_SYMBOL(set_selection);
EXPORT_SYMBOL(paste_selection);
