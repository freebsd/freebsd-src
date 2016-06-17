/* $Id: eicon_io.c,v 1.1.4.1 2001/11/20 14:19:35 kai Exp $
 *
 * ISDN low-level module for Eicon active ISDN-Cards.
 * Code for communicating with hardware.
 *
 * Copyright 1999,2000  by Armin Schindler (mac@melware.de)
 * Copyright 1999,2000  Cytronics & Melware (info@melware.de)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 * Thanks to	Eicon Networks for 
 *		documents, informations and hardware. 
 *
 */

#include <linux/config.h>
#include "eicon.h"
#include "uxio.h"

void
eicon_io_rcv_dispatch(eicon_card *ccard) {
	ulong flags;
        struct sk_buff *skb, *skb2, *skb_new;
        eicon_IND *ind, *ind2, *ind_new;
        eicon_chan *chan;

        if (!ccard) {
	        eicon_log(ccard, 1, "eicon_err: NULL card in rcv_dispatch !\n");
                return;
        }

	while((skb = skb_dequeue(&ccard->rcvq))) {
        	ind = (eicon_IND *)skb->data;

		spin_lock_irqsave(&eicon_lock, flags);
        	if ((chan = ccard->IdTable[ind->IndId]) == NULL) {
			spin_unlock_irqrestore(&eicon_lock, flags);
			if (DebugVar & 1) {
				switch(ind->Ind) {
					case N_DISC_ACK: 
						/* doesn't matter if this happens */ 
						break;
					default: 
						eicon_log(ccard, 1, "idi: Indication for unknown channel Ind=%d Id=%x\n", ind->Ind, ind->IndId);
						eicon_log(ccard, 1, "idi_hdl: Ch??: Ind=%d Id=%x Ch=%d MInd=%d MLen=%d Len=%d\n",
							ind->Ind,ind->IndId,ind->IndCh,ind->MInd,ind->MLength,ind->RBuffer.length);
				}
			}
	                dev_kfree_skb(skb);
	                continue;
	        }
		spin_unlock_irqrestore(&eicon_lock, flags);

		if (chan->e.complete) { /* check for rec-buffer chaining */
			if (ind->MLength == ind->RBuffer.length) {
				chan->e.complete = 1;
				idi_handle_ind(ccard, skb);
				continue;
			}
			else {
				chan->e.complete = 0;
				ind->Ind = ind->MInd;
				skb_queue_tail(&chan->e.R, skb);
				continue;
			}
		}
		else {
			if (!(skb2 = skb_dequeue(&chan->e.R))) {
				chan->e.complete = 1;
                		eicon_log(ccard, 1, "eicon: buffer incomplete, but 0 in queue\n");
	                	dev_kfree_skb(skb);
				continue;	
			}
	        	ind2 = (eicon_IND *)skb2->data;
			skb_new = alloc_skb(((sizeof(eicon_IND)-1)+ind->RBuffer.length+ind2->RBuffer.length),
					GFP_ATOMIC);
			if (!skb_new) {
                		eicon_log(ccard, 1, "eicon_io: skb_alloc failed in rcv_dispatch()\n");
	                	dev_kfree_skb(skb);
	                	dev_kfree_skb(skb2);
				continue;	
			}
			ind_new = (eicon_IND *)skb_put(skb_new,
					((sizeof(eicon_IND)-1)+ind->RBuffer.length+ind2->RBuffer.length));
			ind_new->Ind = ind2->Ind;
			ind_new->IndId = ind2->IndId;
			ind_new->IndCh = ind2->IndCh;
			ind_new->MInd = ind2->MInd;
			ind_new->MLength = ind2->MLength;
			ind_new->RBuffer.length = ind2->RBuffer.length + ind->RBuffer.length;
			memcpy(&ind_new->RBuffer.P, &ind2->RBuffer.P, ind2->RBuffer.length);
			memcpy((&ind_new->RBuffer.P)+ind2->RBuffer.length, &ind->RBuffer.P, ind->RBuffer.length);
                	dev_kfree_skb(skb);
                	dev_kfree_skb(skb2);
			if (ind->MLength == ind->RBuffer.length) {
				chan->e.complete = 2;
				idi_handle_ind(ccard, skb_new);
				continue;
			}
			else {
				chan->e.complete = 0;
				skb_queue_tail(&chan->e.R, skb_new);
				continue;
			}
		}
	}
}

void
eicon_io_ack_dispatch(eicon_card *ccard) {
        struct sk_buff *skb;

        if (!ccard) {
		eicon_log(ccard, 1, "eicon_err: NULL card in ack_dispatch!\n");
                return;
        }
	while((skb = skb_dequeue(&ccard->rackq))) {
		idi_handle_ack(ccard, skb);
	}
}


/*
 *  IO-Functions for ISA cards
 */

u8 ram_inb(eicon_card *card, void *adr) {
        u32 addr = (u32) adr;
	
	return(readb(addr));
}

u16 ram_inw(eicon_card *card, void *adr) {
        u32 addr = (u32) adr;

	return(readw(addr));
}

void ram_outb(eicon_card *card, void *adr, u8 data) {
        u32 addr = (u32) adr;

	writeb(data, addr);
}

void ram_outw(eicon_card *card, void *adr , u16 data) {
        u32 addr = (u32) adr;

	writew(data, addr);
}

void ram_copyfromcard(eicon_card *card, void *adrto, void *adr, int len) {
	memcpy_fromio(adrto, adr, len);
}

void ram_copytocard(eicon_card *card, void *adrto, void *adr, int len) {
	memcpy_toio(adrto, adr, len);
}


#ifdef CONFIG_ISDN_DRV_EICON_PCI
/*
 *  IDI-Callback function
 */
void
eicon_idi_callback(ENTITY *de)
{
	eicon_card *ccard = (eicon_card *)de->R;
	struct sk_buff *skb;
	eicon_RC *ack;
	eicon_IND *ind;
	int len = 0;

	if (de->complete == 255) {
		/* Return Code */
		skb = alloc_skb(sizeof(eicon_RC), GFP_ATOMIC);
		if (!skb) {
			eicon_log(ccard, 1, "eicon_io: skb_alloc failed in _idi_callback()\n");
		} else {
			ack = (eicon_RC *)skb_put(skb, sizeof(eicon_RC));
			ack->Rc = de->Rc;
			if (de->Rc == ASSIGN_OK) {
				ack->RcId = de->Id;
				de->user[1] = de->Id;
			} else {
				ack->RcId = de->user[1];
			}
			ack->RcCh = de->RcCh;
			ack->Reference = de->user[0];
			skb_queue_tail(&ccard->rackq, skb);
			eicon_schedule_ack(ccard);
			eicon_log(ccard, 128, "idi_cbk: Ch%d: Rc=%x Id=%x RLen=%x compl=%x\n",
				de->user[0], de->Rc, ack->RcId, de->RLength, de->complete);
		}
	} else {
		/* Indication */
		if (de->complete) {
			len = de->RLength;
		} else {
			len = 270;
			if (de->RLength <= 270)
				eicon_log(ccard, 1, "eicon_cbk: ind not complete but <= 270\n");
		}
		skb = alloc_skb((sizeof(eicon_IND) + len - 1), GFP_ATOMIC);
		if (!skb) {
			eicon_log(ccard, 1, "eicon_io: skb_alloc failed in _idi_callback()\n");
		} else {
			ind = (eicon_IND *)skb_put(skb, (sizeof(eicon_IND) + len - 1));
			ind->Ind = de->Ind;
			ind->IndId = de->user[1];
			ind->IndCh = de->IndCh;
			ind->MInd  = de->Ind;
			ind->RBuffer.length = len;
			ind->MLength = de->RLength;
			memcpy(&ind->RBuffer.P, &de->RBuffer->P, len);
			skb_queue_tail(&ccard->rcvq, skb);
			eicon_schedule_rx(ccard);
			eicon_log(ccard, 128, "idi_cbk: Ch%d: Ind=%x Id=%x RLen=%x compl=%x\n",
				de->user[0], de->Ind, ind->IndId, de->RLength, de->complete);
		}
	}

	de->RNum = 0;
	de->RNR = 0;
	de->Rc = 0;
	de->Ind = 0;
}
#endif /* CONFIG_ISDN_DRV_EICON_PCI */

/*
 *  Transmit-Function
 */
void
eicon_io_transmit(eicon_card *ccard) {
        eicon_isa_card *isa_card;
        struct sk_buff *skb;
        struct sk_buff *skb2;
        unsigned long flags;
	eicon_pr_ram  *prram = 0;
	eicon_isa_com	*com = 0;
	eicon_REQ *ReqOut = 0;
	eicon_REQ *reqbuf = 0;
	eicon_chan *chan;
	eicon_chan_ptr *chan2;
	int ReqCount;
	int scom = 0;
	int tmp = 0;
	int tmpid = 0;
	int quloop = 1;
	int dlev = 0;
	ENTITY *ep = 0;

	isa_card = &ccard->hwif.isa;

        if (!ccard) {
               	eicon_log(ccard, 1, "eicon_transmit: NULL card!\n");
                return;
        }

	switch(ccard->type) {
#ifdef CONFIG_ISDN_DRV_EICON_ISA
		case EICON_CTYPE_S:
		case EICON_CTYPE_SX:
		case EICON_CTYPE_SCOM:
		case EICON_CTYPE_QUADRO:
			scom = 1;
			com = (eicon_isa_com *)isa_card->shmem;
			break;
		case EICON_CTYPE_S2M:
			scom = 0;
			prram = (eicon_pr_ram *)isa_card->shmem;
			break;
#endif
#ifdef CONFIG_ISDN_DRV_EICON_PCI
		case EICON_CTYPE_MAESTRAP:
			scom = 2;
			break;
		case EICON_CTYPE_MAESTRAQ:
			scom = 2;
			break;
		case EICON_CTYPE_MAESTRA:
			scom = 2;
			break;
#endif
		default:
                	eicon_log(ccard, 1, "eicon_transmit: unsupported card-type!\n");
			return;
	}

	ReqCount = 0;
	if (!(skb2 = skb_dequeue(&ccard->sndq)))
		quloop = 0; 
	while(quloop) { 
		spin_lock_irqsave(&eicon_lock, flags);
		switch (scom) {
		  case 1:
			if ((ram_inb(ccard, &com->Req)) || (ccard->ReadyInt)) {
				if (!ccard->ReadyInt) {
					tmp = ram_inb(ccard, &com->ReadyInt) + 1;
					ram_outb(ccard, &com->ReadyInt, tmp);
					ccard->ReadyInt++;
				}
				spin_unlock_irqrestore(&eicon_lock, flags);
                	        skb_queue_head(&ccard->sndq, skb2);
       	                	eicon_log(ccard, 32, "eicon: transmit: Card not ready\n");
	                        return;
			}
			break;
		  case 0:
	                if (!(ram_inb(ccard, &prram->ReqOutput) - ram_inb(ccard, &prram->ReqInput))) {
				spin_unlock_irqrestore(&eicon_lock, flags);
                	        skb_queue_head(&ccard->sndq, skb2);
       	                	eicon_log(ccard, 32, "eicon: transmit: Card not ready\n");
	                        return;
        	        }
			break;
		}
		spin_unlock_irqrestore(&eicon_lock, flags);

		chan2 = (eicon_chan_ptr *)skb2->data;
		chan = chan2->ptr;
		if (!chan->e.busy) {
		 if((skb = skb_dequeue(&chan->e.X))) { 

		  reqbuf = (eicon_REQ *)skb->data;
		  if ((reqbuf->Reference) && (chan->e.B2Id == 0) && (reqbuf->ReqId & 0x1f)) {
			eicon_log(ccard, 16, "eicon: transmit: error Id=0 on %d (Net)\n", chan->No); 
		  } else {
			spin_lock_irqsave(&eicon_lock, flags);

			switch (scom) {
			  case 1:
				ram_outw(ccard, &com->XBuffer.length, reqbuf->XBuffer.length);
				ram_copytocard(ccard, &com->XBuffer.P, &reqbuf->XBuffer.P, reqbuf->XBuffer.length);
				ram_outb(ccard, &com->ReqCh, reqbuf->ReqCh);
				break;	
			  case 0:
				/* get address of next available request buffer */
				ReqOut = (eicon_REQ *)&prram->B[ram_inw(ccard, &prram->NextReq)];
				ram_outw(ccard, &ReqOut->XBuffer.length, reqbuf->XBuffer.length);
				ram_copytocard(ccard, &ReqOut->XBuffer.P, &reqbuf->XBuffer.P, reqbuf->XBuffer.length);
				ram_outb(ccard, &ReqOut->ReqCh, reqbuf->ReqCh);
				ram_outb(ccard, &ReqOut->Req, reqbuf->Req); 
				break;
			}

			dlev = 160;

			if (reqbuf->ReqId & 0x1f) { /* if this is no ASSIGN */

				if (!reqbuf->Reference) { /* Signal Layer */
					switch (scom) {
					  case 1:
						ram_outb(ccard, &com->ReqId, chan->e.D3Id); 
						break;
					  case 0:
						ram_outb(ccard, &ReqOut->ReqId, chan->e.D3Id); 
						break;
					  case 2:
						ep = &chan->de;
						break;
					}
					tmpid = chan->e.D3Id;
					chan->e.ReqCh = 0; 
				}
				else {			/* Net Layer */
					switch(scom) {
					  case 1:
						ram_outb(ccard, &com->ReqId, chan->e.B2Id); 
						break;
					  case 0:
						ram_outb(ccard, &ReqOut->ReqId, chan->e.B2Id); 
						break;
					  case 2:
						ep = &chan->be;
						break;
					}
					tmpid = chan->e.B2Id;
					chan->e.ReqCh = 1;
					if (((reqbuf->Req & 0x0f) == 0x08) ||
					   ((reqbuf->Req & 0x0f) == 0x01)) { /* Send Data */
						chan->waitq = reqbuf->XBuffer.length;
						chan->waitpq += reqbuf->XBuffer.length;
						dlev = 128;
					}
				}

			} else {	/* It is an ASSIGN */

				switch(scom) {
				  case 1:
					ram_outb(ccard, &com->ReqId, reqbuf->ReqId); 
					break;
				  case 0:
					ram_outb(ccard, &ReqOut->ReqId, reqbuf->ReqId); 
					break;
				  case 2:
					if (!reqbuf->Reference) 
						ep = &chan->de;
					else
						ep = &chan->be;
					ep->Id = reqbuf->ReqId;
					break;
				}
				tmpid = reqbuf->ReqId;

				if (!reqbuf->Reference) 
					chan->e.ReqCh = 0; 
				 else
					chan->e.ReqCh = 1; 
			} 

			switch(scom) {
			  case 1:
			 	chan->e.ref = ccard->ref_out++;
				break;
			  case 0:
			 	chan->e.ref = ram_inw(ccard, &ReqOut->Reference);
				break;
			  case 2:
				chan->e.ref = chan->No;
				break;
			}

			chan->e.Req = reqbuf->Req;
			ReqCount++; 

			switch (scom) {
			  case 1:
				ram_outb(ccard, &com->Req, reqbuf->Req); 
				break;
			  case 0:
				ram_outw(ccard, &prram->NextReq, ram_inw(ccard, &ReqOut->next)); 
				break;
			  case 2:
#ifdef CONFIG_ISDN_DRV_EICON_PCI
				if (!ep) break;
				ep->callback = eicon_idi_callback;
				ep->R = (BUFFERS *)ccard;
				ep->user[0] = (word)chan->No;
				ep->user[1] = (word)tmpid;
				ep->XNum = 1;
				ep->RNum = 0;
				ep->RNR = 0;
				ep->Rc = 0;
				ep->Ind = 0;
				ep->X->PLength = reqbuf->XBuffer.length;
				memcpy(ep->X->P, &reqbuf->XBuffer.P, reqbuf->XBuffer.length);
				ep->ReqCh = reqbuf->ReqCh;
				ep->Req = reqbuf->Req;
#endif
				break;
			}

			chan->e.busy = 1;
			spin_unlock_irqrestore(&eicon_lock, flags);
	               	eicon_log(ccard, dlev, "eicon: Req=%d Id=%x Ch=%d Len=%d Ref=%d\n", 
					reqbuf->Req, tmpid, 
					reqbuf->ReqCh, reqbuf->XBuffer.length,
					chan->e.ref); 
#ifdef CONFIG_ISDN_DRV_EICON_PCI
			if (scom == 2) {
				if (ep) {
					ccard->d->request(ep);
					if (ep->Rc)
						eicon_idi_callback(ep);
				}
			}
#endif
		  }
		  dev_kfree_skb(skb);
		 }
		 dev_kfree_skb(skb2);
		} 
		else {
			skb_queue_tail(&ccard->sackq, skb2);
        	       	eicon_log(ccard, 128, "eicon: transmit: busy chan %d\n", chan->No); 
		}

		switch(scom) {
			case 1:
				quloop = 0;
				break;
			case 0:
			case 2:
				if (!(skb2 = skb_dequeue(&ccard->sndq)))
					quloop = 0;
				break;
		}

	}
	if (!scom)
		ram_outb(ccard, &prram->ReqInput, (__u8)(ram_inb(ccard, &prram->ReqInput) + ReqCount)); 

	while((skb = skb_dequeue(&ccard->sackq))) { 
		skb_queue_tail(&ccard->sndq, skb);
	}
}

#ifdef CONFIG_ISDN_DRV_EICON_ISA
/*
 * IRQ handler 
 */
void
eicon_irq(int irq, void *dev_id, struct pt_regs *regs) {
	eicon_card *ccard = (eicon_card *)dev_id;
        eicon_isa_card *isa_card;
	eicon_pr_ram  *prram = 0;
	eicon_isa_com	*com = 0;
        eicon_RC *RcIn;
        eicon_IND *IndIn;
	struct sk_buff *skb;
        int Count = 0;
	int Rc = 0;
	int Ind = 0;
	unsigned char *irqprobe = 0;
	int scom = 0;
	int tmp = 0;
	int dlev = 0;


        if (!ccard) {
                eicon_log(ccard, 1, "eicon_irq: spurious interrupt %d\n", irq);
                return;
        }

	if (ccard->type == EICON_CTYPE_QUADRO) {
		tmp = 4;
		while(tmp) {
			com = (eicon_isa_com *)ccard->hwif.isa.shmem;
			if ((readb(ccard->hwif.isa.intack))) { /* quadro found */
				break;
			}
			ccard = ccard->qnext;
			tmp--;
		}
	}

	isa_card = &ccard->hwif.isa;

	switch(ccard->type) {
		case EICON_CTYPE_S:
		case EICON_CTYPE_SX:
		case EICON_CTYPE_SCOM:
		case EICON_CTYPE_QUADRO:
			scom = 1;
			com = (eicon_isa_com *)isa_card->shmem;
			irqprobe = &isa_card->irqprobe;
			break;
		case EICON_CTYPE_S2M:
			scom = 0;
			prram = (eicon_pr_ram *)isa_card->shmem;
			irqprobe = &isa_card->irqprobe;
			break;
		default:
                	eicon_log(ccard, 1, "eicon_irq: unsupported card-type!\n");
			return;
	}

	if (*irqprobe) {
		switch(ccard->type) {
			case EICON_CTYPE_S:
			case EICON_CTYPE_SX:
			case EICON_CTYPE_SCOM:
			case EICON_CTYPE_QUADRO:
				if (readb(isa_card->intack)) {
        		               	writeb(0, &com->Rc);
					writeb(0, isa_card->intack);
				}
				(*irqprobe)++;
				break;
			case EICON_CTYPE_S2M:
				if (readb(isa_card->intack)) {
        		               	writeb(0, &prram->RcOutput);
					writeb(0, isa_card->intack);
				}
				(*irqprobe)++;
				break;
		}
		return;
	}

	switch(ccard->type) {
		case EICON_CTYPE_S:
		case EICON_CTYPE_SX:
		case EICON_CTYPE_SCOM:
		case EICON_CTYPE_QUADRO:
		case EICON_CTYPE_S2M:
			if (!(readb(isa_card->intack))) { /* card did not interrupt */
				eicon_log(ccard, 1, "eicon: IRQ: card reports no interrupt!\n");
				return;
			} 
			break;
	}

    if (scom) {

        /* if a return code is available ...  */
	if ((tmp = ram_inb(ccard, &com->Rc))) {
		eicon_RC *ack;
		if (tmp == READY_INT) {
                       	eicon_log(ccard, 64, "eicon: IRQ Rc=READY_INT\n");
			if (ccard->ReadyInt) {
				ccard->ReadyInt--;
				ram_outb(ccard, &com->Rc, 0);
				eicon_schedule_tx(ccard);
			}
		} else {
			skb = alloc_skb(sizeof(eicon_RC), GFP_ATOMIC);
			if (!skb) {
                		eicon_log(ccard, 1, "eicon_io: skb_alloc failed in _irq()\n");
			} else {
				ack = (eicon_RC *)skb_put(skb, sizeof(eicon_RC));
				ack->Rc = tmp;
				ack->RcId = ram_inb(ccard, &com->RcId);
				ack->RcCh = ram_inb(ccard, &com->RcCh);
				ack->Reference = ccard->ref_in++;
               	        	eicon_log(ccard, 128, "eicon: IRQ Rc=%d Id=%x Ch=%d Ref=%d\n",
					tmp,ack->RcId,ack->RcCh,ack->Reference);
				skb_queue_tail(&ccard->rackq, skb);
				eicon_schedule_ack(ccard);
			}
			ram_outb(ccard, &com->Req, 0);
			ram_outb(ccard, &com->Rc, 0);
		}

	} else {

	        /* if an indication is available ...  */
		if ((tmp = ram_inb(ccard, &com->Ind))) {
			eicon_IND *ind;
			int len = ram_inw(ccard, &com->RBuffer.length);
			skb = alloc_skb((sizeof(eicon_IND) + len - 1), GFP_ATOMIC);
			if (!skb) {
                		eicon_log(ccard, 1, "eicon_io: skb_alloc failed in _irq()\n");
			} else {
				ind = (eicon_IND *)skb_put(skb, (sizeof(eicon_IND) + len - 1));
				ind->Ind = tmp;
				ind->IndId = ram_inb(ccard, &com->IndId);
				ind->IndCh = ram_inb(ccard, &com->IndCh);
				ind->MInd  = ram_inb(ccard, &com->MInd);
				ind->MLength = ram_inw(ccard, &com->MLength);
				ind->RBuffer.length = len;
				if ((tmp == 1) || (tmp == 8))
					dlev = 128;
				else
					dlev = 192;
                       		eicon_log(ccard, dlev, "eicon: IRQ Ind=%d Id=%x Ch=%d MInd=%d MLen=%d Len=%d\n",
					tmp,ind->IndId,ind->IndCh,ind->MInd,ind->MLength,len);
				ram_copyfromcard(ccard, &ind->RBuffer.P, &com->RBuffer.P, len);
				skb_queue_tail(&ccard->rcvq, skb);
				eicon_schedule_rx(ccard);
			}
			ram_outb(ccard, &com->Ind, 0);
		}
	}

    } else {

        /* if return codes are available ...  */
        if((Count = ram_inb(ccard, &prram->RcOutput))) {
		eicon_RC *ack;
                /* get the buffer address of the first return code */
                RcIn = (eicon_RC *)&prram->B[ram_inw(ccard, &prram->NextRc)];
                /* for all return codes do ...  */
                while(Count--) {

                        if((Rc=ram_inb(ccard, &RcIn->Rc))) {
				skb = alloc_skb(sizeof(eicon_RC), GFP_ATOMIC);
				if (!skb) {
                			eicon_log(ccard, 1, "eicon_io: skb_alloc failed in _irq()\n");
				} else {
					ack = (eicon_RC *)skb_put(skb, sizeof(eicon_RC));
					ack->Rc = Rc;
					ack->RcId = ram_inb(ccard, &RcIn->RcId);
					ack->RcCh = ram_inb(ccard, &RcIn->RcCh);
					ack->Reference = ram_inw(ccard, &RcIn->Reference);
        	                	eicon_log(ccard, 128, "eicon: IRQ Rc=%d Id=%x Ch=%d Ref=%d\n",
						Rc,ack->RcId,ack->RcCh,ack->Reference);
					skb_queue_tail(&ccard->rackq, skb);
					eicon_schedule_ack(ccard);
				}
                       		ram_outb(ccard, &RcIn->Rc, 0);
                        }
                        /* get buffer address of next return code   */
                        RcIn = (eicon_RC *)&prram->B[ram_inw(ccard, &RcIn->next)];
                }
                /* clear all return codes (no chaining!) */
                ram_outb(ccard, &prram->RcOutput, 0);
        }

        /* if indications are available ... */
        if((Count = ram_inb(ccard, &prram->IndOutput))) {
		eicon_IND *ind;
                /* get the buffer address of the first indication */
                IndIn = (eicon_IND *)&prram->B[ram_inw(ccard, &prram->NextInd)];
                /* for all indications do ... */
                while(Count--) {
			Ind = ram_inb(ccard, &IndIn->Ind);
			if(Ind) {
				int len = ram_inw(ccard, &IndIn->RBuffer.length);
				skb = alloc_skb((sizeof(eicon_IND) + len - 1), GFP_ATOMIC);
				if (!skb) {
                			eicon_log(ccard, 1, "eicon_io: skb_alloc failed in _irq()\n");
				} else {
					ind = (eicon_IND *)skb_put(skb, (sizeof(eicon_IND) + len - 1));
					ind->Ind = Ind;
					ind->IndId = ram_inb(ccard, &IndIn->IndId);
					ind->IndCh = ram_inb(ccard, &IndIn->IndCh);
					ind->MInd  = ram_inb(ccard, &IndIn->MInd);
					ind->MLength = ram_inw(ccard, &IndIn->MLength);
					ind->RBuffer.length = len;
					if ((Ind == 1) || (Ind == 8))
						dlev = 128;
					else
						dlev = 192;
                	        	eicon_log(ccard, dlev, "eicon: IRQ Ind=%d Id=%x Ch=%d MInd=%d MLen=%d Len=%d\n",
						Ind,ind->IndId,ind->IndCh,ind->MInd,ind->MLength,len);
	                                ram_copyfromcard(ccard, &ind->RBuffer.P, &IndIn->RBuffer.P, len);
					skb_queue_tail(&ccard->rcvq, skb);
					eicon_schedule_rx(ccard);
				}
				ram_outb(ccard, &IndIn->Ind, 0);
                        }
                        /* get buffer address of next indication  */
                        IndIn = (eicon_IND *)&prram->B[ram_inw(ccard, &IndIn->next)];
                }
                ram_outb(ccard, &prram->IndOutput, 0);
        }

    } 

	/* clear interrupt */
	switch(ccard->type) {
		case EICON_CTYPE_QUADRO:
			writeb(0, isa_card->intack);
			writeb(0, &com[0x401]);
			break;
		case EICON_CTYPE_S:
		case EICON_CTYPE_SX:
		case EICON_CTYPE_SCOM:
		case EICON_CTYPE_S2M:
			writeb(0, isa_card->intack);
			break;
	}

  return;
}
#endif
