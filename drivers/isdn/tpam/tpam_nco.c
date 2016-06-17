/* $Id: tpam_nco.c,v 1.1.2.1 2001/11/20 14:19:37 kai Exp $
 *
 * Turbo PAM ISDN driver for Linux. 
 * (Kernel Driver - Low Level NCO Manipulation)
 *
 * Copyright 2001 Stelian Pop <stelian.pop@fr.alcove.com>, Alcôve
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 * For all support questions please contact: <support@auvertech.fr>
 *
 */

#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/tqueue.h>
#include <linux/interrupt.h>
#include <asm/io.h>

#include "tpam.h"

/* Local function prototypes */
static struct sk_buff *build_NCOpacket(u16, u16, u16, u16, u16);
static int extract_NCOParameter(struct sk_buff *, u8, void *, u16);

/*
 * Build a NCO packet (PCI message).
 *
 * 	messageID: the message type (ID_*)
 * 	size: size of the TLV block
 * 	data_size: size of the data block
 * 	ack: packet needs to send ack upon send
 * 	ack_size: size of data to be acknowledged upon send
 *
 * Return: the sk_buff filled with the NCO packet, or NULL if error.
 */
static struct sk_buff *build_NCOpacket(u16 messageID, u16 size, 
				       u16 data_size, u16 ack, 
				       u16 ack_size) {
	struct sk_buff *skb;
	skb_header *h;
	pci_mpb *p;
	u16 finalsize;

	/* reserve enough space for the sk_buff header, the pci * header, 
	 * size bytes for the TLV block, size bytes for the data and 4 more
	 * bytes in order to make sure we can write dwords to the board. */
	finalsize = sizeof(skb_header) + sizeof(pci_mpb) + size + data_size + 4;

	/* allocate the sk_buff */
	if (!(skb = alloc_skb(finalsize, GFP_ATOMIC))) {
		printk(KERN_ERR "TurboPAM(make_NCOpacket): alloc_skb failed\n");
		return NULL;
	}

	/* construct the skb_header */
	h = (skb_header *)skb_put(skb, sizeof(skb_header));
	h->size = sizeof(pci_mpb) + size;
	h->data_size = data_size;
	h->ack = ack;
	h->ack_size = ack_size;

	/* construct the pci_mpb */
	p = (pci_mpb *)skb_put(skb, sizeof(pci_mpb));
	p->exID = 0;
	p->flags = 0;
	p->errorCode = 0;
	p->messageID = messageID;
	p->maximumBlockTLVSize = MPB_MAXIMUMBLOCKTLVSIZE;
	p->actualBlockTLVSize = size;
	p->maximumDataSize = MPB_MAXIMUMDATASIZE;
	p->actualDataSize = data_size;
	return skb;
}

/*
 * Build a ACreateNCOReq message.
 *
 * 	phone: the local phone number.
 *
 * Return: the sk_buff filled with the NCO packet, or NULL if error.
 */
struct sk_buff *build_ACreateNCOReq(const u8 *phone) {
	struct sk_buff *skb;
	u8 *tlv;

	dprintk("TurboPAM(build_ACreateNCOReq): phone=%s\n", phone);

	/* build the NCO packet */
	if (!(skb = build_NCOpacket(ID_ACreateNCOReq, 23 + strlen(phone), 0, 0, 0))) 
		return NULL;

	/* add the parameters */
	tlv = (u8 *)skb_put(skb, 3);
	*tlv = PAR_NCOType; 
	*(tlv+1) = 1;
	*(tlv+2) = 5;	/* mistery value... */

	tlv = (u8 *)skb_put(skb, 4);
	*tlv = PAR_U3Protocol;
	*(tlv+1) = 2;
	*(tlv+2) = 4;	/* no level 3 protocol */
	*(tlv+3) = 1;	/* HDLC in level 2 */

	tlv = (u8 *)skb_put(skb, 3);
	*tlv = PAR_Cdirection;
	*(tlv+1) = 1;
	*(tlv+2) = 3; /* PCI_DIRECTION_BOTH */

	tlv = (u8 *)skb_put(skb, 3);
	*tlv = PAR_Udirection;
	*(tlv+1) = 1;
	*(tlv+2) = 3; /* PCI_DIRECTION_BOTH */

	tlv = (u8 *)skb_put(skb, 4);
	*tlv = PAR_BearerCap;
	*(tlv+1) = 2;
	*(tlv+2) = 0x88;
	*(tlv+3) = 0x90;

	tlv = (u8 *)skb_put(skb, 6 + strlen(phone));
	*tlv = PAR_CallingNumber;
	*(tlv+1) = strlen(phone) + 4;
	*(tlv+2) = 0x01; /* international */
	*(tlv+3) = 0x01; /* isdn */
	*(tlv+4) = 0x00;
	*(tlv+5) = 0x00;
	memcpy(tlv + 6, phone, strlen(phone));

	return skb;
}

/*
 * Build a ADestroyNCOReq message.
 *
 * 	ncoid: the NCO id.
 *
 * Return: the sk_buff filled with the NCO packet, or NULL if error.
 */
struct sk_buff *build_ADestroyNCOReq(u32 ncoid) {
	struct sk_buff *skb;
	u8 *tlv;

	dprintk("TurboPAM(build_ADestroyNCOReq): ncoid=%lu\n", 
		(unsigned long)ncoid);

	/* build the NCO packet */
	if (!(skb = build_NCOpacket(ID_ADestroyNCOReq, 6, 0, 0, 0)))
		return NULL;
	
	/* add the parameters */
	tlv = (u8 *)skb_put(skb, 6);
	*tlv = PAR_NCOID;
	*(tlv+1) = 4;
	*((u32 *)(tlv+2)) = ncoid;

	return skb;
}

/*
 * Build a CConnectReq message.
 *
 * 	ncoid: the NCO id.
 * 	called: the destination phone number
 * 	hdlc: type of connection: 1 (HDLC) or 0(modem)
 *
 * Return: the sk_buff filled with the NCO packet, or NULL if error.
 */
struct sk_buff *build_CConnectReq(u32 ncoid, const u8 *called, u8 hdlc) {
	struct sk_buff *skb;
	u8 *tlv;

	dprintk("TurboPAM(build_CConnectReq): ncoid=%lu, called=%s, hdlc=%d\n",
		(unsigned long)ncoid, called, hdlc);

	/* build the NCO packet */
	if (!(skb = build_NCOpacket(ID_CConnectReq, 20 + strlen(called), 0, 0, 0)))
		return NULL;
	
	/* add the parameters */
	tlv = (u8 *)skb_put(skb, 6);
	*tlv = PAR_NCOID;
	*(tlv+1) = 4;
	*((u32 *)(tlv+2)) = ncoid;

	tlv = (u8 *)skb_put(skb, 4 + strlen(called));
	*tlv = PAR_CalledNumber;
	*(tlv+1) = strlen(called) + 2;
	*(tlv+2) = 0x01; /* international */
	*(tlv+3) = 0x01; /* isdn */
	memcpy(tlv + 4, called, strlen(called));

	tlv = (u8 *)skb_put(skb, 3);
	*tlv = PAR_BearerCap;
	*(tlv+1) = 1;
	*(tlv+2) = hdlc ? 0x88 /* HDLC */ : 0x80 /* MODEM */;

	tlv = (u8 *)skb_put(skb, 4);
	*tlv = PAR_HLC;
	*(tlv+1) = 2;
	*(tlv+2) = 0x2;
	*(tlv+3) = 0x7f;

	tlv = (u8 *)skb_put(skb, 3);
	*tlv = PAR_Facility;
	*(tlv+1) = 1;
	*(tlv+2) = 2;

	return skb;
}

/*
 * Build a CConnectRsp message.
 *
 * 	ncoid: the NCO id.
 *
 * Return: the sk_buff filled with the NCO packet, or NULL if error.
 */
struct sk_buff *build_CConnectRsp(u32 ncoid) {
	struct sk_buff *skb;
	u8 *tlv;

	dprintk("TurboPAM(build_CConnectRsp): ncoid=%lu\n",
		(unsigned long)ncoid);

	/* build the NCO packet */
	if (!(skb = build_NCOpacket(ID_CConnectRsp, 6, 0, 0, 0)))
		return NULL;

	/* add the parameters */
	tlv = (u8 *)skb_put(skb, 6);
	*tlv = PAR_NCOID;
	*(tlv+1) = 4;
	*((u32 *)(tlv+2)) = ncoid;

	return skb;
}

/*
 * Build a CDisconnectReq message.
 *
 * 	ncoid: the NCO id.
 *
 * Return: the sk_buff filled with the NCO packet, or NULL if error.
 */
struct sk_buff *build_CDisconnectReq(u32 ncoid) {
	struct sk_buff *skb;
	u8 *tlv;

	dprintk("TurboPAM(build_CDisconnectReq): ncoid=%lu\n",
		(unsigned long)ncoid);

	/* build the NCO packet */
	if (!(skb = build_NCOpacket(ID_CDisconnectReq, 6, 0, 0, 0)))
		return NULL;

	/* add the parameters */
	tlv = (u8 *)skb_put(skb, 6);
	*tlv = PAR_NCOID;
	*(tlv+1) = 4;
	*((u32 *)(tlv+2)) = ncoid;

	return skb;
}

/*
 * Build a CDisconnectRsp message.
 *
 * 	ncoid: the NCO id.
 *
 * Return: the sk_buff filled with the NCO packet, or NULL if error.
 */
struct sk_buff *build_CDisconnectRsp(u32 ncoid) {
	struct sk_buff *skb;
	u8 *tlv;

	dprintk("TurboPAM(build_CDisconnectRsp): ncoid=%lu\n",
		(unsigned long)ncoid);

	/* build the NCO packet */
	if (!(skb = build_NCOpacket(ID_CDisconnectRsp, 6, 0, 0, 0)))
		return NULL;

	/* add the parameters */
	tlv = (u8 *)skb_put(skb, 6);
	*tlv = PAR_NCOID;
	*(tlv+1) = 4;
	*((u32 *)(tlv+2)) = ncoid;

	return skb;
}

/*
 * Build a U3DataReq message.
 *
 * 	ncoid: the NCO id.
 * 	data: the data to be send
 * 	len: length of the data
 * 	ack: send ack upon send
 * 	ack_size: size of data to be acknowledged upon send
 *
 * Return: the sk_buff filled with the NCO packet, or NULL if error.
 */
struct sk_buff *build_U3DataReq(u32 ncoid, void *data, u16 len,
				u16 ack, u16 ack_size) {
	struct sk_buff *skb;
	u8 *tlv;
	void *p;

	dprintk("TurboPAM(build_U3DataReq): "
		"ncoid=%lu, len=%d, ack=%d, ack_size=%d\n", 
		(unsigned long)ncoid, len, ack, ack_size);

	/* build the NCO packet */
	if (!(skb = build_NCOpacket(ID_U3DataReq, 6, len, ack, ack_size)))
		return NULL;

	/* add the parameters */
	tlv = (u8 *)skb_put(skb, 6);
	*tlv = PAR_NCOID;
	*(tlv+1) = 4;
	*((u32 *)(tlv+2)) = ncoid;

	p = skb_put(skb, len);
	memcpy(p, data, len);

	return skb;
}

/*
 * Extract a parameter from a TLV block.
 *
 * 	skb: sk_buff containing the PCI message
 * 	type: parameter to search for (PARAM_*)
 * 	value: to be filled with the value of the parameter
 * 	len: maximum length of the parameter value
 *
 * Return: 0 if OK, <0 if error.
 */
static int extract_NCOParameter(struct sk_buff *skb, u8 type, 
				void *value, u16 len) {
	void *buffer = (void *)skb->data;
	pci_mpb *p;
	void * bufferend;
	u8 valtype;
	u16 vallen;

	/* calculate the start and end of the TLV block */
	buffer += sizeof(skb_header);
	p = (pci_mpb *)buffer;
	buffer += sizeof(pci_mpb);
	bufferend = buffer + p->actualBlockTLVSize;

	/* walk through the parameters */
	while (buffer < bufferend) {

		/* parameter type */
		valtype = *((u8 *)buffer++);
		/* parameter length */
		vallen = *((u8 *)buffer++);
		if (vallen == 0xff) {
			/* parameter length is on 2 bytes */
			vallen = *((u8 *)buffer++);
			vallen <<= 8;
			vallen |= *((u8 *)buffer++);
		}
		/* got the right parameter */
		if (valtype == type) {
			/* not enough space for returning the value */
			if (vallen > len)
				return -1;
			/* OK, return it */
			memcpy(value, buffer, vallen);
			return 0;
		}
		buffer += vallen;
	}
	return -1;
}

/*
 * Parse a ACreateNCOCnf message.
 *
 * 	skb: the sk_buff containing the message
 * 	status: to be filled with the status field value
 * 	ncoid: to be filled with the ncoid field value
 *
 * Return: 0 if OK, <0 if error.
 */
int parse_ACreateNCOCnf(struct sk_buff *skb, u8 *status, u32 *ncoid) {

	/* extract the status */
	if (extract_NCOParameter(skb, PAR_CompletionStatus, status, 1)) {
		printk(KERN_ERR "TurboPAM(parse_ACreateNCOCnf): "
		       "CompletionStatus not found\n");
		return -1;
	}

	if (*status) {
		dprintk("TurboPAM(parse_ACreateNCOCnf): status=%d\n", *status);
		return 0;
	}

	/* extract the ncoid */
	if (extract_NCOParameter(skb, PAR_NCOID, ncoid, 4)) {
		printk(KERN_ERR "TurboPAM(parse_ACreateNCOCnf): "
		       "NCOID not found\n");
		return -1;
	}

	dprintk("TurboPAM(parse_ACreateNCOCnf): ncoid=%lu, status=%d\n",
		(unsigned long)*ncoid, *status);
	return 0;
}

/*
 * Parse a ADestroyNCOCnf message. Not used in the driver.
 *
 * 	skb: the sk_buff containing the message
 * 	status: to be filled with the status field value
 * 	ncoid: to be filled with the ncoid field value
 *
 * Return: 0 if OK, <0 if error.
 */
int parse_ADestroyNCOCnf(struct sk_buff *skb, u8 *status, u32 *ncoid) {

	/* extract the status */
	if (extract_NCOParameter(skb, PAR_CompletionStatus, status, 1)) {
		printk(KERN_ERR "TurboPAM(parse_ADestroyNCOCnf): "
		       "CompletionStatus not found\n");
		return -1;
	}

	if (*status) {
		dprintk("TurboPAM(parse_ADestroyNCOCnf): status=%d\n", *status);
		return 0;
	}

	/* extract the ncoid */
	if (extract_NCOParameter(skb, PAR_NCOID, ncoid, 4)) {
		printk(KERN_ERR "TurboPAM(parse_ADestroyNCOCnf): "
		       "NCOID not found\n");
		return -1;
	}

	dprintk("TurboPAM(parse_ADestroyNCOCnf): ncoid=%lu, status=%d\n", 
		(unsigned long)*ncoid, *status);
	return 0;
}

/*
 * Parse a CConnectCnf message.
 *
 * 	skb: the sk_buff containing the message
 * 	ncoid: to be filled with the ncoid field value
 *
 * Return: 0 if OK, <0 if error.
 */
int parse_CConnectCnf(struct sk_buff *skb, u32 *ncoid) {

	/* extract the ncoid */
	if (extract_NCOParameter(skb, PAR_NCOID, ncoid, 4)) {
		printk(KERN_ERR "TurboPAM(parse_CConnectCnf): "
		       "NCOID not found\n");
		return -1;
	}
	dprintk("TurboPAM(parse_CConnectCnf): ncoid=%lu\n", 
		(unsigned long)*ncoid);
	return 0;
}

/*
 * Parse a CConnectInd message.
 *
 * 	skb: the sk_buff containing the message
 * 	ncoid: to be filled with the ncoid field value
 * 	hdlc: to be filled with 1 if the incoming connection is a HDLC one,
 * 		with 0 if the incoming connection is a modem one
 * 	calling: to be filled with the calling phone number value
 * 	called: to be filled with the called phone number value
 * 	plan: to be filled with the plan value
 * 	screen: to be filled with the screen value
 *
 * Return: 0 if OK, <0 if error.
 */
int parse_CConnectInd(struct sk_buff *skb, u32 *ncoid, u8 *hdlc, 
		      u8 *calling, u8 *called, u8 *plan, u8 *screen) {
	u8 phone[PHONE_MAXIMUMSIZE + 4];

	/* extract the ncoid */
	if (extract_NCOParameter(skb, PAR_NCOID, ncoid, 4)) {
		printk(KERN_ERR "TurboPAM(parse_CConnectInd): "
		       "NCOID not found\n");
		return -1;
	}

	/* extract the bearer capability field */
	if (extract_NCOParameter(skb, PAR_BearerCap, hdlc, 1)) {
		printk(KERN_ERR "TurboPAM(parse_CConnectInd): "
		       "BearerCap not found\n");
		return -1;
	}
	*hdlc = (*hdlc == 0x88) ? 1 : 0;

	/* extract the calling number / plan / screen */
	if (extract_NCOParameter(skb, PAR_CallingNumber, phone, 
				 PHONE_MAXIMUMSIZE + 4)) {
		printk(KERN_ERR "TurboPAM(parse_CConnectInd): "
		       "CallingNumber not found\n");
		return -1;
	}
	memcpy(calling, phone + 4, PHONE_MAXIMUMSIZE);
	*plan = phone[1];
	*screen = phone[3];

	/* extract the called number */
	if (extract_NCOParameter(skb, PAR_CalledNumber, phone, 
				 PHONE_MAXIMUMSIZE + 2)) {
		printk(KERN_ERR "TurboPAM(parse_CConnectInd): "
		       "CalledNumber not found\n");
		return -1;
	}
	memcpy(called, phone + 2, PHONE_MAXIMUMSIZE);

	dprintk("TurboPAM(parse_CConnectInd): "
		"ncoid=%lu, hdlc=%d, plan=%d, scr=%d, calling=%s, called=%s\n",
		(unsigned long)*ncoid, *hdlc, *plan, *screen, calling, called);
	return 0;
}

/*
 * Parse a CDisconnectCnf message.
 *
 * 	skb: the sk_buff containing the message
 * 	ncoid: to be filled with the ncoid field value
 * 	causetopuf: to be filled with the cause field value
 *
 * Return: 0 if OK, <0 if error.
 */
int parse_CDisconnectCnf(struct sk_buff *skb, u32 *ncoid, u32 *causetopuf) {

	/* extract the ncoid */
	if (extract_NCOParameter(skb, PAR_NCOID, ncoid, 4)) {
		printk(KERN_ERR "TurboPAM(parse_CDisconnectCnf): "
		       "NCOID not found\n");
		return -1;
	}

	/* extract the cause of disconnection */
	if (extract_NCOParameter(skb, PAR_CauseToPUF, causetopuf, 4)) {
		printk(KERN_ERR "TurboPAM(parse_CDisconnectCnf): "
		       "CauseToPUF not found\n");
		return -1;
	}

	dprintk("TurboPAM(parse_CDisconnectCnf): ncoid=%lu, causetopuf=%lu\n", 
		(unsigned long)*ncoid, (unsigned long)*causetopuf);
	return 0;
}

/*
 * Parse a CDisconnectInd message.
 *
 * 	skb: the sk_buff containing the message
 * 	ncoid: to be filled with the ncoid field value
 * 	causetopuf: to be filled with the cause field value
 *
 * Return: 0 if OK, <0 if error.
 */
int parse_CDisconnectInd(struct sk_buff *skb, u32 *ncoid, u32 *causetopuf) {

	/* extract the ncoid */
	if (extract_NCOParameter(skb, PAR_NCOID, ncoid, 4)) {
		printk(KERN_ERR "TurboPAM(parse_CDisconnectInd): "
		       "NCOID not found\n");
		return -1;
	}

	/* extract the cause of disconnection */
	if (extract_NCOParameter(skb, PAR_CauseToPUF, causetopuf, 4)) {
		printk(KERN_ERR "TurboPAM(parse_CDisconnectInd): "
		       "CauseToPUF not found\n");
		return -1;
	}

	dprintk("TurboPAM(parse_CDisconnectInd): ncoid=%lu, causetopuf=%lu\n", 
		(unsigned long)*ncoid, (unsigned long)*causetopuf);
	return 0;
}

/*
 * Parse a U3ReadyToReceiveInd message.
 *
 * 	skb: the sk_buff containing the message
 * 	ncoid: to be filled with the ncoid field value
 * 	ready: to be filled with the ready field value
 *
 * Return: 0 if OK, <0 if error.
 */
int parse_U3ReadyToReceiveInd(struct sk_buff *skb, u32 *ncoid, u8 *ready) {

	/* extract the ncoid */
	if (extract_NCOParameter(skb, PAR_NCOID, ncoid, 4)) {
		printk(KERN_ERR "TurboPAM(parse_U3ReadyToReceiveInd): "
		       "NCOID not found\n");
		return -1;
	}

	/* extract the ready flag */
	if (extract_NCOParameter(skb, PAR_ReadyFlag, ready, 1)) {
		printk(KERN_ERR "TurboPAM(parse_U3ReadyToReceiveInd): "
		       "ReadyFlag not found\n");
		return -1;
	}

	dprintk("TurboPAM(parse_U3ReadyToReceiveInd): ncoid=%lu, ready=%d\n", 
		(unsigned long)*ncoid, *ready);
	return 0;
}

/*
 * Parse a U3DataInd message.
 *
 * 	skb: the sk_buff containing the message + data
 * 	ncoid: to be filled with the ncoid field value
 * 	data: to be filled with the data 
 * 	ready: to be filled with the data length
 *
 * Return: 0 if OK, <0 if error.
 */
int parse_U3DataInd(struct sk_buff *skb, u32 *ncoid, u8 **data, u16 *len) {
	pci_mpb *p;

	/* extract the ncoid */
	if (extract_NCOParameter(skb, PAR_NCOID, ncoid, 4) == -1) {
		printk(KERN_ERR "TurboPAM(parse_U3DataInd): NCOID not found\n");
		return -1;
	}

	/* get a pointer to the beginning of the data block and its length */
	p = (pci_mpb *)(skb->data + sizeof(skb_header));
	*len = p->actualDataSize;
	skb_pull(skb, 
		 sizeof(skb_header) + sizeof(pci_mpb) + p->actualBlockTLVSize);
	*data = skb->data;

	dprintk("TurboPAM(parse_U3DataInd): ncoid=%lu, datalen=%d\n", 
		(unsigned long)*ncoid, *len);
	return 0;
}

