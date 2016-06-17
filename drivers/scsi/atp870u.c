/* $Id: atp870u.c,v 1.0 1997/05/07 15:22:00 root Exp root $
 *  linux/kernel/atp870u.c
 *
 *  Copyright (C) 1997	Wu Ching Chen
 *  2.1.x update (C) 1998  Krzysztof G. Baranowski
 *
 * Marcelo Tosatti <marcelo@conectiva.com.br> : SMP fixes
 *
 * Wu Ching Chen : NULL pointer fixes  2000/06/02
 *		   support atp876 chip
 *		   enable 32 bit fifo transfer
 *		   support cdrom & remove device run ultra speed
 *		   fix disconnect bug  2000/12/21
 *		   support atp880 chip lvd u160 2001/05/15
 *		   fix prd table bug 2001/09/12 (7.1)
 */

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <asm/system.h>
#include <asm/io.h>
#include <linux/pci.h>
#include <linux/blk.h>
#include "scsi.h"
#include "hosts.h"


#include "atp870u.h"

#include<linux/stat.h>

void mydlyu(unsigned int);

/*
 *   static const char RCSid[] = "$Header: /usr/src/linux/kernel/blk_drv/scsi/RCS/atp870u.c,v 1.0 1997/05/07 15:22:00 root Exp root $";
 */

static unsigned char admaxu = 1;
static unsigned short int sync_idu;

static unsigned int irqnumu[2] = {0, 0};

struct atp_unit
{
	unsigned long ioport;
	unsigned long irq;
	unsigned long pciport;
	unsigned char last_cmd;
	unsigned char in_snd;
	unsigned char in_int;
	unsigned char quhdu;
	unsigned char quendu;
	unsigned char scam_on;
	unsigned char global_map;
	unsigned char chip_veru;
	unsigned char host_idu;
	int working;
	unsigned short wide_idu;
	unsigned short active_idu;
	unsigned short ultra_map;
	unsigned short async;
	unsigned short deviceid;
	unsigned char ata_cdbu[16];
	unsigned char sp[16];
	Scsi_Cmnd *querequ[qcnt];
	struct atp_id
	{
		unsigned char dirctu;
		unsigned char devspu;
		unsigned char devtypeu;
		unsigned long prdaddru;
		unsigned long tran_lenu;
		unsigned long last_lenu;
		unsigned char *prd_posu;
		unsigned char *prd_tableu;
		Scsi_Cmnd *curr_req;
	} id[16];
};

static struct Scsi_Host *atp_host[2] = {NULL, NULL};
static struct atp_unit atp_unit[2];

static void atp870u_intr_handle(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long flags;
	unsigned short int tmpcip, id;
	unsigned char i, j, h, target_id, lun;
	unsigned char *prd;
	Scsi_Cmnd *workrequ;
	unsigned int workportu, tmport;
	unsigned long adrcntu, k;
	int errstus;
	struct atp_unit *dev = dev_id;

	for (h = 0; h < 2; h++) {
		if (irq == irqnumu[h]) {
			goto irq_numok;
		}
	}
	return;
irq_numok:
	dev->in_int = 1;
	workportu = dev->ioport;
	tmport = workportu;

	if (dev->working != 0)
	{
		tmport += 0x1f;
		j = inb(tmport);
		if ((j & 0x80) == 0)
		{
			dev->in_int = 0;
			return;
		}

		tmpcip = dev->pciport;
		if ((inb(tmpcip) & 0x08) != 0)
		{
			tmpcip += 0x2;
			for (k=0; k < 1000; k++)
			{
				if ((inb(tmpcip) & 0x08) == 0)
				{
					goto stop_dma;
				}
				if ((inb(tmpcip) & 0x01) == 0)
				{
					goto stop_dma;
				}
			}
		}
stop_dma:
		tmpcip = dev->pciport;
		outb(0x00, tmpcip);
		tmport -= 0x08;

		i = inb(tmport);

		tmport -= 0x02;
		target_id = inb(tmport);
		tmport += 0x02;

		/*
		 *	Remap wide devices onto id numbers
		 */

		if ((target_id & 0x40) != 0) {
			target_id = (target_id & 0x07) | 0x08;
		} else {
			target_id &= 0x07;
		}

		if ((j & 0x40) != 0)
		{
		     if (dev->last_cmd == 0xff)
		     {
			dev->last_cmd = target_id;
		     }
		     dev->last_cmd |= 0x40;
		}

		if (i == 0x85)
		{
			if ((dev->last_cmd & 0xf0) != 0x40)
			{
			   dev->last_cmd = 0xff;
			}
			/*
			 *	Flip wide
			 */
			if (dev->wide_idu != 0)
			{
				tmport = workportu + 0x1b;
				outb(0x01,tmport);
				while ((inb(tmport) & 0x01) != 0x01)
				{
				   outb(0x01,tmport);
				}
			}
			/*
			 *	Issue more commands
			 */
			if (((dev->quhdu != dev->quendu) || (dev->last_cmd != 0xff)) &&
			    (dev->in_snd == 0))
			{
				send_s870(h);
			}
			/*
			 *	Done
			 */
			dev->in_int = 0;
			return;
		}

		if (i == 0x40)
		{
		     dev->last_cmd |= 0x40;
		     dev->in_int = 0;
		     return;
		}

		if (i == 0x21)
		{
			if ((dev->last_cmd & 0xf0) != 0x40)
			{
			   dev->last_cmd = 0xff;
			}
			tmport -= 0x05;
			adrcntu = 0;
			((unsigned char *) &adrcntu)[2] = inb(tmport++);
			((unsigned char *) &adrcntu)[1] = inb(tmport++);
			((unsigned char *) &adrcntu)[0] = inb(tmport);
			k = dev->id[target_id].last_lenu;
			k -= adrcntu;
			dev->id[target_id].tran_lenu = k;
			dev->id[target_id].last_lenu = adrcntu;
			tmport -= 0x04;
			outb(0x41, tmport);
			tmport += 0x08;
			outb(0x08, tmport);
			dev->in_int = 0;
			return;
		}
		if ((i == 0x80) || (i == 0x8f))
		{
			lun = 0;
			tmport -= 0x07;
			j = inb(tmport);
			if (j == 0x44 || i==0x80) {
				tmport += 0x0d;
				lun = inb(tmport) & 0x07;
			} else {
				if ((dev->last_cmd & 0xf0) != 0x40)
				{
				   dev->last_cmd = 0xff;
				}
				if (j == 0x41)
				{
					tmport += 0x02;
					adrcntu = 0;
					((unsigned char *) &adrcntu)[2] = inb(tmport++);
					((unsigned char *) &adrcntu)[1] = inb(tmport++);
					((unsigned char *) &adrcntu)[0] = inb(tmport);
					k = dev->id[target_id].last_lenu;
					k -= adrcntu;
					dev->id[target_id].tran_lenu = k;
					dev->id[target_id].last_lenu = adrcntu;
					tmport += 0x04;
					outb(0x08, tmport);
					dev->in_int = 0;
					return;
				}
				else
				{
					outb(0x46, tmport);
					dev->id[target_id].dirctu = 0x00;
					tmport += 0x02;
					outb(0x00, tmport++);
					outb(0x00, tmport++);
					outb(0x00, tmport++);
					tmport += 0x03;
					outb(0x08, tmport);
					dev->in_int = 0;
					return;
				}
			}
			if (dev->last_cmd != 0xff)
			{
			   dev->last_cmd |= 0x40;
			}
			tmport = workportu + 0x10;
			outb(0x45, tmport);
			tmport += 0x06;
			target_id = inb(tmport);
			/*
			 *	Remap wide identifiers
			 */
			if ((target_id & 0x10) != 0)
			{
				target_id = (target_id & 0x07) | 0x08;
			} else {
				target_id &= 0x07;
			}
			workrequ = dev->id[target_id].curr_req;
			tmport = workportu + 0x0f;
			outb(lun, tmport);
			tmport += 0x02;
			outb(dev->id[target_id].devspu, tmport++);
			adrcntu = dev->id[target_id].tran_lenu;
			k = dev->id[target_id].last_lenu;
			outb(((unsigned char *) &k)[2], tmport++);
			outb(((unsigned char *) &k)[1], tmport++);
			outb(((unsigned char *) &k)[0], tmport++);
			/* Remap wide */
			j = target_id;
			if (target_id > 7) {
				j = (j & 0x07) | 0x40;
			}
			/* Add direction */
			j |= dev->id[target_id].dirctu;
			outb(j, tmport++);
			outb(0x80, tmport);

			/* enable 32 bit fifo transfer */
			if (dev->deviceid != 0x8081)
			{
			   tmport = workportu + 0x3a;
			   if ((dev->ata_cdbu[0] == 0x08) || (dev->ata_cdbu[0] == 0x28) ||
			       (dev->ata_cdbu[0] == 0x0a) || (dev->ata_cdbu[0] == 0x2a))
			   {
			      outb((unsigned char)((inb(tmport) & 0xf3) | 0x08),tmport);
			   }
			   else
			   {
			      outb((unsigned char)(inb(tmport) & 0xf3),tmport);
			   }
			}
			else
			{
			   tmport = workportu - 0x05;
			   if ((dev->ata_cdbu[0] == 0x08) || (dev->ata_cdbu[0] == 0x28) ||
			       (dev->ata_cdbu[0] == 0x0a) || (dev->ata_cdbu[0] == 0x2a))
			   {
			      outb((unsigned char)((inb(tmport) & 0x3f) | 0xc0),tmport);
			   }
			   else
			   {
			      outb((unsigned char)(inb(tmport) & 0x3f),tmport);
			   }
			}

			tmport = workportu + 0x1b;
			j = 0;
			id = 1;
			id = id << target_id;
			/*
			 *	Is this a wide device
			 */
			if ((id & dev->wide_idu) != 0) {
				j |= 0x01;
			}
			outb(j, tmport);
			while ((inb(tmport) & 0x01) != j)
			{
			   outb(j,tmport);
			}

			if (dev->id[target_id].last_lenu == 0) {
				tmport = workportu + 0x18;
				outb(0x08, tmport);
				dev->in_int = 0;
				return;
			}
			prd = dev->id[target_id].prd_posu;
			while (adrcntu != 0)
			{
				id = ((unsigned short int *) (prd))[2];
				if (id == 0) {
					k = 0x10000;
				} else {
					k = id;
				}
				if (k > adrcntu) {
					((unsigned short int *) (prd))[2] = (unsigned short int)
					    (k - adrcntu);
					((unsigned long *) (prd))[0] += adrcntu;
					adrcntu = 0;
					dev->id[target_id].prd_posu = prd;
				} else {
					adrcntu -= k;
					dev->id[target_id].prdaddru += 0x08;
					prd += 0x08;
					if (adrcntu == 0) {
						dev->id[target_id].prd_posu = prd;
					}
				}
			}
			tmpcip = dev->pciport + 0x04;
			outl(dev->id[target_id].prdaddru, tmpcip);
			tmpcip -= 0x02;
			outb(0x06, tmpcip);
			outb(0x00, tmpcip);
			tmpcip -= 0x02;
			tmport = workportu + 0x18;
			/*
			 *	Check transfer direction
			 */
			if (dev->id[target_id].dirctu != 0) {
				outb(0x08, tmport);
				outb(0x01, tmpcip);
				dev->in_int = 0;
				return;
			}
			outb(0x08, tmport);
			outb(0x09, tmpcip);
			dev->in_int = 0;
			return;
		}

		/*
		 *	Current scsi request on this target
		 */

		workrequ = dev->id[target_id].curr_req;

		if (i == 0x42) {
			if ((dev->last_cmd & 0xf0) != 0x40)
			{
			   dev->last_cmd = 0xff;
			}
			errstus = 0x02;
			workrequ->result = errstus;
			goto go_42;
		}
		if (i == 0x16)
		{
			if ((dev->last_cmd & 0xf0) != 0x40)
			{
			   dev->last_cmd = 0xff;
			}
			errstus = 0;
			tmport -= 0x08;
			errstus = inb(tmport);
			workrequ->result = errstus;
go_42:
			/*
			 *	Complete the command
			 */
			spin_lock_irqsave(&io_request_lock, flags);
			(*workrequ->scsi_done) (workrequ);

			/*
			 *	Clear it off the queue
			 */
			dev->id[target_id].curr_req = 0;
			dev->working--;
			spin_unlock_irqrestore(&io_request_lock, flags);
			/*
			 *	Take it back wide
			 */
			if (dev->wide_idu != 0) {
				tmport = workportu + 0x1b;
				outb(0x01,tmport);
				while ((inb(tmport) & 0x01) != 0x01)
				{
				   outb(0x01,tmport);
				}
			}
			/*
			 *	If there is stuff to send and nothing going then send it
			 */
			if (((dev->last_cmd != 0xff) || (dev->quhdu != dev->quendu)) &&
			    (dev->in_snd == 0))
			{
			   send_s870(h);
			}
			dev->in_int = 0;
			return;
		}
		if ((dev->last_cmd & 0xf0) != 0x40)
		{
		   dev->last_cmd = 0xff;
		}
		if (i == 0x4f) {
			i = 0x89;
		}
		i &= 0x0f;
		if (i == 0x09) {
			tmpcip = tmpcip + 4;
			outl(dev->id[target_id].prdaddru, tmpcip);
			tmpcip = tmpcip - 2;
			outb(0x06, tmpcip);
			outb(0x00, tmpcip);
			tmpcip = tmpcip - 2;
			tmport = workportu + 0x10;
			outb(0x41, tmport);
			dev->id[target_id].dirctu = 0x00;
			tmport += 0x08;
			outb(0x08, tmport);
			outb(0x09, tmpcip);
			dev->in_int = 0;
			return;
		}
		if (i == 0x08) {
			tmpcip = tmpcip + 4;
			outl(dev->id[target_id].prdaddru, tmpcip);
			tmpcip = tmpcip - 2;
			outb(0x06, tmpcip);
			outb(0x00, tmpcip);
			tmpcip = tmpcip - 2;
			tmport = workportu + 0x10;
			outb(0x41, tmport);
			tmport += 0x05;
			outb((unsigned char) (inb(tmport) | 0x20), tmport);
			dev->id[target_id].dirctu = 0x20;
			tmport += 0x03;
			outb(0x08, tmport);
			outb(0x01, tmpcip);
			dev->in_int = 0;
			return;
		}
		tmport -= 0x07;
		if (i == 0x0a) {
			outb(0x30, tmport);
		} else {
			outb(0x46, tmport);
		}
		dev->id[target_id].dirctu = 0x00;
		tmport += 0x02;
		outb(0x00, tmport++);
		outb(0x00, tmport++);
		outb(0x00, tmport++);
		tmport += 0x03;
		outb(0x08, tmport);
		dev->in_int = 0;
		return;
	} else {
//		tmport = workportu + 0x17;
//		inb(tmport);
//		dev->working = 0;
		dev->in_int = 0;
		return;
	}
}

int atp870u_queuecommand(Scsi_Cmnd * req_p, void (*done) (Scsi_Cmnd *))
{
	unsigned char h;
	unsigned long flags;
	unsigned short int m;
	unsigned int tmport;
	struct atp_unit *dev;

	for (h = 0; h <= admaxu; h++) {
		if (req_p->host == atp_host[h]) {
			goto host_ok;
		}
	}
	return 0;
host_ok:
	if (req_p->channel != 0) {
		req_p->result = 0x00040000;
		done(req_p);
		return 0;
	}
	dev = &atp_unit[h];
	m = 1;
	m = m << req_p->target;

	/*
	 *	Fake a timeout for missing targets
	 */

	if ((m & dev->active_idu) == 0) {
		req_p->result = 0x00040000;
		done(req_p);
		return 0;
	}
	if (done) {
		req_p->scsi_done = done;
	} else {
		printk(KERN_WARNING "atp870u_queuecommand: done can't be NULL\n");
		req_p->result = 0;
		done(req_p);
		return 0;
	}
	/*
	 *	Count new command
	 */
	save_flags(flags);
	cli();
	dev->quendu++;
	if (dev->quendu >= qcnt) {
		dev->quendu = 0;
	}
	/*
	 *	Check queue state
	 */
	if (dev->quhdu == dev->quendu) {
		if (dev->quendu == 0) {
			dev->quendu = qcnt;
		}
		dev->quendu--;
		req_p->result = 0x00020000;
		done(req_p);
		restore_flags(flags);
		return 0;
	}
	dev->querequ[dev->quendu] = req_p;
	tmport = dev->ioport + 0x1c;
	restore_flags(flags);
	if ((inb(tmport) == 0) && (dev->in_int == 0) && (dev->in_snd == 0)) {
		send_s870(h);
	}
	return 0;
}

void mydlyu(unsigned int dlycnt)
{
	unsigned int i;
	for (i = 0; i < dlycnt; i++) {
		inb(0x80);
	}
}

void send_s870(unsigned char h)
{
	unsigned int tmport;
	Scsi_Cmnd *workrequ;
	unsigned long flags;
	unsigned int i;
	unsigned char j, target_id;
	unsigned char *prd;
	unsigned short int tmpcip, w;
	unsigned long l, bttl;
	unsigned int workportu;
	struct scatterlist *sgpnt;
	struct atp_unit *dev = &atp_unit[h];

	save_flags(flags);
	cli();
	if (dev->in_snd != 0) {
		restore_flags(flags);
		return;
	}
	dev->in_snd = 1;
	if ((dev->last_cmd != 0xff) && ((dev->last_cmd & 0x40) != 0)) {
		dev->last_cmd &= 0x0f;
		workrequ = dev->id[dev->last_cmd].curr_req;
		if (workrequ != NULL)	     /* check NULL pointer */
		{
		   goto cmd_subp;
		}
		dev->last_cmd = 0xff;
		if (dev->quhdu == dev->quendu)
		{
		   dev->in_snd = 0;
		   restore_flags(flags);
		   return ;
		}
	}
	if ((dev->last_cmd != 0xff) && (dev->working != 0))
	{
	     dev->in_snd = 0;
	     restore_flags(flags);
	     return ;
	}
	dev->working++;
	j = dev->quhdu;
	dev->quhdu++;
	if (dev->quhdu >= qcnt) {
		dev->quhdu = 0;
	}
	workrequ = dev->querequ[dev->quhdu];
	if (dev->id[workrequ->target].curr_req == 0) {
		dev->id[workrequ->target].curr_req = workrequ;
		dev->last_cmd = workrequ->target;
		goto cmd_subp;
	}
	dev->quhdu = j;
	dev->working--;
	dev->in_snd = 0;
	restore_flags(flags);
	return;
cmd_subp:
	workportu = dev->ioport;
	tmport = workportu + 0x1f;
	if ((inb(tmport) & 0xb0) != 0) {
		goto abortsnd;
	}
	tmport = workportu + 0x1c;
	if (inb(tmport) == 0) {
		goto oktosend;
	}
abortsnd:
	dev->last_cmd |= 0x40;
	dev->in_snd = 0;
	restore_flags(flags);
	return;
oktosend:
	memcpy(&dev->ata_cdbu[0], &workrequ->cmnd[0], workrequ->cmd_len);
	if (dev->ata_cdbu[0] == READ_CAPACITY) {
		if (workrequ->request_bufflen > 8) {
			workrequ->request_bufflen = 0x08;
		}
	}
	if (dev->ata_cdbu[0] == 0x00) {
		workrequ->request_bufflen = 0;
	}

	tmport = workportu + 0x1b;
	j = 0;
	target_id = workrequ->target;

	/*
	 *	Wide ?
	 */
	w = 1;
	w = w << target_id;
	if ((w & dev->wide_idu) != 0) {
		j |= 0x01;
	}
	outb(j, tmport);
	while ((inb(tmport) & 0x01) != j)
	{
	   outb(j,tmport);
	}

	/*
	 *	Write the command
	 */

	tmport = workportu;
	outb(workrequ->cmd_len, tmport++);
	outb(0x2c, tmport++);
	outb(0xcf, tmport++);
	for (i = 0; i < workrequ->cmd_len; i++) {
		outb(dev->ata_cdbu[i], tmport++);
	}
	tmport = workportu + 0x0f;
	outb(workrequ->lun, tmport);
	tmport += 0x02;
	/*
	 *	Write the target
	 */
	outb(dev->id[target_id].devspu, tmport++);

	/*
	 *	Figure out the transfer size
	 */
	if (workrequ->use_sg)
	{
		l = 0;
		sgpnt = (struct scatterlist *) workrequ->request_buffer;
		for (i = 0; i < workrequ->use_sg; i++)
		{
			if (sgpnt[i].length == 0 || workrequ->use_sg > ATP870U_SCATTER)
			{
				panic("Foooooooood fight!");
			}
			l += sgpnt[i].length;
		}
	} else {
		l = workrequ->request_bufflen;
	}
	/*
	 *	Write transfer size
	 */
	outb((unsigned char) (((unsigned char *) (&l))[2]), tmport++);
	outb((unsigned char) (((unsigned char *) (&l))[1]), tmport++);
	outb((unsigned char) (((unsigned char *) (&l))[0]), tmport++);
	j = target_id;
	dev->id[j].last_lenu = l;
	dev->id[j].tran_lenu = 0;
	/*
	 *	Flip the wide bits
	 */
	if ((j & 0x08) != 0) {
		j = (j & 0x07) | 0x40;
	}
	/*
	 *	Check transfer direction
	 */
	if ((dev->ata_cdbu[0] == WRITE_6) || (dev->ata_cdbu[0] == WRITE_10) ||
	    (dev->ata_cdbu[0] == WRITE_12) || (dev->ata_cdbu[0] == MODE_SELECT)) {
		outb((unsigned char) (j | 0x20), tmport++);
	} else {
		outb(j, tmport++);
	}
	outb((unsigned char)(inb(tmport) | 0x80),tmport);
	outb(0x80, tmport);
	tmport = workportu + 0x1c;
	dev->id[target_id].dirctu = 0;
	if (l == 0) {
		if (inb(tmport) == 0) {
			tmport = workportu + 0x18;
			outb(0x08, tmport);
		} else {
			dev->last_cmd |= 0x40;
		}
		dev->in_snd = 0;
		restore_flags(flags);
		return;
	}
	tmpcip = dev->pciport;
	prd = dev->id[target_id].prd_tableu;
	dev->id[target_id].prd_posu = prd;

	/*
	 *	Now write the request list. Either as scatter/gather or as
	 *	a linear chain.
	 */

	if (workrequ->use_sg)
	{
		sgpnt = (struct scatterlist *) workrequ->request_buffer;
		i = 0;
		for (j = 0; j < workrequ->use_sg; j++) {
			bttl = virt_to_bus(sgpnt[j].address);
			l = sgpnt[j].length;
			while (l > 0x10000) {
				(unsigned short int) (((unsigned short int *) (prd))[i + 3]) = 0x0000;
				(unsigned short int) (((unsigned short int *) (prd))[i + 2]) = 0x0000;
				(unsigned long) (((unsigned long *) (prd))[i >> 1]) = bttl;
				l -= 0x10000;
				bttl += 0x10000;
				i += 0x04;
			}
			(unsigned long) (((unsigned long *) (prd))[i >> 1]) = bttl;
			(unsigned short int) (((unsigned short int *) (prd))[i + 2]) = l;
			(unsigned short int) (((unsigned short int *) (prd))[i + 3]) = 0;
			i += 0x04;
		}
		(unsigned short int) (((unsigned short int *) (prd))[i - 1]) = 0x8000;
	} else {
		/*
		 *	For a linear request write a chain of blocks
		 */
		bttl = virt_to_bus(workrequ->request_buffer);
		l = workrequ->request_bufflen;
		i = 0;
		while (l > 0x10000) {
			(unsigned short int) (((unsigned short int *) (prd))[i + 3]) = 0x0000;
			(unsigned short int) (((unsigned short int *) (prd))[i + 2]) = 0x0000;
			(unsigned long) (((unsigned long *) (prd))[i >> 1]) = bttl;
			l -= 0x10000;
			bttl += 0x10000;
			i += 0x04;
		}
		(unsigned short int) (((unsigned short int *) (prd))[i + 3]) = 0x8000;
		(unsigned short int) (((unsigned short int *) (prd))[i + 2]) = l;
		(unsigned long) (((unsigned long *) (prd))[i >> 1]) = bttl;
	}
	tmpcip = tmpcip + 4;
	dev->id[target_id].prdaddru = virt_to_bus(dev->id[target_id].prd_tableu);
	outl(dev->id[target_id].prdaddru, tmpcip);
	tmpcip = tmpcip - 2;
	outb(0x06, tmpcip);
	outb(0x00, tmpcip);
	tmpcip = tmpcip - 2;

	if (dev->deviceid != 0x8081)
	{
	   tmport = workportu + 0x3a;
	   if ((dev->ata_cdbu[0] == 0x08) || (dev->ata_cdbu[0] == 0x28) ||
	       (dev->ata_cdbu[0] == 0x0a) || (dev->ata_cdbu[0] == 0x2a))
	   {
	      outb((unsigned char)((inb(tmport) & 0xf3) | 0x08),tmport);
	   }
	   else
	   {
	      outb((unsigned char)(inb(tmport) & 0xf3),tmport);
	   }
	}
	else
	{
	   tmport = workportu - 0x05;
	   if ((dev->ata_cdbu[0] == 0x08) || (dev->ata_cdbu[0] == 0x28) ||
	       (dev->ata_cdbu[0] == 0x0a) || (dev->ata_cdbu[0] == 0x2a))
	   {
	      outb((unsigned char)((inb(tmport) & 0x3f) | 0xc0),tmport);
	   }
	   else
	   {
	      outb((unsigned char)(inb(tmport) & 0x3f),tmport);
	   }
	}
	tmport = workportu + 0x1c;

	if ((dev->ata_cdbu[0] == WRITE_6) || (dev->ata_cdbu[0] == WRITE_10) ||
	    (dev->ata_cdbu[0] == WRITE_12) || (dev->ata_cdbu[0] == MODE_SELECT))
	{
		dev->id[target_id].dirctu = 0x20;
		if (inb(tmport) == 0) {
			tmport = workportu + 0x18;
			outb(0x08, tmport);
			outb(0x01, tmpcip);
		} else {
			dev->last_cmd |= 0x40;
		}
		dev->in_snd = 0;
		restore_flags(flags);
		return;
	}
	if (inb(tmport) == 0)
	{
		tmport = workportu + 0x18;
		outb(0x08, tmport);
		outb(0x09, tmpcip);
	} else {
		dev->last_cmd |= 0x40;
	}
	dev->in_snd = 0;
	restore_flags(flags);
	return;

}

static void internal_done(Scsi_Cmnd * SCpnt)
{
	SCpnt->SCp.Status++;
}

int atp870u_command(Scsi_Cmnd * SCpnt)
{

	atp870u_queuecommand(SCpnt, internal_done);

	SCpnt->SCp.Status = 0;
	while (!SCpnt->SCp.Status)
		barrier();
	return SCpnt->result;
}

unsigned char fun_scam(struct atp_unit *dev, unsigned short int *val)
{
	unsigned int tmport;
	unsigned short int i, k;
	unsigned char j;

	tmport = dev->ioport + 0x1c;
	outw(*val, tmport);
FUN_D7:
	for (i = 0; i < 10; i++) {	/* stable >= bus settle delay(400 ns)  */
		k = inw(tmport);
		j = (unsigned char) (k >> 8);
		if ((k & 0x8000) != 0) {	/* DB7 all release?    */
			goto FUN_D7;
		}
	}
	*val |= 0x4000; 	/* assert DB6		*/
	outw(*val, tmport);
	*val &= 0xdfff; 	/* assert DB5		*/
	outw(*val, tmport);
FUN_D5:
	for (i = 0; i < 10; i++) {	/* stable >= bus settle delay(400 ns) */
		if ((inw(tmport) & 0x2000) != 0) {	/* DB5 all release?	  */
			goto FUN_D5;
		}
	}
	*val |= 0x8000; 	/* no DB4-0, assert DB7    */
	*val &= 0xe0ff;
	outw(*val, tmport);
	*val &= 0xbfff; 	/* release DB6		   */
	outw(*val, tmport);
      FUN_D6:
	for (i = 0; i < 10; i++) {	/* stable >= bus settle delay(400 ns)  */
		if ((inw(tmport) & 0x4000) != 0) {	/* DB6 all release?  */
			goto FUN_D6;
		}
	}

	return j;
}

void tscam(unsigned char host)
{

	unsigned int tmport;
	unsigned char i, j, k;
	unsigned long n;
	unsigned short int m, assignid_map, val;
	unsigned char mbuf[33], quintet[2];
	struct atp_unit *dev = &atp_unit[host];
	static unsigned char g2q_tab[8] = {
		0x38, 0x31, 0x32, 0x2b, 0x34, 0x2d, 0x2e, 0x27
	};


	for (i = 0; i < 0x10; i++) {
		mydlyu(0xffff);
	}

	tmport = dev->ioport + 1;
	outb(0x08, tmport++);
	outb(0x7f, tmport);
	tmport = dev->ioport + 0x11;
	outb(0x20, tmport);

	if ((dev->scam_on & 0x40) == 0) {
		return;
	}
	m = 1;
	m <<= dev->host_idu;
	j = 16;
	if (dev->chip_veru < 4) {
		m |= 0xff00;
		j = 8;
	}
	assignid_map = m;
	tmport = dev->ioport + 0x02;
	outb(0x02, tmport++);	/* 2*2=4ms,3EH 2/32*3E=3.9ms */
	outb(0, tmport++);
	outb(0, tmport++);
	outb(0, tmport++);
	outb(0, tmport++);
	outb(0, tmport++);
	outb(0, tmport++);

	for (i = 0; i < j; i++) {
		m = 1;
		m = m << i;
		if ((m & assignid_map) != 0) {
			continue;
		}
		tmport = dev->ioport + 0x0f;
		outb(0, tmport++);
		tmport += 0x02;
		outb(0, tmport++);
		outb(0, tmport++);
		outb(0, tmport++);
		if (i > 7) {
			k = (i & 0x07) | 0x40;
		} else {
			k = i;
		}
		outb(k, tmport++);
		tmport = dev->ioport + 0x1b;
		if (dev->chip_veru == 4) {
			outb(0x01, tmport);
		} else {
			outb(0x00, tmport);
		}
wait_rdyok:
		tmport = dev->ioport + 0x18;
		outb(0x09, tmport);
		tmport += 0x07;

		while ((inb(tmport) & 0x80) == 0x00);
		tmport -= 0x08;
		k = inb(tmport);
		if (k != 0x16) {
			if ((k == 0x85) || (k == 0x42)) {
				continue;
			}
			tmport = dev->ioport + 0x10;
			outb(0x41, tmport);
			goto wait_rdyok;
		}
		assignid_map |= m;

	}
	tmport = dev->ioport + 0x02;
	outb(0x7f, tmport);
	tmport = dev->ioport + 0x1b;
	outb(0x02, tmport);

	outb(0, 0x80);

	val = 0x0080;		/* bsy	*/
	tmport = dev->ioport + 0x1c;
	outw(val, tmport);
	val |= 0x0040;		/* sel	*/
	outw(val, tmport);
	val |= 0x0004;		/* msg	*/
	outw(val, tmport);
	inb(0x80);		/* 2 deskew delay(45ns*2=90ns) */
	val &= 0x007f;		/* no bsy  */
	outw(val, tmport);
	mydlyu(0xffff); 	/* recommanded SCAM selection response time */
	mydlyu(0xffff);
	val &= 0x00fb;		/* after 1ms no msg */
	outw(val, tmport);
wait_nomsg:
	if ((inb(tmport) & 0x04) != 0) {
		goto wait_nomsg;
	}
	outb(1, 0x80);
	mydlyu(100);
	for (n = 0; n < 0x30000; n++) {
		if ((inb(tmport) & 0x80) != 0) {	/* bsy ? */
			goto wait_io;
		}
	}
	goto TCM_SYNC;
wait_io:
	for (n = 0; n < 0x30000; n++) {
		if ((inb(tmport) & 0x81) == 0x0081) {
			goto wait_io1;
		}
	}
	goto TCM_SYNC;
wait_io1:
	inb(0x80);
	val |= 0x8003;		/* io,cd,db7  */
	outw(val, tmport);
	inb(0x80);
	val &= 0x00bf;		/* no sel     */
	outw(val, tmport);
	outb(2, 0x80);
TCM_SYNC:
	mydlyu(0x800);
	if ((inb(tmport) & 0x80) == 0x00) {	/* bsy ? */
		outw(0, tmport--);
		outb(0, tmport);
		tmport = dev->ioport + 0x15;
		outb(0, tmport);
		tmport += 0x03;
		outb(0x09, tmport);
		tmport += 0x07;
		while ((inb(tmport) & 0x80) == 0);
		tmport -= 0x08;
		inb(tmport);
		return;
	}
	val &= 0x00ff;		/* synchronization  */
	val |= 0x3f00;
	fun_scam(dev, &val);
	outb(3, 0x80);
	val &= 0x00ff;		/* isolation	    */
	val |= 0x2000;
	fun_scam(dev, &val);
	outb(4, 0x80);
	i = 8;
	j = 0;
TCM_ID:
	if ((inw(tmport) & 0x2000) == 0) {
		goto TCM_ID;
	}
	outb(5, 0x80);
	val &= 0x00ff;		/* get ID_STRING */
	val |= 0x2000;
	k = fun_scam(dev, &val);
	if ((k & 0x03) == 0) {
		goto TCM_5;
	}
	mbuf[j] <<= 0x01;
	mbuf[j] &= 0xfe;
	if ((k & 0x02) != 0) {
		mbuf[j] |= 0x01;
	}
	i--;
	if (i > 0) {
		goto TCM_ID;
	}
	j++;
	i = 8;
	goto TCM_ID;

TCM_5:			/* isolation complete..  */
/*    mbuf[32]=0;
	printk(" \n%x %x %x %s\n ",assignid_map,mbuf[0],mbuf[1],&mbuf[2]); */
	i = 15;
	j = mbuf[0];
	if ((j & 0x20) != 0) {	/* bit5=1:ID upto 7	 */
		i = 7;
	}
	if ((j & 0x06) == 0) {	/* IDvalid?		*/
		goto G2Q5;
	}
	k = mbuf[1];
small_id:
	m = 1;
	m <<= k;
	if ((m & assignid_map) == 0) {
		goto G2Q_QUIN;
	}
	if (k > 0) {
		k--;
		goto small_id;
	}
G2Q5:				/* srch from max acceptable ID#  */
	k = i;			/* max acceptable ID#		 */
G2Q_LP:
	m = 1;
	m <<= k;
	if ((m & assignid_map) == 0) {
		goto G2Q_QUIN;
	}
	if (k > 0) {
		k--;
		goto G2Q_LP;
	}
G2Q_QUIN:		/* k=binID#,	   */
	assignid_map |= m;
	if (k < 8) {
		quintet[0] = 0x38;	/* 1st dft ID<8    */
	} else {
		quintet[0] = 0x31;	/* 1st	ID>=8	   */
	}
	k &= 0x07;
	quintet[1] = g2q_tab[k];

	val &= 0x00ff;		/* AssignID 1stQuintet,AH=001xxxxx  */
	m = quintet[0] << 8;
	val |= m;
	fun_scam(dev, &val);
	val &= 0x00ff;		/* AssignID 2ndQuintet,AH=001xxxxx */
	m = quintet[1] << 8;
	val |= m;
	fun_scam(dev, &val);

	goto TCM_SYNC;

}

void is870(unsigned long host, unsigned int wkport)
{
	unsigned int tmport;
	unsigned char i, j, k, rmb, n;
	unsigned short int m;
	static unsigned char mbuf[512];
	static unsigned char satn[9] =	{0, 0, 0, 0, 0, 0, 0, 6, 6};
	static unsigned char inqd[9] =	{0x12, 0, 0, 0, 0x24, 0, 0, 0x24, 6};
	static unsigned char synn[6] =	{0x80, 1, 3, 1, 0x19, 0x0e};
	static unsigned char synu[6] =	{0x80, 1, 3, 1, 0x0c, 0x0e};
	static unsigned char synw[6] =	{0x80, 1, 3, 1, 0x0c, 0x07};
	static unsigned char wide[6] =	{0x80, 1, 2, 3, 1, 0};
	struct atp_unit *dev = &atp_unit[host];

	sync_idu = 0;
	tmport = wkport + 0x3a;
	outb((unsigned char) (inb(tmport) | 0x10), tmport);

	for (i = 0; i < 16; i++) {
		if ((dev->chip_veru != 4) && (i > 7)) {
			break;
		}
		m = 1;
		m = m << i;
		if ((m & dev->active_idu) != 0) {
			continue;
		}
		if (i == dev->host_idu) {
			printk(KERN_INFO "         ID: %2d  Host Adapter\n", dev->host_idu);
			continue;
		}
		tmport = wkport + 0x1b;
		if (dev->chip_veru == 4) {
		   outb(0x01, tmport);
		}
		else
		{
		   outb(0x00, tmport);
		}
		tmport = wkport + 1;
		outb(0x08, tmport++);
		outb(0x7f, tmport++);
		outb(satn[0], tmport++);
		outb(satn[1], tmport++);
		outb(satn[2], tmport++);
		outb(satn[3], tmport++);
		outb(satn[4], tmport++);
		outb(satn[5], tmport++);
		tmport += 0x06;
		outb(0, tmport);
		tmport += 0x02;
		outb(dev->id[i].devspu, tmport++);
		outb(0, tmport++);
		outb(satn[6], tmport++);
		outb(satn[7], tmport++);
		j = i;
		if ((j & 0x08) != 0) {
			j = (j & 0x07) | 0x40;
		}
		outb(j, tmport);
		tmport += 0x03;
		outb(satn[8], tmport);
		tmport += 0x07;

		while ((inb(tmport) & 0x80) == 0x00);
		tmport -= 0x08;
		if ((inb(tmport) != 0x11) && (inb(tmport) != 0x8e)) {
			continue;
		}
		while (inb(tmport) != 0x8e);
		dev->active_idu |= m;

		tmport = wkport + 0x10;
		outb(0x30, tmport);
		tmport = wkport + 0x04;
		outb(0x00, tmport);

phase_cmd:
		tmport = wkport + 0x18;
		outb(0x08, tmport);
		tmport += 0x07;
		while ((inb(tmport) & 0x80) == 0x00);
		tmport -= 0x08;
		j = inb(tmport);
		if (j != 0x16) {
			tmport = wkport + 0x10;
			outb(0x41, tmport);
			goto phase_cmd;
		}
sel_ok:
		tmport = wkport + 3;
		outb(inqd[0], tmport++);
		outb(inqd[1], tmport++);
		outb(inqd[2], tmport++);
		outb(inqd[3], tmport++);
		outb(inqd[4], tmport++);
		outb(inqd[5], tmport);
		tmport += 0x07;
		outb(0, tmport);
		tmport += 0x02;
		outb(dev->id[i].devspu, tmport++);
		outb(0, tmport++);
		outb(inqd[6], tmport++);
		outb(inqd[7], tmport++);
		tmport += 0x03;
		outb(inqd[8], tmport);
		tmport += 0x07;
		while ((inb(tmport) & 0x80) == 0x00);
		tmport -= 0x08;
		if ((inb(tmport) != 0x11) && (inb(tmport) != 0x8e)) {
			continue;
		}
		while (inb(tmport) != 0x8e);
		tmport = wkport + 0x1b;
		if (dev->chip_veru == 4) {
			outb(0x00, tmport);
		}
		tmport = wkport + 0x18;
		outb(0x08, tmport);
		tmport += 0x07;
		j = 0;
rd_inq_data:
		k = inb(tmport);
		if ((k & 0x01) != 0) {
			tmport -= 0x06;
			mbuf[j++] = inb(tmport);
			tmport += 0x06;
			goto rd_inq_data;
		}
		if ((k & 0x80) == 0) {
			goto rd_inq_data;
		}
		tmport -= 0x08;
		j = inb(tmport);
		if (j == 0x16) {
			goto inq_ok;
		}
		tmport = wkport + 0x10;
		outb(0x46, tmport);
		tmport += 0x02;
		outb(0, tmport++);
		outb(0, tmport++);
		outb(0, tmport++);
		tmport += 0x03;
		outb(0x08, tmport);
		tmport += 0x07;
		while ((inb(tmport) & 0x80) == 0x00);
		tmport -= 0x08;
		if (inb(tmport) != 0x16) {
			goto sel_ok;
		}
inq_ok:
		mbuf[36] = 0;
		printk(KERN_INFO "         ID: %2d  %s\n", i, &mbuf[8]);
		dev->id[i].devtypeu = mbuf[0];
		rmb = mbuf[1];
		n = mbuf[7];
		if (dev->chip_veru != 4) {
			goto not_wide;
		}
		if ((mbuf[7] & 0x60) == 0) {
			goto not_wide;
		}
		if ((dev->global_map & 0x20) == 0) {
			goto not_wide;
		}
		tmport = wkport + 0x1b;
		outb(0x01, tmport);
		tmport = wkport + 3;
		outb(satn[0], tmport++);
		outb(satn[1], tmport++);
		outb(satn[2], tmport++);
		outb(satn[3], tmport++);
		outb(satn[4], tmport++);
		outb(satn[5], tmport++);
		tmport += 0x06;
		outb(0, tmport);
		tmport += 0x02;
		outb(dev->id[i].devspu, tmport++);
		outb(0, tmport++);
		outb(satn[6], tmport++);
		outb(satn[7], tmport++);
		tmport += 0x03;
		outb(satn[8], tmport);
		tmport += 0x07;

		while ((inb(tmport) & 0x80) == 0x00);
		tmport -= 0x08;
		if ((inb(tmport) != 0x11) && (inb(tmport) != 0x8e)) {
			continue;
		}
		while (inb(tmport) != 0x8e);
try_wide:
		j = 0;
		tmport = wkport + 0x14;
		outb(0x05, tmport);
		tmport += 0x04;
		outb(0x20, tmport);
		tmport += 0x07;

		while ((inb(tmport) & 0x80) == 0) {
			if ((inb(tmport) & 0x01) != 0) {
				tmport -= 0x06;
				outb(wide[j++], tmport);
				tmport += 0x06;
			}
		}
		tmport -= 0x08;
		while ((inb(tmport) & 0x80) == 0x00);
		j = inb(tmport) & 0x0f;
		if (j == 0x0f) {
			goto widep_in;
		}
		if (j == 0x0a) {
			goto widep_cmd;
		}
		if (j == 0x0e) {
			goto try_wide;
		}
		continue;
widep_out:
		tmport = wkport + 0x18;
		outb(0x20, tmport);
		tmport += 0x07;
		while ((inb(tmport) & 0x80) == 0) {
			if ((inb(tmport) & 0x01) != 0) {
				tmport -= 0x06;
				outb(0, tmport);
				tmport += 0x06;
			}
		}
		tmport -= 0x08;
		j = inb(tmport) & 0x0f;
		if (j == 0x0f) {
			goto widep_in;
		}
		if (j == 0x0a) {
			goto widep_cmd;
		}
		if (j == 0x0e) {
			goto widep_out;
		}
		continue;
widep_in:
		tmport = wkport + 0x14;
		outb(0xff, tmport);
		tmport += 0x04;
		outb(0x20, tmport);
		tmport += 0x07;
		k = 0;
widep_in1:
		j = inb(tmport);
		if ((j & 0x01) != 0) {
			tmport -= 0x06;
			mbuf[k++] = inb(tmport);
			tmport += 0x06;
			goto widep_in1;
		}
		if ((j & 0x80) == 0x00) {
			goto widep_in1;
		}
		tmport -= 0x08;
		j = inb(tmport) & 0x0f;
		if (j == 0x0f) {
			goto widep_in;
		}
		if (j == 0x0a) {
			goto widep_cmd;
		}
		if (j == 0x0e) {
			goto widep_out;
		}
		continue;
widep_cmd:
		tmport = wkport + 0x10;
		outb(0x30, tmport);
		tmport = wkport + 0x14;
		outb(0x00, tmport);
		tmport += 0x04;
		outb(0x08, tmport);
		tmport += 0x07;
		while ((inb(tmport) & 0x80) == 0x00);
		tmport -= 0x08;
		j = inb(tmport);
		if (j != 0x16) {
			if (j == 0x4e) {
				goto widep_out;
			}
			continue;
		}
		if (mbuf[0] != 0x01) {
			goto not_wide;
		}
		if (mbuf[1] != 0x02) {
			goto not_wide;
		}
		if (mbuf[2] != 0x03) {
			goto not_wide;
		}
		if (mbuf[3] != 0x01) {
			goto not_wide;
		}
		m = 1;
		m = m << i;
		dev->wide_idu |= m;
not_wide:
		if ((dev->id[i].devtypeu == 0x00) || (dev->id[i].devtypeu == 0x07) ||
		    ((dev->id[i].devtypeu == 0x05) && ((n & 0x10) != 0)))
		{
			goto set_sync;
		}
		continue;
set_sync:
		tmport = wkport + 0x1b;
		j = 0;
		if ((m & dev->wide_idu) != 0) {
			j |= 0x01;
		}
		outb(j, tmport);
		tmport = wkport + 3;
		outb(satn[0], tmport++);
		outb(satn[1], tmport++);
		outb(satn[2], tmport++);
		outb(satn[3], tmport++);
		outb(satn[4], tmport++);
		outb(satn[5], tmport++);
		tmport += 0x06;
		outb(0, tmport);
		tmport += 0x02;
		outb(dev->id[i].devspu, tmport++);
		outb(0, tmport++);
		outb(satn[6], tmport++);
		outb(satn[7], tmport++);
		tmport += 0x03;
		outb(satn[8], tmport);
		tmport += 0x07;

		while ((inb(tmport) & 0x80) == 0x00);
		tmport -= 0x08;
		if ((inb(tmport) != 0x11) && (inb(tmport) != 0x8e)) {
			continue;
		}
		while (inb(tmport) != 0x8e);
try_sync:
		j = 0;
		tmport = wkport + 0x14;
		outb(0x06, tmport);
		tmport += 0x04;
		outb(0x20, tmport);
		tmport += 0x07;

		while ((inb(tmport) & 0x80) == 0) {
			if ((inb(tmport) & 0x01) != 0) {
				tmport -= 0x06;
				if ((m & dev->wide_idu) != 0) {
					outb(synw[j++], tmport);
				} else {
					if ((m & dev->ultra_map) != 0) {
						outb(synu[j++], tmport);
					} else {
						outb(synn[j++], tmport);
					}
				}
				tmport += 0x06;
			}
		}
		tmport -= 0x08;
		while ((inb(tmport) & 0x80) == 0x00);
		j = inb(tmport) & 0x0f;
		if (j == 0x0f) {
			goto phase_ins;
		}
		if (j == 0x0a) {
			goto phase_cmds;
		}
		if (j == 0x0e) {
			goto try_sync;
		}
		continue;
phase_outs:
		tmport = wkport + 0x18;
		outb(0x20, tmport);
		tmport += 0x07;
		while ((inb(tmport) & 0x80) == 0x00) {
			if ((inb(tmport) & 0x01) != 0x00) {
				tmport -= 0x06;
				outb(0x00, tmport);
				tmport += 0x06;
			}
		}
		tmport -= 0x08;
		j = inb(tmport);
		if (j == 0x85) {
			goto tar_dcons;
		}
		j &= 0x0f;
		if (j == 0x0f) {
			goto phase_ins;
		}
		if (j == 0x0a) {
			goto phase_cmds;
		}
		if (j == 0x0e) {
			goto phase_outs;
		}
		continue;
phase_ins:
		tmport = wkport + 0x14;
		outb(0xff, tmport);
		tmport += 0x04;
		outb(0x20, tmport);
		tmport += 0x07;
		k = 0;
phase_ins1:
		j = inb(tmport);
		if ((j & 0x01) != 0x00) {
			tmport -= 0x06;
			mbuf[k++] = inb(tmport);
			tmport += 0x06;
			goto phase_ins1;
		}
		if ((j & 0x80) == 0x00) {
			goto phase_ins1;
		}
		tmport -= 0x08;
		while ((inb(tmport) & 0x80) == 0x00);
		j = inb(tmport);
		if (j == 0x85) {
			goto tar_dcons;
		}
		j &= 0x0f;
		if (j == 0x0f) {
			goto phase_ins;
		}
		if (j == 0x0a) {
			goto phase_cmds;
		}
		if (j == 0x0e) {
			goto phase_outs;
		}
		continue;
phase_cmds:
		tmport = wkport + 0x10;
		outb(0x30, tmport);
tar_dcons:
		tmport = wkport + 0x14;
		outb(0x00, tmport);
		tmport += 0x04;
		outb(0x08, tmport);
		tmport += 0x07;
		while ((inb(tmport) & 0x80) == 0x00);
		tmport -= 0x08;
		j = inb(tmport);
		if (j != 0x16) {
			continue;
		}
		if (mbuf[0] != 0x01) {
			continue;
		}
		if (mbuf[1] != 0x03) {
			continue;
		}
		if (mbuf[4] == 0x00) {
			continue;
		}
		if (mbuf[3] > 0x64) {
			continue;
		}
		if (mbuf[4] > 0x0c) {
			mbuf[4] = 0x0c;
		}
		dev->id[i].devspu = mbuf[4];
		if ((mbuf[3] < 0x0d) && (rmb == 0)) {
			j = 0xa0;
			goto set_syn_ok;
		}
		if (mbuf[3] < 0x1a) {
			j = 0x20;
			goto set_syn_ok;
		}
		if (mbuf[3] < 0x33) {
			j = 0x40;
			goto set_syn_ok;
		}
		if (mbuf[3] < 0x4c) {
			j = 0x50;
			goto set_syn_ok;
		}
		j = 0x60;
	      set_syn_ok:
		dev->id[i].devspu = (dev->id[i].devspu & 0x0f) | j;
	}
	tmport = wkport + 0x3a;
	outb((unsigned char) (inb(tmport) & 0xef), tmport);
}

void is880(unsigned long host, unsigned int wkport)
{
	unsigned int tmport;
	unsigned char i, j, k, rmb, n, lvdmode;
	unsigned short int m;
	static unsigned char mbuf[512];
	static unsigned char satn[9] =	{0, 0, 0, 0, 0, 0, 0, 6, 6};
	static unsigned char inqd[9] =	{0x12, 0, 0, 0, 0x24, 0, 0, 0x24, 6};
	static unsigned char synn[6] =	{0x80, 1, 3, 1, 0x19, 0x0e};
	unsigned char synu[6] =  {0x80, 1, 3, 1, 0x0a, 0x0e};
	static unsigned char synw[6] =	{0x80, 1, 3, 1, 0x19, 0x0e};
	unsigned char synuw[6] =  {0x80, 1, 3, 1, 0x0a, 0x0e};
	static unsigned char wide[6] =	{0x80, 1, 2, 3, 1, 0};
	static unsigned char u3[9] = { 0x80,1,6,4,0x09,00,0x0e,0x01,0x02 };
	struct atp_unit *dev = &atp_unit[host];

	sync_idu = 0;
	lvdmode=inb(wkport + 0x3f) & 0x40;

	for (i = 0; i < 16; i++) {
		m = 1;
		m = m << i;
		if ((m & dev->active_idu) != 0) {
			continue;
		}
		if (i == dev->host_idu) {
			printk(KERN_INFO "         ID: %2d  Host Adapter\n", dev->host_idu);
			continue;
		}
		tmport = wkport + 0x5b;
		outb(0x01, tmport);
		tmport = wkport + 0x41;
		outb(0x08, tmport++);
		outb(0x7f, tmport++);
		outb(satn[0], tmport++);
		outb(satn[1], tmport++);
		outb(satn[2], tmport++);
		outb(satn[3], tmport++);
		outb(satn[4], tmport++);
		outb(satn[5], tmport++);
		tmport += 0x06;
		outb(0, tmport);
		tmport += 0x02;
		outb(dev->id[i].devspu, tmport++);
		outb(0, tmport++);
		outb(satn[6], tmport++);
		outb(satn[7], tmport++);
		j = i;
		if ((j & 0x08) != 0) {
			j = (j & 0x07) | 0x40;
		}
		outb(j, tmport);
		tmport += 0x03;
		outb(satn[8], tmport);
		tmport += 0x07;

		while ((inb(tmport) & 0x80) == 0x00);
		tmport -= 0x08;
		if ((inb(tmport) != 0x11) && (inb(tmport) != 0x8e)) {
			continue;
		}
		while (inb(tmport) != 0x8e);
		dev->active_idu |= m;

		tmport = wkport + 0x50;
		outb(0x30, tmport);
		tmport = wkport + 0x54;
		outb(0x00, tmport);

phase_cmd:
		tmport = wkport + 0x58;
		outb(0x08, tmport);
		tmport += 0x07;
		while ((inb(tmport) & 0x80) == 0x00);
		tmport -= 0x08;
		j = inb(tmport);
		if (j != 0x16) {
			tmport = wkport + 0x50;
			outb(0x41, tmport);
			goto phase_cmd;
		}
sel_ok:
		tmport = wkport + 0x43;
		outb(inqd[0], tmport++);
		outb(inqd[1], tmport++);
		outb(inqd[2], tmport++);
		outb(inqd[3], tmport++);
		outb(inqd[4], tmport++);
		outb(inqd[5], tmport);
		tmport += 0x07;
		outb(0, tmport);
		tmport += 0x02;
		outb(dev->id[i].devspu, tmport++);
		outb(0, tmport++);
		outb(inqd[6], tmport++);
		outb(inqd[7], tmport++);
		tmport += 0x03;
		outb(inqd[8], tmport);
		tmport += 0x07;
		while ((inb(tmport) & 0x80) == 0x00);
		tmport -= 0x08;
		if ((inb(tmport) != 0x11) && (inb(tmport) != 0x8e)) {
			continue;
		}
		while (inb(tmport) != 0x8e);
		tmport = wkport + 0x5b;
		outb(0x00, tmport);
		tmport = wkport + 0x58;
		outb(0x08, tmport);
		tmport += 0x07;
		j = 0;
rd_inq_data:
		k = inb(tmport);
		if ((k & 0x01) != 0) {
			tmport -= 0x06;
			mbuf[j++] = inb(tmport);
			tmport += 0x06;
			goto rd_inq_data;
		}
		if ((k & 0x80) == 0) {
			goto rd_inq_data;
		}
		tmport -= 0x08;
		j = inb(tmport);
		if (j == 0x16) {
			goto inq_ok;
		}
		tmport = wkport + 0x50;
		outb(0x46, tmport);
		tmport += 0x02;
		outb(0, tmport++);
		outb(0, tmport++);
		outb(0, tmport++);
		tmport += 0x03;
		outb(0x08, tmport);
		tmport += 0x07;
		while ((inb(tmport) & 0x80) == 0x00);
		tmport -= 0x08;
		if (inb(tmport) != 0x16) {
			goto sel_ok;
		}
inq_ok:
		mbuf[36] = 0;
		printk(KERN_INFO "         ID: %2d  %s\n", i, &mbuf[8]);
		dev->id[i].devtypeu = mbuf[0];
		rmb = mbuf[1];
		n = mbuf[7];
		if ((mbuf[7] & 0x60) == 0) {
			goto not_wide;
		}
		if ((i < 8) && ((dev->global_map & 0x20) == 0)) {
			goto not_wide;
		}
		if (lvdmode == 0)
		{
		   goto chg_wide;
		}
		if (dev->sp[i] != 0x04) 	 // force u2
		{
		   goto chg_wide;
		}

		tmport = wkport + 0x5b;
		outb(0x01, tmport);
		tmport = wkport + 0x43;
		outb(satn[0], tmport++);
		outb(satn[1], tmport++);
		outb(satn[2], tmport++);
		outb(satn[3], tmport++);
		outb(satn[4], tmport++);
		outb(satn[5], tmport++);
		tmport += 0x06;
		outb(0, tmport);
		tmport += 0x02;
		outb(dev->id[i].devspu, tmport++);
		outb(0, tmport++);
		outb(satn[6], tmport++);
		outb(satn[7], tmport++);
		tmport += 0x03;
		outb(satn[8], tmport);
		tmport += 0x07;

		while ((inb(tmport) & 0x80) == 0x00);
		tmport -= 0x08;
		if ((inb(tmport) != 0x11) && (inb(tmport) != 0x8e)) {
			continue;
		}
		while (inb(tmport) != 0x8e);
try_u3:
		j = 0;
		tmport = wkport + 0x54;
		outb(0x09, tmport);
		tmport += 0x04;
		outb(0x20, tmport);
		tmport += 0x07;

		while ((inb(tmport) & 0x80) == 0) {
			if ((inb(tmport) & 0x01) != 0) {
				tmport -= 0x06;
				outb(u3[j++], tmport);
				tmport += 0x06;
			}
		}
		tmport -= 0x08;
		while ((inb(tmport) & 0x80) == 0x00);
		j = inb(tmport) & 0x0f;
		if (j == 0x0f) {
			goto u3p_in;
		}
		if (j == 0x0a) {
			goto u3p_cmd;
		}
		if (j == 0x0e) {
			goto try_u3;
		}
		continue;
u3p_out:
		tmport = wkport + 0x58;
		outb(0x20, tmport);
		tmport += 0x07;
		while ((inb(tmport) & 0x80) == 0) {
			if ((inb(tmport) & 0x01) != 0) {
				tmport -= 0x06;
				outb(0, tmport);
				tmport += 0x06;
			}
		}
		tmport -= 0x08;
		j = inb(tmport) & 0x0f;
		if (j == 0x0f) {
			goto u3p_in;
		}
		if (j == 0x0a) {
			goto u3p_cmd;
		}
		if (j == 0x0e) {
			goto u3p_out;
		}
		continue;
u3p_in:
		tmport = wkport + 0x54;
		outb(0x09, tmport);
		tmport += 0x04;
		outb(0x20, tmport);
		tmport += 0x07;
		k = 0;
u3p_in1:
		j = inb(tmport);
		if ((j & 0x01) != 0) {
			tmport -= 0x06;
			mbuf[k++] = inb(tmport);
			tmport += 0x06;
			goto u3p_in1;
		}
		if ((j & 0x80) == 0x00) {
			goto u3p_in1;
		}
		tmport -= 0x08;
		j = inb(tmport) & 0x0f;
		if (j == 0x0f) {
			goto u3p_in;
		}
		if (j == 0x0a) {
			goto u3p_cmd;
		}
		if (j == 0x0e) {
			goto u3p_out;
		}
		continue;
u3p_cmd:
		tmport = wkport + 0x50;
		outb(0x30, tmport);
		tmport = wkport + 0x54;
		outb(0x00, tmport);
		tmport += 0x04;
		outb(0x08, tmport);
		tmport += 0x07;
		while ((inb(tmport) & 0x80) == 0x00);
		tmport -= 0x08;
		j = inb(tmport);
		if (j != 0x16) {
			if (j == 0x4e) {
				goto u3p_out;
			}
			continue;
		}
		if (mbuf[0] != 0x01) {
			goto chg_wide;
		}
		if (mbuf[1] != 0x06) {
			goto chg_wide;
		}
		if (mbuf[2] != 0x04) {
			goto chg_wide;
		}
		if (mbuf[3] == 0x09) {
		   m = 1;
		   m = m << i;
		   dev->wide_idu |= m;
		   dev->id[i].devspu = 0xce;
		   continue;
		}
chg_wide:
		tmport = wkport + 0x5b;
		outb(0x01, tmport);
		tmport = wkport + 0x43;
		outb(satn[0], tmport++);
		outb(satn[1], tmport++);
		outb(satn[2], tmport++);
		outb(satn[3], tmport++);
		outb(satn[4], tmport++);
		outb(satn[5], tmport++);
		tmport += 0x06;
		outb(0, tmport);
		tmport += 0x02;
		outb(dev->id[i].devspu, tmport++);
		outb(0, tmport++);
		outb(satn[6], tmport++);
		outb(satn[7], tmport++);
		tmport += 0x03;
		outb(satn[8], tmport);
		tmport += 0x07;

		while ((inb(tmport) & 0x80) == 0x00);
		tmport -= 0x08;
		if ((inb(tmport) != 0x11) && (inb(tmport) != 0x8e)) {
			continue;
		}
		while (inb(tmport) != 0x8e);
try_wide:
		j = 0;
		tmport = wkport + 0x54;
		outb(0x05, tmport);
		tmport += 0x04;
		outb(0x20, tmport);
		tmport += 0x07;

		while ((inb(tmport) & 0x80) == 0) {
			if ((inb(tmport) & 0x01) != 0) {
				tmport -= 0x06;
				outb(wide[j++], tmport);
				tmport += 0x06;
			}
		}
		tmport -= 0x08;
		while ((inb(tmport) & 0x80) == 0x00);
		j = inb(tmport) & 0x0f;
		if (j == 0x0f) {
			goto widep_in;
		}
		if (j == 0x0a) {
			goto widep_cmd;
		}
		if (j == 0x0e) {
			goto try_wide;
		}
		continue;
widep_out:
		tmport = wkport + 0x58;
		outb(0x20, tmport);
		tmport += 0x07;
		while ((inb(tmport) & 0x80) == 0) {
			if ((inb(tmport) & 0x01) != 0) {
				tmport -= 0x06;
				outb(0, tmport);
				tmport += 0x06;
			}
		}
		tmport -= 0x08;
		j = inb(tmport) & 0x0f;
		if (j == 0x0f) {
			goto widep_in;
		}
		if (j == 0x0a) {
			goto widep_cmd;
		}
		if (j == 0x0e) {
			goto widep_out;
		}
		continue;
widep_in:
		tmport = wkport + 0x54;
		outb(0xff, tmport);
		tmport += 0x04;
		outb(0x20, tmport);
		tmport += 0x07;
		k = 0;
widep_in1:
		j = inb(tmport);
		if ((j & 0x01) != 0) {
			tmport -= 0x06;
			mbuf[k++] = inb(tmport);
			tmport += 0x06;
			goto widep_in1;
		}
		if ((j & 0x80) == 0x00) {
			goto widep_in1;
		}
		tmport -= 0x08;
		j = inb(tmport) & 0x0f;
		if (j == 0x0f) {
			goto widep_in;
		}
		if (j == 0x0a) {
			goto widep_cmd;
		}
		if (j == 0x0e) {
			goto widep_out;
		}
		continue;
widep_cmd:
		tmport = wkport + 0x50;
		outb(0x30, tmport);
		tmport = wkport + 0x54;
		outb(0x00, tmport);
		tmport += 0x04;
		outb(0x08, tmport);
		tmport += 0x07;
		while ((inb(tmport) & 0x80) == 0x00);
		tmport -= 0x08;
		j = inb(tmport);
		if (j != 0x16) {
			if (j == 0x4e) {
				goto widep_out;
			}
			continue;
		}
		if (mbuf[0] != 0x01) {
			goto not_wide;
		}
		if (mbuf[1] != 0x02) {
			goto not_wide;
		}
		if (mbuf[2] != 0x03) {
			goto not_wide;
		}
		if (mbuf[3] != 0x01) {
			goto not_wide;
		}
		m = 1;
		m = m << i;
		dev->wide_idu |= m;
not_wide:
		if ((dev->id[i].devtypeu == 0x00) || (dev->id[i].devtypeu == 0x07) ||
		    ((dev->id[i].devtypeu == 0x05) && ((n & 0x10) != 0)))
		{
			m = 1;
			m = m << i;
			if ((dev->async & m) != 0)
			{
			   goto set_sync;
			}
		}
		continue;
set_sync:
		if (dev->sp[i] == 0x02)
		{
		   synu[4]=0x0c;
		   synuw[4]=0x0c;
		}
		else
		{
		   if (dev->sp[i] >= 0x03)
		   {
		      synu[4]=0x0a;
		      synuw[4]=0x0a;
		   }
		}
		tmport = wkport + 0x5b;
		j = 0;
		if ((m & dev->wide_idu) != 0) {
			j |= 0x01;
		}
		outb(j, tmport);
		tmport = wkport + 0x43;
		outb(satn[0], tmport++);
		outb(satn[1], tmport++);
		outb(satn[2], tmport++);
		outb(satn[3], tmport++);
		outb(satn[4], tmport++);
		outb(satn[5], tmport++);
		tmport += 0x06;
		outb(0, tmport);
		tmport += 0x02;
		outb(dev->id[i].devspu, tmport++);
		outb(0, tmport++);
		outb(satn[6], tmport++);
		outb(satn[7], tmport++);
		tmport += 0x03;
		outb(satn[8], tmport);
		tmport += 0x07;

		while ((inb(tmport) & 0x80) == 0x00);
		tmport -= 0x08;
		if ((inb(tmport) != 0x11) && (inb(tmport) != 0x8e)) {
			continue;
		}
		while (inb(tmport) != 0x8e);
try_sync:
		j = 0;
		tmport = wkport + 0x54;
		outb(0x06, tmport);
		tmport += 0x04;
		outb(0x20, tmport);
		tmport += 0x07;

		while ((inb(tmport) & 0x80) == 0) {
			if ((inb(tmport) & 0x01) != 0) {
				tmport -= 0x06;
				if ((m & dev->wide_idu) != 0) {
					if ((m & dev->ultra_map) != 0) {
						outb(synuw[j++], tmport);
					} else {
						outb(synw[j++], tmport);
					}
				} else {
					if ((m & dev->ultra_map) != 0) {
						outb(synu[j++], tmport);
					} else {
						outb(synn[j++], tmport);
					}
				}
				tmport += 0x06;
			}
		}
		tmport -= 0x08;
		while ((inb(tmport) & 0x80) == 0x00);
		j = inb(tmport) & 0x0f;
		if (j == 0x0f) {
			goto phase_ins;
		}
		if (j == 0x0a) {
			goto phase_cmds;
		}
		if (j == 0x0e) {
			goto try_sync;
		}
		continue;
phase_outs:
		tmport = wkport + 0x58;
		outb(0x20, tmport);
		tmport += 0x07;
		while ((inb(tmport) & 0x80) == 0x00) {
			if ((inb(tmport) & 0x01) != 0x00) {
				tmport -= 0x06;
				outb(0x00, tmport);
				tmport += 0x06;
			}
		}
		tmport -= 0x08;
		j = inb(tmport);
		if (j == 0x85) {
			goto tar_dcons;
		}
		j &= 0x0f;
		if (j == 0x0f) {
			goto phase_ins;
		}
		if (j == 0x0a) {
			goto phase_cmds;
		}
		if (j == 0x0e) {
			goto phase_outs;
		}
		continue;
phase_ins:
		tmport = wkport + 0x54;
		outb(0x06, tmport);
		tmport += 0x04;
		outb(0x20, tmport);
		tmport += 0x07;
		k = 0;
phase_ins1:
		j = inb(tmport);
		if ((j & 0x01) != 0x00) {
			tmport -= 0x06;
			mbuf[k++] = inb(tmport);
			tmport += 0x06;
			goto phase_ins1;
		}
		if ((j & 0x80) == 0x00) {
			goto phase_ins1;
		}
		tmport -= 0x08;
		while ((inb(tmport) & 0x80) == 0x00);
		j = inb(tmport);
		if (j == 0x85) {
			goto tar_dcons;
		}
		j &= 0x0f;
		if (j == 0x0f) {
			goto phase_ins;
		}
		if (j == 0x0a) {
			goto phase_cmds;
		}
		if (j == 0x0e) {
			goto phase_outs;
		}
		continue;
phase_cmds:
		tmport = wkport + 0x50;
		outb(0x30, tmport);
tar_dcons:
		tmport = wkport + 0x54;
		outb(0x00, tmport);
		tmport += 0x04;
		outb(0x08, tmport);
		tmport += 0x07;
		while ((inb(tmport) & 0x80) == 0x00);
		tmport -= 0x08;
		j = inb(tmport);
		if (j != 0x16) {
			continue;
		}
		if (mbuf[0] != 0x01) {
			continue;
		}
		if (mbuf[1] != 0x03) {
			continue;
		}
		if (mbuf[4] == 0x00) {
			continue;
		}
		if (mbuf[3] > 0x64) {
			continue;
		}
		if (mbuf[4] > 0x0e) {
			mbuf[4] = 0x0e;
		}
		dev->id[i].devspu = mbuf[4];
		if (mbuf[3] < 0x0c){
			j = 0xb0;
			goto set_syn_ok;
		}
		if ((mbuf[3] < 0x0d) && (rmb == 0)) {
			j = 0xa0;
			goto set_syn_ok;
		}
		if (mbuf[3] < 0x1a) {
			j = 0x20;
			goto set_syn_ok;
		}
		if (mbuf[3] < 0x33) {
			j = 0x40;
			goto set_syn_ok;
		}
		if (mbuf[3] < 0x4c) {
			j = 0x50;
			goto set_syn_ok;
		}
		j = 0x60;
	      set_syn_ok:
		dev->id[i].devspu = (dev->id[i].devspu & 0x0f) | j;
	}
}

/* return non-zero on detection */
int atp870u_detect(Scsi_Host_Template * tpnt)
{
	unsigned char irq, h, k, m;
	unsigned long flags;
	unsigned int base_io, error, tmport;
	unsigned short index = 0;
	struct pci_dev *pdev[3];
	unsigned char chip_ver[3], host_id;
	unsigned short dev_id[3], n;
	struct Scsi_Host *shpnt = NULL;
	int tmpcnt = 0;
	int count = 0;

	static unsigned short devid[9] = {
	  0x8081, 0x8002, 0x8010, 0x8020, 0x8030, 0x8040, 0x8050, 0x8060, 0
	};

	printk(KERN_INFO "aec671x_detect: \n");
	if (!pci_present()) {
		printk(KERN_INFO"   NO PCI SUPPORT.\n");
		return count;
	}
	tpnt->proc_name = "atp870u";

	for (h = 0; h < 2; h++) {
		struct atp_unit *dev = &atp_unit[h];
		for(k=0;k<16;k++)
		{
			dev->id[k].prd_tableu = kmalloc(1024, GFP_KERNEL);
			dev->id[k].devspu=0x20;
			dev->id[k].devtypeu = 0;
			dev->id[k].curr_req = NULL;
		}
		dev->active_idu = 0;
		dev->wide_idu = 0;
		dev->host_idu = 0x07;
		dev->quhdu = 0;
		dev->quendu = 0;
		pdev[h]=NULL;
		pdev[2]=NULL;
		dev->chip_veru = 0;
		dev->last_cmd = 0xff;
		dev->in_snd = 0;
		dev->in_int = 0;
		for (k = 0; k < qcnt; k++) {
			dev->querequ[k] = 0;
		}
		for (k = 0; k < 16; k++) {
			dev->id[k].curr_req = 0;
			dev->sp[k] = 0x04;
		}
	}
	h = 0;
	while (devid[h] != 0) {
		pdev[2] = pci_find_device(0x1191, devid[h], pdev[2]);
		if (pdev[2] == NULL || pci_enable_device(pdev[2])) {
			h++;
			index = 0;
			continue;
		}
		chip_ver[2] = 0;
		dev_id[2] = devid[h];

		if (devid[h] == 0x8002) {
			error = pci_read_config_byte(pdev[2], 0x08, &chip_ver[2]);
			if (chip_ver[2] < 2) {
				goto nxt_devfn;
			}
		}
		if (devid[h] == 0x8010 || devid[h] == 0x8081 || devid[h] == 0x8050)
		{
			chip_ver[2] = 0x04;
		}
		pdev[tmpcnt] = pdev[2];
		chip_ver[tmpcnt] = chip_ver[2];
		dev_id[tmpcnt] = dev_id[2];
		tmpcnt++;
	      nxt_devfn:
		index++;
		if (index > 3) {
			index = 0;
			h++;
		}
		if(tmpcnt>1)
			break;
	}
	for (h = 0; h < 2; h++) {
		struct atp_unit *dev=&atp_unit[h];
		if (pdev[h]==NULL) {
			return count;
		}

		/* Found an atp870u/w. */
		base_io = pci_resource_start(pdev[h], 0);
		irq = pdev[h]->irq;

		if (dev_id[h] != 0x8081)
		{
		   error = pci_read_config_byte(pdev[h],0x49,&host_id);

		   base_io &= 0xfffffff8;

		   if (check_region(base_io,0x40) != 0)
		   {
			   return 0;
		   }
		   printk(KERN_INFO "   ACARD AEC-671X PCI Ultra/W SCSI-3 Host Adapter: %d    IO:%x, IRQ:%d.\n"
			  ,h, base_io, irq);
		   dev->ioport = base_io;
		   dev->pciport = base_io + 0x20;
		   dev->deviceid = dev_id[h];
		   irqnumu[h] = irq;
		   host_id &= 0x07;
		   dev->host_idu = host_id;
		   dev->chip_veru = chip_ver[h];

		   tmport = base_io + 0x22;
		   dev->scam_on = inb(tmport);
		   tmport += 0x0b;
		   dev->global_map = inb(tmport++);
		   dev->ultra_map = inw(tmport);
		   if (dev->ultra_map == 0) {
			   dev->scam_on = 0x00;
			   dev->global_map = 0x20;
			   dev->ultra_map = 0xffff;
		   }
		   shpnt = scsi_register(tpnt, 4);
		   if(shpnt==NULL)
			   return count;

		   save_flags(flags);
		   cli();
		   if (request_irq(irq, atp870u_intr_handle, SA_SHIRQ, "atp870u", dev)) {
			   printk(KERN_ERR "Unable to allocate IRQ for Acard controller.\n");
			   goto unregister;
		   }

		   if (chip_ver[h] > 0x07)	     /* check if atp876 chip   */
		   {				     /* then enable terminator */
		      tmport = base_io + 0x3e;
		      outb(0x00, tmport);
		   }

		   tmport = base_io + 0x3a;
		   k = (inb(tmport) & 0xf3) | 0x10;
		   outb(k, tmport);
		   outb((k & 0xdf), tmport);
		   mydlyu(0x8000);
		   outb(k, tmport);
		   mydlyu(0x8000);
		   tmport = base_io;
		   outb((host_id | 0x08), tmport);
		   tmport += 0x18;
		   outb(0, tmport);
		   tmport += 0x07;
		   while ((inb(tmport) & 0x80) == 0);
		   tmport -= 0x08;
		   inb(tmport);
		   tmport = base_io + 1;
		   outb(8, tmport++);
		   outb(0x7f, tmport);
		   tmport = base_io + 0x11;
		   outb(0x20, tmport);

		   tscam(h);
		   is870(h, base_io);
		   tmport = base_io + 0x3a;
		   outb((inb(tmport) & 0xef), tmport);
		   tmport++;
		   outb((inb(tmport) | 0x20),tmport);
		}
		else
		{
		   base_io &= 0xfffffff8;

		   if (check_region(base_io,0x60) != 0)
		   {
			   return 0;
		   }
		   host_id = inb(base_io + 0x39);
		   host_id >>= 0x04;

		   printk(KERN_INFO "   ACARD AEC-67160 PCI Ultra3 LVD Host Adapter: %d    IO:%x, IRQ:%d.\n"
			  ,h, base_io, irq);
		   dev->ioport = base_io + 0x40;
		   dev->pciport = base_io + 0x28;
		   dev->deviceid = dev_id[h];
		   irqnumu[h] = irq;
		   dev->host_idu = host_id;
		   dev->chip_veru = chip_ver[h];

		   tmport = base_io + 0x22;
		   dev->scam_on = inb(tmport);
		   tmport += 0x13;
		   dev->global_map = inb(tmport);
		   tmport += 0x07;
		   dev->ultra_map = inw(tmport);

		   n=0x3f09;
next_fblk:
		   if (n >= 0x4000)
		   {
		      goto flash_ok;
		   }
		   m=0;
		   outw(n,base_io + 0x34);
		   n += 0x0002;
		   if (inb(base_io + 0x30) == 0xff)
		   {
		      goto flash_ok;
		   }
		   dev->sp[m++]=inb(base_io + 0x30);
		   dev->sp[m++]=inb(base_io + 0x31);
		   dev->sp[m++]=inb(base_io + 0x32);
		   dev->sp[m++]=inb(base_io + 0x33);
		   outw(n,base_io + 0x34);
		   n += 0x0002;
		   dev->sp[m++]=inb(base_io + 0x30);
		   dev->sp[m++]=inb(base_io + 0x31);
		   dev->sp[m++]=inb(base_io + 0x32);
		   dev->sp[m++]=inb(base_io + 0x33);
		   outw(n,base_io + 0x34);
		   n += 0x0002;
		   dev->sp[m++]=inb(base_io + 0x30);
		   dev->sp[m++]=inb(base_io + 0x31);
		   dev->sp[m++]=inb(base_io + 0x32);
		   dev->sp[m++]=inb(base_io + 0x33);
		   outw(n,base_io + 0x34);
		   n += 0x0002;
		   dev->sp[m++]=inb(base_io + 0x30);
		   dev->sp[m++]=inb(base_io + 0x31);
		   dev->sp[m++]=inb(base_io + 0x32);
		   dev->sp[m++]=inb(base_io + 0x33);
		   n += 0x0018;
		   goto next_fblk;
flash_ok:
		   outw(0,base_io + 0x34);
		   dev->ultra_map=0;
		   dev->async = 0;
		   for (k=0; k < 16; k++)
		   {
		       n=1;
		       n = n << k;
		       if (dev->sp[k] > 1)
		       {
			  dev->ultra_map |= n;
		       }
		       else
		       {
			  if (dev->sp[k] == 0)
			  {
			     dev->async |= n;
			  }
		       }
		   }
		   dev->async = ~(dev->async);
		   outb(dev->global_map,base_io + 0x35);

		   shpnt = scsi_register(tpnt, 4);
		   if(shpnt==NULL)
			   return count;

		   save_flags(flags);
		   cli();
		   if (request_irq(irq, atp870u_intr_handle, SA_SHIRQ, "atp870u", dev)) {
			   printk(KERN_ERR "Unable to allocate IRQ for Acard controller.\n");
			   goto unregister;
		   }

		   tmport = base_io + 0x38;
		   k = inb(tmport) & 0x80;
		   outb(k, tmport);
		   tmport += 0x03;
		   outb(0x20, tmport);
		   mydlyu(0x8000);
		   outb(0, tmport);
		   mydlyu(0x8000);
		   tmport = base_io + 0x5b;
		   inb(tmport);
		   tmport -= 0x04;
		   inb(tmport);
		   tmport = base_io + 0x40;
		   outb((host_id | 0x08), tmport);
		   tmport += 0x18;
		   outb(0, tmport);
		   tmport += 0x07;
		   while ((inb(tmport) & 0x80) == 0);
		   tmport -= 0x08;
		   inb(tmport);
		   tmport = base_io + 0x41;
		   outb(8, tmport++);
		   outb(0x7f, tmport);
		   tmport = base_io + 0x51;
		   outb(0x20, tmport);

		   tscam(h);
		   is880(h, base_io);
		   tmport = base_io + 0x38;
		   outb(0xb0, tmport);
		}

		atp_host[h] = shpnt;
		if (dev->chip_veru == 4) {
			shpnt->max_id = 16;
		}
		shpnt->this_id = host_id;
		shpnt->unique_id = base_io;
		shpnt->io_port = base_io;
		if (dev_id[h] == 0x8081)
		{
		   shpnt->n_io_port = 0x60;	   /* Number of bytes of I/O space used */
		}
		else
		{
		   shpnt->n_io_port = 0x40;	   /* Number of bytes of I/O space used */
		}
		shpnt->irq = irq;
		restore_flags(flags);
		if (dev_id[h] == 0x8081)
		{
		   request_region(base_io, 0x60, "atp870u");       /* Register the IO ports that we use */
		}
		else
		{
		   request_region(base_io, 0x40, "atp870u");       /* Register the IO ports that we use */
		}
		count++;
		index++;
		continue;
unregister:
		scsi_unregister(shpnt);
		restore_flags(flags);
		index++;
		continue;
	}

	return count;
}

/* The abort command does not leave the device in a clean state where
   it is available to be used again.  Until this gets worked out, we will
   leave it commented out.  */

int atp870u_abort(Scsi_Cmnd * SCpnt)
{
	unsigned char h, j, k;
	Scsi_Cmnd *workrequ;
	unsigned int tmport;
	struct atp_unit *dev;
	for (h = 0; h <= admaxu; h++) {
		if (SCpnt->host == atp_host[h]) {
			goto find_adp;
		}
	}
	panic("Abort host not found !");
find_adp:
	dev=&atp_unit[h];
	printk(KERN_DEBUG "working=%x last_cmd=%x ", dev->working, dev->last_cmd);
	printk(" quhdu=%x quendu=%x ", dev->quhdu, dev->quendu);
	tmport = dev->ioport;
	for (j = 0; j < 0x17; j++) {
		printk(" r%2x=%2x", j, inb(tmport++));
	}
	tmport += 0x05;
	printk(" r1c=%2x", inb(tmport));
	tmport += 0x03;
	printk(" r1f=%2x in_snd=%2x ", inb(tmport), dev->in_snd);
	tmport= dev->pciport;
	printk(" r20=%2x", inb(tmport));
	tmport += 0x02;
	printk(" r22=%2x", inb(tmport));
	tmport = dev->ioport + 0x3a;
	printk(" r3a=%2x \n",inb(tmport));
	tmport = dev->ioport + 0x3b;
	printk(" r3b=%2x \n",inb(tmport));
	for(j=0;j<16;j++)
	{
	   if (dev->id[j].curr_req != NULL)
	   {
		workrequ = dev->id[j].curr_req;
		printk("\n que cdb= ");
		for (k=0; k < workrequ->cmd_len; k++)
		{
		    printk(" %2x ",workrequ->cmnd[k]);
		}
		printk(" last_lenu= %lx ",dev->id[j].last_lenu);
	   }
	}
	return (SCSI_ABORT_SNOOZE);
}

int atp870u_reset(Scsi_Cmnd * SCpnt, unsigned int reset_flags)
{
	unsigned char h;
	struct atp_unit *dev;
	/*
	 * See if a bus reset was suggested.
	 */
	for (h = 0; h <= admaxu; h++) {
		if (SCpnt->host == atp_host[h]) {
			goto find_host;
		}
	}
	panic("Reset bus host not found !");
find_host:
	dev=&atp_unit[h];
/*	SCpnt->result = 0x00080000;
	SCpnt->scsi_done(SCpnt);
	dev->working=0;
	dev->quhdu=0;
	dev->quendu=0;
	return (SCSI_RESET_SUCCESS | SCSI_RESET_BUS_RESET);  */
	return (SCSI_RESET_SNOOZE);
}

const char *atp870u_info(struct Scsi_Host *notused)
{
	static char buffer[128];

	strcpy(buffer, "ACARD AEC-6710/6712/67160 PCI Ultra/W/LVD SCSI-3 Adapter Driver V2.6+ac ");

	return buffer;
}

int atp870u_set_info(char *buffer, int length, struct Scsi_Host *HBAptr)
{
	return -ENOSYS; 	/* Currently this is a no-op */
}

#define BLS buffer + len + size
int atp870u_proc_info(char *buffer, char **start, off_t offset, int length,
		      int hostno, int inout)
{
	struct Scsi_Host *HBAptr;
	static u8 buff[512];
	int i;
	int size = 0;
	int len = 0;
	off_t begin = 0;
	off_t pos = 0;

	HBAptr = NULL;
	for (i = 0; i < 2; i++) {
		if ((HBAptr = atp_host[i]) != NULL) {
			if (HBAptr->host_no == hostno) {
				break;
			}
			HBAptr = NULL;
		}
	}

	if (HBAptr == NULL) {
		size += sprintf(BLS, "Can't find adapter for host number %d\n", hostno);
		len += size;
		pos = begin + len;
		size = 0;
		goto stop_output;
	}
	if (inout == TRUE) {	/* Has data been written to the file? */
		return (atp870u_set_info(buffer, length, HBAptr));
	}
	if (offset == 0) {
		memset(buff, 0, sizeof(buff));
	}
	size += sprintf(BLS, "ACARD AEC-671X Driver Version: 2.6+ac\n");
	len += size;
	pos = begin + len;
	size = 0;

	size += sprintf(BLS, "\n");
	size += sprintf(BLS, "Adapter Configuration:\n");
	size += sprintf(BLS, "               Base IO: %#.4lx\n", HBAptr->io_port);
	size += sprintf(BLS, "                   IRQ: %d\n", HBAptr->irq);
	len += size;
	pos = begin + len;
	size = 0;

stop_output:
	*start = buffer + (offset - begin);	/* Start of wanted data */
	len -= (offset - begin);	/* Start slop */
	if (len > length) {
		len = length;	/* Ending slop */
	}
	return (len);
}

#include "sd.h"

int atp870u_biosparam(Scsi_Disk * disk, kdev_t dev, int *ip)
{
	int heads, sectors, cylinders;

	heads = 64;
	sectors = 32;
	cylinders = disk->capacity / (heads * sectors);

	if (cylinders > 1024) {
		heads = 255;
		sectors = 63;
		cylinders = disk->capacity / (heads * sectors);
	}
	ip[0] = heads;
	ip[1] = sectors;
	ip[2] = cylinders;

	return 0;
}


int atp870u_release (struct Scsi_Host *pshost)
{
	int h;
	for (h = 0; h <= admaxu; h++)
	{
		if (pshost == atp_host[h]) {
			int k;
			free_irq (pshost->irq, &atp_unit[h]);
			release_region (pshost->io_port, pshost->n_io_port);
			scsi_unregister(pshost);
			for(k=0;k<16;k++)
				kfree(atp_unit[h].id[k].prd_tableu);
			return 0;
		}
	}
	panic("atp870u: bad scsi host passed.\n");

}
MODULE_LICENSE("GPL");

static Scsi_Host_Template driver_template = ATP870U;
#include "scsi_module.c"
