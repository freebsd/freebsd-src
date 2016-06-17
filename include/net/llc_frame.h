/* if_ether.h needed for definition of ETH_DATA_LEN and ETH_ALEN
 */
#include "linux/if_ether.h"

/* frame layout based on par3.2 "LLC PDU format"
 */
typedef union {			/* pdu layout from pages 40 & 44 */
    struct {			/* general header, all pdu types */
	unsigned dsap : 8;	/* dest service access point */
	unsigned ssap : 8;	/* source service access point */
	unsigned f1 : 1;	/* I- U- or S- format id bits */
	unsigned f2 : 1;
	unsigned : 6;
	unsigned : 8;
   } pdu_hdr;
   struct {
        char dummy1[2];   	/* dsap + ssap */
	char byte1;
	char byte2;
   } pdu_cntl;			/* unformatted control bytes */
   struct {			/* header of an Information pdu */
	unsigned char dummy2[2];
	unsigned : 1;
	unsigned ns : 7;
	unsigned i_pflag : 1;   /* poll/final bit */
	unsigned nr : 7;	/* N(R)  */	
	unsigned char is_info[ ETH_DATA_LEN ];
   }  i_hdr;
   struct {			/* header of a Supervisory pdu */
 	unsigned char dummy3[2];
	unsigned : 2;
	unsigned ss : 2;	/* supervisory function bits */
	unsigned : 4;
	unsigned s_pflag : 1;   /* poll/final bit  */
	unsigned nr : 7;	/* N(R)  */
   } s_hdr;

/* when accessing the P/F bit or the N(R) field there's no need to distinguish
   I pdus from S pdus i_pflag and s_pflag / i_nr and s_nr map to the same
   physical location.
 */ 
   struct {			/* header of an Unnumbered pdu */
	unsigned char dummy4[2];
	unsigned : 2;
	unsigned mm1 : 2;	/* modifier function part1 */
	unsigned u_pflag : 1;    /* P/F for U- pdus */
	unsigned mm2 : 3;	/* modifier function part2 */
	unsigned char u_info[ ETH_DATA_LEN-1];
   } u_hdr;
   struct {			/* mm field in an Unnumbered pdu */
	unsigned char dummy5[2];
	unsigned : 2;
	unsigned mm : 6;	/* must be masked to get ridd of P/F !  */
   } u_mm;
   	 
} frame_type, *frameptr;

/* frame format test macros: */

#define IS_UFRAME( fr ) ( ( (fr)->pdu_hdr.f1) & ( (fr)->pdu_hdr.f2) )

#define IS_IFRAME( fr ) ( !( (fr)->pdu_hdr.f1) )

#define IS_SFRAME( fr ) ( ( (fr)->pdu_hdr.f1) & !( (fr)->pdu_hdr.f2) )

#define IS_RSP( fr ) ( fr->pdu_hdr.ssap & 0x01 )


/* The transition table, the _encode tables and some tests in the
   source code depend on the numeric order of these values.
   Think twice before changing.
 */

/* frame names for TYPE 2 operation: */
#define I_CMD		0
#define RR_CMD		1
#define RNR_CMD		2
#define REJ_CMD		3
#define DISC_CMD	4
#define SABME_CMD	5
#define I_RSP		6
#define RR_RSP		7
#define RNR_RSP		8
#define REJ_RSP		9
#define UA_RSP		10
#define DM_RSP		11
#define FRMR_RSP	12

/* junk frame name: */
#define BAD_FRAME	13
#define NO_FRAME	13

/* frame names for TYPE 1 operation: */
#define UI_CMD		14
#define XID_CMD		15
#define TEST_CMD	16
#define XID_RSP		17
#define TEST_RSP	18
