/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD: src/sys/netatm/atm.h,v 1.2 1999/08/28 00:48:34 peter Exp $
 *
 */

/*
 * Core ATM Services
 * -----------------
 *
 * ATM address family definitions
 *
 */

#ifndef _NETATM_ATM_H
#define _NETATM_ATM_H


/*
 * The definitions in this file are intended to conform to the 
 * specifications defined in:
 *
 *	The Open Group, Networking Services (XNS) Issue 5
 *
 *	ATM Transport Protocol Information for Sockets
 *
 * which is Copyright (c) 1997, The Open Group.
 *
 * All extensions contained in this file to the base specification 
 * are denoted with a comment string of "XNS_EXT".
 */

/*
 * ATM socket protocols
 */
#define	ATM_PROTO_AAL5		0x5301	/* AAL type 5 protocol */
#define	ATM_PROTO_SSCOP		0x5302	/* SSCOP protocol      */


/*
 * ATM address defintions
 */
/*
 * General format of an ATM address
 */
#define	ATM_ADDR_LEN	20		/* Size of address field (XNS_EXT) */

struct t_atm_addr {
	int8_t		address_format;	/* Address format (see below) */
	u_int8_t	address_length;	/* Length of address field */
	u_int8_t	address[ATM_ADDR_LEN];	/* Address field */
};
typedef struct t_atm_addr	Atm_addr;	/* XNS_EXT */

/*
 * ATM address formats
 */
#define	T_ATM_ABSENT		(-1)	/* No address present */
#define	T_ATM_ENDSYS_ADDR	1	/* ATM Endsystem */
#define	T_ATM_NSAP_ADDR		1	/* NSAP */
#define	T_ATM_E164_ADDR		2	/* E.164 */
#define	T_ATM_SPANS_ADDR	3	/* FORE SPANS (XNS_EXT) */
#define	T_ATM_PVC_ADDR		4	/* PVC (VPI,VCI) (XNS_EXT) */

/*
 * ATM Endsystem / NSAP address format
 */
struct atm_addr_nsap {			/* XNS_EXT */
	u_char		aan_afi;	/* Authority and Format Identifier */
					/* (see below) */
	u_char		aan_afspec[12];	/* AFI specific fields */
	u_char		aan_esi[6];	/* End System Identifier */
	u_char		aan_sel;	/* Selector */
};
typedef struct atm_addr_nsap	Atm_addr_nsap;

/*
 * AFI codes
 */
#define	AFI_DCC		0x39		/* DCC ATM Format (XNS_EXT) */
#define	AFI_ICD		0x47		/* ICD ATM Format (XNS_EXT) */
#define	AFI_E164	0x45		/* E.164 ATM Format (XNS_EXT) */

/*
 * E.164 address format
 */
struct atm_addr_e164 {			/* XNS_EXT */
	u_char		aae_addr[15];	/* E.164 address */
};
typedef struct atm_addr_e164	Atm_addr_e164;

/*
 * SPANS address format
 */
struct atm_addr_spans {			/* XNS_EXT */
	u_char		aas_addr[8];	/* See SPANS code for specific fields */
};
typedef struct atm_addr_spans	Atm_addr_spans;

/*
 * PVC address format
 */
struct atm_addr_pvc {			/* XNS_EXT */
	u_int8_t	aap_vpi[2];	/* VPI */
	u_int8_t	aap_vci[2];	/* VCI */
};
typedef struct atm_addr_pvc	Atm_addr_pvc;

#define	ATM_PVC_GET_VPI(addr)		/* XNS_EXT */		\
	((u_int16_t)(((addr)->aap_vpi[0] << 8) | (addr)->aap_vpi[1]))
#define	ATM_PVC_GET_VCI(addr)		/* XNS_EXT */		\
	((u_int16_t)(((addr)->aap_vci[0] << 8) | (addr)->aap_vci[1]))
#define	ATM_PVC_SET_VPI(addr,vpi) {	/* XNS_EXT */		\
	(addr)->aap_vpi[0] = ((vpi) >> 8) & 0xff;		\
	(addr)->aap_vpi[1] = (vpi) & 0xff;			\
}
#define	ATM_PVC_SET_VCI(addr,vci) {	/* XNS_EXT */		\
	(addr)->aap_vci[0] = ((vci) >> 8) & 0xff;		\
	(addr)->aap_vci[1] = (vci) & 0xff;			\
}


/*
 * ATM service access point (SAP)
 *
 * A SAP address consists of SAP Vector Elements (SVE).  Each SVE consists 
 * of the following fields:
 * 	o tag - defines the interpretation of the SVE;
 * 	o length - the length of the SVE value field;
 * 	o value - the value associated with the SVE;
 *
 * All of the possible SAP field values are either defined below
 * or in the corresponding option value definitions.
 */

/*
 * ATM Address and Selector SVE
 */
struct t_atm_sap_addr {
	int8_t		SVE_tag_addr;	/* SVE tag (address) */
	int8_t		SVE_tag_selector; /* SVE tag (selector) */
					/* Address/selector value */
	int8_t		address_format;	/* Address format */
	u_int8_t	address_length;	/* Length of address field */
	u_int8_t	address[ATM_ADDR_LEN];	/* Address field */
};

/*
 * B-LLI Layer 2 SVE
 */
struct t_atm_sap_layer2 {
	int8_t		SVE_tag;	/* SVE tag */
	u_int8_t	ID_type;	/* Layer 2 protocol discriminator */
	union {				/* Layer 2 protocol */
		u_int8_t	simple_ID;	/* ITU */
		u_int8_t	user_defined_ID;/* User-defined */
	} ID;
};

/*
 * B-LLI Layer 3 SVE
 */
struct t_atm_sap_layer3 {
	int8_t		SVE_tag;	/* SVE tag */
	u_int8_t	ID_type;	/* Layer 3 protocol discriminator */
	union {				/* Layer 3 protocol */
		u_int8_t	simple_ID;	/* ITU */
		u_int8_t	IPI_ID;		/* ISO IPI */
		struct {			/* IEEE 802.1 SNAP ID */
			u_int8_t	OUI[3];
			u_int8_t	PID[2];
		} SNAP_ID;
		u_int8_t	user_defined_ID;/* User-defined */
	} ID;
};

/*
 * B_HLI SVE
 */
struct t_atm_sap_appl {
	int8_t		SVE_tag;	/* SVE tag */
	u_int8_t	ID_type;	/* High Layer type discriminator */
	union {				/* High Layer type */
		u_int8_t	ISO_ID[8];	/* ISO */
		struct {			/* Vendor-specific */
			u_int8_t	OUI[3];
			u_int8_t	app_ID[4];
		} vendor_ID;
		u_int8_t	user_defined_ID[8];/* User-defined */
	} ID;
};

/*
 * ATM SAP (protocol) address structure
 */
struct t_atm_sap {
	struct t_atm_sap_addr		t_atm_sap_addr;
	struct t_atm_sap_layer2		t_atm_sap_layer2;
	struct t_atm_sap_layer3		t_atm_sap_layer3;
	struct t_atm_sap_appl		t_atm_sap_appl;
};

/*
 * SVE Tag values
 */
#define	T_ATM_ABSENT		(-1)	/* Value field invalid; match none */
#define	T_ATM_PRESENT		(-2)	/* Value field valid; match value */
#define	T_ATM_ANY		(-3)	/* Value field invalid; match any */


/*
 * ATM socket address
 */
struct sockaddr_atm {			/* XNS_EXT */
#if (defined(BSD) && (BSD >= 199103))
	u_char		satm_len;	/* Length of socket structure */
	u_char		satm_family;	/* Address family */
#else
	u_short		satm_family;	/* Address family */
#endif
	struct t_atm_sap	satm_addr;	/* Protocol address */
};


/*
 * ATM socket options for use with [gs]etsockopt()
 */
#define	T_ATM_SIGNALING		0x5301	/* Option level */

#define	T_ATM_AAL5		1	/* ATM adaptation layer 5      */
#define	T_ATM_TRAFFIC		2	/* ATM traffic descriptor      */
#define	T_ATM_BEARER_CAP	3	/* ATM service capabilities    */
#define	T_ATM_BHLI		4	/* Higher-layer protocol       */
#define	T_ATM_BLLI		5	/* Lower-layer protocol        */
#define	T_ATM_DEST_ADDR		6	/* Call responder's address    */
#define	T_ATM_DEST_SUB		7	/* Call responder's subaddress */
#define	T_ATM_ORIG_ADDR		8	/* Call initiator's address    */
#define	T_ATM_ORIG_SUB		9	/* Call initiator's subaddress */
#define	T_ATM_CALLER_ID		10	/* Caller's ID attributes      */
#define	T_ATM_CAUSE		11	/* Cause of disconection       */
#define	T_ATM_QOS		12	/* Quality of service          */
#define	T_ATM_TRANSIT		13	/* Choice of public carrier    */
#define	T_ATM_ADD_LEAF		14	/* Add leaf to connection      */
#define	T_ATM_DROP_LEAF		15	/* Remove leaf from connection */
#define	T_ATM_LEAF_IND		16	/* Indication of leaf status   */
#define	T_ATM_NET_INTF		17	/* Network interface XNS_EXT   */
#define	T_ATM_LLC		18	/* LLC multiplexing XNS_EXT    */
#define	T_ATM_APP_NAME		19	/* Application name XNS_EXT    */


/*
 * Common socket option values
 *
 * See API specification for individual option applicability/meaning
 */
#define	T_ATM_ABSENT		(-1)	/* No option value present */
#define	T_ATM_NULL		0	/* Option value is null */
#define	T_NO			0	/* Option is not requested */
#define	T_YES			1	/* Option is requested */


/*
 * T_ATM_AAL5 option value structure
 */
struct t_atm_aal5 {
	int32_t		forward_max_SDU_size;
	int32_t		backward_max_SDU_size;
	int32_t		SSCS_type;
};

/*
 * T_ATM_AAL5 option values
 */
		/* SSCS_type */
#define	T_ATM_SSCS_SSCOP_REL	1	/* SSCOP assured operation */
#define	T_ATM_SSCS_SSCOP_UNREL	2	/* SSCOP non-assured operation */
#define	T_ATM_SSCS_FR		4	/* Frame relay */


/*
 * T_ATM_TRAFFIC option value structure
 */
struct t_atm_traffic_substruct {
	int32_t		PCR_high_priority;
	int32_t		PCR_all_traffic;
	int32_t		SCR_high_priority;
	int32_t		SCR_all_traffic;
	int32_t		MBS_high_priority;
	int32_t		MBS_all_traffic;
	int32_t		tagging;
};

struct t_atm_traffic {
	struct t_atm_traffic_substruct	forward;
	struct t_atm_traffic_substruct	backward;
	u_int8_t	best_effort;
};


/*
 * T_ATM_BEARER_CAP option value structure
 */
struct t_atm_bearer {
	u_int8_t	bearer_class;
	u_int8_t	traffic_type;
	u_int8_t	timing_requirements;
	u_int8_t	clipping_susceptibility;
	u_int8_t	connection_configuration;
};

/*
 * T_ATM_BEARER_CAP option values
 */
		/* bearer_class */
#define	T_ATM_CLASS_A		0x01	/* Bearer class A                 */
#define	T_ATM_CLASS_C		0x03	/* Bearer class C                 */
#define	T_ATM_CLASS_X		0x10	/* Bearer class X                 */

		/* traffic_type */
#define	T_ATM_CBR		0x01	/* Constant bit rate              */
#define	T_ATM_VBR		0x02	/* Variable bit rate              */

		/* timing_requirements */
#define	T_ATM_END_TO_END	0x01	/* End-to-end timing required     */
#define	T_ATM_NO_END_TO_END	0x02	/* End-to-end timing not required */

		/* connection_configuration */
#define	T_ATM_1_TO_1		0x00	/* Point-to-point connection      */
#define	T_ATM_1_TO_MANY		0x01	/* Point-to-multipoint connection */


/*
 * T_ATM_BHLI option value structure
 */
struct t_atm_bhli {
	int32_t		ID_type;
	union {
		u_int8_t	ISO_ID[8];
		struct {
			u_int8_t	OUI[3];
			u_int8_t	app_ID[4];
		} vendor_ID;
		u_int8_t	user_defined_ID[8];
	} ID;
};

/*
 * T_ATM_BHLI option values
 */
		/* ID_type */
#define	T_ATM_ISO_APP_ID	0	/* ISO codepoint             */
#define	T_ATM_USER_APP_ID	1	/* User-specific codepoint   */
#define	T_ATM_VENDOR_APP_ID	3	/* Vendor-specific codepoint */

/*
 * T_ATM_BLLI option value structure
 */
struct t_atm_blli {
	struct {
		int8_t		ID_type;
		union {
			u_int8_t	simple_ID;
			u_int8_t	user_defined_ID;
		} ID;
		int8_t		mode;
		int8_t		window_size;
	} layer_2_protocol;
	struct {
		int8_t		ID_type;
		union {
			u_int8_t	simple_ID;
			int32_t		IPI_ID;
			struct {
				u_int8_t	OUI[3];
				u_int8_t	PID[2];
			} SNAP_ID;
			u_int8_t	user_defined_ID;
		} ID;
		int8_t		mode;
		int8_t		packet_size;
		int8_t		window_size;
	} layer_3_protocol;
};


/*
 * T_ATM_BLLI option values
 */
		/* layer_[23]_protocol.ID_type */
#define	T_ATM_SIMPLE_ID		1	/* ID via ITU encoding    */
#define	T_ATM_IPI_ID		2	/* ID via ISO/IEC TR 9577 */
#define	T_ATM_SNAP_ID		3	/* ID via SNAP            */
#define	T_ATM_USER_ID		4	/* ID via user codepoints */

		/* layer_[23]_protocol.mode */
#define	T_ATM_BLLI_NORMAL_MODE	1
#define	T_ATM_BLLI_EXTENDED_MODE	2

		/* layer_2_protocol.simple_ID */
#define	T_ATM_BLLI2_I1745	1	/* I.1745           */
#define	T_ATM_BLLI2_Q921	2	/* Q.921            */
#define	T_ATM_BLLI2_X25_LINK	6	/* X.25, link layer */
#define	T_ATM_BLLI2_X25_MLINK	7	/* X.25, multilink  */
#define	T_ATM_BLLI2_LAPB	8	/* Extended LAPB    */
#define	T_ATM_BLLI2_HDLC_ARM	9	/* I.4335, ARM      */
#define	T_ATM_BLLI2_HDLC_NRM	10	/* I.4335, NRM      */
#define	T_ATM_BLLI2_HDLC_ABM	11	/* I.4335, ABM      */
#define	T_ATM_BLLI2_I8802	12	/* I.8802           */
#define	T_ATM_BLLI2_X75		13	/* X.75             */
#define	T_ATM_BLLI2_Q922	14	/* Q.922            */
#define	T_ATM_BLLI2_I7776	17	/* I.7776           */

		/* layer_3_protocol.simple_ID */
#define	T_ATM_BLLI3_X25		6	/* X.25             */
#define	T_ATM_BLLI3_I8208	7	/* I.8208           */
#define	T_ATM_BLLI3_X223	8	/* X.223            */
#define	T_ATM_BLLI3_I8473	9	/* I.8473           */
#define	T_ATM_BLLI3_T70		10	/* T.70             */
#define	T_ATM_BLLI3_I9577	11	/* I.9577           */

		/* layer_3_protocol.packet_size */
#define	T_ATM_PACKET_SIZE_16	4
#define	T_ATM_PACKET_SIZE_32	5
#define	T_ATM_PACKET_SIZE_64	6
#define	T_ATM_PACKET_SIZE_128	7
#define	T_ATM_PACKET_SIZE_256	8
#define	T_ATM_PACKET_SIZE_512	9
#define	T_ATM_PACKET_SIZE_1024	10
#define	T_ATM_PACKET_SIZE_2048	11
#define	T_ATM_PACKET_SIZE_4096	12


/*
 * T_ATM_CALLER_ID option value structure
 */
struct t_atm_caller_id {
	int8_t		presentation;
	u_int8_t	screening;
};

/*
 * T_ATM_CALLER_ID option values
 */
		/* presentation */
#define	T_ATM_PRES_ALLOWED		0
#define	T_ATM_PRES_RESTRICTED		1
#define	T_ATM_PRES_UNAVAILABLE		2
		/* screening */
#define	T_ATM_USER_ID_NOT_SCREENED	0
#define	T_ATM_USER_ID_PASSED_SCREEN	1
#define	T_ATM_USER_ID_FAILED_SCREEN	2
#define	T_ATM_NETWORK_PROVIDED_ID	3


/*
 * T_ATM_CAUSE option value structure
 */
struct t_atm_cause {
	int8_t		coding_standard;
	u_int8_t	location;
	u_int8_t	cause_value;
	u_int8_t	diagnostics[4];
};

/*
 * T_ATM_CAUSE option values
 */
		/* coding_standard */
#define	T_ATM_ITU_CODING		0
#define	T_ATM_NETWORK_CODING		3

		/* location */
#define	T_ATM_LOC_USER			0
#define	T_ATM_LOC_LOCAL_PRIVATE_NET	1
#define	T_ATM_LOC_LOCAL_PUBLIC_NET	2
#define	T_ATM_LOC_TRANSIT_NET		3
#define	T_ATM_LOC_REMOTE_PUBLIC_NET	4
#define	T_ATM_LOC_REMOTE_PRIVATE_NET	5
#define	T_ATM_LOC_INTERNATIONAL_NET	7
#define	T_ATM_LOC_BEYOND_INTERWORKING	10

		/* cause_value */
#define	T_ATM_CAUSE_UNALLOCATED_NUMBER				1
#define	T_ATM_CAUSE_NO_ROUTE_TO_TRANSIT_NETWORK			2
#define	T_ATM_CAUSE_NO_ROUTE_TO_DESTINATION			3
#define	T_ATM_CAUSE_NORMAL_CALL_CLEARING			16
#define	T_ATM_CAUSE_USER_BUSY					17
#define	T_ATM_CAUSE_NO_USER_RESPONDING				18
#define	T_ATM_CAUSE_CALL_REJECTED				21
#define	T_ATM_CAUSE_NUMBER_CHANGED				22
#define	T_ATM_CAUSE_ALL_CALLS_WITHOUT_CALLER_ID_REJECTED	23
#define	T_ATM_CAUSE_DESTINATION_OUT_OF_ORDER			27
#define	T_ATM_CAUSE_INVALID_NUMBER_FORMAT			28
#define	T_ATM_CAUSE_RESPONSE_TO_STATUS_ENQUIRY			30
#define	T_ATM_CAUSE_UNSPECIFIED_NORMAL				31
#define	T_ATM_CAUSE_REQUESTED_VPCI_VCI_NOT_AVAILABLE		35
#define	T_ATM_CAUSE_VPCI_VCI_ASSIGNMENT_FAILURE			36
#define	T_ATM_CAUSE_USER_CELL_RATE_NOT_AVAILABLE		37
#define	T_ATM_CAUSE_NETWORK_OUT_OF_ORDER			38
#define	T_ATM_CAUSE_TEMPORARY_FAILURE				41
#define	T_ATM_CAUSE_ACCESS_INFO_DISCARDED			43
#define	T_ATM_CAUSE_NO_VPCI_VCI_AVAILABLE			45
#define	T_ATM_CAUSE_UNSPECIFIED_RESOURCE_UNAVAILABLE		47
#define	T_ATM_CAUSE_QUALITY_OF_SERVICE_UNAVAILABLE		49
#define	T_ATM_CAUSE_BEARER_CAPABILITY_NOT_AUTHORIZED		57
#define	T_ATM_CAUSE_BEARER_CAPABILITY_UNAVAILABLE		58
#define	T_ATM_CAUSE_SERVICE_OR_OPTION_UNAVAILABLE		63
#define	T_ATM_CAUSE_BEARER_CAPABILITY_NOT_IMPLEMENTED		65
#define	T_ATM_CAUSE_INVALID_TRAFFIC_PARAMETERS			73
#define	T_ATM_CAUSE_AAL_PARAMETERS_NOT_SUPPORTED		78
#define	T_ATM_CAUSE_INVALID_CALL_REFERENCE_VALUE		81
#define	T_ATM_CAUSE_IDENTIFIED_CHANNEL_DOES_NOT_EXIST		82
#define	T_ATM_CAUSE_INCOMPATIBLE_DESTINATION			88
#define	T_ATM_CAUSE_INVALID_ENDPOINT_REFERENCE			89
#define	T_ATM_CAUSE_INVALID_TRANSIT_NETWORK_SELECTION		91
#define	T_ATM_CAUSE_TOO_MANY_PENDING_ADD_PARTY_REQUESTS		92
#define	T_ATM_CAUSE_MANDITORY_INFO_ELEMENT_MISSING		96
#define	T_ATM_CAUSE_MESSAGE_TYPE_NOT_IMPLEMENTED		97
#define	T_ATM_CAUSE_INFO_ELEMENT_NOT_IMPLEMENTED		99
#define	T_ATM_CAUSE_INVALID_INFO_ELEMENT_CONTENTS		100
#define	T_ATM_CAUSE_MESSAGE_INCOMPATIBLE_WITH_CALL_STATE	101
#define	T_ATM_CAUSE_RECOVERY_ON_TIMER_EXPIRY			102
#define	T_ATM_CAUSE_INCORRECT_MESSAGE_LENGTH			104
#define	T_ATM_CAUSE_UNSPECIFIED_PROTOCOL_ERROR			111


/*
 * T_ATM_QOS option value structure
 */
struct t_atm_qos_substruct {
	int32_t		qos_class;
};

struct t_atm_qos {
	int8_t		coding_standard;
	struct t_atm_qos_substruct	forward;
	struct t_atm_qos_substruct	backward;
};

/*
 * T_ATM_QOS option values
 */
		/* qos_class */
#define	T_ATM_QOS_CLASS_0	0
#define	T_ATM_QOS_CLASS_1	1
#define	T_ATM_QOS_CLASS_2	2
#define	T_ATM_QOS_CLASS_3	3
#define	T_ATM_QOS_CLASS_4	4


/*
 * T_ATM_TRANSIT structure
 */
#define	T_ATM_MAX_NET_ID	4		/* XNS_EXT */
struct t_atm_transit {
	u_int8_t	length;
	u_int8_t	network_id[T_ATM_MAX_NET_ID];
};


/*
 * T_ATM_ADD_LEAF option value structure
 */
struct t_atm_add_leaf {
	int32_t		leaf_ID;
	struct t_atm_addr	leaf_address;
};


/*
 * T_ATM_DROP_LEAF option value structure
 */
struct t_atm_drop_leaf {
	int32_t		leaf_ID;
	int32_t		reason;
};

/*
 * T_ATM_LEAF_IND option value structure
 */
struct t_atm_leaf_ind {
	int32_t		status;
	int32_t		leaf_ID;
	int32_t		reason;
};

/*
 * T_ATM_LEAF_IND option values
 */
		/* status */
#define	T_LEAF_NOCHANGE		0
#define	T_LEAF_CONNECTED	1
#define	T_LEAF_DISCONNECTED	2

/*
 * T_ATM_NET_INTF option value structure	(XNS_EXT)
 */
struct t_atm_net_intf {				/* XNS_EXT */
	char		net_intf[IFNAMSIZ];
};

/*
 * T_ATM_LLC option value structure		(XNS_EXT)
 */
#define	T_ATM_LLC_MIN_LEN	3
#define	T_ATM_LLC_MAX_LEN	8

struct t_atm_llc {				/* XNS_EXT */
	u_int8_t	flags;			/* LLC flags (see below) */
	u_int8_t	llc_len;		/* Length of LLC information */
	u_int8_t	llc_info[T_ATM_LLC_MAX_LEN];	/* LLC information */
};

/*
 * T_ATM_LLC option values
 */
		/* flags */
#define	T_ATM_LLC_SHARING	0x01		/* LLC sharing allowed */

/*
 * T_ATM_APP_NAME option value structure	(XNS_EXT)
 */
#define	T_ATM_APP_NAME_LEN	8
struct t_atm_app_name {				/* XNS_EXT */
	char		app_name[T_ATM_APP_NAME_LEN];
};

#endif	/* _NETATM_ATM_H */
