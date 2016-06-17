/*
 * External IDI interface
 *
 * Copyright (C) Eicon Technology Corporation, 2000.
 *
 * Eicon File Revision :    1.0  
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#if !defined(IDI_H)
#define IDI_H

#include "sys.h"

/* typedefs for our data structures */

typedef struct get_name_s GET_NAME;
typedef struct entity_s ENTITY;
typedef struct buffers_s BUFFERS;

/* IDI request/callback function pointer */

typedef void (* IDI_CALL)(ENTITY *);

typedef struct {
  word length;          /* length of data/parameter field           */
  byte P[270];          /* data/parameter field                     */
} DBUFFER;

#define REQ_NAME	0x0100
#define BOARD_NAME_LENGTH 9
struct get_name_s {
  word command;         /* command = 0x0100 */
  byte name[BOARD_NAME_LENGTH];
};

#define REQ_REMOVE    0x0000    /* pointer to word which is 0 */
#define REQ_SERIAL    0x0200  
struct get_serial_s {
  word      command;            /* command = 0x0200 */
  dword     serial;             /* serial number */
};

#define REQ_POSTCALL  0x0300  
struct postcall_s {
  word        command;         /* command = 0x0300 */
  word        dummy;           /* not used */
  IDI_CALL    callback;        /* routine address to call back */
  ENTITY      *contxt;         /* ptr to entity to use */
};

#define REQ_XLOG      0x0400   /* structure is card dependent/defined locally */

struct buffers_s {
  word PLength;
  byte *P;
};

struct entity_s {
  byte                  Req;            /* pending request          */
  byte                  Rc;             /* return code received     */
  byte                  Ind;            /* indication received      */
  byte                  ReqCh;          /* channel of current Req   */
  byte                  RcCh;           /* channel of current Rc    */
  byte                  IndCh;          /* channel of current Ind   */
  byte                  Id;             /* ID used by this entity   */
  byte                  GlobalId;       /* reserved field           */
  byte                  XNum;           /* number of X-buffers      */
  byte                  RNum;           /* number of R-buffers      */
  BUFFERS               *X;        		/* pointer to X-buffer list */
  BUFFERS               *R;        		/* pointer to R-buffer list */
  word                  RLength;        /* length of current R-data */
  DBUFFER               *RBuffer;       /* buffer of current R-data */
  byte                  RNR;            /* receive not ready flag   */
  byte                  complete;       /* receive complete status  */
  IDI_CALL              callback;

  word                  user[2];

        /* fields used by the driver internally                     */
  byte                  No;             /* entity number            */
  byte                  reserved2;      /* reserved field           */
  byte                  More;           /* R/X More flags           */
  byte                  MInd;           /* MDATA coding for this ID */
  byte                  XCurrent;       /* current transmit buffer  */
  byte                  RCurrent;       /* current receive buffer   */
  word                  XOffset;        /* offset in x-buffer       */
  word                  ROffset;        /* offset in r-buffer       */
};

typedef struct {
  byte                  type;
  byte                  channels;
  word                  features;
  /* dword		serial; */
  IDI_CALL              request;
} DESCRIPTOR;

extern void    DIVA_DIDD_Read(DESCRIPTOR *, int);

        /* descriptor type field coding */
#define IDI_ADAPTER_S           1
#define IDI_ADAPTER_PR          2
#define IDI_ADAPTER_DIVA        3
#define IDI_ADAPTER_MAESTRA     4
#define IDI_ADAPTER_MAESTRAQ    5
#define IDI_ADAPTER_MAESTRAP    6
#define IDI_VADAPTER            0x40
#define IDI_DRIVER              0x80
#define IDI_DIMAINT             0xff

/* feature bit mask values */

#define DI_VOICE        0x0 /* obsolete define */
#define DI_FAX3         0x1
#define DI_MODEM        0x2
#define DI_POST         0x4
#define DI_V110         0x8
#define DI_V120         0x10
#define DI_POTS         0x20
#define DI_CODEC        0x40
#define DI_MANAGE       0x80
#define DI_V_42         0x0100
#define DI_EXTD_FAX     0x0200 /* Extended FAX (ECM, 2D, T.6, Polling) */

#endif /* IDI_H */
