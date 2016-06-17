/*
 * Copyright (C) Eicon Technology Corporation, 2000.
 *
 * Eicon File Revision :    1.0  
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */


#if !defined(PR_PC_H)
#define PR_PC_H

struct pr_ram {
  word NextReq;         /* pointer to next Req Buffer               */
  word NextRc;          /* pointer to next Rc Buffer                */
  word NextInd;         /* pointer to next Ind Buffer               */
  byte ReqInput;        /* number of Req Buffers sent               */
  byte ReqOutput;       /* number of Req Buffers returned           */
  byte ReqReserved;     /* number of Req Buffers reserved           */
  byte Int;             /* ISDN-P interrupt                         */
  byte XLock;           /* Lock field for arbitration               */
  byte RcOutput;        /* number of Rc buffers received            */
  byte IndOutput;       /* number of Ind buffers received           */
  byte IMask;           /* Interrupt Mask Flag                      */
  byte Reserved1[2];    /* reserved field, do not use               */
  byte ReadyInt;        /* request field for ready interrupt        */
  byte Reserved2[12];   /* reserved field, do not use               */
  byte InterfaceType;   /* interface type 1=16K interface           */
  word Signature;       /* ISDN-P initialized indication            */
  byte B[1];            /* buffer space for Req,Ind and Rc          */
};

typedef struct {
  word next;
  byte Req;
  byte ReqId;
  byte ReqCh;
  byte Reserved1;
  word Reference;
  byte Reserved[8];
  PBUFFER XBuffer;
} REQ;

typedef struct {
  word next;
  byte Rc;
  byte RcId;
  byte RcCh;
  byte Reserved1;
  word Reference;
  byte Reserved2[8];
} RC;

typedef struct {
  word next;
  byte Ind;
  byte IndId;
  byte IndCh;
  byte MInd;
  word MLength;
  word Reference;
  byte RNR;
  byte Reserved;
  dword Ack;
  PBUFFER RBuffer;
} IND;

#endif
