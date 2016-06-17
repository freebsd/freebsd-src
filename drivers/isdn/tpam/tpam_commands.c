/* $Id: tpam_commands.c,v 1.1.2.1 2001/11/20 14:19:37 kai Exp $
 *
 * Turbo PAM ISDN driver for Linux. (Kernel Driver - ISDN commands)
 *
 * Copyright 2001 Stelian Pop <stelian.pop@fr.alcove.com>, Alcôve
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 * For all support questions please contact: <support@auvertech.fr>
 *
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/tqueue.h>
#include <linux/interrupt.h>
#include <asm/io.h>

#include <linux/isdn/tpam.h>
#include "tpam.h"

/* Local functions prototypes */
static int tpam_command_ioctl_dspload(tpam_card *, u32);
static int tpam_command_ioctl_dspsave(tpam_card *, u32);
static int tpam_command_ioctl_dsprun(tpam_card *);
static int tpam_command_ioctl_loopmode(tpam_card *, u8);
static int tpam_command_dial(tpam_card *, u32, u8 *);
static int tpam_command_setl2(tpam_card *, u32, u8);
static int tpam_command_getl2(tpam_card *, u32);
static int tpam_command_acceptd(tpam_card *, u32);
static int tpam_command_acceptb(tpam_card *, u32);
static int tpam_command_hangup(tpam_card *, u32);
static int tpam_command_proceed(tpam_card *, u32);
static void tpam_statcallb_run(unsigned long);
static void tpam_statcallb(tpam_card *, isdn_ctrl);

/*
 * Function called when the ISDN link level send a command to the driver.
 *
 * 	c: ISDN command.
 *
 * Return: 0 if OK, <0 on errors.
 */
int tpam_command(isdn_ctrl *c) {
	tpam_card *card;
	unsigned long argp;

	dprintk("TurboPAM(tpam_command) card=%d, command=%d\n", 
		c->driver, c->command);	

	/* search for the board */
	if (!(card = tpam_findcard(c->driver))) {
		printk(KERN_ERR "TurboPAM(tpam_command): invalid driverId %d\n",
		       c->driver);	
		return -ENODEV;
	}

	/* dispatch the command */
	switch (c->command) {
		case ISDN_CMD_IOCTL:
			argp = c->parm.userdata;
			switch (c->arg) {
				case TPAM_CMD_DSPLOAD:
					return tpam_command_ioctl_dspload(card,
									  argp);
				case TPAM_CMD_DSPSAVE:
					return tpam_command_ioctl_dspsave(card,
									  argp);
				case TPAM_CMD_DSPRUN:
					return tpam_command_ioctl_dsprun(card);
				case TPAM_CMD_LOOPMODEON:
					return tpam_command_ioctl_loopmode(card,
									   1);
				case TPAM_CMD_LOOPMODEOFF:
					return tpam_command_ioctl_loopmode(card,
									   0);
				default:
					dprintk("TurboPAM(tpam_command): "
						"invalid tpam ioctl %ld\n", 
						c->arg);	
					return -EINVAL;
			}
		case ISDN_CMD_DIAL:
			return tpam_command_dial(card, c->arg, 
						 c->parm.setup.phone);
		case ISDN_CMD_ACCEPTD:
			return tpam_command_acceptd(card, c->arg);
		case ISDN_CMD_ACCEPTB:
			return tpam_command_acceptb(card, c->arg);
		case ISDN_CMD_HANGUP:
			return tpam_command_hangup(card, c->arg);
		case ISDN_CMD_SETL2:
			return tpam_command_setl2(card, c->arg & 0xff, 
						  c->arg >> 8);
		case ISDN_CMD_GETL2:
			return tpam_command_getl2(card, c->arg);
		case ISDN_CMD_LOCK:
			MOD_INC_USE_COUNT;
			return 0;
		case ISDN_CMD_UNLOCK:
			MOD_DEC_USE_COUNT;
			return 0;
		case ISDN_CMD_PROCEED:
			return tpam_command_proceed(card, c->arg);
		default:
			dprintk("TurboPAM(tpam_command): "
				"unknown or unused isdn ioctl %d\n", 
				c->command);	
			return -EINVAL;
	}

	/* not reached */
	return -EINVAL;
}

/*
 * Load some data into the board's memory.
 *
 * 	card: the board
 * 	arg: IOCTL argument containing the user space address of 
 * 		the tpam_dsp_ioctl structure describing the IOCTL.
 *
 * Return: 0 if OK, <0 on errors.
 */
static int tpam_command_ioctl_dspload(tpam_card *card, u32 arg) {
	tpam_dsp_ioctl tdl;
	int ret;

	dprintk("TurboPAM(tpam_command_ioctl_dspload): card=%d\n", card->id);

	/* get the IOCTL parameter from userspace */
	if (copy_from_user(&tdl, (void *)arg, sizeof(tpam_dsp_ioctl)))
		return -EFAULT;

	/* if the board's firmware was started, protect against writes
	 * to unallowed memory areas. If the board's firmware wasn't started,
	 * all is allowed. */
	if (card->running && tpam_verify_area(tdl.address, tdl.data_len)) 
		return -EPERM;

	/* write the data in the board's memory */
	ret = copy_from_user_to_pam(card, (void *)tdl.address, 
				    (void *)arg + sizeof(tpam_dsp_ioctl), 
				    tdl.data_len);
	return 0;
}

/*
 * Extract some data from the board's memory.
 *
 * 	card: the board
 * 	arg: IOCTL argument containing the user space address of 
 * 		the tpam_dsp_ioctl structure describing the IOCTL.
 *
 * Return: 0 if OK, <0 on errors.
 */
static int tpam_command_ioctl_dspsave(tpam_card *card, u32 arg) {
	tpam_dsp_ioctl tdl;
	int ret;

	dprintk("TurboPAM(tpam_command_ioctl_dspsave): card=%d\n", card->id);

	/* get the IOCTL parameter from userspace */
	if (copy_from_user(&tdl, (void *)arg, sizeof(tpam_dsp_ioctl)))
		return -EFAULT;

	/* protect against read from unallowed memory areas */
	if (tpam_verify_area(tdl.address, tdl.data_len)) 
		return -EPERM;

	/* read the data from the board's memory */
	ret = copy_from_pam_to_user(card, (void *)arg + sizeof(tpam_dsp_ioctl),
				    (void *)tdl.address, tdl.data_len);
	return ret;
}

/*
 * Launch the board's firmware. This function must be called after the 
 * firmware was loaded into the board's memory using TPAM_CMD_DSPLOAD 
 * IOCTL commands. After launching the firmware, this function creates
 * the NCOs and waits for their creation.
 *
 * 	card: the board
 *
 * Return: 0 if OK, <0 on errors.
 */
static int tpam_command_ioctl_dsprun(tpam_card *card) {
	u32 signature = 0, timeout, i;
	isdn_ctrl ctrl;
	struct sk_buff *skb;

	dprintk("TurboPAM(tpam_command_ioctl_dsprun): card=%d\n", card->id);

	/* board must _not_ be running */
	if (card->running)
		return -EBUSY;

	/* reset the board */
	spin_lock_irq(&card->lock);
	copy_to_pam_dword(card, (void *)TPAM_MAGICNUMBER_REGISTER, 0xdeadface);
	readl(card->bar0 + TPAM_DSPINT_REGISTER);
	readl(card->bar0 + TPAM_HINTACK_REGISTER);
	spin_unlock_irq(&card->lock);
	
	/* wait for the board signature */
	timeout = jiffies + SIGNATURE_TIMEOUT;
	while (time_before(jiffies, timeout)) {
		spin_lock_irq(&card->lock);
		signature = copy_from_pam_dword(card, 
						(void *)TPAM_MAGICNUMBER_REGISTER);
		spin_unlock_irq(&card->lock);
		if (signature == TPAM_MAGICNUMBER)
			break;
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(2);
	}

	/* signature not present -> board not started */
	if (signature != TPAM_MAGICNUMBER) {
		printk(KERN_ERR "TurboPAM(tpam_command_ioctl_dsprun): "
		       "card=%d, signature 0x%lx, expected 0x%lx\n", 
		       card->id, (unsigned long)signature, 
		       (unsigned long)TPAM_MAGICNUMBER);
		printk(KERN_ERR "TurboPAM(tpam_command_ioctl_dsprun): "
		       "card=%d, firmware not started\n", card->id);
		return -EIO;
	}

	/* the firmware is started */
	printk(KERN_INFO "TurboPAM: card=%d, firmware started\n", card->id);

	/* init the CRC routines */
	init_CRC();

	/* create all the NCOs */
	for (i = 0; i < TPAM_NBCHANNEL; ++i)
		if ((skb = build_ACreateNCOReq("")))
			tpam_enqueue(card, skb);

	/* wait for NCO creation confirmation */
	timeout = jiffies + NCOCREATE_TIMEOUT;
	while (time_before(jiffies, timeout)) {
		if (card->channels_tested == TPAM_NBCHANNEL)
			break;
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(2);
	}

	card->running = 1;

	if (card->channels_tested != TPAM_NBCHANNEL)
		printk(KERN_ERR "TurboPAM(tpam_command_ioctl_dsprun): "
		       "card=%d, tried to init %d channels, "
		       "got reply from only %d channels\n", card->id, 
		       TPAM_NBCHANNEL, card->channels_tested);

	/* if all the channels were not initialized, signal to the ISDN
	 * link layer that fact that some channels are not usable */
	if (card->channels_used != TPAM_NBCHANNEL)
		for (i = card->channels_used; i < TPAM_NBCHANNEL; ++i) {
			ctrl.driver = card->id;
			ctrl.command = ISDN_STAT_DISCH;
			ctrl.arg = i;
			ctrl.parm.num[0] = 0;
			(* card->interface.statcallb)(&ctrl);
		}

	printk(KERN_INFO "TurboPAM: card=%d, ready, %d channels available\n", 
	       card->id, card->channels_used);

	/* let's rock ! */
	ctrl.driver = card->id;
	ctrl.command = ISDN_STAT_RUN;
	ctrl.arg = 0;
	tpam_statcallb(card, ctrl);

	return 0;
}

/* 
 * Set/reset the board's looptest mode.
 *
 * 	card: the board
 * 	mode: if 1, sets the board's looptest mode, if 0 resets it.
 *
 * Return: 0 if OK, <0 if error.
 */
static int tpam_command_ioctl_loopmode(tpam_card *card, u8 mode) {

	/* board must be running */
	if (!card->running)
		return -ENODEV;

	card->loopmode = mode;
	return 0;
}

/*
 * Issue a dial command. This function builds and sends a CConnectReq.
 * 
 * 	card: the board
 * 	channel: the channel number
 * 	phone: the remote phone number (EAZ)
 *
 * Return: 0 if OK, <0 if error.
 */
static int tpam_command_dial(tpam_card *card, u32 channel, u8 *phone) {
	struct sk_buff *skb;
	isdn_ctrl ctrl;

	dprintk("TurboPAM(tpam_command_dial): card=%d, channel=%lu, phone=%s\n",
		card->id, (unsigned long)channel, phone);

	/* board must be running */
	if (!card->running)
		return -ENODEV;

	/* initialize channel parameters */
	card->channels[channel].realhdlc = card->channels[channel].hdlc;
	card->channels[channel].hdlcshift = 0;
	card->channels[channel].readytoreceive = 0;

	/* build and send a CConnectReq */
	skb = build_CConnectReq(card->channels[channel].ncoid, phone, 
				card->channels[channel].realhdlc);
	if (!skb)
		return -ENOMEM;
	tpam_enqueue(card, skb);

	/* making a connection in modem mode is slow and causes the ISDN
	 * link layer to hangup the connection before even it gets a chance
	 * to establish... All we can do is simulate a successful connection
	 * for now, and send a DHUP later if the connection fails */
	if (!card->channels[channel].realhdlc) {
		ctrl.driver = card->id;
		ctrl.command = ISDN_STAT_DCONN;
		ctrl.arg = channel;
		tpam_statcallb(card, ctrl);
	}
	
	return 0;
}

/*
 * Set the level2 protocol (modem or HDLC).
 *
 * 	card: the board
 * 	channel: the channel number
 * 	proto: the level2 protocol (one of ISDN_PROTO_L2*)
 *
 * Return: 0 if OK, <0 if error.
 */
static int tpam_command_setl2(tpam_card *card, u32 channel, u8 proto) {

	dprintk("TurboPAM(tpam_command_setl2): card=%d, channel=%lu, proto=%d\n",
		card->id, (unsigned long)channel, proto);

	/* board must be running */
	if (!card->running)
		return -ENODEV;

	/* set the hdlc/modem mode */
	switch (proto) {
		case ISDN_PROTO_L2_HDLC:
			card->channels[channel].hdlc = 1;
			break;
		case ISDN_PROTO_L2_MODEM:
			card->channels[channel].hdlc = 0;
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

/*
 * Return the level2 protocol (modem or HDLC).
 *
 * 	card: the board
 * 	channel: the channel number
 *
 * Return: ISDN_PROTO_L2_HDLC/MODEM if OK, <0 if error.
 */
static int tpam_command_getl2(tpam_card *card, u32 channel) {

	dprintk("TurboPAM(tpam_command_getl2): card=%d, channel=%lu\n",
		card->id, (unsigned long)channel);
	
	/* board must be running */
	if (!card->running)
		return -ENODEV;

	/* return the current mode */
	if (card->channels[channel].realhdlc)
		return ISDN_PROTO_L2_HDLC;
	else
		return ISDN_PROTO_L2_MODEM;
}

/*
 * Accept a D-channel connection (incoming connection). This function
 * builds and sends a CConnectRsp message and signals DCONN to the ISDN
 * link level.
 *
 * 	card: the board
 * 	channel: the channel number
 *
 * Return: 0 if OK, <0 if error.
 */
static int tpam_command_acceptd(tpam_card *card, u32 channel) {
	isdn_ctrl ctrl;
	struct sk_buff *skb;

	dprintk("TurboPAM(tpam_command_acceptd): card=%d, channel=%lu\n",
		card->id, (unsigned long)channel);

	/* board must be running */
	if (!card->running)
		return -ENODEV;

	/* build and send a CConnectRsp */
	skb = build_CConnectRsp(card->channels[channel].ncoid);
	if (!skb)
		return -ENOMEM;
	tpam_enqueue(card, skb);

	/* issue DCONN to the ISDN link level */
	ctrl.driver = card->id;
	ctrl.command = ISDN_STAT_DCONN;
	ctrl.arg = channel;
	tpam_statcallb(card, ctrl);
	return 0;
}

/*
 * Accepts a B-channel connection. This is not used by the driver, 
 * since the TurboPAM is an active card hiding its B-channels from
 * us. We just signal BCONN to the ISDN link layer.
 *
 * 	card: the board
 * 	channel: the channel number
 *
 * Return: 0 if OK, <0 if error.
 */
static int tpam_command_acceptb(tpam_card *card, u32 channel) {
	isdn_ctrl ctrl;

	dprintk("TurboPAM(tpam_command_acceptb): card=%d, channel=%lu\n",
		card->id, (unsigned long)channel);

	/* board must be running */
	if (!card->running)
		return -ENODEV;

	/* issue BCONN to the ISDN link level */
	ctrl.driver = card->id;
	ctrl.command = ISDN_STAT_BCONN;
	ctrl.arg = channel;
	ctrl.parm.num[0] = '\0';
	tpam_statcallb(card, ctrl);
	return 0;
}

/*
 * Hang up a connection. This function builds and sends a CDisconnectReq.
 *
 * 	card: the board
 * 	channel: the channel number.
 *
 * Return: 0 if OK, <0 if error.
 */
static int tpam_command_hangup(tpam_card *card, u32 channel) {
	struct sk_buff *skb;

	dprintk("TurboPAM(tpam_command_hangup): card=%d, channel=%lu\n",
		card->id, (unsigned long)channel);

	/* board must be running */
	if (!card->running)
		return -ENODEV;

	/* build and send a CDisconnectReq */
	skb = build_CDisconnectReq(card->channels[channel].ncoid);
	if (!skb)
		return -ENOMEM;
	tpam_enqueue(card, skb);
	return 0;
}

/*
 * Proceed with an incoming connection. This function builds and sends a 
 * CConnectRsp.
 *
 * 	card: the board
 * 	channel: the channel number.
 *
 * Return: 0 if OK, <0 if error.
 */
static int tpam_command_proceed(tpam_card *card, u32 channel) {
	struct sk_buff *skb;

	dprintk("TurboPAM(tpam_command_proceed): card=%d, channel=%lu\n",
		card->id, (unsigned long)channel);

	/* board must be running */
	if (!card->running)
		return -ENODEV;

	/* build and send a CConnectRsp */
	skb = build_CConnectRsp(card->channels[channel].ncoid);
	if (!skb)
		return -ENOMEM;
	tpam_enqueue(card, skb);
	return 0;
}

/*
 * Send data through the board. This function encodes the data depending
 * on the connection type (modem or HDLC), then builds and sends a U3DataReq.
 *
 * 	driverId: the driver id (really meaning here the board)
 * 	channel: the channel number
 * 	ack: data needs to be acknowledged upon send
 * 	skb: sk_buff containing the data
 *
 * Return: size of data send if OK, <0 if error.
 */
int tpam_writebuf_skb(int driverId, int channel, int ack, struct sk_buff *skb) {
	tpam_card *card;
	int orig_size = skb->len;
	void *finaldata;
	u32 finallen;

	dprintk("TurboPAM(tpam_writebuf_skb): "
		"card=%d, channel=%ld, ack=%d, data size=%d\n", 
		driverId, (unsigned long)channel, ack, skb->len);

	/* find the board based on its driver ID */
	if (!(card = tpam_findcard(driverId))) {
		printk(KERN_ERR "TurboPAM(tpam_writebuf_skb): "
		       "invalid driverId %d\n", driverId);	
		return -ENODEV;
	}

	/* board must be running */
	if (!card->running)
		return -ENODEV;

	/* allocate some temporary memory */
	if (!(finaldata = (void *)__get_free_page(GFP_ATOMIC))) {
		printk(KERN_ERR "TurboPAM(tpam_writebuf_skb): "
		       "get_free_page failed\n");
		return -ENOMEM;
	}

	/* encode the data */
	if (!card->channels[channel].realhdlc) {
		/* modem mode */
		hdlc_encode_modem(skb->data, skb->len, finaldata, &finallen);
	}
	else {
		/* HDLC mode */
		void *tempdata;
		u32 templen;

		if (!(tempdata = (void *)__get_free_page(GFP_ATOMIC))) {
			printk(KERN_ERR "TurboPAM(tpam_writebuf_skb): "
			       "get_free_page failed\n");
			free_page((u32)finaldata);
			return -ENOMEM;
		}
		hdlc_no_accm_encode(skb->data, skb->len, tempdata, &templen);
		finallen = tpam_hdlc_encode(tempdata, finaldata, 
				       &card->channels[channel].hdlcshift, 
				       templen);
		free_page((u32)tempdata);
	}

	/* free the old sk_buff */
	kfree_skb(skb);

	/* build and send a U3DataReq */
	skb = build_U3DataReq(card->channels[channel].ncoid, finaldata, 
			      finallen, ack, orig_size);
	if (!skb) {
		free_page((u32)finaldata);
		return -ENOMEM;
	}
	tpam_enqueue_data(&card->channels[channel], skb);

	/* free the temporary memory */
	free_page((u32)finaldata);
	return orig_size;
}

/*
 * Treat a received ACreateNCOCnf message.
 *
 * 	card: the board
 * 	skb: the received message
 */
void tpam_recv_ACreateNCOCnf(tpam_card *card, struct sk_buff *skb) {
	u32 ncoid;
	u8 status;
	u32 channel;

	dprintk("TurboPAM(tpam_recv_ACreateNCOCnf): card=%d\n", card->id);

	/* parse the message contents */
	if (parse_ACreateNCOCnf(skb, &status, &ncoid))
		return;

	/* if the card is alreay running, it means that this message
	 * arrives too late... */
	if (card->running) {
		printk(KERN_ERR "TurboPAM(tpam_recv_ACreateNCOCnf): "
		       "ACreateNCOCnf received too late, status=%d\n", status);
		return;
	}

	/* the NCO creation failed, the corresponding channel will
	 * be unused */
	if (status) {
		printk(KERN_ERR "TurboPAM(tpam_recv_ACreateNCOCnf): "
		       "ACreateNCO failed, status=%d\n", status);
		card->channels_tested++;
		return;
	}

	/* find the first free channel and assign the nco ID to it */
	if ((channel = tpam_findchannel(card, TPAM_NCOID_INVALID)) == TPAM_CHANNEL_INVALID) {
		printk(KERN_ERR "TurboPAM(tpam_recv_ACreateNCOCnf): "
		       "All channels are assigned\n");
		return;
	}
	card->channels[channel].ncoid = ncoid;
	card->channels_tested++;
	card->channels_used++;
}

/*
 * Treat a received ADestroyNCOCnf message. Not used by the driver.
 *
 * 	card: the board
 * 	skb: the received message
 */
void tpam_recv_ADestroyNCOCnf(tpam_card *card, struct sk_buff *skb) {
	u32 ncoid;
	u8 status;
	u32 channel;

	dprintk("TurboPAM(tpam_recv_ADestroyNCOCnf): card=%d\n", card->id);

	/* parse the message contents */
	if (parse_ADestroyNCOCnf(skb, &status, &ncoid))
		return;
	
	if (status) {
		printk(KERN_ERR "TurboPAM(tpam_recv_ADestroyNCOCnf): "
		       "ADestroyNCO failed, status=%d\n", status);
		return;
	}

	/* clears the channel's nco ID */
	if ((channel = tpam_findchannel(card, ncoid)) == TPAM_CHANNEL_INVALID) {
		printk(KERN_ERR "TurboPAM(tpam_recv_ADestroyNCOCnf): "
		       "ncoid invalid %lu\n", (unsigned long)ncoid);
		return;
	}

	card->channels[channel].ncoid = TPAM_NCOID_INVALID;
}

/*
 * Treat a received CConnectCnf message.
 *
 * 	card: the board
 * 	skb: the received message
 */
void tpam_recv_CConnectCnf(tpam_card *card, struct sk_buff *skb) {
	u32 ncoid;
	u32 channel;
	isdn_ctrl ctrl;

	dprintk("TurboPAM(tpam_recv_CConnectCnf): card=%d\n", card->id);

	/* parse the message contents */
	if (parse_CConnectCnf(skb, &ncoid))
		return;

	/* find the channel by its nco ID */
	if ((channel = tpam_findchannel(card, ncoid)) == TPAM_CHANNEL_INVALID) {
		printk(KERN_ERR "TurboPAM(tpam_recv_CConnectCnf): "
		       "ncoid invalid %lu\n", (unsigned long)ncoid);
		return;
	}

	/* issue a DCONN command to the ISDN link layer if we are in HDLC mode.
	 * In modem mode, we alreay did it - the ISDN timer kludge */
	if (card->channels[channel].realhdlc) {
		ctrl.driver = card->id;
		ctrl.command = ISDN_STAT_DCONN;
		ctrl.arg = channel;
		(* card->interface.statcallb)(&ctrl);
	}
}

/*
 * Treat a received CConnectInd message. This function signals a ICALL
 * to the ISDN link layer.
 *
 * 	card: the board
 * 	skb: the received message
 */
void tpam_recv_CConnectInd(tpam_card *card, struct sk_buff *skb) {
	u32 ncoid;
	u32 channel;
	u8 hdlc, plan, screen;
	u8 calling[PHONE_MAXIMUMSIZE], called[PHONE_MAXIMUMSIZE];
	isdn_ctrl ctrl;
	int status;

	dprintk("TurboPAM(tpam_recv_CConnectInd): card=%d\n", card->id);

	/* parse the message contents */
	if (parse_CConnectInd(skb, &ncoid, &hdlc, calling, called, &plan, &screen))
		return;

	/* find the channel by its nco ID */
	if ((channel = tpam_findchannel(card, ncoid)) == TPAM_CHANNEL_INVALID) {
		printk(KERN_ERR "TurboPAM(tpam_recv_CConnectInd): "
		       "ncoid invalid %lu\n", (unsigned long)ncoid);
		return;
	}

	/* initialize the channel parameters */
	card->channels[channel].realhdlc = hdlc;
	card->channels[channel].hdlcshift = 0;
	card->channels[channel].readytoreceive = 0;

	/* issue a ICALL command to the ISDN link layer */
	ctrl.driver = card->id;
	ctrl.command = ISDN_STAT_ICALL;
	ctrl.arg = channel;
	memcpy(ctrl.parm.setup.phone, calling, 32);
	memcpy(ctrl.parm.setup.eazmsn, called, 32);
	ctrl.parm.setup.si1 = 7;	/* data capability */
	ctrl.parm.setup.si2 = 0;
	ctrl.parm.setup.plan = plan;
	ctrl.parm.setup.screen = screen;

	status = (* card->interface.statcallb)(&ctrl);
	switch (status) {
		case 1:
		case 4:
			/* call accepted, link layer will send us a ACCEPTD 
			 * command later */
			dprintk("TurboPAM(tpam_recv_CConnectInd): "
				"card=%d, channel=%d, icall waiting, status=%d\n", 
				card->id, channel, status);
			break;
		default:
			/* call denied, we build and send a CDisconnectReq */
			dprintk("TurboPAM(tpam_recv_CConnectInd): "
				"card=%d, channel=%d, icall denied, status=%d\n", 
				card->id, channel, status);
			skb = build_CDisconnectReq(ncoid);
			if (!skb)
				return;
			tpam_enqueue(card, skb);
	}
}

/*
 * Treat a received CDisconnectInd message. This function signals a DHUP and
 * a BHUP to the ISDN link layer.
 *
 * 	card: the board
 * 	skb: the received message
 */
void tpam_recv_CDisconnectInd(tpam_card *card, struct sk_buff *skb) {
	u32 ncoid;
	u32 channel;
	u32 cause;
	isdn_ctrl ctrl;

	dprintk("TurboPAM(tpam_recv_CDisconnectInd): card=%d\n", card->id);

	/* parse the message contents */
	if (parse_CDisconnectInd(skb, &ncoid, &cause))
		return;

	/* find the channel by its nco ID */
	if ((channel = tpam_findchannel(card, ncoid)) == TPAM_CHANNEL_INVALID) {
		printk(KERN_ERR "TurboPAM(tpam_recv_CDisconnectInd): "
		       "ncoid invalid %lu\n", (unsigned long)ncoid);
		return;
	}

	/* build and send a CDisconnectRsp */
	skb = build_CDisconnectRsp(ncoid);
	if (!skb)
		return;
	tpam_enqueue(card, skb);

	/* issue a DHUP to the ISDN link layer */
	ctrl.driver = card->id;
	ctrl.command = ISDN_STAT_DHUP;
	ctrl.arg = channel;
	(* card->interface.statcallb)(&ctrl);

	/* issue a BHUP to the ISDN link layer */
	ctrl.driver = card->id;
	ctrl.command = ISDN_STAT_BHUP;
	ctrl.arg = channel;
	(* card->interface.statcallb)(&ctrl);
}

/*
 * Treat a received CDisconnectCnf message. This function signals a DHUP and
 * a BHUP to the ISDN link layer.
 *
 * 	card: the board
 * 	skb: the received message
 */
void tpam_recv_CDisconnectCnf(tpam_card *card, struct sk_buff *skb) {
	u32 ncoid;
	u32 channel;
	u32 cause;
	isdn_ctrl ctrl;

	dprintk("TurboPAM(tpam_recv_CDisconnectCnf): card=%d\n", card->id);

	/* parse the message contents */
	if (parse_CDisconnectCnf(skb, &ncoid, &cause))
		return;

	/* find the channel by its nco ID */
	if ((channel = tpam_findchannel(card, ncoid)) == TPAM_CHANNEL_INVALID) {
		printk(KERN_ERR "TurboPAM(tpam_recv_CDisconnectCnf): "
		       "ncoid invalid %lu\n", (unsigned long)ncoid);
		return;
	}

	/* issue a DHUP to the ISDN link layer */
	ctrl.driver = card->id;
	ctrl.command = ISDN_STAT_DHUP;
	ctrl.arg = channel;
	(* card->interface.statcallb)(&ctrl);

	/* issue a BHUP to the ISDN link layer */
	ctrl.driver = card->id;
	ctrl.command = ISDN_STAT_BHUP;
	ctrl.arg = channel;
	(* card->interface.statcallb)(&ctrl);
}

/*
 * Treat a received U3DataInd message. This function decodes the data
 * depending on the connection type (modem or HDLC) and passes it to the
 * ISDN link layer by using rcvcallb_skb.
 *
 * 	card: the board
 * 	skb: the received message + data
 */
void tpam_recv_U3DataInd(tpam_card *card, struct sk_buff *skb) {
	u32 ncoid;
	u32 channel;
	u8 *data;
	u16 len;
	struct sk_buff *result;

	dprintk("TurboPAM(tpam_recv_U3DataInd): card=%d, datalen=%d\n", 
		card->id, skb->len);

	/* parse the message contents */
	if (parse_U3DataInd(skb, &ncoid, &data, &len))
		return;

	/* find the channel by its nco ID */
	if ((channel = tpam_findchannel(card, ncoid)) == TPAM_CHANNEL_INVALID) {
		printk(KERN_ERR "TurboPAM(tpam_recv_U3DataInd): "
		       "ncoid invalid %lu\n", (unsigned long)ncoid);
		return;
	}

	/* decode the data */
	if (card->channels[ncoid].realhdlc) {
		/* HDLC mode */
		u8 *tempdata;
		u32 templen;

		if (!(tempdata = (void *)__get_free_page(GFP_ATOMIC))) {
			printk(KERN_ERR "TurboPAM(tpam_recv_U3DataInd): "
			       "get_free_page failed\n");
			return;
		}
		templen = tpam_hdlc_decode(data, tempdata, len);
		templen = hdlc_no_accm_decode(tempdata, templen);
		if (!(result = alloc_skb(templen, GFP_ATOMIC))) {
			printk(KERN_ERR "TurboPAM(tpam_recv_U3DataInd): "
			       "alloc_skb failed\n");
			free_page((u32)tempdata);
			return;
		}
		memcpy(skb_put(result, templen), tempdata, templen);
		free_page((u32)tempdata);
	}
	else {
		/* modem mode */
		if (!(result = alloc_skb(len, GFP_ATOMIC))) {
			printk(KERN_ERR "TurboPAM(tpam_recv_U3DataInd): "
			       "alloc_skb failed\n");
			return;
		}
		memcpy(skb_put(result, len), data, len);
	}

	/* In loop mode, resend the data immediatly */
	if (card->loopmode) {
		struct sk_buff *loopskb;

		if (!(loopskb = alloc_skb(skb->len, GFP_ATOMIC))) {
			printk(KERN_ERR "TurboPAM(tpam_recv_U3DataInd): "
			       "alloc_skb failed\n");
			kfree_skb(result);
			return;
		}
		memcpy(skb_put(loopskb, result->len), result->data, 
		       result->len);
		if (tpam_writebuf_skb(card->id, channel, 0, loopskb) < 0)
			kfree_skb(loopskb);
	}

	/* pass the data to the ISDN link layer */
	(* card->interface.rcvcallb_skb)(card->id, channel, result);
}

/*
 * Treat a received U3ReadyToReceiveInd message. This function sets the
 * channel ready flag and triggers the send of data if the channel becomed
 * ready.
 *
 * 	card: the board
 * 	skb: the received message + data
 */
void tpam_recv_U3ReadyToReceiveInd(tpam_card *card, struct sk_buff *skb) {
	u32 ncoid;
	u32 channel;
	u8 ready;

	dprintk("TurboPAM(tpam_recv_U3ReadyToReceiveInd): card=%d\n", card->id);

	/* parse the message contents */
	if (parse_U3ReadyToReceiveInd(skb, &ncoid, &ready))
		return;

	/* find the channel by its nco ID */
	if ((channel = tpam_findchannel(card, ncoid)) == TPAM_CHANNEL_INVALID) {
		printk(KERN_ERR "TurboPAM(tpam_recv_U3ReadyToReceiveInd): "
		       "ncoid invalid %lu\n", (unsigned long)ncoid);
		return;
	}

	/* set the readytoreceive flag */
	card->channels[channel].readytoreceive = ready;

	/* if the channel just becomed ready, trigger the send of queued data */
	if (ready)
		tpam_enqueue_data(&card->channels[channel], NULL);
}

/*
 * Runs the delayed statcallb when its timer expires.
 *
 * 	parm: pointer to the tpam_statcallb_data statcallb argument.
 */
static void tpam_statcallb_run(unsigned long parm) {
	tpam_statcallb_data *ds = (tpam_statcallb_data *)parm;

	dprintk("TurboPAM(tpam_statcallb_run)\n");

	(* ds->card->interface.statcallb)(&ds->ctrl);

	kfree(ds->timer);
	kfree(ds);
}

/*
 * Queues a statcallb call for delayed invocation.
 *
 * 	card: the board
 * 	ctrl: the statcallb argument
 */
static void tpam_statcallb(tpam_card *card, isdn_ctrl ctrl) {
	struct timer_list *timer;
	tpam_statcallb_data *ds;

	dprintk("TurboPAM(tpam_statcallb): card=%d\n", card->id);

	if (!(timer = (struct timer_list *) kmalloc(sizeof(struct timer_list), 
						    GFP_ATOMIC))) {
		printk(KERN_ERR "TurboPAM: tpam_statcallb: kmalloc failed!\n");
		return;
	}

	if (!(ds = (tpam_statcallb_data *) kmalloc(sizeof(tpam_statcallb_data),
						   GFP_ATOMIC))) {
		printk(KERN_ERR "TurboPAM: tpam_statcallb: kmalloc failed!\n");
		kfree(timer);
		return;
	}
	ds->card = card;
	ds->timer = timer;
	memcpy(&ds->ctrl, &ctrl, sizeof(isdn_ctrl));

	init_timer(timer);
	timer->function = tpam_statcallb_run;
	timer->data = (unsigned long)ds;
	timer->expires = jiffies + HZ / 10;   /* 0.1 second */
	add_timer(timer);
}
