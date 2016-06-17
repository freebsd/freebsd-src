/* $Id: eicon_mod.c,v 1.1.4.1 2001/11/20 14:19:35 kai Exp $
 *
 * ISDN lowlevel-module for Eicon active cards.
 * 
 * Copyright 1997      by Fritz Elfert (fritz@isdn4linux.de)
 * Copyright 1998-2000 by Armin Schindler (mac@melware.de) 
 * Copyright 1999,2000 Cytronics & Melware (info@melware.de)
 * 
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 * Thanks to    Eicon Networks for
 *              documents, informations and hardware.
 *
 *		Deutsche Mailbox Saar-Lor-Lux GmbH
 *		for sponsoring and testing fax
 *		capabilities with Diva Server cards.
 *		(dor@deutschemailbox.de)
 *
 */

#define DRIVERNAME "Eicon active ISDN driver"
#define DRIVERRELEASE "2.0"
#define DRIVERPATCH ".16"


#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#ifdef CONFIG_MCA
#include <linux/mca.h>
#endif /* CONFIG_MCA */

#include "eicon.h"

#include "../avmb1/capicmd.h"  /* this should be moved in a common place */

#undef N_DATA
#include "adapter.h"
#include "uxio.h"

#define INCLUDE_INLINE_FUNCS

static eicon_card *cards = (eicon_card *) NULL;   /* glob. var , contains
                                                     start of card-list   */

static char *eicon_revision = "$Revision: 1.1.4.1 $";

extern char *eicon_pci_revision;
extern char *eicon_isa_revision;
extern char *eicon_idi_revision;

extern int do_ioctl(struct inode *pDivasInode, struct file *pDivasFile,
			unsigned int command, unsigned long arg);
extern void eicon_pci_init_conf(eicon_card *card);

#ifdef MODULE
#define MOD_USE_COUNT (GET_USE_COUNT (&__this_module))
#endif

#define EICON_CTRL_VERSION 2 

ulong DebugVar;

spinlock_t eicon_lock;

DESCRIPTOR idi_d[32];

/* Parameters to be set by insmod */
#ifdef CONFIG_ISDN_DRV_EICON_ISA
static int   membase      = -1;
static int   irq          = -1;
#endif
static char *id           = "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

MODULE_DESCRIPTION(             "ISDN4Linux: Driver for Eicon active ISDN cards");
MODULE_AUTHOR(                  "Armin Schindler");
MODULE_LICENSE(                 "GPL");
MODULE_PARM_DESC(id,   		"ID-String of first card");
MODULE_PARM(id,           	"s");
#ifdef CONFIG_ISDN_DRV_EICON_ISA
MODULE_PARM_DESC(membase,	"Base address of first ISA card");
MODULE_PARM_DESC(irq,    	"IRQ of first card");
MODULE_PARM(membase,    	"i");
MODULE_PARM(irq,          	"i");
#endif

char *eicon_ctype_name[] = {
        "ISDN-S",
        "ISDN-SX",
        "ISDN-SCOM",
        "ISDN-QUADRO",
        "ISDN-S2M",
        "DIVA Server BRI/PCI",
        "DIVA Server 4BRI/PCI",
        "DIVA Server 4BRI/PCI",
        "DIVA Server PRI/PCI"
};

static char *
eicon_getrev(const char *revision)
{
	char *rev;
	char *p;
	if ((p = strchr(revision, ':'))) {
		rev = p + 2;
		p = strchr(rev, '$');
		*--p = 0;
	} else rev = "?.??";
	return rev;

}

static eicon_chan *
find_channel(eicon_card *card, int channel)
{
	if ((channel >= 0) && (channel < card->nchannels))
        	return &(card->bch[channel]);
	eicon_log(card, 1, "eicon: Invalid channel %d\n", channel);
	return NULL;
}

#ifdef CONFIG_PCI
#ifdef CONFIG_ISDN_DRV_EICON_PCI
/*
 * Find pcicard with given card number 
 */
static inline eicon_card *
eicon_findnpcicard(int driverid)
{
        eicon_card *p = cards;

        while (p) {
                if ((p->regname[strlen(p->regname)-1] == (driverid + '0')) &&
			(p->bus == EICON_BUS_PCI))
                        return p;
                p = p->next;
        }
        return (eicon_card *) 0;
}
#endif
#endif /* CONFIG_PCI */

static void
eicon_rcv_dispatch(struct eicon_card *card)
{
	switch (card->bus) {
		case EICON_BUS_ISA:
		case EICON_BUS_MCA:
		case EICON_BUS_PCI:
			eicon_io_rcv_dispatch(card);
			break;
		default:
			eicon_log(card, 1,
			       "eicon_ack_dispatch: Illegal bustype %d\n", card->bus);
	}
}

static void
eicon_ack_dispatch(struct eicon_card *card)
{
	switch (card->bus) {
		case EICON_BUS_ISA:
		case EICON_BUS_MCA:
		case EICON_BUS_PCI:
			eicon_io_ack_dispatch(card);
			break;
		default:
			eicon_log(card, 1,
		       		"eicon_ack_dispatch: Illegal bustype %d\n", card->bus);
	}
}

static void
eicon_transmit(struct eicon_card *card)
{
	switch (card->bus) {
		case EICON_BUS_ISA:
		case EICON_BUS_MCA:
		case EICON_BUS_PCI:
			eicon_io_transmit(card);
			break;
		default:
			eicon_log(card, 1,
			       "eicon_transmit: Illegal bustype %d\n", card->bus);
	}
}

static int
eicon_command(eicon_card * card, isdn_ctrl * c)
{
        ulong a;
        eicon_chan *chan;
	eicon_cdef cdef;
#ifdef CONFIG_PCI
#ifdef CONFIG_ISDN_DRV_EICON_PCI
	dia_start_t dstart;
        int idi_length = 0;
#endif
#endif
	isdn_ctrl cmd;
	int ret = 0;
	unsigned long flags;
 
	eicon_log(card, 16, "eicon_cmd 0x%x with arg 0x%lx (0x%lx)\n",
		c->command, c->arg, (ulong) *c->parm.num);

        switch (c->command) {
		case ISDN_CMD_IOCTL:
			memcpy(&a, c->parm.num, sizeof(ulong));
			switch (c->arg) {
				case EICON_IOCTL_GETVER:
					return(EICON_CTRL_VERSION);
				case EICON_IOCTL_GETTYPE:
					if (card->bus == EICON_BUS_PCI) {
						copy_to_user((char *)a, &card->hwif.pci.master, sizeof(int));
					}
					return(card->type);
				case EICON_IOCTL_GETMMIO:
					switch (card->bus) {
						case EICON_BUS_ISA:
						case EICON_BUS_MCA:
							return (int)card->hwif.isa.shmem;
						default:
							eicon_log(card, 1,
							       "eicon: Illegal BUS type %d\n",
							       card->bus);
							ret = -ENODEV;
					}
#ifdef CONFIG_ISDN_DRV_EICON_ISA
				case EICON_IOCTL_SETMMIO:
					if (card->flags & EICON_FLAGS_LOADED)
						return -EBUSY;
					switch (card->bus) {
						case EICON_BUS_ISA:
							if (eicon_isa_find_card(a,
								card->hwif.isa.irq,
								card->regname) < 0)
								return -EFAULT;
							card->hwif.isa.shmem = (eicon_isa_shmem *)a;
							return 0;
						case EICON_BUS_MCA:
#if CONFIG_MCA
							if (eicon_mca_find_card(
								0, a,
								card->hwif.isa.irq,
								card->regname) < 0)
								return -EFAULT;
							card->hwif.isa.shmem = (eicon_isa_shmem *)a;
							return 0;
#endif /* CONFIG_MCA */
						default:
							eicon_log(card, 1,
						      		"eicon: Illegal BUS type %d\n",
							       card->bus);
							ret = -ENODEV;
					}					
#endif
				case EICON_IOCTL_GETIRQ:
					switch (card->bus) {
						case EICON_BUS_ISA:
						case EICON_BUS_MCA:
							return card->hwif.isa.irq;
						default:
							eicon_log(card, 1,
							       "eicon: Illegal BUS type %d\n",
							       card->bus);
							ret = -ENODEV;
					}
				case EICON_IOCTL_SETIRQ:
					if (card->flags & EICON_FLAGS_LOADED)
						return -EBUSY;
					if ((a < 2) || (a > 15))
						return -EFAULT;
					switch (card->bus) {
						case EICON_BUS_ISA:
						case EICON_BUS_MCA:
							card->hwif.isa.irq = a;
							return 0;
						default:
							eicon_log(card, 1,
						      		"eicon: Illegal BUS type %d\n",
							       card->bus);
							ret = -ENODEV;
					}					
#ifdef CONFIG_ISDN_DRV_EICON_ISA
				case EICON_IOCTL_LOADBOOT:
					if (card->flags & EICON_FLAGS_RUNNING)
						return -EBUSY;  
					switch (card->bus) {
						case EICON_BUS_ISA:
						case EICON_BUS_MCA:
							ret = eicon_isa_bootload(
								&(card->hwif.isa),
								&(((eicon_codebuf *)a)->isa));
							break;
						default:
							eicon_log(card, 1,
							       "eicon: Illegal BUS type %d\n",
							       card->bus);
							ret = -ENODEV;
					}
					return ret;
#endif
#ifdef CONFIG_ISDN_DRV_EICON_ISA
				case EICON_IOCTL_LOADISA:
					if (card->flags & EICON_FLAGS_RUNNING)
						return -EBUSY;  
					switch (card->bus) {
						case EICON_BUS_ISA:
						case EICON_BUS_MCA:
							ret = eicon_isa_load(
								&(card->hwif.isa),
								&(((eicon_codebuf *)a)->isa));
							if (!ret) {
                                                                card->flags |= EICON_FLAGS_LOADED;
                                                                card->flags |= EICON_FLAGS_RUNNING;
								if (card->hwif.isa.channels > 1) {
									cmd.command = ISDN_STAT_ADDCH;
									cmd.driver = card->myid;
									cmd.arg = card->hwif.isa.channels - 1;
									card->interface.statcallb(&cmd);
								}
								cmd.command = ISDN_STAT_RUN;    
								cmd.driver = card->myid;        
								cmd.arg = 0;                    
								card->interface.statcallb(&cmd);
							}
							break;
						default:
							eicon_log(card, 1,
							       "eicon: Illegal BUS type %d\n",
							       card->bus);
							ret = -ENODEV;
					}
					return ret;
#endif
				case EICON_IOCTL_MANIF:
					if (!card->flags & EICON_FLAGS_RUNNING)
						return -ENODEV;
					if (!card->d)
						return -ENODEV;
					if (!card->d->features & DI_MANAGE)
						return -ENODEV;
					ret = eicon_idi_manage(
						card, 
						(eicon_manifbuf *)a);
					return ret;

				case EICON_IOCTL_GETXLOG:
					return -ENODEV;

				case EICON_IOCTL_ADDCARD:
					if ((ret = copy_from_user(&cdef, (char *)a, sizeof(cdef))))
						return -EFAULT;
					if (!(eicon_addcard(0, cdef.membase, cdef.irq, cdef.id, 0)))
						return -EIO;
					return 0;
				case EICON_IOCTL_DEBUGVAR:
					DebugVar = a;
					eicon_log(card, 1, "Eicon: Debug Value set to %ld\n", DebugVar);
					return 0;
#ifdef MODULE
				case EICON_IOCTL_FREEIT:
					while (MOD_USE_COUNT > 0) MOD_DEC_USE_COUNT;
					MOD_INC_USE_COUNT;
					return 0;
#endif
				case EICON_IOCTL_LOADPCI:
					eicon_log(card, 1, "Eicon: Wrong version of load-utility,\n");
					eicon_log(card, 1, "Eicon: re-compile eiconctrl !\n");
					eicon_log(card, 1, "Eicon: Maybe update of utility is necessary !\n");
					return -EINVAL;
				default:	
#ifdef CONFIG_PCI
#ifdef CONFIG_ISDN_DRV_EICON_PCI
					if (c->arg < EICON_IOCTL_DIA_OFFSET)
						return -EINVAL;
					if (copy_from_user(&dstart, (char *)a, sizeof(dstart)))
						return -1;
					if (!(card = eicon_findnpcicard(dstart.card_id)))
						return -EINVAL;
					ret = do_ioctl(NULL, NULL,
						c->arg - EICON_IOCTL_DIA_OFFSET,
						(unsigned long) a);
					if (((c->arg - EICON_IOCTL_DIA_OFFSET)==DIA_IOCTL_START) && (!ret)) {
						if (card->type != EICON_CTYPE_MAESTRAQ) {
							DIVA_DIDD_Read(idi_d, sizeof(idi_d));
                                                        for(idi_length = 0; idi_length < 32; idi_length++) {
                                                          if (idi_d[idi_length].type == 0) break;
                                                        }
                                                        if ((idi_length < 1) || (idi_length >= 32)) {
					                  eicon_log(card, 1, "eicon: invalid idi table length.\n");
                                                          break;
                                                        }
							card->d = &idi_d[idi_length - 1];
							card->flags |= EICON_FLAGS_LOADED;
							card->flags |= EICON_FLAGS_RUNNING;
							eicon_pci_init_conf(card);
							if (card->d->channels > 1) {
								cmd.command = ISDN_STAT_ADDCH;
								cmd.driver = card->myid;
								cmd.arg = card->d->channels - 1;
								card->interface.statcallb(&cmd);
							}
							cmd.command = ISDN_STAT_RUN;    
							cmd.driver = card->myid;        
							cmd.arg = 0;                    
							card->interface.statcallb(&cmd);
							eicon_log(card, 1, "Eicon: %s started, %d channels (feat. 0x%x)\n",
								(card->type == EICON_CTYPE_MAESTRA) ? "BRI" : "PRI",
								card->d->channels, card->d->features);
						} else {
							int i;
							DIVA_DIDD_Read(idi_d, sizeof(idi_d));
                                                        for(idi_length = 0; idi_length < 32; idi_length++)
                                                          if (idi_d[idi_length].type == 0) break;
                                                        if ((idi_length < 1) || (idi_length >= 32)) {
					                  eicon_log(card, 1, "eicon: invalid idi table length.\n");
                                                          break;
                                                        }
        						for(i = 3; i >= 0; i--) {
								if (!(card = eicon_findnpcicard(dstart.card_id - i)))
									return -EINVAL;
	
								card->flags |= EICON_FLAGS_LOADED;
								card->flags |= EICON_FLAGS_RUNNING;
								card->d = &idi_d[idi_length - (i+1)];
								eicon_pci_init_conf(card);
								if (card->d->channels > 1) {
									cmd.command = ISDN_STAT_ADDCH;
									cmd.driver = card->myid;
									cmd.arg = card->d->channels - 1;
									card->interface.statcallb(&cmd);
								}
								cmd.command = ISDN_STAT_RUN;    
								cmd.driver = card->myid;        
								cmd.arg = 0;                    
								card->interface.statcallb(&cmd);
								eicon_log(card, 1, "Eicon: %d/4BRI started, %d channels (feat. 0x%x)\n",
									4-i, card->d->channels, card->d->features);
							}
						}
					}
					return ret;
#else
					return -EINVAL;
#endif
#endif /* CONFIG_PCI */
			}
			break;
		case ISDN_CMD_DIAL:
			if (!card->flags & EICON_FLAGS_RUNNING)
				return -ENODEV;
			if (!(chan = find_channel(card, c->arg & 0x1f)))
				break;
			spin_lock_irqsave(&eicon_lock, flags);
			if ((chan->fsm_state != EICON_STATE_NULL) && (chan->fsm_state != EICON_STATE_LISTEN)) {
				spin_unlock_irqrestore(&eicon_lock, flags);
				eicon_log(card, 1, "Dial on channel %d with state %d\n",
					chan->No, chan->fsm_state);
				return -EBUSY;
			}
			chan->fsm_state = EICON_STATE_OCALL;
			spin_unlock_irqrestore(&eicon_lock, flags);
			
			ret = idi_connect_req(card, chan, c->parm.setup.phone,
						     c->parm.setup.eazmsn,
						     c->parm.setup.si1,
						     c->parm.setup.si2);
			if (ret) {
				cmd.driver = card->myid;
				cmd.command = ISDN_STAT_DHUP;
				cmd.arg &= 0x1f;
				card->interface.statcallb(&cmd);
			}
			return ret;
		case ISDN_CMD_ACCEPTD:
			if (!card->flags & EICON_FLAGS_RUNNING)
				return -ENODEV;
			if (!(chan = find_channel(card, c->arg & 0x1f)))
				break;
			if (chan->fsm_state == EICON_STATE_ICALL) { 
				idi_connect_res(card, chan);
			}
			return 0;
		case ISDN_CMD_ACCEPTB:
			if (!card->flags & EICON_FLAGS_RUNNING)
				return -ENODEV;
			return 0;
		case ISDN_CMD_HANGUP:
			if (!card->flags & EICON_FLAGS_RUNNING)
				return -ENODEV;
			if (!(chan = find_channel(card, c->arg & 0x1f)))
				break;
			idi_hangup(card, chan);
			return 0;
		case ISDN_CMD_SETEAZ:
			if (!card->flags & EICON_FLAGS_RUNNING)
				return -ENODEV;
			if (!(chan = find_channel(card, c->arg & 0x1f)))
				break;
			chan->eazmask = 0x3ff;
			eicon_idi_listen_req(card, chan);
			return 0;
		case ISDN_CMD_CLREAZ:
			if (!card->flags & EICON_FLAGS_RUNNING)
				return -ENODEV;
			if (!(chan = find_channel(card, c->arg & 0x1f)))
				break;
			chan->eazmask = 0;
			eicon_idi_listen_req(card, chan);
			return 0;
		case ISDN_CMD_SETL2:
			if (!card->flags & EICON_FLAGS_RUNNING)
				return -ENODEV;
			if (!(chan = find_channel(card, c->arg & 0x1f)))
				break;
			chan->l2prot = (c->arg >> 8);
			return 0;
		case ISDN_CMD_GETL2:
			if (!card->flags & EICON_FLAGS_RUNNING)
				return -ENODEV;
			if (!(chan = find_channel(card, c->arg & 0x1f)))
				break;
			return chan->l2prot;
		case ISDN_CMD_SETL3:
			if (!card->flags & EICON_FLAGS_RUNNING)
				return -ENODEV;
			if (!(chan = find_channel(card, c->arg & 0x1f)))
				break;
			chan->l3prot = (c->arg >> 8);
#ifdef CONFIG_ISDN_TTY_FAX
			if (chan->l3prot == ISDN_PROTO_L3_FCLASS2) {
				chan->fax = c->parm.fax;
				eicon_log(card, 128, "idi_cmd: Ch%d: SETL3 struct fax=0x%x\n",chan->No, chan->fax);
			}
#endif
			return 0;
		case ISDN_CMD_GETL3:
			if (!card->flags & EICON_FLAGS_RUNNING)
				return -ENODEV;
			if (!(chan = find_channel(card, c->arg & 0x1f)))
				break;
			return chan->l3prot;
		case ISDN_CMD_GETEAZ:
			if (!card->flags & EICON_FLAGS_RUNNING)
				return -ENODEV;
			eicon_log(card, 1, "eicon CMD_GETEAZ not implemented\n");
			return 0;
		case ISDN_CMD_SETSIL:
			if (!card->flags & EICON_FLAGS_RUNNING)
				return -ENODEV;
			eicon_log(card, 1, "eicon CMD_SETSIL not implemented\n");
			return 0;
		case ISDN_CMD_GETSIL:
			if (!card->flags & EICON_FLAGS_RUNNING)
				return -ENODEV;
			eicon_log(card, 1, "eicon CMD_GETSIL not implemented\n");
			return 0;
		case ISDN_CMD_LOCK:
			MOD_INC_USE_COUNT;
			return 0;
		case ISDN_CMD_UNLOCK:
			MOD_DEC_USE_COUNT;
			return 0;
#ifdef CONFIG_ISDN_TTY_FAX
		case ISDN_CMD_FAXCMD:
			if (!card->flags & EICON_FLAGS_RUNNING)
				return -ENODEV;
			if (!(chan = find_channel(card, c->arg & 0x1f)))
				break;
			if (!chan->fax)
				break;
			idi_fax_cmd(card, chan);
			return 0;
#endif
		case ISDN_CMD_AUDIO:
			if (!card->flags & EICON_FLAGS_RUNNING)
				return -ENODEV;
			if (!(chan = find_channel(card, c->arg & 0x1f)))
				break;
			idi_audio_cmd(card, chan, c->arg >> 8, c->parm.num);
			return 0;
		case CAPI_PUT_MESSAGE:
			if (!card->flags & EICON_FLAGS_RUNNING)
				return -ENODEV;
			if (!(chan = find_channel(card, c->arg & 0x1f)))
				break;
			if (c->parm.cmsg.Length < 8)
				break;
			switch(c->parm.cmsg.Command) {
				case CAPI_FACILITY:
					if (c->parm.cmsg.Subcommand == CAPI_REQ)
						return(capipmsg(card, chan, &c->parm.cmsg));
					break;
				case CAPI_MANUFACTURER:
				default:
					break;
			}
			return 0;
        }
	
        return -EINVAL;
}

/*
 * Find card with given driverId
 */
static inline eicon_card *
eicon_findcard(int driverid)
{
        eicon_card *p = cards;

        while (p) {
                if (p->myid == driverid)
                        return p;
                p = p->next;
        }
        return (eicon_card *) 0;
}

/*
 * Wrapper functions for interface to linklevel
 */
static int
if_command(isdn_ctrl * c)
{
        eicon_card *card = eicon_findcard(c->driver);

        if (card)
                return (eicon_command(card, c));
        printk(KERN_ERR
             "eicon: if_command %d called with invalid driverId %d!\n",
               c->command, c->driver);
        return -ENODEV;
}

static int
if_writecmd(const u_char * buf, int len, int user, int id, int channel)
{
        return (len);
}

static int
if_readstatus(u_char * buf, int len, int user, int id, int channel)
{
	int count = 0;
	int cnt = 0;
	ulong flags = 0;
	u_char *p = buf;
	struct sk_buff *skb;

        eicon_card *card = eicon_findcard(id);
	
        if (card) {
                if (!card->flags & EICON_FLAGS_RUNNING)
                        return -ENODEV;
	
		spin_lock_irqsave(&eicon_lock, flags);
		while((skb = skb_dequeue(&card->statq))) {

			if ((skb->len + count) > len)
				cnt = len - count;
			else
				cnt = skb->len;

			if (user) {
				spin_unlock_irqrestore(&eicon_lock, flags);
				copy_to_user(p, skb->data, cnt);
				spin_lock_irqsave(&eicon_lock, flags);
			}
			else
				memcpy(p, skb->data, cnt);

			count += cnt;
			p += cnt;

			if (cnt == skb->len) {
				dev_kfree_skb(skb);
				if (card->statq_entries > 0)
					card->statq_entries--;
			} else {
				skb_pull(skb, cnt);
				skb_queue_head(&card->statq, skb);
				spin_unlock_irqrestore(&eicon_lock, flags);
				return count;
			}
		}
		card->statq_entries = 0;
		spin_unlock_irqrestore(&eicon_lock, flags);
		return count;
        }
        printk(KERN_ERR
               "eicon: if_readstatus called with invalid driverId!\n");
        return 0;
}

static int
if_sendbuf(int id, int channel, int ack, struct sk_buff *skb)
{
        eicon_card *card = eicon_findcard(id);
	eicon_chan *chan;
	int ret = 0;
	int len;

	len = skb->len;
	
        if (card) {
                if (!card->flags & EICON_FLAGS_RUNNING)
                        return -ENODEV;
        	if (!(chan = find_channel(card, channel)))
			return -ENODEV;

		if (chan->fsm_state == EICON_STATE_ACTIVE) {
#ifdef CONFIG_ISDN_TTY_FAX
			if (chan->l2prot == ISDN_PROTO_L2_FAX) {
				if ((ret = idi_faxdata_send(card, chan, skb)) > 0)
					ret = len;
			}
			else
#endif
				ret = idi_send_data(card, chan, ack, skb, 1, 1);
			return (ret);
		} else {
			return -ENODEV;
		}
        }
        printk(KERN_ERR
               "eicon: if_sendbuf called with invalid driverId!\n");
        return -ENODEV;
}

/* jiftime() copied from HiSax */
static inline int jiftime(char *s, long mark)
{
        s += 8;

        *s-- = '\0';
        *s-- = mark % 10 + '0';
        mark /= 10;
        *s-- = mark % 10 + '0';
        mark /= 10;
        *s-- = '.';
        *s-- = mark % 10 + '0';
        mark /= 10;
        *s-- = mark % 6 + '0';
        mark /= 6;
        *s-- = ':';
        *s-- = mark % 10 + '0';
        mark /= 10;
        *s-- = mark % 10 + '0';
        return(8);
}

void
eicon_putstatus(eicon_card * card, char * buf)
{
	ulong flags;
	int count;
	isdn_ctrl cmd;
	u_char *p;
	struct sk_buff *skb;

	if (!card) {
		if (!(card = cards))
			return;
	}

	spin_lock_irqsave(&eicon_lock, flags);
	count = strlen(buf);
	skb = alloc_skb(count, GFP_ATOMIC);
	if (!skb) {
		spin_unlock_irqrestore(&eicon_lock, flags);
		printk(KERN_ERR "eicon: could not alloc skb in putstatus\n");
		return;
	}
	p = skb_put(skb, count);
	memcpy(p, buf, count);

	skb_queue_tail(&card->statq, skb);

	if (card->statq_entries >= MAX_STATUS_BUFFER) {
		if ((skb = skb_dequeue(&card->statq))) {
			count -= skb->len;
			dev_kfree_skb(skb);
		} else
			count = 0;
	} else
		card->statq_entries++;

	spin_unlock_irqrestore(&eicon_lock, flags);
        if (count) {
                cmd.command = ISDN_STAT_STAVAIL;
                cmd.driver = card->myid;
                cmd.arg = count;
		card->interface.statcallb(&cmd);
        }
}

/*
 * Debug and Log 
 */
void
eicon_log(eicon_card * card, int level, const char *fmt, ...)
{
	va_list args;
	char Line[160];
	u_char *p;


	if ((DebugVar & level) || (DebugVar & 256)) {
		va_start(args, fmt);

		if (DebugVar & level) {
			if (DebugVar & 256) {
				/* log-buffer */
				p = Line;
				p += jiftime(p, jiffies);
				*p++ = 32;
				p += vsprintf(p, fmt, args);
				*p = 0;	
				eicon_putstatus(card, Line);
			} else {
				/* printk, syslogd */
				vsprintf(Line, fmt, args);
				printk(KERN_DEBUG "%s", Line);
			}
		}

		va_end(args);
	}
}


/*
 * Allocate a new card-struct, initialize it
 * link it into cards-list.
 */
static void
eicon_alloccard(int Type, int membase, int irq, char *id, int card_id)
{
	int i;
	int j;
	int qloop;
#ifdef CONFIG_ISDN_DRV_EICON_ISA
	char qid[5];
#endif
        eicon_card *card;

	qloop = (Type == EICON_CTYPE_QUADRO)?2:0;
	for (i = 0; i <= qloop; i++) {
		if (!(card = (eicon_card *) kmalloc(sizeof(eicon_card), GFP_KERNEL))) {
			eicon_log(card, 1,
			       "eicon: (%s) Could not allocate card-struct.\n", id);
			return;
		}
		memset((char *) card, 0, sizeof(eicon_card));
		skb_queue_head_init(&card->sndq);
		skb_queue_head_init(&card->rcvq);
		skb_queue_head_init(&card->rackq);
		skb_queue_head_init(&card->sackq);
		skb_queue_head_init(&card->statq);
		card->statq_entries = 0;
		card->snd_tq.routine = (void *) (void *) eicon_transmit;
		card->snd_tq.data = card;
		card->rcv_tq.routine = (void *) (void *) eicon_rcv_dispatch;
		card->rcv_tq.data = card;
		card->ack_tq.routine = (void *) (void *) eicon_ack_dispatch;
		card->ack_tq.data = card;
		card->interface.maxbufsize = 4000;
		card->interface.command = if_command;
		card->interface.writebuf_skb = if_sendbuf;
		card->interface.writecmd = if_writecmd;
		card->interface.readstat = if_readstatus;
		card->interface.features =
			ISDN_FEATURE_L2_X75I |
			ISDN_FEATURE_L2_HDLC |
			ISDN_FEATURE_L2_TRANS |
			ISDN_FEATURE_L3_TRANS |
			ISDN_FEATURE_P_UNKNOWN;
		card->interface.hl_hdrlen = 20;
		card->ptype = ISDN_PTYPE_UNKNOWN;
		strncpy(card->interface.id, id, sizeof(card->interface.id) - 1);
		card->myid = -1;
		card->type = Type;
		switch (Type) {
#ifdef CONFIG_ISDN_DRV_EICON_ISA
#if CONFIG_MCA /* only needed for MCA */
                        case EICON_CTYPE_S:
                        case EICON_CTYPE_SX:
                        case EICON_CTYPE_SCOM:
				if (MCA_bus) {
	                                if (membase == -1)
        	                                membase = EICON_ISA_MEMBASE;
                	                if (irq == -1)
                        	                irq = EICON_ISA_IRQ;
	                                card->bus = EICON_BUS_MCA;
        	                        card->hwif.isa.card = (void *)card;
                	                card->hwif.isa.shmem = (eicon_isa_shmem *)membase;
					card->hwif.isa.physmem = (unsigned long)membase;
                        	        card->hwif.isa.master = 1;

	                                card->hwif.isa.irq = irq;
        	                        card->hwif.isa.type = Type;
                	                card->nchannels = 2;
                        	        card->interface.channels = 1;
				} else {
					printk(KERN_WARNING
						"eicon (%s): no MCA bus detected.\n",
						card->interface.id);
					kfree(card);
					return;
				}
                                break;
#endif /* CONFIG_MCA */
			case EICON_CTYPE_QUADRO:
				if (membase == -1)
					membase = EICON_ISA_MEMBASE;
				if (irq == -1)
					irq = EICON_ISA_IRQ;
                                card->bus = EICON_BUS_ISA;
				card->hwif.isa.card = (void *)card;
				card->hwif.isa.shmem = (eicon_isa_shmem *)(membase + (i+1) * EICON_ISA_QOFFSET);
				card->hwif.isa.physmem = (unsigned long)(membase + (i+1) * EICON_ISA_QOFFSET);
				card->hwif.isa.master = 0;
				strcpy(card->interface.id, id);
				if (id[strlen(id) - 1] == 'a') {
					card->interface.id[strlen(id) - 1] = 'a' + i + 1;
				} else {
					sprintf(qid, "_%c",'2' + i);
					strcat(card->interface.id, qid);
				}
				printk(KERN_INFO "Eicon: Quadro: Driver-Id %s added.\n",
					card->interface.id);
				if (i == 0) {
					eicon_card *p = cards;
					while(p) {
						if ((p->hwif.isa.master) && (p->hwif.isa.irq == irq)) {
							p->qnext = card;
							break;
						}
						p = p->next;
					}
					if (!p) {
						eicon_log(card, 1, "eicon_alloccard: Quadro Master not found.\n");
						kfree(card);
						return;
					}
				} else {
					cards->qnext = card;
				}
				card->hwif.isa.irq = irq;
				card->hwif.isa.type = Type;
				card->nchannels = 2;
				card->interface.channels = 1;
				break;
#endif
#ifdef CONFIG_PCI
#ifdef CONFIG_ISDN_DRV_EICON_PCI
			case EICON_CTYPE_MAESTRA:
                                card->bus = EICON_BUS_PCI;
				card->interface.features |=
					ISDN_FEATURE_L2_V11096 |
					ISDN_FEATURE_L2_V11019 |
					ISDN_FEATURE_L2_V11038 |
					ISDN_FEATURE_L2_MODEM |
					ISDN_FEATURE_L2_FAX | 
					ISDN_FEATURE_L3_TRANSDSP |
					ISDN_FEATURE_L3_FCLASS2;
                                card->hwif.pci.card = (void *)card;
                                card->hwif.pci.master = card_id;
                                card->hwif.pci.irq = irq;
                                card->hwif.pci.type = Type;
				card->flags = 0;
                                card->nchannels = 2;
				card->interface.channels = 1;
				break;

			case EICON_CTYPE_MAESTRAQ:
                                card->bus = EICON_BUS_PCI;
				card->interface.features |=
					ISDN_FEATURE_L2_V11096 |
					ISDN_FEATURE_L2_V11019 |
					ISDN_FEATURE_L2_V11038 |
					ISDN_FEATURE_L2_MODEM |
					ISDN_FEATURE_L2_FAX | 
					ISDN_FEATURE_L3_TRANSDSP |
					ISDN_FEATURE_L3_FCLASS2;
                                card->hwif.pci.card = (void *)card;
                                card->hwif.pci.master = card_id;
                                card->hwif.pci.irq = irq;
                                card->hwif.pci.type = Type;
				card->flags = 0;
                                card->nchannels = 2;
				card->interface.channels = 1;
				break;

			case EICON_CTYPE_MAESTRAP:
                                card->bus = EICON_BUS_PCI;
				card->interface.features |=
					ISDN_FEATURE_L2_V11096 |
					ISDN_FEATURE_L2_V11019 |
					ISDN_FEATURE_L2_V11038 |
					ISDN_FEATURE_L2_MODEM |
					ISDN_FEATURE_L2_FAX |
					ISDN_FEATURE_L3_TRANSDSP |
					ISDN_FEATURE_L3_FCLASS2;
                                card->hwif.pci.card = (void *)card;
                                card->hwif.pci.master = card_id;
                                card->hwif.pci.irq = irq;
                                card->hwif.pci.type = Type;
				card->flags = 0;
                                card->nchannels = 30;
				card->interface.channels = 1;
				break;
#endif
#endif
#ifdef CONFIG_ISDN_DRV_EICON_ISA
			case EICON_CTYPE_ISABRI:
				if (membase == -1)
					membase = EICON_ISA_MEMBASE;
				if (irq == -1)
					irq = EICON_ISA_IRQ;
				card->bus = EICON_BUS_ISA;
				card->hwif.isa.card = (void *)card;
				card->hwif.isa.shmem = (eicon_isa_shmem *)membase;
				card->hwif.isa.physmem = (unsigned long)membase;
				card->hwif.isa.master = 1;
				card->hwif.isa.irq = irq;
				card->hwif.isa.type = Type;
				card->nchannels = 2;
				card->interface.channels = 1;
				break;
			case EICON_CTYPE_ISAPRI:
				if (membase == -1)
					membase = EICON_ISA_MEMBASE;
				if (irq == -1)
					irq = EICON_ISA_IRQ;
                                card->bus = EICON_BUS_ISA;
				card->hwif.isa.card = (void *)card;
				card->hwif.isa.shmem = (eicon_isa_shmem *)membase;
				card->hwif.isa.physmem = (unsigned long)membase;
				card->hwif.isa.master = 1;
				card->hwif.isa.irq = irq;
				card->hwif.isa.type = Type;
				card->nchannels = 30;
				card->interface.channels = 1;
				break;
#endif
			default:
				eicon_log(card, 1, "eicon_alloccard: Invalid type %d\n", Type);
				kfree(card);
				return;
		}
		if (!(card->bch = (eicon_chan *) kmalloc(sizeof(eicon_chan) * (card->nchannels + 1)
							 , GFP_KERNEL))) {
			eicon_log(card, 1,
			       "eicon: (%s) Could not allocate bch-struct.\n", id);
			kfree(card);
			return;
		}
		for (j=0; j< (card->nchannels + 1); j++) {
			memset((char *)&card->bch[j], 0, sizeof(eicon_chan));
			card->bch[j].statectrl = 0;
			card->bch[j].l2prot = ISDN_PROTO_L2_X75I;
			card->bch[j].l3prot = ISDN_PROTO_L3_TRANS;
			card->bch[j].e.D3Id = 0;
			card->bch[j].e.B2Id = 0;
			card->bch[j].e.Req = 0;
			card->bch[j].No = j;
			card->bch[j].tskb1 = NULL;
			card->bch[j].tskb2 = NULL;
			skb_queue_head_init(&card->bch[j].e.X);
			skb_queue_head_init(&card->bch[j].e.R);
		}

#ifdef CONFIG_ISDN_DRV_EICON_PCI
		/* *** Diva Server *** */
		if (!(card->dbuf = (DBUFFER *) kmalloc((sizeof(DBUFFER) * (card->nchannels + 1))*2
							 , GFP_KERNEL))) {
			eicon_log(card, 1,
			       "eicon: (%s) Could not allocate DBUFFER-struct.\n", id);
			kfree(card);
			kfree(card->bch);
			return;
		}
		if (!(card->sbuf = (BUFFERS *) kmalloc((sizeof(BUFFERS) * (card->nchannels + 1)) * 2, GFP_KERNEL))) {
			eicon_log(card, 1,
			       "eicon: (%s) Could not allocate BUFFERS-struct.\n", id);
			kfree(card);
			kfree(card->bch);
			kfree(card->dbuf);
			return;
		}
		if (!(card->sbufp = (char *) kmalloc((270 * (card->nchannels + 1)) * 2, GFP_KERNEL))) {
			eicon_log(card, 1,
			       "eicon: (%s) Could not allocate BUFFERSP-struct.\n", id);
			kfree(card);
			kfree(card->bch);
			kfree(card->dbuf);
			kfree(card->sbuf);
			return;
		}
		for (j=0; j< (card->nchannels + 1); j++) {
			memset((char *)&card->dbuf[j], 0, sizeof(DBUFFER));
			card->bch[j].de.RBuffer = (DBUFFER *)&card->dbuf[j];
			memset((char *)&card->dbuf[j+(card->nchannels+1)], 0, sizeof(BUFFERS));
			card->bch[j].be.RBuffer = (DBUFFER *)&card->dbuf[j+(card->nchannels+1)];

			memset((char *)&card->sbuf[j], 0, sizeof(BUFFERS));
			card->bch[j].de.X = (BUFFERS *)&card->sbuf[j];
			memset((char *)&card->sbuf[j+(card->nchannels+1)], 0, sizeof(BUFFERS));
			card->bch[j].be.X = (BUFFERS *)&card->sbuf[j+(card->nchannels+1)];

			memset((char *)&card->sbufp[j], 0, 270);
			card->bch[j].de.X->P = (char *)&card->sbufp[j * 270];
			memset((char *)&card->sbufp[j+(card->nchannels+1)], 0, 270);
			card->bch[j].be.X->P = (char *)&card->sbufp[(j+(card->nchannels+1)) * 270];
		}
		/* *** */
#endif /* CONFIG_ISDN_DRV_EICON_PCI */

		card->next = cards;
		cards = card;
	}
}

/*
 * register card at linklevel
 */
static int
eicon_registercard(eicon_card * card)
{
        switch (card->bus) {
#ifdef CONFIG_ISDN_DRV_EICON_ISA
		case EICON_BUS_ISA:
			/* TODO something to print */
			break;
#ifdef CONFIG_MCA
		case EICON_BUS_MCA:
			eicon_isa_printpar(&card->hwif.isa);
			break;
#endif /* CONFIG_MCA */
#endif
		case EICON_BUS_PCI:
			break;
		default:
			eicon_log(card, 1,
			       "eicon_registercard: Illegal BUS type %d\n",
			       card->bus);
			return -1;
        }
        if (!register_isdn(&card->interface)) {
                printk(KERN_WARNING
                       "eicon_registercard: Unable to register %s\n",
                       card->interface.id);
                return -1;
        }
        card->myid = card->interface.channels;
        sprintf(card->regname, "%s", card->interface.id);
        return 0;
}

static void __exit
unregister_card(eicon_card * card)
{
        isdn_ctrl cmd;

        cmd.command = ISDN_STAT_UNLOAD;
        cmd.driver = card->myid;
        card->interface.statcallb(&cmd);
        switch (card->bus) {
#ifdef CONFIG_ISDN_DRV_EICON_ISA
		case EICON_BUS_ISA:
#ifdef CONFIG_MCA
		case EICON_BUS_MCA:
#endif /* CONFIG_MCA */
			eicon_isa_release(&card->hwif.isa);
			break;
#endif
		case EICON_BUS_PCI:
			break;
		default:
			eicon_log(card, 1,
			       "eicon: Invalid BUS type %d\n",
			       card->bus);
			break;
        }
}

static void
eicon_freecard(eicon_card *card) {
	int i;

	for(i = 0; i < (card->nchannels + 1); i++) {
		skb_queue_purge(&card->bch[i].e.X);
		skb_queue_purge(&card->bch[i].e.R);
	}
	skb_queue_purge(&card->sndq);
	skb_queue_purge(&card->rcvq);
	skb_queue_purge(&card->rackq);
	skb_queue_purge(&card->sackq);
	skb_queue_purge(&card->statq);

#ifdef CONFIG_ISDN_DRV_EICON_PCI
	kfree(card->sbufp);
	kfree(card->sbuf);
	kfree(card->dbuf);
#endif
	kfree(card->bch);
	kfree(card);
}

int
eicon_addcard(int Type, int membase, int irq, char *id, int card_id)
{
	eicon_card *p;
	eicon_card *q = NULL;
	int registered;
	int added = 0;
	int failed = 0;

#ifdef CONFIG_ISDN_DRV_EICON_ISA
	if (!Type) /* ISA */
		if ((Type = eicon_isa_find_card(membase, irq, id)) < 0)
			return 0;
#endif
	eicon_alloccard(Type, membase, irq, id, card_id);
        p = cards;
        while (p) {
		registered = 0;
		if (!p->interface.statcallb) {
			/* Not yet registered.
			 * Try to register and activate it.
			 */
			added++;
			switch (p->bus) {
#ifdef CONFIG_ISDN_DRV_EICON_ISA
				case EICON_BUS_ISA:
				case EICON_BUS_MCA:
					if (eicon_registercard(p))
						break;
					registered = 1;
					break;
#endif
				case EICON_BUS_PCI:
#ifdef CONFIG_PCI
#ifdef CONFIG_ISDN_DRV_EICON_PCI
					if (eicon_registercard(p))
						break;
					registered = 1;
					break;
#endif
#endif
				default:
					printk(KERN_ERR
					       "eicon: addcard: Invalid BUS type %d\n",
					       p->bus);
			}
		} else
			/* Card already registered */
			registered = 1;
                if (registered) {
			/* Init OK, next card ... */
                        q = p;
                        p = p->next;
                } else {
                        /* registering failed, remove card from list, free memory */
                        printk(KERN_ERR
                               "eicon: Initialization of %s failed\n",
                               p->interface.id);
                        if (q) {
                                q->next = p->next;
                                eicon_freecard(p);
                                p = q->next;
                        } else {
                                cards = p->next;
                                eicon_freecard(p);
                                p = cards;
                        }
			failed++;
                }
	}
        return (added - failed);
}


static int __init
eicon_init(void)
{
	int card_count = 0;
	char tmprev[50];

	DebugVar = 1;
	eicon_lock = (spinlock_t) SPIN_LOCK_UNLOCKED;

        printk(KERN_INFO "%s Rev: ", DRIVERNAME);
	strcpy(tmprev, eicon_revision);
	printk("%s/", eicon_getrev(tmprev));
	strcpy(tmprev, eicon_pci_revision);
#ifdef CONFIG_ISDN_DRV_EICON_PCI
	printk("%s/", eicon_getrev(tmprev));
#else
	printk("---/");
#endif
	strcpy(tmprev, eicon_isa_revision);
#ifdef CONFIG_ISDN_DRV_EICON_ISA
	printk("%s/", eicon_getrev(tmprev));
#else
	printk("---/");
#endif
	strcpy(tmprev, eicon_idi_revision);
	printk("%s\n", eicon_getrev(tmprev));
        printk(KERN_INFO "%s Release: %s%s\n", DRIVERNAME,
		DRIVERRELEASE, DRIVERPATCH);

#ifdef CONFIG_ISDN_DRV_EICON_ISA
#ifdef CONFIG_MCA
	/* Check if we have MCA-bus */
        if (!MCA_bus)
                {
                printk(KERN_INFO
                        "eicon: No MCA bus, ISDN-interfaces  not probed.\n");
        } else {
		eicon_log(NULL, 8,
			"eicon_mca_find_card, irq=%d.\n", 
				irq);
               	if (!eicon_mca_find_card(0, membase, irq, id))
                       card_count++;
        };
#else
	card_count = eicon_addcard(0, membase, irq, id, 0);
#endif /* CONFIG_MCA */
#endif /* CONFIG_ISDN_DRV_EICON_ISA */
 
#ifdef CONFIG_PCI
#ifdef CONFIG_ISDN_DRV_EICON_PCI
	DivasCardsDiscover();
	card_count += eicon_pci_find_card(id);
#endif
#endif

        if (!cards) {
#ifdef MODULE
#ifndef CONFIG_ISDN_DRV_EICON_PCI
#ifndef CONFIG_ISDN_DRV_EICON_ISA
                printk(KERN_INFO "Eicon: Driver is neither ISA nor PCI compiled !\n");
                printk(KERN_INFO "Eicon: Driver not loaded !\n");
#else
                printk(KERN_INFO "Eicon: No cards defined, driver not loaded !\n");
#endif
#else
                printk(KERN_INFO "Eicon: No PCI-cards found, driver not loaded !\n");
#endif
#endif /* MODULE */
		return -ENODEV;

	} else
		printk(KERN_INFO "Eicon: %d card%s added\n", card_count, 
                       (card_count>1)?"s":"");
        return 0;
}

#ifdef CONFIG_ISDN_DRV_EICON_PCI
void DIVA_DIDD_Write(DESCRIPTOR *, int);
EXPORT_SYMBOL_NOVERS(DIVA_DIDD_Read);
EXPORT_SYMBOL_NOVERS(DIVA_DIDD_Write);
EXPORT_SYMBOL_NOVERS(DivasPrintf);
#else
int DivasCardNext;
card_t DivasCards[1];
#endif

static void __exit
eicon_exit(void)
{
#if CONFIG_PCI	
#ifdef CONFIG_ISDN_DRV_EICON_PCI
	card_t *pCard;
	word wCardIndex;
	extern int Divas_major;
	int iTmp = 0;
#endif
#endif
	
        eicon_card *card = cards;
        eicon_card *last;

        while (card) {
#ifdef CONFIG_ISDN_DRV_EICON_ISA
#ifdef CONFIG_MCA
        	if (MCA_bus)
                        {
                        mca_mark_as_unused (card->mca_slot);
                        mca_set_adapter_procfn(card->mca_slot, NULL, NULL);
                        };
#endif /* CONFIG_MCA */
#endif
                unregister_card(card); 
                card = card->next;
        }
        card = cards;
        while (card) {
                last = card;
                card = card->next;
		eicon_freecard(last);
        }

#if CONFIG_PCI	
#ifdef CONFIG_ISDN_DRV_EICON_PCI
	pCard = DivasCards;
	for (wCardIndex = 0; wCardIndex < MAX_CARDS; wCardIndex++)
	{
		if ((pCard->hw) && (pCard->hw->in_use))
		{
			(*pCard->card_reset)(pCard);
			
			UxIsrRemove(pCard->hw, pCard);
			UxCardHandleFree(pCard->hw);

			if(pCard->e_tbl != NULL)
			{
				kfree(pCard->e_tbl);
			}

			if(pCard->hw->card_type == DIA_CARD_TYPE_DIVA_SERVER_B)
			{
				release_region(pCard->hw->io_base,0x20);
				release_region(pCard->hw->reset_base,0x80);
			}

                        // If this is a 4BRI ...
                        if (pCard->hw->card_type == DIA_CARD_TYPE_DIVA_SERVER_Q)
                        {
                                // Skip over the next 3 virtual adapters
                                wCardIndex += 3;

                                // But free their handles
				for (iTmp = 0; iTmp < 3; iTmp++)
				{
					pCard++;
					UxCardHandleFree(pCard->hw);

					if(pCard->e_tbl != NULL)
					{
						kfree(pCard->e_tbl);
					}
				}
                        }
		}
		pCard++;
	}
	unregister_chrdev(Divas_major, "Divas");
#endif
#endif /* CONFIG_PCI */
        printk(KERN_INFO "%s unloaded\n", DRIVERNAME);
}

#ifndef MODULE

static int __init
eicon_setup(char *line)
{
        int i, argc;
	int ints[5];
	char *str;

	str = get_options(line, 4, ints);

        argc = ints[0];
        i = 1;
#ifdef CONFIG_ISDN_DRV_EICON_ISA
        if (argc) {
		membase = irq = -1;
		if (argc) {
			membase = ints[i];
			i++;
			argc--;
		}
		if (argc) {
			irq = ints[i];
			i++;
			argc--;
		}
		if (strlen(str)) {
			strcpy(id, str);
		} else {
			strcpy(id, "eicon");
		} 
       		printk(KERN_INFO "Eicon ISDN active driver setup (id=%s membase=0x%x irq=%d)\n",
			id, membase, irq);
	}
#else
	printk(KERN_INFO "Eicon ISDN active driver setup\n");
#endif
	return(1);
}
__setup("eicon=", eicon_setup);

#endif /* MODULE */

#ifdef CONFIG_ISDN_DRV_EICON_ISA
#ifdef CONFIG_MCA

struct eicon_mca_adapters_struct {
	char * name;
	int adf_id;
};
/* possible MCA-brands of eicon cards                                         */
struct eicon_mca_adapters_struct eicon_mca_adapters[] = {
	{ "ISDN-P/2 Adapter", 0x6abb },
	{ "ISDN-[S|SX|SCOM]/2 Adapter", 0x6a93 },
	{ "DIVA /MCA", 0x6336 },
	{ NULL, 0 },
};

int eicon_mca_find_card(int type,          /* type-idx of eicon-card          */
                        int membase,
		        int irq,
			char * id)         /* name of eicon-isdn-dev          */
{
	int j, curr_slot = 0;

       	eicon_log(NULL, 8,
		"eicon_mca_find_card type: %d, membase: %#x, irq %d \n",
		type, membase, irq);
	/* find a no-driver-assigned eicon card                               */
	for (j=0; eicon_mca_adapters[j].adf_id != 0; j++) 
		{
		for ( curr_slot=0; curr_slot<=MCA_MAX_SLOT_NR; curr_slot++) 
			{
			curr_slot = mca_find_unused_adapter(
				         eicon_mca_adapters[j].adf_id, curr_slot);
			if (curr_slot != MCA_NOTFOUND) 
				{
				/* check if pre-set parameters match
				   these of the card, check cards memory      */
				if (!(int) eicon_mca_probe(curr_slot,
                                                           j,
                                                	   membase, 
                                                           irq,
                                                           id))
					{
					return 0;
					/* means: adapter parms did match     */
					};
			};
			break;
			/* MCA_NOTFOUND-branch: no matching adapter of
			   THIS flavor found, next flavor                     */

            	};
	};
	/* all adapter flavors checked without match, finito with:            */
        return -ENODEV;
};


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *  stolen from 3c523.c/elmc_getinfo, ewe, 10.5.1999 
 */
int eicon_info(char * buf, int slot, void *d)
{
	int len = 0;
	struct eicon_card *dev;

        dev = (struct eicon_card *) d;

	if (dev == NULL)
		return len;
	len += sprintf(buf+len, "eicon ISDN adapter, type %d.\n",dev->type);
	len += sprintf(buf+len, "IRQ: %d\n", dev->hwif.isa.irq);
	len += sprintf(buf+len, "MEMBASE: %#lx\n", (unsigned long)dev->hwif.isa.shmem);

	return len;
};

int eicon_mca_probe(int slot,  /* slot-nr where the card was detected         */
		    int a_idx, /* idx-nr of probed card in eicon_mca_adapters */
                    int membase,
                    int irq,
		    char * id) /* name of eicon-isdn-dev                      */
{				
	unsigned char adf_pos0;
	int cards_irq, cards_membase, cards_io;
	int type = EICON_CTYPE_S;
	int irq_array[]={0,3,4,2};
	int irq_array1[]={3,4,0,0,2,10,11,12};

        adf_pos0 = mca_read_stored_pos(slot,2);
	eicon_log(NULL, 8,
		"eicon_mca_probe irq=%d, membase=%d\n", 
		irq,
		membase);
	switch (a_idx) {
		case 0:                /* P/2-Adapter (== PRI/S2M ? )         */
			cards_membase= 0xC0000+((adf_pos0>>4)*0x4000);
			if (membase == -1) { 
				membase = cards_membase;
			} else {
				if (membase != cards_membase)
					return -ENODEV;
			};
			cards_irq=irq_array[((adf_pos0 & 0xC)>>2)];
			if (irq == -1) { 
				irq = cards_irq;
			} else {
				if (irq != cards_irq)
					return -ENODEV;
			};
			cards_io= 0xC00 + ((adf_pos0>>4)*0x10);
			type = EICON_CTYPE_ISAPRI; 
			break;

		case 1:                /* [S|SX|SCOM]/2                       */
			cards_membase= 0xC0000+((adf_pos0>>4)*0x2000);
			if (membase == -1) { 
				membase = cards_membase;
			} else {
				if (membase != cards_membase)
					return -ENODEV;
			};
			cards_irq=irq_array[((adf_pos0 & 0xC)>>2)];
			if (irq == -1) { 
				irq = cards_irq;
			} else {
				if (irq != cards_irq)
					return -ENODEV;
			};

			cards_io= 0xC00 + ((adf_pos0>>4)*0x10);
			type = EICON_CTYPE_SCOM; 
		 	break;	

		case 2:                /* DIVA/MCA                            */
			cards_io = 0x200+ ((adf_pos0>>4)* 0x20);
			cards_irq = irq_array1[(adf_pos0 & 0x7)];
			if (irq == -1) { 
				irq = cards_irq;
			} else {
				if (irq != cards_irq)
					return -ENODEV;
			};
			type = 0; 
			break;
		default:
			return -ENODEV;
	};
	/* matching membase & irq */
	if ( 1 == eicon_addcard(type, membase, irq, id, 0)) { 
		mca_set_adapter_name(slot, eicon_mca_adapters[a_idx].name);
  		mca_set_adapter_procfn(slot, (MCA_ProcFn) eicon_info, cards);

        	mca_mark_as_used(slot);
		cards->mca_slot = slot; 
		/* card->io noch setzen  oder ?? */
		cards->mca_io = cards_io;
		cards->hwif.isa.io = cards_io;
		/* reset card */
		outb_p(0,cards_io+1);

		eicon_log(NULL, 8, "eicon_addcard: successful for slot # %d.\n", 
			cards->mca_slot+1);
		return  0 ; /* eicon_addcard added a card */
	} else {
		return -ENODEV;
	};
};
#endif /* CONFIG_MCA */
#endif /* CONFIG_ISDN_DRV_EICON_ISA */

module_init(eicon_init);
module_exit(eicon_exit);
