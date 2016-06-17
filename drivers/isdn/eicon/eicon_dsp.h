/* $Id: eicon_dsp.h,v 1.1.4.1 2001/11/20 14:19:35 kai Exp $
 *
 * ISDN lowlevel-module for Eicon active cards.
 * DSP definitions
 *
 * Copyright 1999,2000  by Armin Schindler (mac@melware.de)
 * Copyright 1999,2000  Cytronics & Melware (info@melware.de)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#ifndef DSP_H 
#define DSP_H

#include "dsp_defs.h"


#define DSP_UDATA_REQUEST_SWITCH_FRAMER         1
/*
parameters:
  <byte> transmit framer type
  <byte> receive framer type
*/

#define DSP_REQUEST_SWITCH_FRAMER_HDLC          0
#define DSP_REQUEST_SWITCH_FRAMER_TRANSPARENT   1
#define DSP_REQUEST_SWITCH_FRAMER_ASYNC         2


#define DSP_UDATA_REQUEST_CLEARDOWN             2
/*
parameters:
  - none -
*/


#define DSP_UDATA_REQUEST_TX_CONFIRMATION_ON    3
/*
parameters:
  - none -
*/


#define DSP_UDATA_REQUEST_TX_CONFIRMATION_OFF   4
/*
parameters:
  - none -
*/

typedef struct eicon_dsp_ind {
	__u16	time		__attribute__ ((packed));
	__u8	norm		__attribute__ ((packed));
	__u16	options		__attribute__ ((packed));
	__u32	speed		__attribute__ ((packed));
	__u16	delay		__attribute__ ((packed));
	__u32	txspeed		__attribute__ ((packed));
	__u32	rxspeed		__attribute__ ((packed));
} eicon_dsp_ind;

#define DSP_CONNECTED_OPTION_V42_TRANS           0x0002
#define DSP_CONNECTED_OPTION_V42_LAPM            0x0004
#define DSP_CONNECTED_OPTION_SHORT_TRAIN         0x0008
#define DSP_CONNECTED_OPTION_TALKER_ECHO_PROTECT 0x0010

#define DSP_UDATA_INDICATION_DISCONNECT         5
/*
returns:
  <byte> cause
*/

#define DSP_DISCONNECT_CAUSE_NONE               0x00
#define DSP_DISCONNECT_CAUSE_BUSY_TONE          0x01
#define DSP_DISCONNECT_CAUSE_CONGESTION_TONE    0x02
#define DSP_DISCONNECT_CAUSE_INCOMPATIBILITY    0x03
#define DSP_DISCONNECT_CAUSE_CLEARDOWN          0x04
#define DSP_DISCONNECT_CAUSE_TRAINING_TIMEOUT   0x05

#define DSP_UDATA_INDICATION_TX_CONFIRMATION    6
/*
returns:
  <word> confirmation number
*/


#define DSP_UDATA_REQUEST_SEND_DTMF_DIGITS      16
/*
parameters:
  <word> tone duration (ms)
  <word> gap duration (ms)
  <byte> digit 0 tone code
  ...
  <byte> digit n tone code
*/

#define DSP_SEND_DTMF_DIGITS_HEADER_LENGTH      5

#define DSP_DTMF_DIGIT_TONE_LOW_GROUP_697_HZ    0x00
#define DSP_DTMF_DIGIT_TONE_LOW_GROUP_770_HZ    0x01
#define DSP_DTMF_DIGIT_TONE_LOW_GROUP_852_HZ    0x02
#define DSP_DTMF_DIGIT_TONE_LOW_GROUP_941_HZ    0x03
#define DSP_DTMF_DIGIT_TONE_LOW_GROUP_MASK      0x03
#define DSP_DTMF_DIGIT_TONE_HIGH_GROUP_1209_HZ  0x00
#define DSP_DTMF_DIGIT_TONE_HIGH_GROUP_1336_HZ  0x04
#define DSP_DTMF_DIGIT_TONE_HIGH_GROUP_1477_HZ  0x08
#define DSP_DTMF_DIGIT_TONE_HIGH_GROUP_1633_HZ  0x0c
#define DSP_DTMF_DIGIT_TONE_HIGH_GROUP_MASK     0x0c

#define DSP_DTMF_DIGIT_TONE_CODE_0              0x07
#define DSP_DTMF_DIGIT_TONE_CODE_1              0x00
#define DSP_DTMF_DIGIT_TONE_CODE_2              0x04
#define DSP_DTMF_DIGIT_TONE_CODE_3              0x08
#define DSP_DTMF_DIGIT_TONE_CODE_4              0x01
#define DSP_DTMF_DIGIT_TONE_CODE_5              0x05
#define DSP_DTMF_DIGIT_TONE_CODE_6              0x09
#define DSP_DTMF_DIGIT_TONE_CODE_7              0x02
#define DSP_DTMF_DIGIT_TONE_CODE_8              0x06
#define DSP_DTMF_DIGIT_TONE_CODE_9              0x0a
#define DSP_DTMF_DIGIT_TONE_CODE_STAR           0x03
#define DSP_DTMF_DIGIT_TONE_CODE_HASHMARK       0x0b
#define DSP_DTMF_DIGIT_TONE_CODE_A              0x0c
#define DSP_DTMF_DIGIT_TONE_CODE_B              0x0d
#define DSP_DTMF_DIGIT_TONE_CODE_C              0x0e
#define DSP_DTMF_DIGIT_TONE_CODE_D              0x0f


#define DSP_UDATA_INDICATION_DTMF_DIGITS_SENT   16
/*
returns:
  - none -
  One indication will be sent for every request.
*/


#define DSP_UDATA_REQUEST_ENABLE_DTMF_RECEIVER  17
/*
parameters:
  <word> tone duration (ms)
  <word> gap duration (ms)
*/
typedef struct enable_dtmf_s {
	__u16 tone;
	__u16 gap;
} enable_dtmf_s;

#define DSP_UDATA_REQUEST_DISABLE_DTMF_RECEIVER 18
/*
parameters:
  - none -
*/

#define DSP_UDATA_INDICATION_DTMF_DIGITS_RECEIVED 17
/*
returns:
  <byte> digit 0 tone code
  ...
  <byte> digit n tone code
*/

#define DSP_DTMF_DIGITS_RECEIVED_HEADER_LENGTH  1


#define DSP_UDATA_INDICATION_MODEM_CALLING_TONE 18
/*
returns:
  - none -
*/

#define DSP_UDATA_INDICATION_FAX_CALLING_TONE   19
/*
returns:
  - none -
*/

#define DSP_UDATA_INDICATION_ANSWER_TONE        20
/*
returns:
  - none -
*/

/* ============= FAX ================ */

#define EICON_FAXID_LEN 20

typedef struct eicon_t30_s {
  __u8          code;
  __u8          rate;
  __u8          resolution;
  __u8          format;
  __u8          pages_low;
  __u8          pages_high;
  __u8          atf;
  __u8          control_bits_low;
  __u8          control_bits_high;
  __u8          feature_bits_low;
  __u8          feature_bits_high;
  __u8          universal_5;
  __u8          universal_6;
  __u8          universal_7;
  __u8          station_id_len;
  __u8          head_line_len;
  __u8          station_id[EICON_FAXID_LEN];
/* __u8          head_line[]; */
} eicon_t30_s;

        /* EDATA transmit messages */
#define EDATA_T30_DIS       0x01
#define EDATA_T30_FTT       0x02
#define EDATA_T30_MCF       0x03

        /* EDATA receive messages */
#define EDATA_T30_DCS       0x81
#define EDATA_T30_TRAIN_OK  0x82
#define EDATA_T30_EOP       0x83
#define EDATA_T30_MPS       0x84
#define EDATA_T30_EOM       0x85
#define EDATA_T30_DTC       0x86

#define T30_FORMAT_SFF            0
#define T30_FORMAT_ASCII          1
#define T30_FORMAT_COUNT          2

#define T30_CONTROL_BIT_DISABLE_FINE      0x0001
#define T30_CONTROL_BIT_ENABLE_ECM        0x0002
#define T30_CONTROL_BIT_ECM_64_BYTES      0x0004
#define T30_CONTROL_BIT_ENABLE_2D_CODING  0x0008
#define T30_CONTROL_BIT_ENABLE_T6_CODING  0x0010
#define T30_CONTROL_BIT_ENABLE_UNCOMPR    0x0020
#define T30_CONTROL_BIT_ACCEPT_POLLING    0x0040
#define T30_CONTROL_BIT_REQUEST_POLLING   0x0080
#define T30_CONTROL_BIT_MORE_DOCUMENTS    0x0100

#define T30_CONTROL_BIT_ALL_FEATURES\
  (T30_CONTROL_BIT_ENABLE_ECM | T30_CONTROL_BIT_ENABLE_2D_CODING |\
   T30_CONTROL_BIT_ENABLE_T6_CODING | T30_CONTROL_BIT_ENABLE_UNCOMPR)

#define T30_FEATURE_BIT_FINE              0x0001
#define T30_FEATURE_BIT_ECM               0x0002
#define T30_FEATURE_BIT_ECM_64_BYTES      0x0004
#define T30_FEATURE_BIT_2D_CODING         0x0008
#define T30_FEATURE_BIT_T6_CODING         0x0010
#define T30_FEATURE_BIT_UNCOMPR_ENABLED   0x0020
#define T30_FEATURE_BIT_POLLING           0x0040

#define FAX_OBJECT_DOCU		1
#define FAX_OBJECT_PAGE		2
#define FAX_OBJECT_LINE		3

#define T4_EOL			0x800
#define T4_EOL_BITSIZE		12
#define T4_EOL_DWORD		(T4_EOL << (32 - T4_EOL_BITSIZE))
#define T4_EOL_MASK_DWORD	((__u32) -1 << (32 - T4_EOL_BITSIZE))

#define SFF_LEN_FLD_SIZE	3

#define _DLE_	0x10
#define _ETX_	0x03

typedef struct eicon_sff_dochead {
	__u32	id		__attribute__ ((packed));
	__u8	version		__attribute__ ((packed));
	__u8	reserved1	__attribute__ ((packed));
	__u16	userinfo	__attribute__ ((packed));
	__u16	pagecount	__attribute__ ((packed));
	__u16	off1pagehead	__attribute__ ((packed));
	__u32	offnpagehead	__attribute__ ((packed));
	__u32	offdocend	__attribute__ ((packed));
} eicon_sff_dochead;

typedef struct eicon_sff_pagehead {
	__u8	pageheadid	__attribute__ ((packed));
	__u8	pageheadlen	__attribute__ ((packed));
	__u8	resvert		__attribute__ ((packed));
	__u8	reshoriz	__attribute__ ((packed));
	__u8	coding		__attribute__ ((packed));
	__u8	reserved2	__attribute__ ((packed));
	__u16	linelength	__attribute__ ((packed));
	__u16	pagelength	__attribute__ ((packed));
	__u32	offprevpage	__attribute__ ((packed));
	__u32	offnextpage	__attribute__ ((packed));
} eicon_sff_pagehead;

#endif	/* DSP_H */

