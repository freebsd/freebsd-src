#ifndef __LINUX_PKT_CLS_H
#define __LINUX_PKT_CLS_H

struct tc_police
{
	__u32			index;
	int			action;
#define TC_POLICE_UNSPEC	(-1)
#define TC_POLICE_OK		0
#define TC_POLICE_RECLASSIFY	1
#define TC_POLICE_SHOT		2

	__u32			limit;
	__u32			burst;
	__u32			mtu;
	struct tc_ratespec	rate;
	struct tc_ratespec	peakrate;
};

enum
{
	TCA_POLICE_UNSPEC,
	TCA_POLICE_TBF,
	TCA_POLICE_RATE,
	TCA_POLICE_PEAKRATE,
	TCA_POLICE_AVRATE,
	TCA_POLICE_RESULT
#define TCA_POLICE_RESULT TCA_POLICE_RESULT
};

#define TCA_POLICE_MAX TCA_POLICE_RESULT

/* U32 filters */

#define TC_U32_HTID(h) ((h)&0xFFF00000)
#define TC_U32_USERHTID(h) (TC_U32_HTID(h)>>20)
#define TC_U32_HASH(h) (((h)>>12)&0xFF)
#define TC_U32_NODE(h) ((h)&0xFFF)
#define TC_U32_KEY(h) ((h)&0xFFFFF)
#define TC_U32_UNSPEC	0
#define TC_U32_ROOT	(0xFFF00000)

enum
{
	TCA_U32_UNSPEC,
	TCA_U32_CLASSID,
	TCA_U32_HASH,
	TCA_U32_LINK,
	TCA_U32_DIVISOR,
	TCA_U32_SEL,
	TCA_U32_POLICE,
};

#define TCA_U32_MAX TCA_U32_POLICE

struct tc_u32_key
{
	__u32		mask;
	__u32		val;
	int		off;
	int		offmask;
};

struct tc_u32_sel
{
	unsigned char		flags;
	unsigned char		offshift;
	unsigned char		nkeys;

	__u16			offmask;
	__u16			off;
	short			offoff;

	short			hoff;
	__u32			hmask;

	struct tc_u32_key	keys[0];
};

/* Flags */

#define TC_U32_TERMINAL		1
#define TC_U32_OFFSET		2
#define TC_U32_VAROFFSET	4
#define TC_U32_EAT		8

#define TC_U32_MAXDEPTH 8


/* RSVP filter */

enum
{
	TCA_RSVP_UNSPEC,
	TCA_RSVP_CLASSID,
	TCA_RSVP_DST,
	TCA_RSVP_SRC,
	TCA_RSVP_PINFO,
	TCA_RSVP_POLICE,
};

#define TCA_RSVP_MAX TCA_RSVP_POLICE

struct tc_rsvp_gpi
{
	__u32	key;
	__u32	mask;
	int	offset;
};

struct tc_rsvp_pinfo
{
	struct tc_rsvp_gpi dpi;
	struct tc_rsvp_gpi spi;
	__u8	protocol;
	__u8	tunnelid;
	__u8	tunnelhdr;
};

/* ROUTE filter */

enum
{
	TCA_ROUTE4_UNSPEC,
	TCA_ROUTE4_CLASSID,
	TCA_ROUTE4_TO,
	TCA_ROUTE4_FROM,
	TCA_ROUTE4_IIF,
	TCA_ROUTE4_POLICE,
};

#define TCA_ROUTE4_MAX TCA_ROUTE4_POLICE


/* FW filter */

enum
{
	TCA_FW_UNSPEC,
	TCA_FW_CLASSID,
	TCA_FW_POLICE,
};

#define TCA_FW_MAX TCA_FW_POLICE

/* TC index filter */

enum
{
	TCA_TCINDEX_UNSPEC,
	TCA_TCINDEX_HASH,
	TCA_TCINDEX_MASK,
	TCA_TCINDEX_SHIFT,
	TCA_TCINDEX_FALL_THROUGH,
	TCA_TCINDEX_CLASSID,
	TCA_TCINDEX_POLICE,
};

#define TCA_TCINDEX_MAX        TCA_TCINDEX_POLICE

#endif
