#ifndef IEEE_802_11
#define IEEE_802_11  

#include <linux/types.h>

enum ieee_802_11_link_status_failure_reason {
	reserved0, Unspecified=1, Previous_not_valid, 
	Sender_Quits_ESS_or_IBSS,
	Due_Inactivity, AP_Overload, 
	Class_2_from_NonAuth,
	Class_3_from_NonAuth,
	Sender_Quits_BSS,
	Association_requester_not_authenticated,
	Reserved10 
};
	
	
#define IEEE_802_11_LINK_STATUS_FAILURE_REASON_STRINGS \
{	\
        {reserved0,		0xff," Reserved reason "},\
        {Unspecified,		0xff," Unspecified Reason "},\
        {Previous_not_valid,	0xff," Previous Authentication no longer valid "},\
        {Sender_Quits_ESS_or_IBSS,0xff," Deauthenticated because sending station is leaving (has left) IBSS or ESS "},\
        {Due_Inactivity,	0xff," Disassociated due to inactivity "},\
        {AP_Overload,		0xff," Disassociated because AP is unable to handle all currently associated stations "},\
        {Class_2_from_NonAuth,	0xff," Class 2 frame received from non-Authenticated station"},\
        {Class_3_from_NonAuth,	0xff," Class 3 frame received from non­Associated station"},\
        {Sender_Quits_BSS,	0xff," Disassociated because sending station is leaving (has left) BSS"},\
        {Association_requester_not_authenticated,0xff," Station requesting (Re)Association is not Authenticated with responding station"},\
        {Reserved10,		0xff," Reserved"},\
	{0,0,NULL}\
};



struct ieee_802_11_header {
	u16	frame_control;// needs to be subtyped
	u16	duration;
	u8	mac1[6];
	u8	mac2[6];
	u8	mac3[6];
	u16	SeqCtl;
	u8	mac4[6];
	u16	gapLen;
	u8	gap[8];
};


struct ieee_802_3_header {

	u16	status;
	u16	payload_length;
	u8	dst_mac[6];
	u8	src_mac[6];
	
};

#define P80211_OUI_LEN 3

struct ieee_802_11_snap_header { 

	u8    dsap;   /* always 0xAA */
	u8    ssap;   /* always 0xAA */
	u8    ctrl;   /* always 0x03 */
	u8    oui[P80211_OUI_LEN];    /* organizational universal id */

} __attribute__ ((packed));

#define P80211_LLC_OUI_LEN 3

struct ieee_802_11_802_1H_header {

	u8    dsap;   
	u8    ssap;   /* always 0xAA */
	u8    ctrl;   /* always 0x03 */
	u8    oui[P80211_OUI_LEN];    /* organizational universal id */
	u16    unknown1;      /* packet type ID fields */
	u16    unknown2;		/* here is something like length in some cases */
} __attribute__ ((packed));

struct ieee_802_11_802_2_header {

	u8    dsap;   
	u8    ssap;   /* always 0xAA */
	u8    ctrl;   /* always 0x03 */
	u8    oui[P80211_OUI_LEN];    /* organizational universal id */
	u8    type;      /* packet type ID field. i guess,  */

} __attribute__ ((packed));



// following is incoplete and may be incorrect and need reorganization

#define ieee_802_11_frame_type_Management	0x00
#define ieee_802_11_frame_type_Control		0x01
#define ieee_802_11_frame_type_Data		0x10
#define ieee_802_11_frame_type_Reserved		0x11

#define ieee_802_11_frame_subtype_Association_Req	0x0 // Association Request
#define ieee_802_11_frame_subtype_Association_Resp	0x1 // Association Response
#define ieee_802_11_frame_subtype_Reassociation_Req	0x2 // Reassociation Request
#define ieee_802_11_frame_subtype_Reassociation_Resp	0x3 // Reassociation Response
#define ieee_802_11_frame_subtype_Probe_Req		0x4 // Probe Request
#define ieee_802_11_frame_subtype_Probe_Resp		0x5 // Probe Response
#define ieee_802_11_frame_subtype_Beacon 		0x8 // Beacon
#define ieee_802_11_frame_subtype_ATIM 			0x9 // ATIM
#define ieee_802_11_frame_subtype_Disassociation 	0xA // Disassociation
#define ieee_802_11_frame_subtype_Authentication 	0xB // Authentication
#define ieee_802_11_frame_subtype_Deauthentication 	0xC // Deauthentication
#define ieee_802_11_frame_subtype_PS_Poll 		0xA // PS-Poll
#define ieee_802_11_frame_subtype_RTS 			0xB // RTS
#define ieee_802_11_frame_subtype_CTS 			0xC // CTS
#define ieee_802_11_frame_subtype_ACK 			0xD // ACK
#define ieee_802_11_frame_subtype_CFEnd 		0xE // CF-End
#define ieee_802_11_frame_subtype_CFEnd_CFAck 		0xF // CF-End + CF-Ack
#define ieee_802_11_frame_subtype_Data 			0x0 // Data
#define ieee_802_11_frame_subtype_Data_CFAck 		0x1 // Data + CF-Ack
#define ieee_802_11_frame_subtype_Data_CF_Poll 		0x2 // Data + CF-Poll
#define ieee_802_11_frame_subtype_Data_CF_AckCF_Poll 	0x3 // Data + CF-Ack + CF-Poll
#define ieee_802_11_frame_subtype_NullFunction 		0x4 // Null Function (no data)
#define ieee_802_11_frame_subtype_CF_Ack 		0x5 // CF-Ack (no data)
#define ieee_802_11_frame_subtype_CF_Poll 		0x6 // CF-Poll (no data)
#define ieee_802_11_frame_subtype_CF_AckCF_Poll 	0x7 // CF-Ack + CF-Poll (no data)


#define ieee_802_11_frame_subtype_strings {\
	{ ieee_802_11_frame_subtype_Association_Req,	0xF,"f  Association Request"},\
	{ ieee_802_11_frame_subtype_Association_Resp,	0xF,"1  Association Response"},\
	{ ieee_802_11_frame_subtype_Reassociation_Req,	0xF,"2  Reassociation Request"},\
	{ ieee_802_11_frame_subtype_Reassociation_Resp,	0xF,"3  Reassociation Response"},\
	{ ieee_802_11_frame_subtype_Probe_Req	,	0xF,"4  Probe Request"},\
	{ ieee_802_11_frame_subtype_Probe_Resp	,	0xF,"5  Probe Response"},\
	{ ieee_802_11_frame_subtype_Beacon 	,	0xF,"8  Beacon"},\
	{ ieee_802_11_frame_subtype_ATIM 	,	0xF,"9  ATIM"},\
	{ ieee_802_11_frame_subtype_Disassociation,	0xF,"A  Disassociation"},\
	{ ieee_802_11_frame_subtype_Authentication,	0xF,"B  Authentication"},\
	{ ieee_802_11_frame_subtype_Deauthentication,	0xF,"C  Deauthentication"},\
	{ ieee_802_11_frame_subtype_PS_Poll 	,	0xF,"A  PS-Poll"},\
	{ ieee_802_11_frame_subtype_RTS 	,	0xF,"B  RTS"},\
	{ ieee_802_11_frame_subtype_CTS 	,	0xF,"C  CTS"},\
	{ ieee_802_11_frame_subtype_ACK 	,	0xF,"D  ACK"},\
	{ ieee_802_11_frame_subtype_CFEnd	,	0xF,"E  CF-End"},\
	{ ieee_802_11_frame_subtype_CFEnd_CFAck ,	0xF,"F  CF-End + CF-Ack"},\
	{ ieee_802_11_frame_subtype_Data 	,	0xF,"0  Data"},\
	{ ieee_802_11_frame_subtype_Data_CFAck 	,	0xF,"1  Data + CF-Ack"},\
	{ ieee_802_11_frame_subtype_Data_CFPoll ,	0xF,"2  Data + CF-Poll"},\
	{ ieee_802_11_frame_subtype_Data_CFAck_CFPoll,	0xF,"3  Data + CF-Ack + CF-Poll"},\
	{ ieee_802_11_frame_subtype_Null_Function ,	0xF,"4  Null Function (no data)"},\
	{ ieee_802_11_frame_subtype_CFAck ,		0xF,"5  CF-Ack (no data)"},\
	{ ieee_802_11_frame_subtype_CFPoll ,		0xF,"6  CF-Poll (no data)"},\
	{ ieee_802_11_frame_subtype_CFAck_CFPoll,	0xF,"y7  CF-Ack + CF-Poll (no data)"},\
	{ 0,0,NULL}\
}
struct ieee_802_11_frame_subtype_class {
	u8	subtype;
	u8	mask;
	u8	class;
	u8	type;
};
#define ieee_802_11_frame_subtype_classes {\
	{ ieee_802_11_frame_subtype_Association_Req,	0xF,2,ieee_802_11_frame_type_Management},\
	{ ieee_802_11_frame_subtype_Association_Resp,	0xF,2,ieee_802_11_frame_type_Management},\
	{ ieee_802_11_frame_subtype_Reassociation_Req,	0xF,2,ieee_802_11_frame_type_Management},\
	{ ieee_802_11_frame_subtype_Reassociation_Resp,	0xF,2,ieee_802_11_frame_type_Management},\
	{ ieee_802_11_frame_subtype_Probe_Req	,	0xF,1,ieee_802_11_frame_type_Management},\
	{ ieee_802_11_frame_subtype_Probe_Resp	,	0xF,1,ieee_802_11_frame_type_Management},\
	{ ieee_802_11_frame_subtype_Beacon 	,	0xF,1,ieee_802_11_frame_type_Management},\
	{ ieee_802_11_frame_subtype_ATIM 	,	0xF,1,ieee_802_11_frame_type_Management},\
	{ ieee_802_11_frame_subtype_Disassociation,	0xF,2,ieee_802_11_frame_type_Management},\
	{ ieee_802_11_frame_subtype_Authentication,	0xF,1,ieee_802_11_frame_type_Management},\
	{ ieee_802_11_frame_subtype_Deauthentication,	0xF,3,ieee_802_11_frame_type_Management},\
	{ ieee_802_11_frame_subtype_PS-Poll 	,	0xF,3,ieee_802_11_frame_type_Control},\
	{ ieee_802_11_frame_subtype_RTS 	,	0xF,1,ieee_802_11_frame_type_Control},\
	{ ieee_802_11_frame_subtype_CTS 	,	0xF,1,ieee_802_11_frame_type_Control},\
	{ ieee_802_11_frame_subtype_ACK 	,	0xF,1,ieee_802_11_frame_type_Control},\
	{ ieee_802_11_frame_subtype_CFEnd	,	0xF,1,ieee_802_11_frame_type_Control},\
	{ ieee_802_11_frame_subtype_CFEnd_CFAck ,	0xF,1,ieee_802_11_frame_type_Control},\
	{ ieee_802_11_frame_subtype_Data 	,	0xF,3,ieee_802_11_frame_type_Data},\
	{ ieee_802_11_frame_subtype_Data_CFAck 	,	0xF,3,ieee_802_11_frame_type_Data},\
	{ ieee_802_11_frame_subtype_Data_CF_Poll 	0xF,3,ieee_802_11_frame_type_Data},\
	{ ieee_802_11_frame_subtype_Data_CF_AckCF_Poll,	0xF,3,ieee_802_11_frame_type_Data},\
	{ ieee_802_11_frame_subtype_NullFunction 	0xF,1,ieee_802_11_frame_type_Data},\
	{ ieee_802_11_frame_subtype_CF_Ack ,		0xF,1,ieee_802_11_frame_type_Data},\
	{ ieee_802_11_frame_subtype_CF_Poll ,		0xF,1,ieee_802_11_frame_type_Data},\
	{ ieee_802_11_frame_subtype_CF_AckCF_Poll,	0xF,1,ieee_802_11_frame_type_Data},\
	{ 0,0,NULL}\
}


#endif
