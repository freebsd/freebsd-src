/* Copyright (c) 2001 NETLAB, Temple University
 * Copyright (c) 2001 Protocol Engineering Lab, University of Delaware
 *
 * Jerry Heinz <gheinz@astro.temple.edu>
 * John Fiore <jfiore@joda.cis.temple.edu>
 * Armando L. Caro Jr. <acaro@cis.udel.edu>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* \summary: Stream Control Transmission Protocol (SCTP) printer */

#include <config.h>

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"
#include "ip.h"
#include "ip6.h"

/* Definitions from:
 *
 * SCTP reference Implementation Copyright (C) 1999 Cisco And Motorola
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of Cisco nor of Motorola may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the SCTP reference Implementation
 *
 *
 * Please send any bug reports or fixes you make to one of the following email
 * addresses:
 *
 * rstewar1@email.mot.com
 * kmorneau@cisco.com
 * qxie1@email.mot.com
 *
 * Any bugs reported given to us we will try to fix... any fixes shared will
 * be incorporated into the next SCTP release.
 */

/* The valid defines for all message
 * types know to SCTP. 0 is reserved
 */
#define SCTP_DATA		0x00
#define SCTP_INITIATION		0x01
#define SCTP_INITIATION_ACK	0x02
#define SCTP_SELECTIVE_ACK	0x03
#define SCTP_HEARTBEAT_REQUEST	0x04
#define SCTP_HEARTBEAT_ACK	0x05
#define SCTP_ABORT_ASSOCIATION	0x06
#define SCTP_SHUTDOWN		0x07
#define SCTP_SHUTDOWN_ACK	0x08
#define SCTP_OPERATION_ERR	0x09
#define SCTP_COOKIE_ECHO	0x0a
#define SCTP_COOKIE_ACK         0x0b
#define SCTP_ECN_ECHO		0x0c
#define SCTP_ECN_CWR		0x0d
#define SCTP_SHUTDOWN_COMPLETE	0x0e
#define SCTP_FORWARD_CUM_TSN    0xc0
#define SCTP_RELIABLE_CNTL      0xc1
#define SCTP_RELIABLE_CNTL_ACK  0xc2

static const struct tok sctp_chunkid_str[] = {
	{ SCTP_DATA,              "DATA"              },
	{ SCTP_INITIATION,        "INIT"              },
	{ SCTP_INITIATION_ACK,    "INIT ACK"          },
	{ SCTP_SELECTIVE_ACK,     "SACK"              },
	{ SCTP_HEARTBEAT_REQUEST, "HB REQ"            },
	{ SCTP_HEARTBEAT_ACK,     "HB ACK"            },
	{ SCTP_ABORT_ASSOCIATION, "ABORT"             },
	{ SCTP_SHUTDOWN,          "SHUTDOWN"          },
	{ SCTP_SHUTDOWN_ACK,      "SHUTDOWN ACK"      },
	{ SCTP_OPERATION_ERR,     "OP ERR"            },
	{ SCTP_COOKIE_ECHO,       "COOKIE ECHO"       },
	{ SCTP_COOKIE_ACK,        "COOKIE ACK"        },
	{ SCTP_ECN_ECHO,          "ECN ECHO"          },
	{ SCTP_ECN_CWR,           "ECN CWR"           },
	{ SCTP_SHUTDOWN_COMPLETE, "SHUTDOWN COMPLETE" },
	{ SCTP_FORWARD_CUM_TSN,   "FOR CUM TSN"       },
	{ SCTP_RELIABLE_CNTL,     "REL CTRL"          },
	{ SCTP_RELIABLE_CNTL_ACK, "REL CTRL ACK"      },
	{ 0, NULL }
};

/* Data Chuck Specific Flags */
#define SCTP_DATA_FRAG_MASK	0x03
#define SCTP_DATA_MIDDLE_FRAG	0x00
#define SCTP_DATA_LAST_FRAG	0x01
#define SCTP_DATA_FIRST_FRAG	0x02
#define SCTP_DATA_NOT_FRAG	0x03
#define SCTP_DATA_UNORDERED	0x04

#define SCTP_ADDRMAX 60

#define CHAN_HP 6704
#define CHAN_MP 6705
#define CHAN_LP 6706

/* the sctp common header */

struct sctpHeader{
  nd_uint16_t source;
  nd_uint16_t destination;
  nd_uint32_t verificationTag;
  nd_uint32_t adler32;
};

/* various descriptor parsers */

struct sctpChunkDesc{
  nd_uint8_t  chunkID;
  nd_uint8_t  chunkFlg;
  nd_uint16_t chunkLength;
};

struct sctpParamDesc{
  nd_uint16_t paramType;
  nd_uint16_t paramLength;
};


struct sctpRelChunkDesc{
  struct sctpChunkDesc chk;
  nd_uint32_t serialNumber;
};

struct sctpVendorSpecificParam {
  struct sctpParamDesc p;  /* type must be 0xfffe */
  nd_uint32_t vendorId;	   /* vendor ID from RFC 1700 */
  nd_uint16_t vendorSpecificType;
  nd_uint16_t vendorSpecificLen;
};


/* Structures for the control parts */



/* Sctp association init request/ack */

/* this is used for init ack, too */
struct sctpInitiation{
  nd_uint32_t initTag;			/* tag of mine */
  nd_uint32_t rcvWindowCredit;		/* rwnd */
  nd_uint16_t NumPreopenStreams;	/* OS */
  nd_uint16_t MaxInboundStreams;	/* MIS */
  nd_uint32_t initialTSN;
  /* optional param's follow in sctpParamDesc form */
};

struct sctpV4IpAddress{
  struct sctpParamDesc p;	/* type is set to SCTP_IPV4_PARAM_TYPE, len=10 */
  nd_ipv4  ipAddress;
};


struct sctpV6IpAddress{
  struct sctpParamDesc p;	/* type is set to SCTP_IPV6_PARAM_TYPE, len=22 */
  nd_ipv6  ipAddress;
};

struct sctpDNSName{
  struct sctpParamDesc param;
  nd_byte name[1];
};


struct sctpCookiePreserve{
  struct sctpParamDesc p;	/* type is set to SCTP_COOKIE_PRESERVE, len=8 */
  nd_uint32_t extraTime;
};


struct sctpTimeStamp{
  nd_uint32_t ts_sec;
  nd_uint32_t ts_usec;
};


/* this guy is for use when
 * I have a initiate message gloming the
 * things together.

 */
struct sctpUnifiedInit{
  struct sctpChunkDesc uh;
  struct sctpInitiation initm;
};

struct sctpSendableInit{
  struct sctpHeader mh;
  struct sctpUnifiedInit msg;
};


/* Selective Acknowledgement
 * has the following structure with
 * a optional amount of trailing int's
 * on the last part (based on the numberOfDesc
 * field).
 */

struct sctpSelectiveAck{
  nd_uint32_t highestConseqTSN;
  nd_uint32_t updatedRwnd;
  nd_uint16_t numberOfdesc;
  nd_uint16_t numDupTsns;
};

struct sctpSelectiveFrag{
  nd_uint16_t fragmentStart;
  nd_uint16_t fragmentEnd;
};


struct sctpUnifiedSack{
  struct sctpChunkDesc uh;
  struct sctpSelectiveAck sack;
};

/* for the abort and shutdown ACK
 * we must carry the init tag in the common header. Just the
 * common header is all that is needed with a chunk descriptor.
 */
struct sctpUnifiedAbort{
  struct sctpChunkDesc uh;
};

struct sctpUnifiedAbortLight{
  struct sctpHeader mh;
  struct sctpChunkDesc uh;
};

struct sctpUnifiedAbortHeavy{
  struct sctpHeader mh;
  struct sctpChunkDesc uh;
  nd_uint16_t causeCode;
  nd_uint16_t causeLen;
};

/* For the graceful shutdown we must carry
 * the tag (in common header)  and the highest consecutive acking value
 */
struct sctpShutdown {
  nd_uint32_t TSN_Seen;
};

struct sctpUnifiedShutdown{
  struct sctpChunkDesc uh;
  struct sctpShutdown shut;
};

/* in the unified message we add the trailing
 * stream id since it is the only message
 * that is defined as a operation error.
 */
struct sctpOpErrorCause{
  nd_uint16_t cause;
  nd_uint16_t causeLen;
};

struct sctpUnifiedOpError{
  struct sctpChunkDesc uh;
  struct sctpOpErrorCause c;
};

struct sctpUnifiedStreamError{
  struct sctpHeader mh;
  struct sctpChunkDesc uh;
  struct sctpOpErrorCause c;
  nd_uint16_t strmNum;
  nd_uint16_t reserved;
};

struct staleCookieMsg{
  struct sctpHeader mh;
  struct sctpChunkDesc uh;
  struct sctpOpErrorCause c;
  nd_uint32_t moretime;
};

/* the following is used in all sends
 * where nothing is needed except the
 * chunk/type i.e. shutdownAck Abort */

struct sctpUnifiedSingleMsg{
  struct sctpHeader mh;
  struct sctpChunkDesc uh;
};

struct sctpDataPart{
  nd_uint32_t TSN;
  nd_uint16_t streamId;
  nd_uint16_t sequence;
  nd_uint32_t payloadtype;
};

struct sctpUnifiedDatagram{
  struct sctpChunkDesc uh;
  struct sctpDataPart dp;
};

struct sctpECN_echo{
  struct sctpChunkDesc uh;
  nd_uint32_t Lowest_TSN;
};


struct sctpCWR{
  struct sctpChunkDesc uh;
  nd_uint32_t TSN_reduced_at;
};

static const struct tok ForCES_channels[] = {
	{ CHAN_HP, "ForCES HP" },
	{ CHAN_MP, "ForCES MP" },
	{ CHAN_LP, "ForCES LP" },
	{ 0, NULL }
};

/* data chunk's payload protocol identifiers */

#define SCTP_PPID_IUA 1
#define SCTP_PPID_M2UA 2
#define SCTP_PPID_M3UA 3
#define SCTP_PPID_SUA 4
#define SCTP_PPID_M2PA 5
#define SCTP_PPID_V5UA 6
#define SCTP_PPID_H248 7
#define SCTP_PPID_BICC 8
#define SCTP_PPID_TALI 9
#define SCTP_PPID_DUA 10
#define SCTP_PPID_ASAP 11
#define SCTP_PPID_ENRP 12
#define SCTP_PPID_H323 13
#define SCTP_PPID_QIPC 14
#define SCTP_PPID_SIMCO 15
#define SCTP_PPID_DDPSC 16
#define SCTP_PPID_DDPSSC 17
#define SCTP_PPID_S1AP 18
#define SCTP_PPID_RUA 19
#define SCTP_PPID_HNBAP 20
#define SCTP_PPID_FORCES_HP 21
#define SCTP_PPID_FORCES_MP 22
#define SCTP_PPID_FORCES_LP 23
#define SCTP_PPID_SBC_AP 24
#define SCTP_PPID_NBAP 25
/* 26 */
#define SCTP_PPID_X2AP 27

static const struct tok PayloadProto_idents[] = {
	{ SCTP_PPID_IUA,    "ISDN Q.921" },
	{ SCTP_PPID_M2UA,   "M2UA"   },
	{ SCTP_PPID_M3UA,   "M3UA"   },
	{ SCTP_PPID_SUA,    "SUA"    },
	{ SCTP_PPID_M2PA,   "M2PA"   },
	{ SCTP_PPID_V5UA,   "V5.2"   },
	{ SCTP_PPID_H248,   "H.248"  },
	{ SCTP_PPID_BICC,   "BICC"   },
	{ SCTP_PPID_TALI,   "TALI"   },
	{ SCTP_PPID_DUA,    "DUA"    },
	{ SCTP_PPID_ASAP,   "ASAP"   },
	{ SCTP_PPID_ENRP,   "ENRP"   },
	{ SCTP_PPID_H323,   "H.323"  },
	{ SCTP_PPID_QIPC,   "Q.IPC"  },
	{ SCTP_PPID_SIMCO,  "SIMCO"  },
	{ SCTP_PPID_DDPSC,  "DDPSC"  },
	{ SCTP_PPID_DDPSSC, "DDPSSC" },
	{ SCTP_PPID_S1AP,   "S1AP"   },
	{ SCTP_PPID_RUA,    "RUA"    },
	{ SCTP_PPID_HNBAP,  "HNBAP"  },
	{ SCTP_PPID_FORCES_HP, "ForCES HP" },
	{ SCTP_PPID_FORCES_MP, "ForCES MP" },
	{ SCTP_PPID_FORCES_LP, "ForCES LP" },
	{ SCTP_PPID_SBC_AP, "SBc-AP" },
	{ SCTP_PPID_NBAP,   "NBAP"   },
	/* 26 */
	{ SCTP_PPID_X2AP,   "X2AP"   },
	{ 0, NULL }
};


static int
isForCES_port(u_short Port)
{
	if (Port == CHAN_HP)
		return 1;
	if (Port == CHAN_MP)
		return 1;
	if (Port == CHAN_LP)
		return 1;

	return 0;
}

void
sctp_print(netdissect_options *ndo,
	   const u_char *bp,        /* beginning of sctp packet */
	   const u_char *bp2,       /* beginning of enclosing */
	   u_int sctpPacketLength)  /* ip packet */
{
  u_int sctpPacketLengthRemaining;
  const struct sctpHeader *sctpPktHdr;
  const struct ip *ip;
  const struct ip6_hdr *ip6;
  uint8_t chunkID;
  u_short sourcePort, destPort;
  u_int chunkCount;
  const struct sctpChunkDesc *chunkDescPtr;
  const char *sep;
  int isforces = 0;

  ndo->ndo_protocol = "sctp";
  if (sctpPacketLength < sizeof(struct sctpHeader))
    {
      ND_PRINT("truncated-sctp - %zu bytes missing!",
               sizeof(struct sctpHeader) - sctpPacketLength);
      return;
    }
  sctpPktHdr = (const struct sctpHeader*) bp;
  ND_TCHECK_SIZE(sctpPktHdr);
  sctpPacketLengthRemaining = sctpPacketLength;

  sourcePort = GET_BE_U_2(sctpPktHdr->source);
  destPort = GET_BE_U_2(sctpPktHdr->destination);

  ip = (const struct ip *)bp2;
  if (IP_V(ip) == 6)
    ip6 = (const struct ip6_hdr *)bp2;
  else
    ip6 = NULL;

  if (ip6) {
    ND_PRINT("%s.%u > %s.%u: sctp",
      GET_IP6ADDR_STRING(ip6->ip6_src),
      sourcePort,
      GET_IP6ADDR_STRING(ip6->ip6_dst),
      destPort);
  } else {
    ND_PRINT("%s.%u > %s.%u: sctp",
      GET_IPADDR_STRING(ip->ip_src),
      sourcePort,
      GET_IPADDR_STRING(ip->ip_dst),
      destPort);
  }

  if (isForCES_port(sourcePort)) {
	 ND_PRINT("[%s]", tok2str(ForCES_channels, NULL, sourcePort));
	 isforces = 1;
  }
  if (isForCES_port(destPort)) {
	 ND_PRINT("[%s]", tok2str(ForCES_channels, NULL, destPort));
	 isforces = 1;
  }

  bp += sizeof(struct sctpHeader);
  sctpPacketLengthRemaining -= sizeof(struct sctpHeader);

  if (ndo->ndo_vflag >= 2)
    sep = "\n\t";
  else
    sep = " (";
  /* cycle through all chunks, printing information on each one */
  for (chunkCount = 0, chunkDescPtr = (const struct sctpChunkDesc *)bp;
      sctpPacketLengthRemaining != 0;
      chunkCount++)
    {
      uint16_t chunkLength, chunkLengthRemaining;
      uint16_t align;

      chunkDescPtr = (const struct sctpChunkDesc *)bp;
      if (sctpPacketLengthRemaining < sizeof(*chunkDescPtr)) {
	ND_PRINT("%s%u) [chunk descriptor cut off at end of packet]", sep, chunkCount+1);
	break;
      }
      ND_TCHECK_SIZE(chunkDescPtr);
      chunkLength = GET_BE_U_2(chunkDescPtr->chunkLength);
      if (chunkLength < sizeof(*chunkDescPtr)) {
	ND_PRINT("%s%u) [Bad chunk length %u, < size of chunk descriptor]", sep, chunkCount+1, chunkLength);
	break;
      }
      chunkLengthRemaining = chunkLength;

      align = chunkLength % 4;
      if (align != 0)
	align = 4 - align;

      if (sctpPacketLengthRemaining < align) {
	ND_PRINT("%s%u) [Bad chunk length %u, > remaining data in packet]", sep, chunkCount+1, chunkLength);
	break;
      }

      ND_TCHECK_LEN(bp, chunkLength);

      bp += sizeof(*chunkDescPtr);
      sctpPacketLengthRemaining -= sizeof(*chunkDescPtr);
      chunkLengthRemaining -= sizeof(*chunkDescPtr);

      ND_PRINT("%s%u) ", sep, chunkCount+1);
      chunkID = GET_U_1(chunkDescPtr->chunkID);
      ND_PRINT("[%s] ", tok2str(sctp_chunkid_str, "Unknown chunk type: 0x%x",
	       chunkID));
      switch (chunkID) {
	case SCTP_DATA :
	  {
	    const struct sctpDataPart *dataHdrPtr;
	    uint8_t chunkFlg;
	    uint32_t ppid;
	    uint16_t payload_size;

	    chunkFlg = GET_U_1(chunkDescPtr->chunkFlg);
	    if ((chunkFlg & SCTP_DATA_UNORDERED) == SCTP_DATA_UNORDERED)
	      ND_PRINT("(U)");

	    if ((chunkFlg & SCTP_DATA_FIRST_FRAG) == SCTP_DATA_FIRST_FRAG)
	      ND_PRINT("(B)");

	    if ((chunkFlg & SCTP_DATA_LAST_FRAG) == SCTP_DATA_LAST_FRAG)
	      ND_PRINT("(E)");

	    if( ((chunkFlg & SCTP_DATA_UNORDERED) == SCTP_DATA_UNORDERED) ||
		((chunkFlg & SCTP_DATA_FIRST_FRAG) == SCTP_DATA_FIRST_FRAG) ||
		((chunkFlg & SCTP_DATA_LAST_FRAG) == SCTP_DATA_LAST_FRAG) )
	      ND_PRINT(" ");

	    if (chunkLengthRemaining < sizeof(*dataHdrPtr)) {
		ND_PRINT("bogus chunk length %u]", chunkLength);
		return;
	    }
	    dataHdrPtr=(const struct sctpDataPart*)bp;

	    ppid = GET_BE_U_4(dataHdrPtr->payloadtype);
	    ND_PRINT("[TSN: %u] ", GET_BE_U_4(dataHdrPtr->TSN));
	    ND_PRINT("[SID: %u] ", GET_BE_U_2(dataHdrPtr->streamId));
	    ND_PRINT("[SSEQ %u] ", GET_BE_U_2(dataHdrPtr->sequence));
	    ND_PRINT("[PPID %s] ",
		    tok2str(PayloadProto_idents, "0x%x", ppid));

	    if (!isforces) {
		isforces = (ppid == SCTP_PPID_FORCES_HP) ||
		    (ppid == SCTP_PPID_FORCES_MP) ||
		    (ppid == SCTP_PPID_FORCES_LP);
	    }

	    bp += sizeof(*dataHdrPtr);
	    sctpPacketLengthRemaining -= sizeof(*dataHdrPtr);
	    chunkLengthRemaining -= sizeof(*dataHdrPtr);
	    payload_size = chunkLengthRemaining;
	    if (payload_size == 0) {
		ND_PRINT("bogus chunk length %u]", chunkLength);
		return;
	    }

	    if (isforces) {
		forces_print(ndo, bp, payload_size);
		/* ndo_protocol reassignment after forces_print() call */
		ndo->ndo_protocol = "sctp";
	    } else if (ndo->ndo_vflag >= 2) {	/* if verbose output is specified */
					/* at the command line */
		switch (ppid) {
		case SCTP_PPID_M3UA :
			m3ua_print(ndo, bp, payload_size);
			/* ndo_protocol reassignment after m3ua_print() call */
			ndo->ndo_protocol = "sctp";
			break;
		default:
			ND_PRINT("[Payload");
			if (!ndo->ndo_suppress_default_print) {
				ND_PRINT(":");
				ND_DEFAULTPRINT(bp, payload_size);
			}
			ND_PRINT("]");
			break;
		}
	    }
	    bp += payload_size;
	    sctpPacketLengthRemaining -= payload_size;
	    chunkLengthRemaining -= payload_size;
	    break;
	  }
	case SCTP_INITIATION :
	  {
	    const struct sctpInitiation *init;

	    if (chunkLengthRemaining < sizeof(*init)) {
		ND_PRINT("bogus chunk length %u]", chunkLength);
		return;
	    }
	    init=(const struct sctpInitiation*)bp;
	    ND_PRINT("[init tag: %u] ", GET_BE_U_4(init->initTag));
	    ND_PRINT("[rwnd: %u] ", GET_BE_U_4(init->rcvWindowCredit));
	    ND_PRINT("[OS: %u] ", GET_BE_U_2(init->NumPreopenStreams));
	    ND_PRINT("[MIS: %u] ", GET_BE_U_2(init->MaxInboundStreams));
	    ND_PRINT("[init TSN: %u] ", GET_BE_U_4(init->initialTSN));
	    bp += sizeof(*init);
	    sctpPacketLengthRemaining -= sizeof(*init);
	    chunkLengthRemaining -= sizeof(*init);

#if 0 /* ALC you can add code for optional params here */
	    if( chunkLengthRemaining != 0 )
	      ND_PRINT(" @@@@@ UNFINISHED @@@@@@%s\n",
		     "Optional params present, but not printed.");
#endif
	    bp += chunkLengthRemaining;
	    sctpPacketLengthRemaining -= chunkLengthRemaining;
	    chunkLengthRemaining = 0;
	    break;
	  }
	case SCTP_INITIATION_ACK :
	  {
	    const struct sctpInitiation *init;

	    if (chunkLengthRemaining < sizeof(*init)) {
		ND_PRINT("bogus chunk length %u]", chunkLength);
		return;
	    }
	    init=(const struct sctpInitiation*)bp;
	    ND_PRINT("[init tag: %u] ", GET_BE_U_4(init->initTag));
	    ND_PRINT("[rwnd: %u] ", GET_BE_U_4(init->rcvWindowCredit));
	    ND_PRINT("[OS: %u] ", GET_BE_U_2(init->NumPreopenStreams));
	    ND_PRINT("[MIS: %u] ", GET_BE_U_2(init->MaxInboundStreams));
	    ND_PRINT("[init TSN: %u] ", GET_BE_U_4(init->initialTSN));
	    bp += sizeof(*init);
	    sctpPacketLengthRemaining -= sizeof(*init);
	    chunkLengthRemaining -= sizeof(*init);

#if 0 /* ALC you can add code for optional params here */
	    if( chunkLengthRemaining != 0 )
	      ND_PRINT(" @@@@@ UNFINISHED @@@@@@%s\n",
		     "Optional params present, but not printed.");
#endif
	    bp += chunkLengthRemaining;
	    sctpPacketLengthRemaining -= chunkLengthRemaining;
	    chunkLengthRemaining = 0;
	    break;
	  }
	case SCTP_SELECTIVE_ACK:
	  {
	    const struct sctpSelectiveAck *sack;
	    const struct sctpSelectiveFrag *frag;
	    u_int fragNo, tsnNo;
	    const u_char *dupTSN;

	    if (chunkLengthRemaining < sizeof(*sack)) {
	      ND_PRINT("bogus chunk length %u]", chunkLength);
	      return;
	    }
	    sack=(const struct sctpSelectiveAck*)bp;
	    ND_PRINT("[cum ack %u] ", GET_BE_U_4(sack->highestConseqTSN));
	    ND_PRINT("[a_rwnd %u] ", GET_BE_U_4(sack->updatedRwnd));
	    ND_PRINT("[#gap acks %u] ", GET_BE_U_2(sack->numberOfdesc));
	    ND_PRINT("[#dup tsns %u] ", GET_BE_U_2(sack->numDupTsns));
	    bp += sizeof(*sack);
	    sctpPacketLengthRemaining -= sizeof(*sack);
	    chunkLengthRemaining -= sizeof(*sack);


	    /* print gaps */
	    for (fragNo=0;
		 chunkLengthRemaining != 0 && fragNo < GET_BE_U_2(sack->numberOfdesc);
		 bp += sizeof(*frag), sctpPacketLengthRemaining -= sizeof(*frag), chunkLengthRemaining -= sizeof(*frag), fragNo++) {
	      if (chunkLengthRemaining < sizeof(*frag)) {
		ND_PRINT("bogus chunk length %u]", chunkLength);
		return;
	      }
	      frag = (const struct sctpSelectiveFrag *)bp;
	      ND_PRINT("\n\t\t[gap ack block #%u: start = %u, end = %u] ",
		     fragNo+1,
		     GET_BE_U_4(sack->highestConseqTSN) + GET_BE_U_2(frag->fragmentStart),
		     GET_BE_U_4(sack->highestConseqTSN) + GET_BE_U_2(frag->fragmentEnd));
	    }

	    /* print duplicate TSNs */
	    for (tsnNo=0;
		 chunkLengthRemaining != 0 && tsnNo<GET_BE_U_2(sack->numDupTsns);
		 bp += 4, sctpPacketLengthRemaining -= 4, chunkLengthRemaining -= 4, tsnNo++) {
	      if (chunkLengthRemaining < 4) {
		ND_PRINT("bogus chunk length %u]", chunkLength);
		return;
	      }
	      dupTSN = (const u_char *)bp;
	      ND_PRINT("\n\t\t[dup TSN #%u: %u] ", tsnNo+1,
		       GET_BE_U_4(dupTSN));
	    }
	    break;
	  }
	default :
	  {
	    bp += chunkLengthRemaining;
	    sctpPacketLengthRemaining -= chunkLengthRemaining;
	    chunkLengthRemaining = 0;
	    break;
	  }
	}

      /*
       * Any extra stuff at the end of the chunk?
       * XXX - report this?
       */
      bp += chunkLengthRemaining;
      sctpPacketLengthRemaining -= chunkLengthRemaining;

      if (ndo->ndo_vflag < 2)
	sep = ", (";

      if (align != 0) {
	/*
	 * Fail if the alignment padding isn't in the captured data.
	 * Otherwise, skip it.
	 */
	ND_TCHECK_LEN(bp, align);
	bp += align;
	sctpPacketLengthRemaining -= align;
      }
    }
    return;

trunc:
    nd_print_trunc(ndo);
}
