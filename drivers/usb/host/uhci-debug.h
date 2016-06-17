/*
 * UHCI-specific debugging code. Invaluable when something
 * goes wrong, but don't get in my face.
 *
 * Kernel visible pointers are surrounded in []'s and bus
 * visible pointers are surrounded in ()'s
 *
 * (C) Copyright 1999 Linus Torvalds
 * (C) Copyright 1999-2001 Johannes Erdfelt
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <asm/io.h>

#include "uhci.h"

/* Handle REALLY large printk's so we don't overflow buffers */
static void inline lprintk(char *buf)
{
	char *p;

	/* Just write one line at a time */
	while (buf) {
		p = strchr(buf, '\n');
		if (p)
			*p = 0;
		printk("%s\n", buf);
		buf = p;
		if (buf)
			buf++;
	}
}

static int inline uhci_is_skeleton_td(struct uhci *uhci, struct uhci_td *td)
{
	int i;

	for (i = 0; i < UHCI_NUM_SKELTD; i++)
		if (td == uhci->skeltd[i])
			return 1;

	return 0;
}

static int inline uhci_is_skeleton_qh(struct uhci *uhci, struct uhci_qh *qh)
{
	int i;

	for (i = 0; i < UHCI_NUM_SKELQH; i++)
		if (qh == uhci->skelqh[i])
			return 1;

	return 0;
}

static int uhci_show_td(struct uhci_td *td, char *buf, int len, int space)
{
	char *out = buf;
	char *spid;

	/* Try to make sure there's enough memory */
	if (len < 160)
		return 0;

	out += sprintf(out, "%*s[%p] link (%08x) ", space, "", td, td->link);
	out += sprintf(out, "e%d %s%s%s%s%s%s%s%s%s%sLength=%x ",
		((td->status >> 27) & 3),
		(td->status & TD_CTRL_SPD) ?      "SPD " : "",
		(td->status & TD_CTRL_LS) ?       "LS " : "",
		(td->status & TD_CTRL_IOC) ?      "IOC " : "",
		(td->status & TD_CTRL_ACTIVE) ?   "Active " : "",
		(td->status & TD_CTRL_STALLED) ?  "Stalled " : "",
		(td->status & TD_CTRL_DBUFERR) ?  "DataBufErr " : "",
		(td->status & TD_CTRL_BABBLE) ?   "Babble " : "",
		(td->status & TD_CTRL_NAK) ?      "NAK " : "",
		(td->status & TD_CTRL_CRCTIMEO) ? "CRC/Timeo " : "",
		(td->status & TD_CTRL_BITSTUFF) ? "BitStuff " : "",
		td->status & 0x7ff);

	switch (td->info & 0xff) {
		case USB_PID_SETUP:
			spid = "SETUP";
			break;
		case USB_PID_OUT:
			spid = "OUT";
			break;
		case USB_PID_IN:
			spid = "IN";
			break;
		default:
			spid = "?";
			break;
	}

	out += sprintf(out, "MaxLen=%x DT%d EndPt=%x Dev=%x, PID=%x(%s) ",
		td->info >> 21,
		((td->info >> 19) & 1),
		(td->info >> 15) & 15,
		(td->info >> 8) & 127,
		(td->info & 0xff),
		spid);
	out += sprintf(out, "(buf=%08x)\n", td->buffer);

	return out - buf;
}

static int uhci_show_sc(int port, unsigned short status, char *buf, int len)
{
	char *out = buf;

	/* Try to make sure there's enough memory */
	if (len < 80)
		return 0;

	out += sprintf(out, "  stat%d     =     %04x   %s%s%s%s%s%s%s%s\n",
		port,
		status,
		(status & USBPORTSC_SUSP) ? "PortSuspend " : "",
		(status & USBPORTSC_PR) ?   "PortReset " : "",
		(status & USBPORTSC_LSDA) ? "LowSpeed " : "",
		(status & USBPORTSC_RD) ?   "ResumeDetect " : "",
		(status & USBPORTSC_PEC) ?  "EnableChange " : "",
		(status & USBPORTSC_PE) ?   "PortEnabled " : "",
		(status & USBPORTSC_CSC) ?  "ConnectChange " : "",
		(status & USBPORTSC_CCS) ?  "PortConnected " : "");

	return out - buf;
}

static int uhci_show_status(struct uhci *uhci, char *buf, int len)
{
	char *out = buf;
	unsigned int io_addr = uhci->io_addr;
	unsigned short usbcmd, usbstat, usbint, usbfrnum;
	unsigned int flbaseadd;
	unsigned char sof;
	unsigned short portsc1, portsc2;

	/* Try to make sure there's enough memory */
	if (len < 80 * 6)
		return 0;

	usbcmd    = inw(io_addr + 0);
	usbstat   = inw(io_addr + 2);
	usbint    = inw(io_addr + 4);
	usbfrnum  = inw(io_addr + 6);
	flbaseadd = inl(io_addr + 8);
	sof       = inb(io_addr + 12);
	portsc1   = inw(io_addr + 16);
	portsc2   = inw(io_addr + 18);

	out += sprintf(out, "  usbcmd    =     %04x   %s%s%s%s%s%s%s%s\n",
		usbcmd,
		(usbcmd & USBCMD_MAXP) ?    "Maxp64 " : "Maxp32 ",
		(usbcmd & USBCMD_CF) ?      "CF " : "",
		(usbcmd & USBCMD_SWDBG) ?   "SWDBG " : "",
		(usbcmd & USBCMD_FGR) ?     "FGR " : "",
		(usbcmd & USBCMD_EGSM) ?    "EGSM " : "",
		(usbcmd & USBCMD_GRESET) ?  "GRESET " : "",
		(usbcmd & USBCMD_HCRESET) ? "HCRESET " : "",
		(usbcmd & USBCMD_RS) ?      "RS " : "");

	out += sprintf(out, "  usbstat   =     %04x   %s%s%s%s%s%s\n",
		usbstat,
		(usbstat & USBSTS_HCH) ?    "HCHalted " : "",
		(usbstat & USBSTS_HCPE) ?   "HostControllerProcessError " : "",
		(usbstat & USBSTS_HSE) ?    "HostSystemError " : "",
		(usbstat & USBSTS_RD) ?     "ResumeDetect " : "",
		(usbstat & USBSTS_ERROR) ?  "USBError " : "",
		(usbstat & USBSTS_USBINT) ? "USBINT " : "");

	out += sprintf(out, "  usbint    =     %04x\n", usbint);
	out += sprintf(out, "  usbfrnum  =   (%d)%03x\n", (usbfrnum >> 10) & 1,
		0xfff & (4*(unsigned int)usbfrnum));
	out += sprintf(out, "  flbaseadd = %08x\n", flbaseadd);
	out += sprintf(out, "  sof       =       %02x\n", sof);
	out += uhci_show_sc(1, portsc1, out, len - (out - buf));
	out += uhci_show_sc(2, portsc2, out, len - (out - buf));

	return out - buf;
}

static int uhci_show_qh(struct uhci_qh *qh, char *buf, int len, int space)
{
	char *out = buf;
	struct urb_priv *urbp;
	struct list_head *head, *tmp;
	struct uhci_td *td;
	int i = 0, checked = 0, prevactive = 0;

	/* Try to make sure there's enough memory */
	if (len < 80 * 6)
		return 0;

	out += sprintf(out, "%*s[%p] link (%08x) element (%08x)\n", space, "",
			qh, qh->link, qh->element);

	if (qh->element & UHCI_PTR_QH)
		out += sprintf(out, "%*s  Element points to QH (bug?)\n", space, "");

	if (qh->element & UHCI_PTR_DEPTH)
		out += sprintf(out, "%*s  Depth traverse\n", space, "");

	if (qh->element & 8)
		out += sprintf(out, "%*s  Bit 3 set (bug?)\n", space, "");

	if (!(qh->element & ~(UHCI_PTR_QH | UHCI_PTR_DEPTH)))
		out += sprintf(out, "%*s  Element is NULL (bug?)\n", space, "");

	if (!qh->urbp) {
		out += sprintf(out, "%*s  urbp == NULL\n", space, "");
		goto out;
	}

	urbp = qh->urbp;

	head = &urbp->td_list;
	tmp = head->next;

	td = list_entry(tmp, struct uhci_td, list);

	if (td->dma_handle != (qh->element & ~UHCI_PTR_BITS))
		out += sprintf(out, "%*s Element != First TD\n", space, "");

	while (tmp != head) {
		struct uhci_td *td = list_entry(tmp, struct uhci_td, list);

		tmp = tmp->next;

		out += sprintf(out, "%*s%d: ", space + 2, "", i++);
		out += uhci_show_td(td, out, len - (out - buf), 0);

		if (i > 10 && !checked && prevactive && tmp != head &&
		    debug <= 2) {
			struct list_head *ntmp = tmp;
			struct uhci_td *ntd = td;
			int active = 1, ni = i;

			checked = 1;

			while (ntmp != head && ntmp->next != head && active) {
				ntd = list_entry(ntmp, struct uhci_td, list);

				ntmp = ntmp->next;

				active = ntd->status & TD_CTRL_ACTIVE;

				ni++;
			}

			if (active && ni > i) {
				out += sprintf(out, "%*s[skipped %d active TD's]\n", space, "", ni - i);
				tmp = ntmp;
				td = ntd;
				i = ni;
			}
		}

		prevactive = td->status & TD_CTRL_ACTIVE;
	}

	if (list_empty(&urbp->queue_list) || urbp->queued)
		goto out;

	out += sprintf(out, "%*sQueued QH's:\n", -space, "--");

	head = &urbp->queue_list;
	tmp = head->next;

	while (tmp != head) {
		struct urb_priv *nurbp = list_entry(tmp, struct urb_priv,
						queue_list);
		tmp = tmp->next;

		out += uhci_show_qh(nurbp->qh, out, len - (out - buf), space);
	}

out:
	return out - buf;
}

static const char *td_names[] = {"skel_int1_td", "skel_int2_td",
				 "skel_int4_td", "skel_int8_td",
				 "skel_int16_td", "skel_int32_td",
				 "skel_int64_td", "skel_int128_td",
				 "skel_int256_td", "skel_term_td" };
static const char *qh_names[] = { "skel_ls_control_qh", "skel_hs_control_qh",
				  "skel_bulk_qh", "skel_term_qh" };

#define show_frame_num()	\
	if (!shown) {		\
	  shown = 1;		\
	  out += sprintf(out, "- Frame %d\n", i); \
	}

#define show_td_name()		\
	if (!shown) {		\
	  shown = 1;		\
	  out += sprintf(out, "- %s\n", td_names[i]); \
	}

#define show_qh_name()		\
	if (!shown) {		\
	  shown = 1;		\
	  out += sprintf(out, "- %s\n", qh_names[i]); \
	}

static int uhci_sprint_schedule(struct uhci *uhci, char *buf, int len)
{
	char *out = buf;
	int i;
	struct uhci_qh *qh;
	struct uhci_td *td;
	struct list_head *tmp, *head;

	out += sprintf(out, "HC status\n");
	out += uhci_show_status(uhci, out, len - (out - buf));

	out += sprintf(out, "Frame List\n");
	for (i = 0; i < UHCI_NUMFRAMES; ++i) {
		int shown = 0;
		td = uhci->fl->frame_cpu[i];
		if (!td)
			continue;

		if (td->dma_handle != (dma_addr_t)uhci->fl->frame[i]) {
			show_frame_num();
			out += sprintf(out, "    frame list does not match td->dma_handle!\n");
		}
		if (uhci_is_skeleton_td(uhci, td))
			continue;
		show_frame_num();

		head = &td->fl_list;
		tmp = head;
		do {
			td = list_entry(tmp, struct uhci_td, fl_list);
			tmp = tmp->next;
			out += uhci_show_td(td, out, len - (out - buf), 4);
		} while (tmp != head);
	}

	out += sprintf(out, "Skeleton TD's\n");
	for (i = UHCI_NUM_SKELTD - 1; i >= 0; i--) {
		int shown = 0;

		td = uhci->skeltd[i];

		if (debug > 1) {
			show_td_name();
			out += uhci_show_td(td, out, len - (out - buf), 4);
		}

		if (list_empty(&td->fl_list)) {
			/* TD 0 is the int1 TD and links to control_ls_qh */
			if (!i) {
				if (td->link !=
				    (uhci->skel_ls_control_qh->dma_handle | UHCI_PTR_QH)) {
					show_td_name();
					out += sprintf(out, "    skeleton TD not linked to ls_control QH!\n");
				}
			} else if (i < 9) {
				if (td->link != uhci->skeltd[i - 1]->dma_handle) {
					show_td_name();
					out += sprintf(out, "    skeleton TD not linked to next skeleton TD!\n");
				}
			} else {
				show_td_name();

				if (td->link != td->dma_handle)
					out += sprintf(out, "    skel_term_td does not link to self\n");

				/* Don't show it twice */
				if (debug <= 1)
					out += uhci_show_td(td, out, len - (out - buf), 4);
			}

			continue;
		}

		show_td_name();

		head = &td->fl_list;
		tmp = head->next;

		while (tmp != head) {
			td = list_entry(tmp, struct uhci_td, fl_list);

			tmp = tmp->next;

			out += uhci_show_td(td, out, len - (out - buf), 4);
		}

		if (!i) {
			if (td->link !=
			    (uhci->skel_ls_control_qh->dma_handle | UHCI_PTR_QH))
				out += sprintf(out, "    last TD not linked to ls_control QH!\n");
		} else if (i < 9) {
			if (td->link != uhci->skeltd[i - 1]->dma_handle)
				out += sprintf(out, "    last TD not linked to next skeleton!\n");
		}
	}

	out += sprintf(out, "Skeleton QH's\n");

	for (i = 0; i < UHCI_NUM_SKELQH; ++i) {
		int shown = 0;

		qh = uhci->skelqh[i];

		if (debug > 1) {
			show_qh_name();
			out += uhci_show_qh(qh, out, len - (out - buf), 4);
		}

		/* QH 3 is the Terminating QH, it's different */
		if (i == 3) {
			if (qh->link != UHCI_PTR_TERM) {
				show_qh_name();
				out += sprintf(out, "    bandwidth reclamation on!\n");
			}

			if (qh->element != uhci->skel_term_td->dma_handle) {
				show_qh_name();
				out += sprintf(out, "    skel_term_qh element is not set to skel_term_td\n");
			}
		}

		if (list_empty(&qh->list)) {
			if (i < 3) {
				if (qh->link !=
				    (uhci->skelqh[i + 1]->dma_handle | UHCI_PTR_QH)) {
					show_qh_name();
					out += sprintf(out, "    skeleton QH not linked to next skeleton QH!\n");
				}
			}

			continue;
		}

		show_qh_name();

		head = &qh->list;
		tmp = head->next;

		while (tmp != head) {
			qh = list_entry(tmp, struct uhci_qh, list);

			tmp = tmp->next;

			out += uhci_show_qh(qh, out, len - (out - buf), 4);
		}

		if (i < 3) {
			if (qh->link !=
			    (uhci->skelqh[i + 1]->dma_handle | UHCI_PTR_QH))
				out += sprintf(out, "    last QH not linked to next skeleton!\n");
		}
	}

	return out - buf;
}

#ifdef CONFIG_PROC_FS
#define MAX_OUTPUT	(64 * 1024)

static struct proc_dir_entry *uhci_proc_root = NULL;

struct uhci_proc {
	int size;
	char *data;
	struct uhci *uhci;
};

static int uhci_proc_open(struct inode *inode, struct file *file)
{
	const struct proc_dir_entry *dp = inode->u.generic_ip;
	struct uhci *uhci = dp->data;
	struct uhci_proc *up;
	unsigned long flags;
	int ret = -ENOMEM;

	lock_kernel();
	up = kmalloc(sizeof(*up), GFP_KERNEL);
	if (!up)
		goto out;

	up->data = kmalloc(MAX_OUTPUT, GFP_KERNEL);
	if (!up->data) {
		kfree(up);
		goto out;
	}

	spin_lock_irqsave(&uhci->frame_list_lock, flags);
	up->size = uhci_sprint_schedule(uhci, up->data, MAX_OUTPUT);
	spin_unlock_irqrestore(&uhci->frame_list_lock, flags);

	file->private_data = up;

	ret = 0;
out:
	unlock_kernel();
	return ret;
}

static loff_t uhci_proc_lseek(struct file *file, loff_t off, int whence)
{
	struct uhci_proc *up = file->private_data;
	loff_t new;

	switch (whence) {
	case 0:
		new = off;
		break;
	case 1:
		new = file->f_pos + off;
		break;
	case 2:
	default:
		return -EINVAL;
	}
	if (new < 0 || new > up->size)
		return -EINVAL;
	return (file->f_pos = new);
}

static ssize_t uhci_proc_read(struct file *file, char *buf, size_t nbytes,
			loff_t *ppos)
{
	struct uhci_proc *up = file->private_data;
	unsigned int pos;
	unsigned int size;

	pos = *ppos;
	size = up->size;
	if (pos >= size)
		return 0;
	if (nbytes >= size)
		nbytes = size;
	if (pos + nbytes > size)
		nbytes = size - pos;

	if (!access_ok(VERIFY_WRITE, buf, nbytes))
		return -EINVAL;

	if (copy_to_user(buf, up->data + pos, nbytes))
		return -EFAULT;

	*ppos += nbytes;

	return nbytes;
}

static int uhci_proc_release(struct inode *inode, struct file *file)
{
	struct uhci_proc *up = file->private_data;

	kfree(up->data);
	kfree(up);

	return 0;
}

static struct file_operations uhci_proc_operations = {
	open:		uhci_proc_open,
	llseek:		uhci_proc_lseek,
	read:		uhci_proc_read,
//	write:		uhci_proc_write,
	release:	uhci_proc_release,
};
#endif

