/*
 * Core driver for Diva Server cards
 * Implements the IDI interface
 *
 * Copyright (C) Eicon Technology Corporation, 2000.
 *
 * Eicon File Revision :    1.8  
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#include "idi.h"
#include "adapter.h"
#include "pc.h"
#include "pr_pc.h"
#include "sys.h"
#include "uxio.h"

/* IDI request functions */

static void	request(card_t *card, ENTITY *e);

static void req_0(ENTITY *e)	{ request(&DivasCards[ 0], e); }
static void req_1(ENTITY *e)	{ request(&DivasCards[ 1], e); }
static void req_2(ENTITY *e)	{ request(&DivasCards[ 2], e); }
static void req_3(ENTITY *e)	{ request(&DivasCards[ 3], e); }
static void req_4(ENTITY *e)	{ request(&DivasCards[ 4], e); }
static void req_5(ENTITY *e)	{ request(&DivasCards[ 5], e); }
static void req_6(ENTITY *e)	{ request(&DivasCards[ 6], e); }
static void req_7(ENTITY *e)	{ request(&DivasCards[ 7], e); }
static void req_8(ENTITY *e)	{ request(&DivasCards[ 8], e); }
static void req_9(ENTITY *e)	{ request(&DivasCards[ 9], e); }
static void req_10(ENTITY *e)	{ request(&DivasCards[10], e); }
static void req_11(ENTITY *e)	{ request(&DivasCards[11], e); }
static void req_12(ENTITY *e)	{ request(&DivasCards[12], e); }
static void req_13(ENTITY *e)	{ request(&DivasCards[13], e); }
static void req_14(ENTITY *e)	{ request(&DivasCards[14], e); }
static void req_15(ENTITY *e)	{ request(&DivasCards[15], e); }

IDI_CALL DivasIdiRequest[16] =
{
    &req_0,		&req_1,		&req_2,		&req_3,
    &req_4,		&req_5,		&req_6,		&req_7,
    &req_8,		&req_9,		&req_10,	&req_11,
	&req_12,	&req_13,	&req_14,	&req_15
};

#define PR_RAM  ((struct pr_ram *)0)
#define RAM ((struct dual *)0)

/*------------------------------------------------------------------*/
/* local function prototypes                                        */
/*------------------------------------------------------------------*/

static byte isdn_rc(ADAPTER *, byte, byte, byte, word);
static byte isdn_ind(ADAPTER *, byte, byte, byte, PBUFFER *, byte, word);

/*
 * IDI related functions
 */

static
ENTITY	*entity_ptr(ADAPTER *a, byte e_no)
{
	card_t	*card;

	card = a->io;

	return card->e_tbl[e_no].e;
}

static
void	CALLBACK(ADAPTER *a, ENTITY *e)
{
	card_t	*card = a->io;

	if (card->log_types & DIVAS_LOG_IDI)
	{
		DivasLogIdi(card, e, FALSE);
	}

	(*e->callback)(e);
}

static
void	*PTR_P(ADAPTER *a, ENTITY *e, void *P)
{
	return(P);
}

static
void	*PTR_R(ADAPTER *a, ENTITY *e)
{
	return((void*)e->R);
}

static
void	*PTR_X(ADAPTER *a, ENTITY *e)
{
	return((void*)e->X);
}

static
void	free_entity(ADAPTER *a, byte e_no) 
{
	card_t	*card;
	int		ipl;

	card = a->io;

	ipl = UxCardLock(card->hw);

	card->e_tbl[e_no].e = NULL;
	card->e_count--;

	UxCardUnlock(card->hw, ipl);

	return;
}

static
void	assign_queue(ADAPTER * a, byte e_no, word ref)
{
	card_t	*card;
	int		ipl;

	card = a->io;

	ipl = UxCardLock(card->hw);

	card->e_tbl[e_no].assign_ref = ref;
	card->e_tbl[e_no].next = card->assign;
	card->assign = e_no;

	UxCardUnlock(card->hw, ipl);

	return;
}

static
byte	get_assign(ADAPTER *a, word ref)
{
	card_t	*card;
	byte	e_no;
	int		ipl;

	card = a->io;

	ipl = UxCardLock(card->hw);

	e_no = (byte)card->assign;
	while (e_no)
	{
		if (card->e_tbl[e_no].assign_ref == ref)
		{
			break;
		}
		e_no = card->e_tbl[e_no].next;
	}

	UxCardUnlock(card->hw, ipl);

	return e_no;
}

static
void	req_queue(ADAPTER * a, byte e_no)
{
	card_t	*card;
	int		ipl;

	card = a->io;

	ipl = UxCardLock(card->hw);

	card->e_tbl[e_no].next = 0;

	if (card->e_head)
	{
		card->e_tbl[card->e_tail].next = e_no;
		card->e_tail = e_no;
	}
	else
	{
		card->e_head = e_no;
		card->e_tail = e_no;
	}

	UxCardUnlock(card->hw, ipl);

	return;
}

static
byte	look_req(ADAPTER * a)
{
	card_t	*card;

	card = a->io;

	return(card->e_head);
}

static
void	next_req(ADAPTER * a)
{
	card_t	*card;
	int		ipl;


	card = a->io;

	ipl = UxCardLock(card->hw);

	card->e_head = card->e_tbl[card->e_head].next;
	if (!card->e_head)
	{
		card->e_tail = 0;
	}

	UxCardUnlock(card->hw, ipl);

	return;
}


/*
 * IDI request function for active cards
 */
static
void	request(card_t *card, ENTITY *e)
{
	word	*special_req;
	int		i;
	int		ipl;


	if (card->log_types & DIVAS_LOG_IDI)
	{
		DivasLogIdi(card, e, TRUE);
	}

	if (!e->Req)
	{
		special_req = (word *) e;

		switch (*special_req)
		{
		case REQ_REMOVE:
			return;

		case REQ_NAME:
			for (i=0; i < DIM(card->cfg.name); i++)
			{
				((struct get_name_s *) e)->name[i] = card->cfg.name[i];
			}
			return;

		case REQ_SERIAL:
		case REQ_XLOG:
			DPRINTF(("IDI: attempted REQ_SERIAL or REQ_XLOG"));
			return;

		default:
			return;
		}
	}

	ipl = UxCardLock(card->hw);

   	if (!(e->Id & 0x1f))
	{
		DPRINTF(("IDI: ASSIGN req"));

		for (i = 1; i < card->e_max; i++)
		{
			if (!card->e_tbl[i].e)
			{
				break;
			}
		}

		if (i == card->e_max)
		{
			DPRINTF(("IDI: request all ids in use (IDI req ignored)"));
			UxCardUnlock(card->hw, ipl);
			e->Rc = OUT_OF_RESOURCES;
			return;
		}

		card->e_tbl[i].e = e;
		card->e_count++;

		e->No = (byte) i;
		e->More = 0;
		e->RCurrent = 0xff;
	}
	else
	{
		i = e->No;
	}
    
    if (e->More & XBUSY)
	{
		DPRINTF(("IDI: request - entity is busy"));
		UxCardUnlock(card->hw, ipl);
		return;
	}

	e->More |= XBUSY;
	e->More &= ~ XMOREF;
	e->XCurrent = 0;
	e->XOffset = 0;

	card->e_tbl[i].next = 0;

	if(card->e_head)
	{
		card->e_tbl[card->e_tail].next = i;
		card->e_tail = i;
	}
	else
	{
		card->e_head = i;
		card->e_tail = i;
	}

	UxCardUnlock(card->hw, ipl);

	DivasScheduleRequestDpc();

	return;
}

static byte pr_ready(ADAPTER * a)
{
  byte ReadyCount;

  ReadyCount = (byte)(a->ram_in(a, &PR_RAM->ReqOutput) -
                      a->ram_in(a, &PR_RAM->ReqInput));

  if(!ReadyCount) {
    if(!a->ReadyInt) {
      a->ram_inc(a, &PR_RAM->ReadyInt);
      a->ReadyInt++;
    }
  }
  return ReadyCount;
}

/*------------------------------------------------------------------*/
/* output function                                                  */
/*------------------------------------------------------------------*/

void DivasOut(ADAPTER * a)
{
  byte e_no;
  ENTITY  * this = NULL;
  BUFFERS  *X;
  word length;
  word i;
  word clength;
  REQ * ReqOut;
  byte more;
  byte ReadyCount;
  byte ReqCount;
  byte Id;

        /* while a request is pending ...                           */
  e_no = look_req(a);
  if(!e_no)
  {
    return;
  }

  ReadyCount = pr_ready(a);
  if(!ReadyCount)
  {
    DPRINTF(("IDI: card not ready for next request"));
    return;
  }

  ReqCount = 0;
  while(e_no && ReadyCount) {

    next_req(a);

    this = entity_ptr(a, e_no);

#ifdef	USE_EXTENDED_DEBUGS
	if ( !this )
	{
		ISDN_ADAPTER *io = (ISDN_ADAPTER *)a->io ;
		DBG_FTL(("!A%d ==> NULL entity ptr - try to ignore", (int)io->ANum))
		e_no = look_req(a) ;
		ReadyCount-- ;
		continue ;
	}
	{
		ISDN_ADAPTER *io = (ISDN_ADAPTER *)a->io ;
		DPRINTF(("IDI: >A%d Id=0x%x Req=0x%x", io->ANum, this->Id, this->Req))
	}
#else
    DPRINTF(("IDI: >REQ=%x,Id=%x,Ch=%x",this->Req,this->Id,this->ReqCh));
#endif

        /* get address of next available request buffer             */
    ReqOut = (REQ *)&PR_RAM->B[a->ram_inw(a, &PR_RAM->NextReq)];

        /* now copy the data from the current data buffer into the  */
        /* adapters request buffer                                  */
    length = 0;
    i = this->XCurrent;
    X = PTR_X(a,this);
    while(i<this->XNum && length<270) {
      clength = (word)(270-length);
      if (clength > X[i].PLength-this->XOffset)
	      clength = X[i].PLength-this->XOffset;
      a->ram_out_buffer(a,
                        &ReqOut->XBuffer.P[length],
                        PTR_P(a,this,&X[i].P[this->XOffset]),
                        clength);

      length +=clength;
      this->XOffset +=clength;
      if(this->XOffset==X[i].PLength) {
        this->XCurrent = (byte)++i;
        this->XOffset = 0;
      }
    }

    a->ram_outw(a, &ReqOut->XBuffer.length, length);
    a->ram_out(a, &ReqOut->ReqId, this->Id);
    a->ram_out(a, &ReqOut->ReqCh, this->ReqCh);

        /* if its a specific request (no ASSIGN) ...                */

    if(this->Id &0x1f) {

        /* if buffers are left in the list of data buffers do       */
        /* do chaining (LL_MDATA, N_MDATA)                          */

      this->More++;
      if(i<this->XNum && this->MInd) {
        a->ram_out(a, &ReqOut->Req, this->MInd);
        more = TRUE;
      }
      else {
        this->More |=XMOREF;
        a->ram_out(a, &ReqOut->Req, this->Req);
        more = FALSE;
      }

        /* if we did chaining, this entity is put back into the     */
        /* request queue                                            */

      if(more) {
        req_queue(a,this->No);
      }
    }

        /* else it's a ASSIGN                                       */

    else {

        /* save the request code used for buffer chaining           */

      this->MInd = 0;
      if (this->Id==BLLC_ID) this->MInd = LL_MDATA;
      if (this->Id==NL_ID   ||
          this->Id==TASK_ID ||
          this->Id==MAN_ID
        ) this->MInd = N_MDATA;

        /* send the ASSIGN                                          */

      this->More |=XMOREF;
      a->ram_out(a, &ReqOut->Req, this->Req);

        /* save the reference of the ASSIGN                         */

      assign_queue(a, this->No, a->ram_inw(a, &ReqOut->Reference));
    }
    a->ram_outw(a, &PR_RAM->NextReq, a->ram_inw(a, &ReqOut->next));
    ReadyCount--;
    ReqCount++;

    e_no = look_req(a);
  }

        /* send the filled request buffers to the ISDN adapter      */

  a->ram_out(a, &PR_RAM->ReqInput,
             (byte)(a->ram_in(a, &PR_RAM->ReqInput) + ReqCount));

        /* if it is a 'unreturncoded' UREMOVE request, remove the  */
        /* Id from our table after sending the request             */
  if(this->Req==UREMOVE && this->Id) {
    Id = this->Id;
    e_no = a->IdTable[Id];
    free_entity(a, e_no);
    a->IdTable[Id] = 0;
    this->Id = 0;
  }

}

/*------------------------------------------------------------------*/
/* isdn interrupt handler                                           */
/*------------------------------------------------------------------*/

byte DivasDpc(ADAPTER * a)
{
  byte Count;
  RC * RcIn;
  IND * IndIn;
  byte c;
  byte RNRId;
  byte Rc;
  byte Ind;

        /* if return codes are available ...                        */
  if((Count = a->ram_in(a, &PR_RAM->RcOutput))) {

    DPRINTF(("IDI: #Rc=%x",Count));

        /* get the buffer address of the first return code          */
    RcIn = (RC *)&PR_RAM->B[a->ram_inw(a, &PR_RAM->NextRc)];

        /* for all return codes do ...                              */
    while(Count--) {

      if((Rc=a->ram_in(a, &RcIn->Rc))) {

        /* call return code handler, if it is not our return code   */
        /* the handler returns 2                                    */
        /* for all return codes we process, we clear the Rc field   */
        isdn_rc(a,
                Rc,
                a->ram_in(a, &RcIn->RcId),
                a->ram_in(a, &RcIn->RcCh),
                a->ram_inw(a, &RcIn->Reference));

        a->ram_out(a, &RcIn->Rc, 0);
      }

        /* get buffer address of next return code                   */
      RcIn = (RC *)&PR_RAM->B[a->ram_inw(a, &RcIn->next)];
    }

        /* clear all return codes (no chaining!)                    */
    a->ram_out(a, &PR_RAM->RcOutput ,0);

        /* call output function                                     */
    DivasOut(a);
  }

        /* clear RNR flag                                           */
  RNRId = 0;

        /* if indications are available ...                         */
  if((Count = a->ram_in(a, &PR_RAM->IndOutput))) {

    DPRINTF(("IDI: #Ind=%x",Count));

        /* get the buffer address of the first indication           */
    IndIn = (IND *)&PR_RAM->B[a->ram_inw(a, &PR_RAM->NextInd)];

        /* for all indications do ...                               */
    while(Count--) {

        /* if the application marks an indication as RNR, all       */
        /* indications from the same Id delivered in this interrupt */
        /* are marked RNR                                           */
      if(RNRId && RNRId==a->ram_in(a, &IndIn->IndId)) {
        a->ram_out(a, &IndIn->Ind, 0);
        a->ram_out(a, &IndIn->RNR, TRUE);
      }
      else {
        Ind = a->ram_in(a, &IndIn->Ind);
        if(Ind) {
          RNRId = 0;

        /* call indication handler, a return value of 2 means chain */
        /* a return value of 1 means RNR                            */
        /* for all indications we process, we clear the Ind field   */
          c = isdn_ind(a,
                       Ind,
                       a->ram_in(a, &IndIn->IndId),
                       a->ram_in(a, &IndIn->IndCh),
                       &IndIn->RBuffer,
                       a->ram_in(a, &IndIn->MInd),
                       a->ram_inw(a, &IndIn->MLength));

          if(c==1) {
            DPRINTF(("IDI: RNR"));
            a->ram_out(a, &IndIn->Ind, 0);
            RNRId = a->ram_in(a, &IndIn->IndId);
            a->ram_out(a, &IndIn->RNR, TRUE);
          }
        }
      }

        /* get buffer address of next indication                    */
      IndIn = (IND *)&PR_RAM->B[a->ram_inw(a, &IndIn->next)];
    }

    a->ram_out(a, &PR_RAM->IndOutput, 0);
  }
  return FALSE;
}

byte DivasTestInt(ADAPTER * a)
{
  return a->ram_in(a,(void *)0x3fe);
}

void DivasClearInt(ADAPTER * a)
{
  a->ram_out(a,(void *)0x3fe,0); 
}

/*------------------------------------------------------------------*/
/* return code handler                                              */
/*------------------------------------------------------------------*/

static
byte isdn_rc(ADAPTER * a,
             byte Rc,
             byte Id,
             byte Ch,
             word Ref)
{
  ENTITY  * this;
  byte e_no;

#ifdef	USE_EXTENDED_DEBUGS
	{
		ISDN_ADAPTER *io = (ISDN_ADAPTER *)a->io ;
		DPRINTF(("IDI: <A%d Id=0x%x Rc=0x%x", io->ANum, Id, Rc))
	}
#else
  DPRINTF(("IDI: <RC(Rc=%x,Id=%x,Ch=%x)",Rc,Id,Ch));
#endif

        /* check for ready interrupt                                */
  if(Rc==READY_INT) {
    if(a->ReadyInt) {
      a->ReadyInt--;
      return 0;
    }
    return 2;
  }

        /* if we know this Id ...                                   */
  e_no = a->IdTable[Id];
  if(e_no) {

    this = entity_ptr(a,e_no);

    this->RcCh = Ch;

        /* if it is a return code to a REMOVE request, remove the   */
        /* Id from our table                                        */
    if(this->Req==REMOVE && Rc==OK) {
      free_entity(a, e_no);
      a->IdTable[Id] = 0;
      this->Id = 0;
/**************************************************************/
      if ((this->More & XMOREC) > 1) {
        this->More &= ~XMOREC;
	this->More |= 1;
	DPRINTF(("isdn_rc, Id=%x, correct More on REMOVE", Id));
      }
    }

    if (Rc==OK_FC) {
      this->Rc = Rc;
      this->More = (this->More & (~XBUSY | XMOREC)) | 1;
      this->complete = 0xFF;
      CALLBACK(a, this);
      return 0;
    }
    if(this->More &XMOREC)
      this->More--;

        /* call the application callback function                   */
    if(this->More &XMOREF && !(this->More &XMOREC)) {
      this->Rc = Rc;
      this->More &=~XBUSY;
      this->complete=0xff;
      CALLBACK(a, this);
    }
    return 0;
  }

        /* if it's an ASSIGN return code check if it's a return     */
        /* code to an ASSIGN request from us                        */
  if((Rc &0xf0)==ASSIGN_RC) {

    e_no = get_assign(a, Ref);

    if(e_no) {

      this = entity_ptr(a,e_no);

      this->Id = Id;

        /* call the application callback function                   */
      this->Rc = Rc;
      this->More &=~XBUSY;
      this->complete=0xff;
      CALLBACK(a, this);

      if(Rc==ASSIGN_OK) {
        a->IdTable[Id] = e_no;
      }
      else
      {
        free_entity(a, e_no);
        a->IdTable[Id] = 0;
        this->Id = 0;
      }
      return 1;
    }
  }
  return 2;
}

/*------------------------------------------------------------------*/
/* indication handler                                               */
/*------------------------------------------------------------------*/

static
byte isdn_ind(ADAPTER * a,
              byte Ind,
              byte Id,
              byte Ch,
              PBUFFER * RBuffer,
              byte MInd,
              word MLength)
{
  ENTITY  * this;
  word clength;
  word offset;
  BUFFERS  *R;

#ifdef	USE_EXTENDED_DEBUGS
	{
		ISDN_ADAPTER *io = (ISDN_ADAPTER *)a->io ;
		DPRINTF(("IDI: <A%d Id=0x%x Ind=0x%x", io->ANum, Id, Ind))
	}
#else
  DPRINTF(("IDI: <IND(Ind=%x,Id=%x,Ch=%x)",Ind,Id,Ch));
#endif

  if(a->IdTable[Id]) {

    this = entity_ptr(a,a->IdTable[Id]);

    this->IndCh = Ch;

        /* if the Receive More flag is not yet set, this is the     */
        /* first buffer of the packet                               */
    if(this->RCurrent==0xff) {

        /* check for receive buffer chaining                        */
      if(Ind==this->MInd) {
        this->complete = 0;
        this->Ind = MInd;
      }
      else {
        this->complete = 1;
        this->Ind = Ind;
      }

        /* call the application callback function for the receive   */
        /* look ahead                                               */
      this->RLength = MLength;

      a->ram_look_ahead(a, RBuffer, this);

      this->RNum = 0;
      CALLBACK(a, this);

        /* map entity ptr, selector could be re-mapped by call to   */
        /* IDI from within callback                                 */
      this = entity_ptr(a,a->IdTable[Id]);

        /* check for RNR                                            */
      if(this->RNR==1) {
        this->RNR = 0;
        return 1;
      }

        /* if no buffers are provided by the application, the       */
        /* application want to copy the data itself including       */
        /* N_MDATA/LL_MDATA chaining                                */
      if(!this->RNR && !this->RNum) {
        return 0;
      }

        /* if there is no RNR, set the More flag                    */
      this->RCurrent = 0;
      this->ROffset = 0;
    }

    if(this->RNR==2) {
      if(Ind!=this->MInd) {
        this->RCurrent = 0xff;
        this->RNR = 0;
      }
      return 0;
    }
        /* if we have received buffers from the application, copy   */
        /* the data into these buffers                              */
    offset = 0;
    R = PTR_R(a,this);
    do {
      if(this->ROffset==R[this->RCurrent].PLength) {
        this->ROffset = 0;
        this->RCurrent++;
      }
      clength = a->ram_inw(a, &RBuffer->length)-offset;
      if (clength > R[this->RCurrent].PLength-this->ROffset)
	      clength = R[this->RCurrent].PLength-this->ROffset;
      if(R[this->RCurrent].P) {
        a->ram_in_buffer(a,
                         &RBuffer->P[offset],
                         PTR_P(a,this,&R[this->RCurrent].P[this->ROffset]),
                         clength);
      }
      offset +=clength;
      this->ROffset +=clength;
    } while(offset<(a->ram_inw(a, &RBuffer->length)));

        /* if it's the last buffer of the packet, call the          */
        /* application callback function for the receive complete   */
        /* call                                                     */
    if(Ind!=this->MInd) {
      R[this->RCurrent].PLength = this->ROffset;
      if(this->ROffset) this->RCurrent++;
      this->RNum = this->RCurrent;
      this->RCurrent = 0xff;
      this->Ind = Ind;
      this->complete = 2;
      CALLBACK(a, this);
    }
    return 0;
  }
  return 2;
}
