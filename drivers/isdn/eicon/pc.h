/*
 * Copyright (C) Eicon Technology Corporation, 2000.
 *
 * Eicon File Revision :    1.2  
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#ifndef PC_H_INCLUDED
#define PC_H_INCLUDED


#define byte unsigned char
#define word unsigned short
#define dword unsigned long

/*------------------------------------------------------------------*/
/* buffer definition                                                */
/*------------------------------------------------------------------*/

typedef struct {
  word length;          /* length of data/parameter field           */
  byte P[270];          /* data/parameter field                     */
} PBUFFER;

/*------------------------------------------------------------------*/
/* dual port ram structure                                          */
/*------------------------------------------------------------------*/

struct dual
{
  byte Req;             /* request register                         */
  byte ReqId;           /* request task/entity identification       */
  byte Rc;              /* return code register                     */
  byte RcId;            /* return code task/entity identification   */
  byte Ind;             /* Indication register                      */
  byte IndId;           /* Indication task/entity identification    */
  byte IMask;           /* Interrupt Mask Flag                      */
  byte RNR;             /* Receiver Not Ready (set by PC)           */
  byte XLock;           /* XBuffer locked Flag                      */
  byte Int;             /* ISDN-S interrupt                         */
  byte ReqCh;           /* Channel field for layer-3 Requests       */
  byte RcCh;            /* Channel field for layer-3 Returncodes    */
  byte IndCh;           /* Channel field for layer-3 Indications    */
  byte MInd;            /* more data indication field               */
  word MLength;         /* more data total packet length            */
  byte ReadyInt;        /* request field for ready interrupt        */
  byte SWReg;           /* Software register for special purposes   */  
  byte Reserved[11];    /* reserved space                           */
  byte InterfaceType;   /* interface type 1=16K interface           */
  word Signature;       /* ISDN-S adapter Signature (GD)            */
  PBUFFER XBuffer;      /* Transmit Buffer                          */
  PBUFFER RBuffer;      /* Receive Buffer                           */
};

/*------------------------------------------------------------------*/
/* SWReg Values (0 means no command)                                */
/*------------------------------------------------------------------*/
#define SWREG_DIE_WITH_LEDON  0x01
#define SWREG_HALT_CPU        0x02 /* Push CPU into a while(1) loop */         

/*------------------------------------------------------------------*/
/* Id Fields Coding                                                 */
/*------------------------------------------------------------------*/

#define ID_MASK 0xe0    /* Mask for the ID field                    */
#define GL_ERR_ID 0x1f  /* ID for error reporting on global requests*/

#define DSIG_ID  0x00   /* ID for D-channel signaling               */
#define NL_ID    0x20   /* ID for network-layer access (B or D)     */
#define BLLC_ID  0x60   /* ID for B-channel link level access       */
#define TASK_ID  0x80   /* ID for dynamic user tasks                */
#define TIMER_ID 0xa0   /* ID for timer task                        */
#define TEL_ID   0xc0   /* ID for telephone support                 */
#define MAN_ID   0xe0   /* ID for management                        */

/*------------------------------------------------------------------*/
/* ASSIGN and REMOVE requests are the same for all entities         */
/*------------------------------------------------------------------*/

#define ASSIGN  0x01
#define UREMOVE  0xfe   /* without returncode */  
#define REMOVE  0xff

/*------------------------------------------------------------------*/
/* Timer Interrupt Task Interface                                   */
/*------------------------------------------------------------------*/

#define ASSIGN_TIM 0x01
#define REMOVE_TIM 0xff

/*------------------------------------------------------------------*/
/* dynamic user task interface                                      */
/*------------------------------------------------------------------*/

#define ASSIGN_TSK 0x01
#define REMOVE_TSK 0xff

#define LOAD 0xf0
#define RELOCATE 0xf1
#define START 0xf2
#define LOAD2 0xf3
#define RELOCATE2 0xf4

/*------------------------------------------------------------------*/
/* dynamic user task messages                                       */
/*------------------------------------------------------------------*/

#define TSK_B2          0x0000
#define TSK_WAKEUP      0x2000
#define TSK_TIMER       0x4000
#define TSK_TSK         0x6000
#define TSK_PC          0xe000

/*------------------------------------------------------------------*/
/* LL management primitives                                         */
/*------------------------------------------------------------------*/

#define ASSIGN_LL 1     /* assign logical link                      */
#define REMOVE_LL 0xff  /* remove logical link                      */

/*------------------------------------------------------------------*/
/* LL service primitives                                            */
/*------------------------------------------------------------------*/

#define LL_UDATA 1      /* link unit data request/indication        */
#define LL_ESTABLISH 2  /* link establish request/indication        */
#define LL_RELEASE 3    /* link release request/indication          */
#define LL_DATA 4       /* data request/indication                  */
#define LL_LOCAL 5      /* switch to local operation (COM only)     */
#define LL_DATA_PEND 5  /* data pending indication (SDLC SHM only)  */
#define LL_REMOTE 6     /* switch to remote operation (COM only)    */
#define LL_TEST 8       /* link test request                        */
#define LL_MDATA 9      /* more data request/indication             */
#define LL_BUDATA 10    /* broadcast unit data request/indication   */
#define LL_XID 12       /* XID command request/indication           */
#define LL_XID_R 13     /* XID response request/indication          */

/*------------------------------------------------------------------*/
/* NL service primitives                                            */
/*------------------------------------------------------------------*/

#define N_MDATA         1       /* more data to come REQ/IND        */
#define N_CONNECT       2       /* OSI N-CONNECT REQ/IND            */
#define N_CONNECT_ACK   3       /* OSI N-CONNECT CON/RES            */
#define N_DISC          4       /* OSI N-DISC REQ/IND               */
#define N_DISC_ACK      5       /* OSI N-DISC CON/RES               */
#define N_RESET         6       /* OSI N-RESET REQ/IND              */
#define N_RESET_ACK     7       /* OSI N-RESET CON/RES              */
#define N_DATA          8       /* OSI N-DATA REQ/IND               */
#define N_EDATA         9       /* OSI N-EXPEDITED DATA REQ/IND     */
#define N_UDATA         10      /* OSI D-UNIT-DATA REQ/IND          */
#define N_BDATA         11      /* BROADCAST-DATA REQ/IND           */
#define N_DATA_ACK      12      /* data ack ind for D-bit procedure */
#define N_EDATA_ACK     13      /* data ack ind for INTERRUPT       */

#define N_Q_BIT         0x10    /* Q-bit for req/ind                */
#define N_M_BIT         0x20    /* M-bit for req/ind                */
#define N_D_BIT         0x40    /* D-bit for req/ind                */

/*------------------------------------------------------------------*/
/* Signaling management primitives                                  */
/*------------------------------------------------------------------*/

#define ASSIGN_SIG 1    /* assign signaling task                    */
#define UREMOVE_SIG 0xfe /* remove signaling task without returncode */
#define REMOVE_SIG 0xff /* remove signaling task                    */

/*------------------------------------------------------------------*/
/* Signaling service primitives                                     */
/*------------------------------------------------------------------*/

#define CALL_REQ 1      /* call request                             */
#define CALL_CON 1      /* call confirmation                        */
#define CALL_IND 2      /* incoming call connected                  */
#define LISTEN_REQ 2    /* listen request                           */
#define HANGUP 3        /* hangup request/indication                */
#define SUSPEND 4       /* call suspend request/confirm             */
#define RESUME 5        /* call resume request/confirm              */
#define SUSPEND_REJ 6   /* suspend rejected indication              */
#define USER_DATA 8     /* user data for user to user signaling     */
#define CONGESTION 9    /* network congestion indication            */
#define INDICATE_REQ 10 /* request to indicate an incoming call     */
#define INDICATE_IND 10 /* indicates that there is an incoming call */
#define CALL_RES 11     /* accept an incoming call                  */
#define CALL_ALERT 12   /* send ALERT for incoming call             */
#define INFO_REQ 13     /* INFO request                             */
#define INFO_IND 13     /* INFO indication                          */
#define REJECT 14       /* reject an incoming call                  */
#define RESOURCES 15    /* reserve B-Channel hardware resources     */
#define TEL_CTRL 16     /* Telephone control request/indication     */
#define STATUS_REQ 17   /* Request D-State (returned in INFO_IND)   */
#define FAC_REG_REQ 18  /* connection idependent fac registration   */
#define FAC_REG_ACK 19  /* fac registration acknowledge             */
#define FAC_REG_REJ 20  /* fac registration reject                  */
#define CALL_COMPLETE 21/* send a CALL_PROC for incoming call       */
#define FACILITY_REQ 22 /* send a Facility Message type             */
#define FACILITY_IND 22 /* Facility Message type indication         */
#define SIG_CTRL     29 /* Control for signalling hardware          */
#define DSP_CTRL     30 /* Control for DSPs                         */
#define LAW_REQ      31 /* Law config request for (returns info_i)  */ 

 
/*------------------------------------------------------------------*/
/* management service primitives                                    */
/*------------------------------------------------------------------*/

#define MAN_READ        2
#define MAN_WRITE       3
#define MAN_EXECUTE     4
#define MAN_EVENT_ON    5
#define MAN_EVENT_OFF   6
#define MAN_LOCK        7
#define MAN_UNLOCK      8   

#define MAN_INFO_IND    2
#define MAN_EVENT_IND   3
#define MAN_TRACE_IND   4  

#define MAN_ESC         0x80

/*------------------------------------------------------------------*/
/* return code coding                                               */
/*------------------------------------------------------------------*/

#define UNKNOWN_COMMAND         0x01    /* unknown command          */
#define WRONG_COMMAND           0x02    /* wrong command            */
#define WRONG_ID                0x03    /* unknown task/entity id   */
#define WRONG_CH                0x04    /* wrong task/entity id     */
#define UNKNOWN_IE              0x05    /* unknown information el.  */
#define WRONG_IE                0x06    /* wrong information el.    */
#define OUT_OF_RESOURCES        0x07    /* ISDN-S card out of res.  */
#define ADAPTER_DEAD            0x08    /* ISDN card CPU halted     */ 
#define N_FLOW_CONTROL          0x10    /* Flow-Control, retry      */
#define ASSIGN_RC               0xe0    /* ASSIGN acknowledgement   */
#define ASSIGN_OK               0xef    /* ASSIGN OK                */
#define OK_FC                   0xfc    /* Flow-Control RC          */
#define READY_INT               0xfd    /* Ready interrupt          */
#define TIMER_INT               0xfe    /* timer interrupt          */
#define OK                      0xff    /* command accepted         */

/*------------------------------------------------------------------*/
/* information elements                                             */
/*------------------------------------------------------------------*/

#define SHIFT 0x90              /* codeset shift                    */
#define MORE 0xa0               /* more data                        */
#define CL 0xb0                 /* congestion level                 */

        /* codeset 0                                                */

#define BC  0x04                /* Bearer Capability                */
#define CAU 0x08                /* cause                            */
#define CAD 0x0c                /* Connected address                */
#define CAI 0x10                /* call identity                    */
#define CHI 0x18                /* channel identification           */
#define LLI 0x19                /* logical link id                  */
#define CHA 0x1a                /* charge advice                    */
#define DT  0x29                /* ETSI date/time                   */
#define KEY 0x2c                /* keypad information element       */
#define FTY 0x1c                /* facility information element     */ 
#define DSP 0x28                /* display                          */
#define OAD 0x6c                /* origination address              */
#define OSA 0x6d                /* origination sub-address          */
#define CPN 0x70                /* called party number              */
#define DSA 0x71                /* destination sub-address          */
#define RDX 0x73                /* redirected number extended       */
#define RDN 0x74                /* redirected number                */  
#define LLC 0x7c                /* low layer compatibility          */
#define HLC 0x7d                /* high layer compatibility         */
#define UUI 0x7e                /* user user information            */
#define ESC 0x7f                /* escape extension                 */

#define DLC 0x20                /* data link layer configuration    */
#define NLC 0x21                /* network layer configuration      */

        /* codeset 6                                                */

#define SIN 0x01                /* service indicator                */
#define CIF 0x02                /* charging information             */
#define DATE 0x03               /* date                             */
#define CPS 0x07                /* called party status              */

/*------------------------------------------------------------------*/
/* TEL_CTRL contents                                                */
/*------------------------------------------------------------------*/

#define RING_ON         0x01
#define RING_OFF        0x02
#define HANDS_FREE_ON   0x03
#define HANDS_FREE_OFF  0x04
#define ON_HOOK         0x80
#define OFF_HOOK        0x90

#endif
