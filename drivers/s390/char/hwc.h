/*
 *  drivers/s390/char/hwc.h
 * 
 *
 *  S390 version
 *    Copyright (C) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Martin Peschke <mpeschke@de.ibm.com>
 *
 * 
 * 
 */

#ifndef __HWC_H__
#define __HWC_H__

#define HWC_EXT_INT_PARAM_ADDR	0xFFFFFFF8
#define HWC_EXT_INT_PARAM_PEND 0x00000001

#define ET_OpCmd		0x01
#define ET_Msg		0x02
#define ET_StateChange	0x08
#define ET_PMsgCmd		0x09
#define ET_CntlProgOpCmd	0x20
#define ET_CntlProgIdent	0x0B
#define ET_SigQuiesce	0x1D

#define ET_OpCmd_Mask	0x80000000
#define ET_Msg_Mask		0x40000000
#define ET_StateChange_Mask	0x01000000
#define ET_PMsgCmd_Mask	0x00800000
#define ET_CtlProgOpCmd_Mask	0x00000001
#define ET_CtlProgIdent_Mask	0x00200000
#define ET_SigQuiesce_Mask	0x00000008

#define GMF_DOM		0x8000
#define GMF_SndAlrm	0x4000
#define GMF_HoldMsg	0x2000

#define LTF_CntlText	0x8000
#define LTF_LabelText	0x4000
#define LTF_DataText	0x2000
#define LTF_EndText	0x1000
#define LTF_PromptText	0x0800

#define HWC_COMMAND_INITIATED	0
#define HWC_BUSY		2
#define HWC_NOT_OPERATIONAL	3

#define hwc_cmdw_t u32;

#define HWC_CMDW_READDATA 0x00770005

#define HWC_CMDW_WRITEDATA 0x00760005

#define HWC_CMDW_WRITEMASK 0x00780005

#define GDS_ID_MDSMU		0x1310

#define GDS_ID_MDSRouteInfo	0x1311

#define GDS_ID_AgUnWrkCorr	0x1549

#define GDS_ID_SNACondReport	0x1532

#define GDS_ID_CPMSU		0x1212

#define GDS_ID_RoutTargInstr	0x154D

#define GDS_ID_OpReq		0x8070

#define GDS_ID_TextCmd		0x1320

#define GDS_KEY_SelfDefTextMsg	0x31

#define _HWCB_HEADER	u16	length; \
			u8	function_code; \
			u8	control_mask[3]; \
			u16	response_code;

#define _EBUF_HEADER 	u16	length; \
			u8	type; \
			u8	flags; \
			u16	_reserved;

typedef struct {
	_EBUF_HEADER
} __attribute__ ((packed)) 

evbuf_t;

#define _MDB_HEADER	u16	length; \
			u16	type; \
			u32	tag; \
			u32	revision_code;

#define _GO_HEADER	u16	length; \
			u16	type; \
			u32	domid; \
			u8	hhmmss_time[8]; \
			u8	th_time[3]; \
			u8	_reserved_0; \
			u8	dddyyyy_date[7]; \
			u8	_reserved_1; \
			u16	general_msg_flags; \
			u8	_reserved_2[10]; \
			u8	originating_system_name[8]; \
			u8	job_guest_name[8];

#define _MTO_HEADER	u16	length; \
			u16	type; \
			u16	line_type_flags; \
			u8	alarm_control; \
			u8	_reserved[3];

typedef struct {
	_GO_HEADER
} __attribute__ ((packed)) 

go_t;

typedef struct {
	go_t go;
} __attribute__ ((packed)) 

mdb_body_t;

typedef struct {
	_MDB_HEADER
	mdb_body_t mdb_body;
} __attribute__ ((packed)) 

mdb_t;

typedef struct {
	_EBUF_HEADER
	mdb_t mdb;
} __attribute__ ((packed)) 

msgbuf_t;

typedef struct {
	_HWCB_HEADER
	msgbuf_t msgbuf;
} __attribute__ ((packed)) 

write_hwcb_t;

typedef struct {
	_MTO_HEADER
} __attribute__ ((packed)) 

mto_t;

static write_hwcb_t write_hwcb_template =
{
	sizeof (write_hwcb_t),
	0x00,
	{
		0x00,
		0x00,
		0x00
	},
	0x0000,
	{
		sizeof (msgbuf_t),
		ET_Msg,
		0x00,
		0x0000,
		{
			sizeof (mdb_t),
			0x0001,
			0xD4C4C240,
			0x00000001,
			{
				{
					sizeof (go_t),
					0x0001

				}
			}
		}
	}
};

static mto_t mto_template =
{
	sizeof (mto_t),
	0x0004,
	LTF_EndText,
	0x00
};

typedef u32 _hwcb_mask_t;

typedef struct {
	_HWCB_HEADER
	u16 _reserved;
	u16 mask_length;
	_hwcb_mask_t cp_receive_mask;
	_hwcb_mask_t cp_send_mask;
	_hwcb_mask_t hwc_receive_mask;
	_hwcb_mask_t hwc_send_mask;
} __attribute__ ((packed)) 

init_hwcb_t;

static init_hwcb_t init_hwcb_template =
{
	sizeof (init_hwcb_t),
	0x00,
	{
		0x00,
		0x00,
		0x00
	},
	0x0000,
	0x0000,
	sizeof (_hwcb_mask_t),
	ET_OpCmd_Mask | ET_PMsgCmd_Mask |
	ET_StateChange_Mask | ET_SigQuiesce_Mask,
	ET_Msg_Mask | ET_PMsgCmd_Mask | ET_CtlProgIdent_Mask
};

typedef struct {
	_EBUF_HEADER
	u8 validity_hwc_active_facility_mask:1;
	u8 validity_hwc_receive_mask:1;
	u8 validity_hwc_send_mask:1;
	u8 validity_read_data_function_mask:1;
	u16 _zeros:12;
	u16 mask_length;
	u64 hwc_active_facility_mask;
	_hwcb_mask_t hwc_receive_mask;
	_hwcb_mask_t hwc_send_mask;
	u32 read_data_function_mask;
} __attribute__ ((packed)) 

statechangebuf_t;

#define _GDS_VECTOR_HEADER	u16	length; \
				u16	gds_id;

#define _GDS_SUBVECTOR_HEADER	u8	length; \
				u8	key;

typedef struct {
	_GDS_VECTOR_HEADER
} __attribute__ ((packed)) 

gds_vector_t;

typedef struct {
	_GDS_SUBVECTOR_HEADER
} __attribute__ ((packed)) 

gds_subvector_t;

typedef struct {
	_HWCB_HEADER
} __attribute__ ((packed)) 

read_hwcb_t;

static read_hwcb_t read_hwcb_template =
{
	PAGE_SIZE,
	0x00,
	{
		0x00,
		0x00,
		0x80
	}
};

#endif				/* __HWC_H__ */
