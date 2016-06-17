/* $Id: kernelcapi.h,v 1.1.4.2 2002/01/28 18:25:10 kai Exp $
 * 
 * Kernel CAPI 2.0 Interface for Linux
 * 
 * (c) Copyright 1997 by Carsten Paeth (calle@calle.in-berlin.de)
 * 
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#ifndef __KERNELCAPI_H__
#define __KERNELCAPI_H__

#define CAPI_MAXAPPL	128	/* maximum number of applications  */
#define CAPI_MAXCONTR	16	/* maximum number of controller    */
#define CAPI_MAXDATAWINDOW	8


typedef struct kcapi_flagdef {
	int contr;
	int flag;
} kcapi_flagdef;

typedef struct kcapi_carddef {
	char		driver[32];
	unsigned int	port;
	unsigned	irq;
	unsigned int	membase;
	int		cardnr;
} kcapi_carddef;

/* new ioctls >= 10 */
#define KCAPI_CMD_TRACE		10
#define KCAPI_CMD_ADDCARD	11	/* add card to named driver */

/* 
 * flag > 2 => trace also data
 * flag & 1 => show trace
 */
#define KCAPI_TRACE_OFF			0
#define KCAPI_TRACE_SHORT_NO_DATA	1
#define KCAPI_TRACE_FULL_NO_DATA	2
#define KCAPI_TRACE_SHORT		3
#define KCAPI_TRACE_FULL		4


#ifdef __KERNEL__

struct capi_interface {
	__u16 (*capi_isinstalled) (void);

	__u16 (*capi_register) (capi_register_params * rparam, __u16 * applidp);
	__u16 (*capi_release) (__u16 applid);
	__u16 (*capi_put_message) (__u16 applid, struct sk_buff * msg);
	__u16 (*capi_get_message) (__u16 applid, struct sk_buff ** msgp);
	__u16 (*capi_set_signal) (__u16 applid,
			      void (*signal) (__u16 applid, void *param),
				  void *param);
	__u16 (*capi_get_manufacturer) (__u32 contr, __u8 buf[CAPI_MANUFACTURER_LEN]);
	__u16 (*capi_get_version) (__u32 contr, struct capi_version * verp);
	 __u16(*capi_get_serial) (__u32 contr, __u8 serial[CAPI_SERIAL_LEN]);
	 __u16(*capi_get_profile) (__u32 contr, struct capi_profile * profp);

	/*
	 * to init controllers, data is always in user memory
	 */
	int (*capi_manufacturer) (unsigned int cmd, void *data);

};

struct capi_ncciinfo {
	__u16 applid;
	__u32 ncci;
};

#define	KCI_CONTRUP	0	/* struct capi_profile */
#define	KCI_CONTRDOWN	1	/* NULL */
#define	KCI_NCCIUP	2	/* struct capi_ncciinfo */
#define	KCI_NCCIDOWN	3	/* struct capi_ncciinfo */

struct capi_interface_user {
	char name[20];
	void (*callback) (unsigned int cmd, __u32 contr, void *data);
	/* internal */
	struct capi_interface_user *next;
};

struct capi_interface *attach_capi_interface(struct capi_interface_user *);
int detach_capi_interface(struct capi_interface_user *);


#define CAPI_NOERROR                      0x0000

#define CAPI_TOOMANYAPPLS		  0x1001
#define CAPI_LOGBLKSIZETOSMALL	          0x1002
#define CAPI_BUFFEXECEEDS64K 	          0x1003
#define CAPI_MSGBUFSIZETOOSMALL	          0x1004
#define CAPI_ANZLOGCONNNOTSUPPORTED	  0x1005
#define CAPI_REGRESERVED		  0x1006
#define CAPI_REGBUSY 		          0x1007
#define CAPI_REGOSRESOURCEERR	          0x1008
#define CAPI_REGNOTINSTALLED 	          0x1009
#define CAPI_REGCTRLERNOTSUPPORTEXTEQUIP  0x100a
#define CAPI_REGCTRLERONLYSUPPORTEXTEQUIP 0x100b

#define CAPI_ILLAPPNR		          0x1101
#define CAPI_ILLCMDORSUBCMDORMSGTOSMALL   0x1102
#define CAPI_SENDQUEUEFULL		  0x1103
#define CAPI_RECEIVEQUEUEEMPTY	          0x1104
#define CAPI_RECEIVEOVERFLOW 	          0x1105
#define CAPI_UNKNOWNNOTPAR		  0x1106
#define CAPI_MSGBUSY 		          0x1107
#define CAPI_MSGOSRESOURCEERR	          0x1108
#define CAPI_MSGNOTINSTALLED 	          0x1109
#define CAPI_MSGCTRLERNOTSUPPORTEXTEQUIP  0x110a
#define CAPI_MSGCTRLERONLYSUPPORTEXTEQUIP 0x110b

typedef enum {
        CapiMessageNotSupportedInCurrentState = 0x2001,
        CapiIllContrPlciNcci                  = 0x2002,
        CapiNoPlciAvailable                   = 0x2003,
        CapiNoNcciAvailable                   = 0x2004,
        CapiNoListenResourcesAvailable        = 0x2005,
        CapiNoFaxResourcesAvailable           = 0x2006,
        CapiIllMessageParmCoding              = 0x2007,
} RESOURCE_CODING_PROBLEM;

typedef enum {
        CapiB1ProtocolNotSupported                      = 0x3001,
        CapiB2ProtocolNotSupported                      = 0x3002,
        CapiB3ProtocolNotSupported                      = 0x3003,
        CapiB1ProtocolParameterNotSupported             = 0x3004,
        CapiB2ProtocolParameterNotSupported             = 0x3005,
        CapiB3ProtocolParameterNotSupported             = 0x3006,
        CapiBProtocolCombinationNotSupported            = 0x3007,
        CapiNcpiNotSupported                            = 0x3008,
        CapiCipValueUnknown                             = 0x3009,
        CapiFlagsNotSupported                           = 0x300a,
        CapiFacilityNotSupported                        = 0x300b,
        CapiDataLengthNotSupportedByCurrentProtocol     = 0x300c,
        CapiResetProcedureNotSupportedByCurrentProtocol = 0x300d,
        CapiTeiAssignmentFailed                         = 0x300e,
} REQUESTED_SERVICES_PROBLEM;

typedef enum {
	CapiSuccess                                     = 0x0000,
	CapiSupplementaryServiceNotSupported            = 0x300e,
	CapiRequestNotAllowedInThisState                = 0x3010,
} SUPPLEMENTARY_SERVICE_INFO;

typedef enum {
	CapiProtocolErrorLayer1                         = 0x3301,
	CapiProtocolErrorLayer2                         = 0x3302,
	CapiProtocolErrorLayer3                         = 0x3303,
	CapiTimeOut                                     = 0x3303, // SuppServiceReason
	CapiCallGivenToOtherApplication                 = 0x3304,
} CAPI_REASON;

#endif				/* __KERNEL__ */

#endif				/* __KERNELCAPI_H__ */
