/*
 * alias_pptp.c
 *
 * Copyright (c) 2000 Whistle Communications, Inc.
 * All rights reserved.
 *
 * Subject to the following obligations and disclaimer of warranty, use and
 * redistribution of this software, in source or object code forms, with or
 * without modifications are expressly permitted by Whistle Communications;
 * provided, however, that:
 * 1. Any and all reproductions of the source or object code must include the
 *    copyright notice above and the following disclaimer of warranties; and
 * 2. No rights are granted, in any manner or form, to use Whistle
 *    Communications, Inc. trademarks, including the mark "WHISTLE
 *    COMMUNICATIONS" on advertising, endorsements, or otherwise except as
 *    such appears in the above copyright notice or in the software.
 *
 * THIS SOFTWARE IS BEING PROVIDED BY WHISTLE COMMUNICATIONS "AS IS", AND
 * TO THE MAXIMUM EXTENT PERMITTED BY LAW, WHISTLE COMMUNICATIONS MAKES NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED, REGARDING THIS SOFTWARE,
 * INCLUDING WITHOUT LIMITATION, ANY AND ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT.
 * WHISTLE COMMUNICATIONS DOES NOT WARRANT, GUARANTEE, OR MAKE ANY
 * REPRESENTATIONS REGARDING THE USE OF, OR THE RESULTS OF THE USE OF THIS
 * SOFTWARE IN TERMS OF ITS CORRECTNESS, ACCURACY, RELIABILITY OR OTHERWISE.
 * IN NO EVENT SHALL WHISTLE COMMUNICATIONS BE LIABLE FOR ANY DAMAGES
 * RESULTING FROM OR ARISING OUT OF ANY USE OF THIS SOFTWARE, INCLUDING
 * WITHOUT LIMITATION, ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * PUNITIVE, OR CONSEQUENTIAL DAMAGES, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES, LOSS OF USE, DATA OR PROFITS, HOWEVER CAUSED AND UNDER ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF WHISTLE COMMUNICATIONS IS ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * Author: Erik Salander <erik@whistle.com>
 *
 * $FreeBSD: src/lib/libalias/alias_pptp.c,v 1.1.2.2 2000/09/14 17:32:39 ru Exp $
 */

/*
   Alias_pptp.c performs special processing for PPTP sessions under TCP.
   Specifically, watch PPTP control messages and alias the Call ID or the
   Peer's Call ID in the appropriate messages.  Note, PPTP requires
   "de-aliasing" of incoming packets, this is different than any other
   TCP applications that are currently (ie. FTP, IRC and RTSP) aliased.

   For Call IDs encountered for the first time, a PPTP alias link is created.
   The PPTP alias link uses the Call ID in place of the original port number.
   An alias Call ID is created.

   For this routine to work, the PPTP control messages must fit entirely
   into a single TCP packet.  This is typically the case, but is not
   required by the spec.

   Unlike some of the other TCP applications that are aliased (ie. FTP,
   IRC and RTSP), the PPTP control messages that need to be aliased are
   guaranteed to remain the same length.  The aliased Call ID is a fixed
   length field.

   Reference: RFC 2637

   Initial version:  May, 2000 (eds)

*/

/* Includes */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#include "alias_local.h"

/*
 * PPTP definitions
 */

struct grehdr			/* Enhanced GRE header. */
{
    u_char    gh_recursion:3,	/* Recursion control. */
              gh_ssr_flag:1,	/* Strict source route present. */
              gh_seq_no_flag:1,	/* Sequence number present. */
              gh_key_flag:1,	/* Key present. */
              gh_rt_flag:1,	/* Routing present. */
              gh_cksum_flag:1;	/* Checksum present. */
    u_char    gh_version:3,	/* GRE version. */
              gh_flags:4,	/* Flags. */
              gh_ack_no_flag:1;	/* Acknowledgment sequence number present. */
    u_short   gh_protocol;	/* Protocol type. */
    u_short   gh_length;	/* Payload length. */
    u_short   gh_call_id;	/* Call ID. */
    u_int32_t gh_seq_no;	/* Sequence number (optional). */
    u_int32_t gh_ack_no;	/* Acknowledgment number (optional). */
};

/* The PPTP protocol ID used in the GRE 'proto' field. */
#define PPTP_GRE_PROTO          0x880b

/* Bits that must be set a certain way in all PPTP/GRE packets. */
#define PPTP_INIT_VALUE		((0x2001 << 16) | PPTP_GRE_PROTO)
#define PPTP_INIT_MASK		0xef7fffff

#define PPTP_MAGIC		0x1a2b3c4d
#define PPTP_CTRL_MSG_TYPE	1

enum {
  PPTP_StartCtrlConnRequest = 1,
  PPTP_StartCtrlConnReply = 2,
  PPTP_StopCtrlConnRequest = 3,
  PPTP_StopCtrlConnReply = 4,
  PPTP_EchoRequest = 5,
  PPTP_EchoReply = 6,
  PPTP_OutCallRequest = 7,
  PPTP_OutCallReply = 8,
  PPTP_InCallRequest = 9,
  PPTP_InCallReply = 10,
  PPTP_InCallConn = 11,
  PPTP_CallClearRequest = 12,
  PPTP_CallDiscNotify = 13,
  PPTP_WanErrorNotify = 14,
  PPTP_SetLinkInfo = 15,
};

  /* Message structures */
  struct pptpMsgHead {
    u_int16_t   length;         /* total length */
    u_int16_t   msgType;        /* PPTP message type */
    u_int32_t   magic;          /* magic cookie */
    u_int16_t   type;           /* control message type */
    u_int16_t   resv0;          /* reserved */
  };
  typedef struct pptpMsgHead    *PptpMsgHead;

  struct pptpCallIds {
    u_int16_t   cid1;           /* Call ID field #1 */
    u_int16_t   cid2;           /* Call ID field #2 */
  };
  typedef struct pptpCallIds    *PptpCallId;

static PptpCallId AliasVerifyPptp(struct ip *, u_int16_t *);

int
PptpGetCallID(struct ip *pip,
              u_short *call_id)
{
    struct grehdr *gr;

    gr = (struct grehdr *)((char *)pip + (pip->ip_hl << 2));

    /* Check GRE header bits. */
    if ((ntohl(*((u_int32_t *)gr)) & PPTP_INIT_MASK) == PPTP_INIT_VALUE) {
	*call_id = gr->gh_call_id;
	return 1;
    } else
	return 0;
};

void PptpSetCallID(struct ip *pip, u_short call_id)
{
    struct grehdr *gr;

    gr = (struct grehdr *)((char *)pip + (pip->ip_hl << 2));
    gr->gh_call_id = call_id;
};

void
AliasHandlePptpOut(struct ip *pip,	    /* IP packet to examine/patch */
                   struct alias_link *link) /* The PPTP control link */
{
    struct alias_link   *pptp_link;
    PptpCallId    	cptr;
    u_int16_t           ctl_type;           /* control message type */
    struct tcphdr 	*tc;

    /* Verify valid PPTP control message */
    if ((cptr = AliasVerifyPptp(pip, &ctl_type)) == NULL)
      return;

    /* Modify certain PPTP messages */
    switch (ctl_type) {
    case PPTP_OutCallRequest:
    case PPTP_OutCallReply:
    case PPTP_InCallRequest:
    case PPTP_InCallReply:
    case PPTP_CallClearRequest:
    case PPTP_CallDiscNotify:

      /* Establish PPTP link for address and Call ID found in PPTP Control Msg */
      pptp_link = FindPptpOut(GetOriginalAddress(link), GetDestAddress(link),
                              cptr->cid1);

      if (pptp_link != NULL) {
	int accumulate = cptr->cid1;

	/* alias the Call Id */
	cptr->cid1 = GetAliasPort(pptp_link);

	/* Compute TCP checksum for revised packet */
	tc = (struct tcphdr *) ((char *) pip + (pip->ip_hl << 2));
	accumulate -= cptr->cid1;
	ADJUST_CHECKSUM(accumulate, tc->th_sum);
      }
      break;
    default:
      return;
    }
}

void
AliasHandlePptpIn(struct ip *pip,	   /* IP packet to examine/patch */
                  struct alias_link *link) /* The PPTP control link */
{
    struct alias_link   *pptp_link;
    PptpCallId    	cptr;
    u_int16_t     	*pcall_id;
    u_int16_t           ctl_type;           /* control message type */
    struct tcphdr 	*tc;

    /* Verify valid PPTP control message */
    if ((cptr = AliasVerifyPptp(pip, &ctl_type)) == NULL)
      return;

    /* Modify certain PPTP messages */
    switch (ctl_type)
    {
    case PPTP_InCallConn:
    case PPTP_WanErrorNotify:
    case PPTP_SetLinkInfo:
      pcall_id = &cptr->cid1;
      break;
    case PPTP_OutCallReply:
    case PPTP_InCallReply:
      pcall_id = &cptr->cid2;
      break;
    default:
      return;
    }

    /* Find PPTP link for address and Call ID found in PPTP Control Msg */
    pptp_link = FindPptpIn(GetDestAddress(link), GetAliasAddress(link),
                           *pcall_id);

    if (pptp_link != NULL) {
      int accumulate = *pcall_id;

      /* alias the Call Id */
      *pcall_id = GetOriginalPort(pptp_link);

      /* Compute TCP checksum for modified packet */
      tc = (struct tcphdr *) ((char *) pip + (pip->ip_hl << 2));
      accumulate -= *pcall_id;
      ADJUST_CHECKSUM(accumulate, tc->th_sum);
    }
}

PptpCallId
AliasVerifyPptp(struct ip *pip, u_int16_t *ptype) /* IP packet to examine/patch */
{
    int           	hlen, tlen, dlen;
    PptpMsgHead   	hptr;
    struct tcphdr 	*tc;

    /* Calculate some lengths */
    tc = (struct tcphdr *) ((char *) pip + (pip->ip_hl << 2));
    hlen = (pip->ip_hl + tc->th_off) << 2;
    tlen = ntohs(pip->ip_len);
    dlen = tlen - hlen;

    /* Verify data length */
    if (dlen < (sizeof(struct pptpMsgHead) + sizeof(struct pptpCallIds)))
      return(NULL);

    /* Move up to PPTP message header */
    hptr = (PptpMsgHead)(((char *) pip) + hlen);

    /* Return the control message type */
    *ptype = ntohs(hptr->type);

    /* Verify PPTP Control Message */
    if ((ntohs(hptr->msgType) != PPTP_CTRL_MSG_TYPE) ||
        (ntohl(hptr->magic) != PPTP_MAGIC))
      return(NULL);
    else
      return((PptpCallId)(((char *)hptr) + sizeof(struct pptpMsgHead)));
}
