/*-
 ****************************************************************************
 *
 * Copyright (c) 1996-2000 Distributed Processing Technology Corporation
 * Copyright (c) 2000 Adaptec Corporation.
 * All rights reserved.
 *
 * Copyright (c) 1998 I2O Special Interest Group (I2O SIG)
 * All rights reserved
 *
 * Redistribution and use in source form, with or without modification, are
 * permitted provided that redistributions of source code must retain the
 * above copyright notice, this list of conditions and the following disclaimer.
 *
 * This software is provided `as is' by Distributed Processing Technology and
 * any express or implied warranties, including, but not limited to, the
 * implied warranties of merchantability and fitness for a particular purpose,
 * are disclaimed. In no event shall Distributed Processing Technology be
 * liable for any direct, indirect, incidental, special, exemplary or
 * consequential damages (including, but not limited to, procurement of
 * substitute goods or services; loss of use, data, or profits; or business
 * interruptions) however caused and on any theory of liability, whether in
 * contract, strict liability, or tort (including negligence or otherwise)
 * arising in any way out of the use of this driver software, even if advised
 * of the possibility of such damage.
 *
 * This information is provided on an as-is basis without warranty of any
 * kind, either express or implied, including but not limited to, implied
 * warranties or merchantability and fitness for a particular purpose. I2O SIG
 * does not warrant that this program will meet the user's requirements or
 * that the operation of these programs will be uninterrupted or error-free.
 * The I2O SIG disclaims all liability, including liability for infringement
 * of any proprietary rights, relating to implementation of information in
 * this specification. The I2O SIG does not warrant or represent that such
 * implementations(s) will not infringe such rights. Acceptance and use of
 * this program constitutes the user's understanding that he will have no
 * recourse to I2O SIG for any actual or consequential damages including, but
 * not limited to, loss profits arising out of use or inability to use this
 * program.
 *
 * This information is provided for the purpose of recompilation of the
 * driver code provided by Distributed Processing Technology only. It is
 * NOT to be used for any other purpose.
 *
 * To develop other products based upon I2O definitions, it is necessary to
 * become a "Registered Developer" of the I2O SIG. This can be done by calling
 * 415-750-8352 in the US, or via http://www.i2osig.org.
 *
 * $FreeBSD: src/sys/dev/asr/i2odep.h,v 1.9 2006/02/04 08:01:49 scottl Exp $
 *
 **************************************************************************/

/*
 * This template provides place holders for architecture and compiler
 * dependencies. It should be filled in and renamed as i2odep.h.
 * i2odep.h is included by i2otypes.h. <xxx> marks the places to fill.
 */

#ifndef __INCi2odeph
#define	__INCi2odeph

#define	I2ODEP_REV 1_5_4

/*
 * Pragma macros. These are to assure appropriate alignment between
 * host/IOP as defined by the I2O Specification. Each one of the shared
 * header files includes these macros.
 */

#define	PRAGMA_ALIGN_PUSH
#define	PRAGMA_ALIGN_POP
#define	PRAGMA_PACK_PUSH
#define	PRAGMA_PACK_POP

/* Setup the basics */

typedef	   signed char	  S8;
typedef	   signed short	  S16;

typedef	   unsigned char  U8;
typedef	   unsigned short U16;

typedef	   u_int32_t U32;
typedef	   int32_t  S32;


/* Bitfields */

#if (defined(__BORLANDC__))
typedef	   U16 BF;
#else
typedef	   U32 BF;
#endif


/* VOID */

#ifndef __VOID
#if (defined(_DPT_ARC))
# define VOID void
#else
 typedef    void  VOID;
#endif
#define	__VOID
#endif


/* Boolean */

#ifndef __BOOL
#define	__BOOL

typedef unsigned char BOOL;
#endif

#if !defined(__FAR__)
# if defined(__BORLANDC__)
#  define __FAR__ far
# else
#  define __FAR__
# endif
#endif

/* NULL */

#if !defined(NULL)
# define NULL  ((VOID __FAR__ *)0L)
#endif


#if defined(__SPARC__) || defined(__linux__)
typedef char		       CHAR;
typedef char		       *pCHAR;
typedef char		       INT8;
typedef char		       *pINT8;
typedef unsigned char	       UINT8;
typedef unsigned char	       *pUINT8;
typedef short		       INT16;
typedef short		       *pINT16;
typedef unsigned short	       UINT16;
typedef unsigned short	       *pUINT16;
typedef long		       INT32;
typedef long		       *pINT32;
typedef unsigned long	       UINT32;
typedef unsigned long	       *pUINT32;
/* typedef SCSI_REQUEST_BLOCK	  OS_REQUEST_T; */
/* typedef PSCSI_REQUEST_BLOCK	  pOS_REQUEST_T; */
#define	STATIC		       static
#ifndef __NEAR__
# if (defined(__BORLANDC__))
#  define __NEAR__ near
# else
#  define __NEAR__
# endif
#endif
#define	pVOID		       void *
#define	pBOOLEAN	       BOOLEAN *
#endif


/*
 * Copyright (c) 1996-2000 Distributed Processing Technology Corporation
 * Copyright (c) 2000 Adaptec Corporation.
 * All rights reserved.
 */
/*
 *	Define some generalized portability macros
 *	These macros follow the following parameterization:
 *	    _F_getXXX(pointer,primaryElement<,offset>,referredElement)
 *	    _F_setXXX(pointer,primaryElement<,offset>,referredElement,newValue)
 *	These parameters are shortened to u, w, x, y and z to reduce clutter.
 */
#if (defined(__BORLANDC__))
# define I2O_TID_MASK	      ((U16)((1L<<I2O_TID_SZ)-1))
/* First 12 bits */
# define _F_getTID(w,x,y)     (*((U16 __FAR__ *)(&((w)->x))) & I2O_TID_MASK)
# define _F_setTID(w,x,y,z)   (*((U16 __FAR__ *)(&((w)->x)))\
			       &= 0xFFFF - I2O_TID_MASK);\
			      (*((U16 __FAR__ *)(&((w)->x)))\
			       |=(U16)(z)&I2O_TID_MASK)
/* Seconds 12 bits (optimized with the assumption of 12 & 12) */
# define _F_getTID1(w,x,y)    ((*(U16 __FAR__ *)(((U8 __FAR__ *)(&((w)->x)))\
			       + (I2O_TID_SZ/8)))\
				>> (I2O_TID_SZ-((I2O_TID_SZ/8)*8)))
# define _F_setTID1(w,x,y,z)  ((*((U16 __FAR__ *)(((U8 __FAR__ *)(&((w)->x)))\
			       + (I2O_TID_SZ/8)))) &= (0xFFFF >> I2O_TID_SZ));\
			      ((*((U16 __FAR__ *)(((U8 __FAR__ *)(&((w)->x)))\
			       + (I2O_TID_SZ/8)))) |= (z)\
				<< (I2O_TID_SZ-((I2O_TID_SZ/8)*8)))
/* Last 8 bits */
# define _F_getFunc(w,x,y)    (*(((U8 __FAR__ *)(&((w)->x)))\
			       + ((I2O_TID_SZ+I2O_TID_SZ)/8)))
# define _F_setFunc(w,x,y,z)  (_F_getFunc(w,x,y) = (z))
# define I2O_SG_COUNT_MASK    ((U32)((1L<<I2O_SG_COUNT_SZ)-1))
/* First 24 bits */
# define _F_getCount(w,x,y)   (*((U32 __FAR__ *)(&((w)->x)))&I2O_SG_COUNT_MASK)
/*
 * The following is less efficient because of compiler inefficiencies:
 *
 * # define _F_setCount(w,x,y,z)  *((U16 __FAR__ *)(&((w)->x))) = (U16)(z);\
 *				((U8 __FAR__ *)(&((w)->x)))[2]= (U8)((z)>>16L)
 *
 * so we will use the apparently more code intensive:
 */
# define _F_setCount(w,x,y,z) (*((U32 __FAR__ *)(&((w)->x)))\
			       &= 0xFFFFFFFFL - I2O_SG_COUNT_MASK);\
			      (*((U32 __FAR__ *)(&((w)->x)))\
			       |= (z) & I2O_SG_COUNT_MASK)
/* Last 8 bits */
# define _F_getFlags(w,x,y)   (*(((U8 __FAR__ *)(&((w)->x)))\
			       + (I2O_SG_COUNT_SZ/8)))
# define _F_setFlags(w,x,y,z) (_F_getFlags(w,x,y) = (z))
/* Other accesses that are simpler */
# define _F_get1bit(w,x,y,z)	 ((U8)((w)->z))
# define _F_set1bit(w,x,y,z,u)	 ((w)->z = (u))
# define _F_get1bit1(w,x,y,z)	 ((U8)((w)->z))
# define _F_set1bit1(w,x,y,z,u)	 ((w)->z = (u))
# define _F_get4bit4(w,x,y,z)	 ((U8)((w)->z))
# define _F_set4bit4(w,x,y,z,u)	 ((w)->z = (u))
# define _F_get8bit(w,x,y,z)	 ((U8)((w)->z))
# define _F_set8bit(w,x,y,z,u)	 ((w)->z = (u))
# define _F_get12bit(w,x,y,z)	 ((U16)((w)->z))
# define _F_set12bit(w,x,y,z,u)	 ((w)->z = (u))
# define _F_get12bit4(w,x,y,z)	 ((U16)((w)->z))
# define _F_set12bit4(w,x,y,z,u) ((w)->z = (u))
# define _F_get16bit(w,x,y,z)	 ((U16)((w)->z))
# define _F_set16bit(w,x,y,z,u)	 ((w)->z = (u))
#elif (defined(_DPT_BIG_ENDIAN))
/* First 12 bits */
# define _F_getTID(w,x,y)     getL12bit(w,x,0)
# define _F_setTID(w,x,y,z)   setL12bit(w,x,0,z)
# define _F_getTID1(w,x,y)    getL12bit1(w,x,0)
# define _F_setTID1(w,x,y,z)  setL12bit1(w,x,0,z)
# define _F_getFunc(w,x,y)    getL8bit(w,x,3)
# define _F_setFunc(w,x,y,z)  setL8bit(w,x,3,z)
# define _F_getCount(w,x,y)   getL24bit1(w,x,0)
# define _F_setCount(w,x,y,z) setL24bit1(w,x,0,z)
# define _F_getFlags(w,x,y)   getL8bit(w,x,3)
# define _F_setFlags(w,x,y,z) setL8bit(w,x,3,z)
/* Other accesses that are simpler */
# define _F_get1bit(w,x,y,z)	 getL1bit(w,x,y)
# define _F_set1bit(w,x,y,z,u)	 setL1bit(w,x,y,u)
# define _F_get1bit1(w,x,y,z)	 getL1bit1(w,x,y)
# define _F_set1bit1(w,x,y,z,u)	 setL1bit1(w,x,y,u)
# define _F_get4bit4(w,x,y,z)	 getL4bit(w,x,y)
# define _F_set4bit4(w,x,y,z,u)	 setL4bit(w,x,y,u)
# define _F_get8bit(w,x,y,z)	 getL8bit(w,x,y)
# define _F_set8bit(w,x,y,z,u)	 setL8bit(w,x,y,u)
# define _F_get12bit(w,x,y,z)	 getL12bit(w,x,y)
# define _F_set12bit(w,x,y,z,u)	 setL12bit(w,x,y,z)
# define _F_get12bit4(w,x,y,z)	 getL12bit1(w,x,(y)-1)
# define _F_set12bit4(w,x,y,z,u) setL12bit1(w,x,(y)-1,u)
# define _F_get16bit(w,x,y,z)	 getL16bit(w,x,y)
# define _F_set16bit(w,x,y,z,u)	 setL16bit(w,x,y,u)
#else
# define _F_getTID(w,x,y)     ((U16)((w)->y))
# define _F_setTID(w,x,y,z)   ((w)->y = (z))
# define _F_getTID1(w,x,y)    ((U16)((w)->y))
# define _F_setTID1(w,x,y,z)  ((w)->y = (z))
# define _F_getFunc(w,x,y)    ((U8)((w)->y))
# define _F_setFunc(w,x,y,z)  ((w)->y = (z))
# define _F_getCount(w,x,y)   ((U32)((w)->y))
# define _F_setCount(w,x,y,z) ((w)->y = (z))
# define _F_getFlags(w,x,y)   ((U8)((w)->y))
# define _F_setFlags(w,x,y,z) ((w)->y = (z))
# define _F_get1bit(w,x,y,z)	 ((U8)((w)->z))
# define _F_set1bit(w,x,y,z,u)	 ((w)->z = (u))
# define _F_get1bit1(w,x,y,z)	 ((U8)((w)->z))
# define _F_set1bit1(w,x,y,z,u)	 ((w)->z = (u))
# define _F_get4bit4(w,x,y,z)	 ((U8)((w)->z))
# define _F_set4bit4(w,x,y,z,u)	 ((w)->z = (u))
# define _F_get8bit(w,x,y,z)	 ((U8)((w)->z))
# define _F_set8bit(w,x,y,z,u)	 ((w)->z = (u))
# define _F_get12bit(w,x,y,z)	 ((U16)((w)->z))
# define _F_set12bit(w,x,y,z,u)	 ((w)->z = (u))
# define _F_get12bit4(w,x,y,z)	 ((U16)((w)->z))
# define _F_set12bit4(w,x,y,z,u) ((w)->z = (u))
# define _F_get16bit(w,x,y,z)	 ((U16)((w)->z))
# define _F_set16bit(w,x,y,z,u)	 ((w)->z = (u))
#endif

/*
 *	Define some specific portability macros
 *	These macros follow the following parameterization:
 *		XXX_getYYY (pointer)
 *		XXX_setYYY (pointer, newValue)
 *	These parameters are shortened to x and y to reduce clutter.
 */

/*
 * General SGE
 */
#define	I2O_FLAGS_COUNT_getCount(x)   _F_getCount(x,Count,Count)
#define	I2O_FLAGS_COUNT_setCount(x,y) _F_setCount(x,Count,Count,y)
#define	I2O_FLAGS_COUNT_getFlags(x)   _F_getFlags(x,Count,Flags)
#define	I2O_FLAGS_COUNT_setFlags(x,y) _F_setFlags(x,Count,Flags,y)

/*
 * I2O_SGE_SIMPLE_ELEMENT
 */
#define	I2O_SGE_SIMPLE_ELEMENT_getPhysicalAddress(x) \
	getLU4((&(x)->PhysicalAddress),0)
#define	I2O_SGE_SIMPLE_ELEMENT_setPhysicalAddress(x,y) \
	setLU4((&(x)->PhysicalAddress),0,y)
/*
 * I2O_SGE_LONG_TRANSACTION_ELEMENT
 */
#define	I2O_SGE_LONG_TRANSACTION_ELEMENT_getLongElementLength(x)\
	_F_getCount(x,LongElementLength,LongElementLength)
#define	I2O_SGE_LONG_TRANSACTION_ELEMENT_setLongElementLength(x,y)\
	_F_setCount(x,LongElementLength,LongElementLength,y)
#define	I2O_SGE_LONG_TRANSACTION_ELEMENT_getFlags(x)\
	_F_getFlags(x,LongElementLength,Flags)
#define	I2O_SGE_LONG_TRANSACTION_ELEMENT_setFlags(x,y)\
	_F_setFlags(x,LongElementLength,Flags,y)

/*
 * I2O_SGE_LONG_TRANSPORT_ELEMENT
 */
#define	I2O_SGE_LONG_TRANSPORT_ELEMENT_getLongElementLength(x)\
	_F_getCount(x,LongElementLength,LongElementLength)
#define	I2O_SGE_LONG_TRANSPORT_ELEMENT_setLongElementLength(x,y)\
	_F_setCount(x,LongElementLength,LongElementLength,y)
#define	I2O_SGE_LONG_TRANSPORT_ELEMENT_getFlags(x)\
	_F_getFlags(x,LongElementLength,Flags)
#define	I2O_SGE_LONG_TRANSPORT_ELEMENT_setFlags(x,y)\
	_F_setFlags(x,LongElementLength,Flags,y)

/*
 * I2O_EXEC_ADAPTER_ASSIGN_MESSAGE
 */
#define	I2O_EXEC_ADAPTER_ASSIGN_MESSAGE_getDdmTID(x)\
	_F_getTID(x,DdmTID,DdmTID)
#define	I2O_EXEC_ADAPTER_ASSIGN_MESSAGE_setDdmTID(x,y)\
	_F_setTID(x,DDdmTID,DdmTID,y)
#define	I2O_EXEC_ADAPTER_ASSIGN_MESSAGE_getOperationFlags(x)\
	_F_getFunc(x,DdmTID,OperationFlags)
#define	I2O_EXEC_ADAPTER_ASSIGN_MESSAGE_setOperationFlags(x,y)\
	_F_setFunc(x,DdmTID,OperationFlags,y)

/*
 * I2O_EXEC_BIOS_INFO_SET_MESSAGE
 */
#define	I2O_EXEC_BIOS_INFO_SET_MESSAGE_getDeviceTID(x)\
	_F_getTID(x,DeviceTID,DeviceTID)
#define	I2O_EXEC_BIOS_INFO_SET_MESSAGE_setDeviceTID(x,y)\
	_F_setTID(x,DeviceTID,DeviceTID,y)
#define	I2O_EXEC_BIOS_INFO_SET_MESSAGE_getBiosInfo(x)\
	_F_getFunc(x,DeviceTID,BiosInfo)
#define	I2O_EXEC_BIOS_INFO_SET_MESSAGE_setBiosInfo(x,y)	 \
	_F_setFunc(x,DeviceTID,BiosInfo,y)

/*
 * I2O_ALIAS_CONNECT_SETUP
 */
#define	I2O_ALIAS_CONNECT_SETUP_getIOP1AliasForTargetDevice(x)\
	_F_getTID(x,IOP1AliasForTargetDevice,IOP1AliasForTargetDevice)
#define	I2O_ALIAS_CONNECT_SETUP_setIOP1AliasForTargetDevice(x,y)\
	_F_setTID(x,IOP1AliasForTargetDevice,IOP1AliasForTargetDevice,y)
#define	I2O_ALIAS_CONNECT_SETUP_getIOP2AliasForInitiatorDevice(x)\
	_F_getTID1(x,IOP1AliasForTargetDevice,IOP2AliasForInitiatorDevice)
#define	I2O_ALIAS_CONNECT_SETUP_setIOP2AliasForInitiatorDevice(x,y)\
	_F_setTID1(x,IOP1AliasForTargetDevice,IOP2AliasForInitiatorDevice,y)

/*
 * I2O_OBJECT_CONNECT_SETUP
 */
#define	I2O_OBJECT_CONNECT_SETUP_getTargetDevice(x)\
	_F_getTID(x,TargetDevice,TargetDevice)
#define	I2O_OBJECT_CONNECT_SETUP_setTargetDevice(x,y)\
	_F_setTID(x,TargetDevice,TargetDevice,y)
#define	I2O_OBJECT_CONNECT_SETUP_getInitiatorDevice(x)\
	_F_getTID1(x,TargetDevice,InitiatorDevice)
#define	I2O_OBJECT_CONNECT_SETUP_setInitiatorDevice(x,y)\
	_F_setTID1(x,TargetDevice,InitiatorDevice,y)
#define	I2O_OBJECT_CONNECT_SETUP_getOperationFlags(x)\
	_F_getFunc(x,TargetDevice,OperationFlags)
#define	I2O_OBJECT_CONNECT_SETUP_setOperationFlags(x,y)\
	_F_setFunc(x,TargetDevice,OperationFlags,y)

/*
 * I2O_OBJECT_CONNECT_REPLY
 */
#define	I2O_OBJECT_CONNECT_REPLY_getTargetDevice(x)\
	_F_getTID(x,TargetDevice,TargetDevice)
#define	I2O_OBJECT_CONNECT_REPLY_setTargetDevice(x,y)\
	_F_setTID(x,TargetDevice,TargetDevice,y)
#define	I2O_OBJECT_CONNECT_REPLY_getInitiatorDevice(x)\
	_F_getTID1(x,TargetDevice,InitiatorDevice)
#define	I2O_OBJECT_CONNECT_REPLY_setInitiatorDevice(x,y)\
	_F_setTID1(x,TargetDevice,InitiatorDevice,y)
#define	I2O_OBJECT_CONNECT_REPLY_getReplyStatusCode(x)\
	_F_getFunc(x,TargetDevice,ReplyStatusCode)
#define	I2O_OBJECT_CONNECT_REPLY_setReplyStatusCode(x,y)\
	_F_setFunc(x,TargetDevice,ReplyStatusCode,y)

/*
 * I2O_EXEC_DEVICE_ASSIGN_MESSAGE
 */
#define	I2O_EXEC_DEVICE_ASSIGN_MESSAGE_getDeviceTID(x)\
	_F_getTID(x,Object.DeviceTID,Object.DeviceTID)
#define	I2O_EXEC_DEVICE_ASSIGN_MESSAGE_setDeviceTID(x,y)\
	_F_setTID(x,Object.DeviceTID,Object.DeviceTID,y)
#define	I2O_EXEC_DEVICE_ASSIGN_MESSAGE_getIOP_ID(x)\
	_F_getTID1(x,Object.DeviceTID,Object.IOP_ID)
#define	I2O_EXEC_DEVICE_ASSIGN_MESSAGE_setIOP_ID(x,y)\
	_F_setTID1(x,Object.DeviceTID,Object.IOP_ID,y)
#define	I2O_EXEC_DEVICE_ASSIGN_MESSAGE_getOperationFlags(x)\
	_F_getFunc(x,Object.DeviceTID,Object.OperationFlags)
#define	I2O_EXEC_DEVICE_ASSIGN_MESSAGE_setOperationFlags(x,y)\
	_F_setFunc(x,Object.DeviceTID,Object.OperationFlags,y)

/*
 * I2O_EXEC_DEVICE_RELEASE_MESSAGE
 */
#define	I2O_EXEC_DEVICE_RELEASE_MESSAGE_getDeviceTID(x)\
	_F_getTID(x,Object.DeviceTID,Object.DeviceTID)
#define	I2O_EXEC_DEVICE_RELEASE_MESSAGE_setDeviceTID(x,y)\
	_F_setTID(x,Object.DeviceTID,Object.DeviceTID,y)
#define	I2O_EXEC_DEVICE_RELEASE_MESSAGE_getIOP_ID(x)\
	_F_getTID1(x,Object.DeviceTID,Object.IOP_ID)
#define	I2O_EXEC_DEVICE_RELEASE_MESSAGE_setIOP_ID(x,y)\
	_F_setTID1(x,Object.DeviceTID,Object.IOP_ID,y)
#define	I2O_EXEC_DEVICE_RELEASE_MESSAGE_getOperationFlags(x)\
	_F_getFunc(x,Object.DeviceTID,Object.OperationFlags)
#define	I2O_EXEC_DEVICE_RELEASE_MESSAGE_setOperationFlags(x,y)\
	_F_setFunc(x,Object.DeviceTID,Object.OperationFlags,y)

/*
 * I2O_EXEC_IOP_RESET_MESSAGE
 */
#define	I2O_EXEC_IOP_RESET_MESSAGE_getTargetAddress(x)\
	_F_getTID(x,TargetAddress,TargetAddress)
#define	I2O_EXEC_IOP_RESET_MESSAGE_setTargetAddress(x,y)\
	_F_setTID(x,TargetAddress,TargetAddress,y)
#define	I2O_EXEC_IOP_RESET_MESSAGE_getInitiatorAddress(x)\
	_F_getTID1(x,TargetAddress,InitiatorAddress)
#define	I2O_EXEC_IOP_RESET_MESSAGE_setInitiatorAddress(x,y)\
	_F_setTID1(x,TargetAddress,InitiatorAddress,y)
#define	I2O_EXEC_IOP_RESET_MESSAGE_getFunction(x)\
	_F_getFunc(x,TargetAddress,Function)
#define	I2O_EXEC_IOP_RESET_MESSAGE_setFunction(x,y)\
	_F_setFunc(x,TargetAddress,Function,y)
#define	I2O_EXEC_IOP_RESET_MESSAGE_getVersionOffset(x)\
		getU1((&(x)->VersionOffset),0)
#define	I2O_EXEC_IOP_RESET_MESSAGE_setVersionOffset(x,y)\
		setU1((&(x)->VersionOffset),0,y)
#define	I2O_EXEC_IOP_RESET_MESSAGE_getMsgFlags(x)\
		getU1((&(x)->VersionOffset),1)
#define	I2O_EXEC_IOP_RESET_MESSAGE_setMsgFlags(x,y)\
		setU1((&(x)->VersionOffset),1,y)
#define	I2O_EXEC_IOP_RESET_MESSAGE_getMessageSize(x)\
		getLU2((&(x)->VersionOffset),2)
#define	I2O_EXEC_IOP_RESET_MESSAGE_setMessageSize(x,y)\
		setLU2((&(x)->VersionOffset),2,y)
#define	I2O_EXEC_IOP_RESET_MESSAGE_getStatusWordLowAddress(x)\
		getLU4((&(x)->StatusWordLowAddress),0)
#define	I2O_EXEC_IOP_RESET_MESSAGE_setStatusWordLowAddress(x,y)\
		setLU4((&(x)->StatusWordLowAddress),0,y)
#define	I2O_EXEC_IOP_RESET_MESSAGE_getStatusWordHighAddress(x)\
		getLU4((&(x)->StatusWordHighAddress),0)
#define	I2O_EXEC_IOP_RESET_MESSAGE_setStatusWordHighAddress(x,y)\
		setLU4((&(x)->StatusWordHighAddress),0,y)


/*
 * I2O_EXEC_STATUS_GET_MESSAGE
 */
#define	I2O_EXEC_STATUS_GET_MESSAGE_getVersionOffset(x)\
		getU1((&(x)->VersionOffset),0)
#define	I2O_EXEC_STATUS_GET_MESSAGE_setVersionOffset(x,y)\
		setU1((&(x)->VersionOffset),0,y)
#define	I2O_EXEC_STATUS_GET_MESSAGE_getMsgFlags(x)\
		getU1((&(x)->VersionOffset),1)
#define	I2O_EXEC_STATUS_GET_MESSAGE_setMsgFlags(x,y)\
		setU1((&(x)->VersionOffset),1,y)
#define	I2O_EXEC_STATUS_GET_MESSAGE_getMessageSize(x)\
		getLU2((&(x)->VersionOffset),2)
#define	I2O_EXEC_STATUS_GET_MESSAGE_setMessageSize(x,y)\
		setLU2((&(x)->VersionOffset),2,y)
#define	I2O_EXEC_STATUS_GET_MESSAGE_getReplyBufferAddressLow(x)\
		getLU4((&(x)->ReplyBufferAddressLow),0)
#define	I2O_EXEC_STATUS_GET_MESSAGE_setReplyBufferAddressLow(x,y)\
		setLU4((&(x)->ReplyBufferAddressLow),0,y)
#define	I2O_EXEC_STATUS_GET_MESSAGE_getReplyBufferAddressHigh(x)\
		getLU4((&(x)->ReplyBufferAddressHigh),0)
#define	I2O_EXEC_STATUS_GET_MESSAGE_setReplyBufferAddressHigh(x,y)\
		setLU4((&(x)->ReplyBufferAddressHigh),0,y)
#define	I2O_EXEC_STATUS_GET_MESSAGE_getReplyBufferLength(x)\
		getLU4((&(x)->ReplyBufferLength),0)
#define	I2O_EXEC_STATUS_GET_MESSAGE_setReplyBufferLength(x,y)\
		setLU4((&(x)->ReplyBufferLength),0,y)
#define	I2O_EXEC_STATUS_GET_MESSAGE_getTargetAddress(x)\
		_F_getTID(x,TargetAddress,TargetAddress)
#define	I2O_EXEC_STATUS_GET_MESSAGE_setTargetAddress(x,y)\
		_F_setTID(x,TargetAddress,TargetAddress,y)
#define	I2O_EXEC_STATUS_GET_MESSAGE_getInitiatorAddress(x)\
		_F_getTID1(x,TargetAddress,InitiatorAddress)
#define	I2O_EXEC_STATUS_GET_MESSAGE_setInitiatorAddress(x,y)\
		_F_setTID1(x,TargetAddress,InitiatorAddress,y)
#define	I2O_EXEC_STATUS_GET_MESSAGE_getFunction(x)\
		_F_getFunc(x,TargetAddress,Function)
#define	I2O_EXEC_STATUS_GET_MESSAGE_setFunction(x,y)\
		_F_setFunc(x,TargetAddress,Function,y)

/*
 * I2O_MESSAGE_FRAME
 */
#define	I2O_MESSAGE_FRAME_getVersionOffset(x)\
		getU1((&((x)->VersionOffset)),0)
#define	I2O_MESSAGE_FRAME_setVersionOffset(x,y)\
		setU1(((&(x)->VersionOffset)),0,y)
#define	I2O_MESSAGE_FRAME_getMsgFlags(x)\
		getU1((&((x)->VersionOffset)),1)
#define	I2O_MESSAGE_FRAME_setMsgFlags(x,y)\
		setU1((&((x)->VersionOffset)),1,y)
#define	I2O_MESSAGE_FRAME_getMessageSize(x)\
		getLU2((&((x)->VersionOffset)),2)
#define	I2O_MESSAGE_FRAME_setMessageSize(x,y)\
		setLU2((&((x)->VersionOffset)),2,y)
#define	I2O_MESSAGE_FRAME_getTargetAddress(x)\
		_F_getTID(x,TargetAddress,TargetAddress)
#define	I2O_MESSAGE_FRAME_setTargetAddress(x,y)\
		_F_setTID(x,TargetAddress,TargetAddress,y)
#define	I2O_MESSAGE_FRAME_getInitiatorAddress(x)\
		_F_getTID1(x,TargetAddress,InitiatorAddress)
#define	I2O_MESSAGE_FRAME_setInitiatorAddress(x,y)\
		_F_setTID1(x,TargetAddress,InitiatorAddress,y)
#define	I2O_MESSAGE_FRAME_getFunction(x)\
		_F_getFunc(x,TargetAddress,Function)
#define	I2O_MESSAGE_FRAME_setFunction(x,y)\
		_F_setFunc(x,TargetAddress,Function,y)
/* 32 bit only for now */
#define	I2O_MESSAGE_FRAME_getInitiatorContext(x)\
		(x)->InitiatorContext
#define	I2O_MESSAGE_FRAME_setInitiatorContext(x,y)\
		((x)->InitiatorContext = (y))
/*
 *	We are spilling the 64 bit Context field into the Transaction
 *	context of the specific frames. Synchronous commands (resetIop
 *	et al) do not have this field, so beware. Also, Failed Reply frames
 *	can not contain the 64 bit context, the software must reference
 *	the PreservedMFA and pick up the 64 bit context from the incoming
 *	message frame. The software must make no reference to the
 *	TransactionContext field at all.
 */
#if defined(_MSC_VER) && _MSC_VER >= 800
#ifndef u_int64_t
#define	u_int64_t unsigned __int64
#endif
#endif
#define	I2O_MESSAGE_FRAME_getInitiatorContext64(x)\
		(*((u_int64_t *)(&((x)->InitiatorContext))))
#define	I2O_MESSAGE_FRAME_setInitiatorContext64(x,y)\
		((*((u_int64_t *)(&((x)->InitiatorContext))))=(y))

/*
 * I2O_EXEC_OUTBOUND_INIT_MESSAGE
 */
#define	I2O_EXEC_OUTBOUND_INIT_MESSAGE_getHostPageFrameSize(x)\
		getLU4((&(x)->HostPageFrameSize),0)
#define	I2O_EXEC_OUTBOUND_INIT_MESSAGE_setHostPageFrameSize(x,y)\
		setLU4((&(x)->HostPageFrameSize),0,y)
#define	I2O_EXEC_OUTBOUND_INIT_MESSAGE_getInitCode(x)\
		getU1((&(x)->InitCode),0)
#define	I2O_EXEC_OUTBOUND_INIT_MESSAGE_setInitCode(x,y)\
		setU1((&(x)->InitCode),0,y)
#define	I2O_EXEC_OUTBOUND_INIT_MESSAGE_getreserved(x)\
		getU1((&(x)->reserved),0)
#define	I2O_EXEC_OUTBOUND_INIT_MESSAGE_setreserved(x,y)\
		setU1((&(x)->reserved),0,y)
#define	I2O_EXEC_OUTBOUND_INIT_MESSAGE_getOutboundMFrameSize(x)\
		getLU2((&(x)->OutboundMFrameSize),0)
#define	I2O_EXEC_OUTBOUND_INIT_MESSAGE_setOutboundMFrameSize(x,y)\
		setLU2((&(x)->OutboundMFrameSize),0,y)

/*
 * I2O_EXEC_SYS_TAB_SET_MESSAGE
 */
#define	I2O_EXEC_SYS_TAB_SET_MESSAGE_getIOP_ID(x)\
		_F_get12bit(x,IOP_ID,IOP_ID)
#define	I2O_EXEC_SYS_TAB_SET_MESSAGE_setIOP_ID(x,y)\
		_F_set12bit(x,IOP_ID,IOP_ID,y)
/* #define	I2O_EXEC_SYS_TAB_SET_MESSAGE_getreserved1(x) */
#define	I2O_EXEC_SYS_TAB_SET_MESSAGE_getHostUnitID(x)\
		_F_get16bit(x,IOP_ID,2,HostUnitID)
#define	I2O_EXEC_SYS_TAB_SET_MESSAGE_setHostUnitID(x,y)\
		_F_set16bit(x,IOP_ID,2,HostUnitID,y)
#define	I2O_EXEC_SYS_TAB_SET_MESSAGE_getSegmentNumber(x)\
		_F_get12bit(x,SegmentNumber,SegmentNumber)
#define	I2O_EXEC_SYS_TAB_SET_MESSAGE_setSegmentNumber(x,y)\
		_F_get12bit(x,SegmentNumber,SegmentNumber,y)

/*	later
 * I2O_EXEC_SYS_ENABLE_MESSAGE
 */

/*
 * I2O_CLASS_ID
 */
#define	I2O_CLASS_ID_getClass(x)\
		_F_get12bit(x,Class,0,Class)
#define	I2O_CLASS_ID_setClass(x,y)\
		_F_set12bit(x,Class,0,Class,y)
#define	I2O_CLASS_ID_getVersion(x)\
		_F_get4bit4(x,Class,1,Version)
#define	I2O_CLASS_ID_setVersion(x,y)\
		_F_set4bit4(x,Class,1,Version,y)
#define	I2O_CLASS_ID_getOrganizationID(x)\
		_F_get16bit(x,Class,2,OrganizationID)
#define	I2O_CLASS_ID_setOrganizationID(x,y)\
		_F_set16bit(x,Class,2,OrganizationID,y)

/*
 * I2O_SET_SYSTAB_HEADER
 */
#define	I2O_SET_SYSTAB_HEADER_getNumberEntries(x)\
		getU1((&((x)->NumberEntries)),0)
#define	I2O_SET_SYSTAB_HEADER_setNumberEntries(x,y)\
		setU1((&(x)->NumberEntries),0,y)
#define	I2O_SET_SYSTAB_HEADER_getSysTabVersion(x)\
		getU1((&((x)->SysTabVersion)),0)
#define	I2O_SET_SYSTAB_HEADER_setSysTabVersion(x,y)\
		setU1((&(x)->SysTabVersion),0,y)
/*  U16 reserved		*/
/*  U32 CurrentChangeIndicator	*/




/*
 * I2O_IOP_ENTRY
 */
#define	I2O_IOP_ENTRY_getOrganizationID(x)\
		getLU2((&((x)->OrganizationID)),0)
#define	I2O_IOP_ENTRY_setOrganizationID(x,y)\
		setLU2((&((x)->OrganizationID)),0,y)
/* #define	I2O_IOP_ENTRY_getreserved U16; */
#define	I2O_IOP_ENTRY_getIOP_ID(x)\
		_F_get12bit(x,IOP_ID,0,IOP_ID)
#define	I2O_IOP_ENTRY_setIOP_ID(x,y)\
		_F_set12bit(x,IOP_ID,0,IOP_ID,y)
/*   BF				 reserved3:I2O_RESERVED_4BITS;	*/
/*   BF				 reserved1:I2O_RESERVED_16BITS; */
#define	I2O_IOP_ENTRY_getSegmentNumber(x)\
		_F_get12bit(x,SegmentNumber,0,SegmentNumber)
#define	I2O_IOP_ENTRY_setSegmentNumber(x,y)\
		_F_set12bit(x,SegmentNumber,0,SegmentNumber,y)
#define	I2O_IOP_ENTRY_getI2oVersion(x)\
		_F_get4bit4(x,SegmentNumber,1,I2oVersion)
#define	I2O_IOP_ENTRY_setI2oVersion(x,y)\
		_F_set4bit4(x,SegmentNumber,1,I2oVersion,y)
#define	I2O_IOP_ENTRY_getIopState(x)\
		_F_get8bit(x,SegmentNumber,2,IopState)
#define	I2O_IOP_ENTRY_setIopState(x,y)\
		_F_set8bit(x,SegmentNumber,2,IopState,y)
#define	I2O_IOP_ENTRY_getMessengerType(x)\
		_F_get8bit(x,SegmentNumber,3,MessengerType)
#define	I2O_IOP_ENTRY_setMessengerType(x,y)\
		_F_set8bit(x,SegmentNumber,3,MessengerType,y)
#define	I2O_IOP_ENTRY_getInboundMessageFrameSize(x)\
		getLU2((&((x)->InboundMessageFrameSize)),0)
#define	I2O_IOP_ENTRY_setInboundMessageFrameSize(x,y)\
		setLU2((&((x)->InboundMessageFrameSize)),0,y)
#define	I2O_IOP_ENTRY_getreserved2(x)\
		getLU2((&((x)->reserved2)),0)
#define	I2O_IOP_ENTRY_setreserved2(x,y)\
		setLU2((&((x)->reserved2)),0,y)
#define	I2O_IOP_ENTRY_getLastChanged(x)\
		getLU4((&((x)->LastChanged)),0)
#define	I2O_IOP_ENTRY_setLastChanged(x,y)\
		setLU4((&((x)->LastChanged)),0,y)
#define	I2O_IOP_ENTRY_getIopCapabilities(x)\
		getLU4((&((x)->IopCapabilities)),0)
#define	I2O_IOP_ENTRY_setIopCapabilities(x,y)\
		setLU4((&((x)->IopCapabilities)),0,y)

/* might want to declare I2O_MESSENGER_INFO struct */

#define	I2O_IOP_ENTRY_getInboundMessagePortAddressLow(x)\
		getLU4((&((x)->MessengerInfo.InboundMessagePortAddressLow)),0)
#define	I2O_IOP_ENTRY_setInboundMessagePortAddressLow(x,y)\
		setLU4((&((x)->MessengerInfo.InboundMessagePortAddressLow)),0,y)

#define	I2O_IOP_ENTRY_getInboundMessagePortAddressHigh(x)\
		getLU4((&((x)->MessengerInfo.InboundMessagePortAddressHigh)),0)
#define	I2O_IOP_ENTRY_setInboundMessagePortAddressHigh(x,y)\
		setLU4((&((x)->MessengerInfo.InboundMessagePortAddressHigh)),0,y)

/*
 *  I2O_HRT
 */
#define	I2O_HRT_getNumberEntries(x)\
		getLU2((&((x)->NumberEntries)),0)
#define	I2O_HRT_setNumberEntries(x,y)\
		setLU2((&(x)->NumberEntries),0,y)
#define	I2O_HRT_getEntryLength(x)\
		getU1((&(x)->EntryLength),0)
#define	I2O_HRT_setEntryLength(x,y)\
		setU1((&(x)->EntryLength),0,y)
#define	I2O_HRT_getHRTVersion(x)\
		getU1((&(x)->HRTVersion),0)
#define	I2O_HRT_setHRTVersion(x,y)\
		setU1((&(x)->HRTVersion),0,y)
#define	I2O_HRT_getCurrentChangeIndicator(x)\
		getLU4((&(x)->CurrentChangeIndicator),0)
#define	I2O_HRT_setCurrentChangeIndicator(x,y)\
		setLU4((&(x)->CurrentChangeIndicator),0,y)
#define	I2O_HRT_getHRTEntryPtr(x,y)\
		((&((x)->HRTEntry[0+y])))

/*
 *  I2O_HRT_ENTRY
 */
#define	I2O_HRT_ENTRY_getAdapterID(x)\
		getLU4((&((x)->AdapterID)),0)
#define	I2O_HRT_ENTRY_setAdapterID(x,y)\
		setLU4((&(x)->AdapterID),0,y)
#define	I2O_HRT_ENTRY_getControllingTID(x)\
		_F_get12bit(x,ControllingTID,ControllingTID)
#define	I2O_HRT_ENTRY_setControllingTID(x,y)\
		_F_set12bit(x,ControllingTID,ControllingTID,y)
#define	I2O_HRT_ENTRY_getAdapterState(x)\
		_F_get4bit4(x,ControllingTID,1,AdapterState)
#define	I2O_HRT_ENTRY_setIAdapterState(x,y)\
		_F_set4bit4(x,ControllingTID,1,AdapterState,y)
#define	I2O_HRT_ENTRY_getBusNumber(x)\
		_F_get8bit(x,ControllingTID,2,BusNumber)
#define	I2O_HRT_ENTRY_setBusNumber(x,y)\
		_F_set8bit(x,ControllingTID,2,BusNumber,y)
#define	I2O_HRT_ENTRY_getBusType(x)\
		_F_get8bit(x,ControllingTID,3,BusType)
#define	I2O_HRT_ENTRY_setBusType(x,y)\
		_F_set8bit(x,ControllingTID,3,BusType,y)
#define	I2O_HRT_ENTRY_getPCIBusPtr(x,y)\
		(&((x)->uBus.PCIBus))

/*
 *  I2O_LCT
 */
#define	I2O_LCT_getTableSize(x)\
		_F_get16bit(x,TableSize,0,TableSize)
#define	I2O_LCT_setTableSize(x,y)\
		_F_set16bit(x,TableSize,0,TableSize,y)
#define	I2O_LCT_getBootDeviceTID(x)\
		_F_get12bit(x,TableSize,2,BootDeviceTID)
#define	I2O_LCT_setBootDeviceTID(x,y)\
		_F_set12bit(x,TableSize,2,BootDeviceTID,y)
#define	I2O_LCT_getLctVer(x)\
		_F_get4bit4(x,TableSize,3,LctVer)
#define	I2O_LCT_setLctVer(x,y)\
		_F_set4bit4(x,TableSize,3,LctVer,y)
#define	I2O_LCT_getIopFlags(x)\
		getLU4((&(x)->IopFlags),0)
#define	I2O_LCT_setIopFlags(x,y)\
		setLU4((&(x)->IopFlags),0,y)
#define	I2O_LCT_getCurrentChangeIndicator(x)\
		getLU4((&(x)->CurrentChangeIndicator),0)
#define	I2O_LCT_setCurrentChangeIndicator(x,y)\
		setLU4((&(x)->CurrentChangeIndicator),0,y)
#define	I2O_LCT_getLCTEntryPtr(x,y)\
		(&((x)->LCTEntry[0+y]))

/*
 *  I2O_LCT_ENTRY
 */
#define	I2O_LCT_ENTRY_getTableEntrySize(x)\
		_F_get16bit(x,TableEntrySize,0,TableEntrySize)
#define	I2O_LCT_ENTRY_setTableEntrySize(x,y)\
		_F_set16bit(x,TableEntrySize,0,TableEntrySize,y)
#define	I2O_LCT_ENTRY_getLocalTID(x)\
		_F_get12bit(x,TableEntrySize,2,LocalTID)
#define	I2O_LCT_ENTRY_setLocalTID(x,y)\
		_F_set12bit(x,TableEntrySize,2,LocalTID,y)
/*    BF		  4	   reserved:I2O_4BIT_VERSION_SZ; */
#define	I2O_LCT_ENTRY_getChangeIndicator(x)\
		getLU4((&(x)->ChangeIndicator),0)
#define	I2O_LCT_ENTRY_setChangeIndicator(x,y)\
		setLU4((&(x)->ChangeIndicator),0,y)
#define	I2O_LCT_ENTRY_getDeviceFlags(x)\
		getLU4((&(x)->DeviceFlags),0)
#define	I2O_LCT_ENTRY_setDeviceFlags(x,y)\
		setLU4((&(x)->DeviceFlags),0,y)
#define	I2O_LCT_ENTRY_getClassIDPtr(x)\
		(&((x)->ClassID))
#define	I2O_LCT_ENTRY_getSubClassInfo(x)\
		getLU4((&(x)->SubClassInfo),0)
#define	I2O_LCT_ENTRY_setSubClassInfo(x,y)\
		setLU4((&(x)->SubClassInfo),0,y)
#define	I2O_LCT_ENTRY_getUserTID(x)\
		_F_getTID(x,UserTID,UserTID)
#define	I2O_LCT_ENTRY_setUserTID(x,y)\
		_F_setTID(x,UserTID,UserTID,y)
#define	I2O_LCT_ENTRY_getParentTID(x)\
		_F_getTID1(x,UserTID,ParentTID)
#define	I2O_LCT_ENTRY_setParentTID(x,y)\
		_F_getTID1(x,UserTID,ParentTID,y)
#define	I2O_LCT_ENTRY_getBiosInfo(x)\
		_F_getFunc(x,UserTID,BiosInfo)
#define	I2O_LCT_ENTRY_setBiosInfo(x,y)\
		_F_setFunc(x,UserTID,BiosInfo,y)
/*  2 ulong   U8		    8	   IdentityTag[I2O_IDENTITY_TAG_SZ]; */
#define	I2O_LCT_ENTRY_getEventCapabilities(x)\
		getLU4((&(x)->EventCapabilities),0)
#define	I2O_LCT_ENTRY_setEventCapabilities(x,y)\
		setLU4((&(x)->EventCapabilities),0,y)

/*
 *  I2O_PARAM_OPERATIONS_LIST_HEADER
 */
#define	I2O_PARAM_OPERATIONS_LIST_HEADER_getOperationCount(x)\
		getLU2((&(x)->OperationCount),0)
#define	I2O_PARAM_OPERATIONS_LIST_HEADER_setOperationCount(x,y)\
		setLU2((&(x)->OperationCount),0,y)
#define	I2O_PARAM_OPERATIONS_LIST_HEADER_getReserved(x)\
		getLU2((&(x)->Reserved),0)
#define	I2O_PARAM_OPERATIONS_LIST_HEADER_setReserved(x,y)\
		setLU2((&(x)->Reserved),0,y)

/*
 *  I2O_PARAM_OPERATION_ALL_TEMPLATE
 */
#define	I2O_PARAM_OPERATION_ALL_TEMPLATE_getOperation(x)\
		getLU2((&(x)->Operation),0)
#define	I2O_PARAM_OPERATION_ALL_TEMPLATE_setOperation(x,y)\
		setLU2((&(x)->Operation),0,y)
#define	I2O_PARAM_OPERATION_ALL_TEMPLATE_getGroupNumber(x)\
		getLU2((&(x)->GroupNumber),0)
#define	I2O_PARAM_OPERATION_ALL_TEMPLATE_setGroupNumber(x,y)\
		setLU2((&(x)->GroupNumber),0,y)
#define	I2O_PARAM_OPERATION_ALL_TEMPLATE_getFieldCount(x)\
		getLU2((&(x)->FieldCount),0)
#define	I2O_PARAM_OPERATION_ALL_TEMPLATE_setFieldCount(x,y)\
		setLU2((&(x)->FieldCount),0,y)

/*
 *  I2O_PARAM_RESULTS_LIST_HEADER
 */
#define	I2O_PARAM_RESULTS_LIST_HEADER_getResultCount(x)\
		getLU2((&(x)->ResultCount),0)
#define	I2O_PARAM_RESULTS_LIST_HEADER_setResultCount(x,y)\
		setLU2((&(x)->ResultCount),0,y)
#define	I2O_PARAM_RESULTS_LIST_HEADER_getReserved(x)\
		getLU2((&(x)->Reserved),0)
#define	I2O_PARAM_RESULTS_LIST_HEADER_setReserved(x,y)\
		setLU2((&(x)->Reserved),0,y)

/*  later
 *  I2O_HBA_ADAPTER_RESET_MESSAGE
 */


/*  LATER
 *  I2O_SCSI_DEVICE_RESET_MESSAGE
 */


/*  LATER
 *  I2O_HBA_BUS_RESET_MESSAGE
 */


/*
 *  I2O_EXEC_LCT_NOTIFY_MESSAGE
 */
/*    I2O_MESSAGE_FRAME		  StdMessageFrame; */
/*    I2O_TRANSACTION_CONTEXT	  TransactionContext; */
#define	I2O_EXEC_LCT_NOTIFY_MESSAGE_getClassIdentifier(x)\
		getLU4((&(x)->ClassIdentifier),0)
#define	I2O_EXEC_LCT_NOTIFY_MESSAGE_setClassIdentifier(x,y)\
		setLU4((&(x)->ClassIdentifier),0,y)
#define	I2O_EXEC_LCT_NOTIFY_MESSAGE_getLastReportedChangeIndicator(x)\
		getLU4((&(x)->LastReportedChangeIndicator),0)
#define	I2O_EXEC_LCT_NOTIFY_MESSAGE_setLastReportedChangeIndicator(x,y)\
		setLU4((&(x)->LastReportedChangeIndicator),0,y)
/*    I2O_SG_ELEMENT		  SGL; */



/*
 *  I2O_UTIL_PARAMS_GET_MESSAGE
 */
/*     I2O_MESSAGE_FRAME	  StdMessageFrame;	*/
/*     I2O_TRANSACTION_CONTEXT	  TransactionContext;	*/
#define	I2O_UTIL_PARAMS_GET_MESSAGE_getOperationFlags(x)\
		getLU4((&(x)->OperationFlags),0)
#define	I2O_UTIL_PARAMS_GET_MESSAGE_setOperationFlags(x,y)\
		setLU4((&(x)->OperationFlags),0,y)
/*     I2O_SG_ELEMENT		  SGL;			*/


/*
 *  I2O_SCSI_SCB_ABORT_MESSAGE
 */
#define	I2O_SCSI_SCB_ABORT_MESSAGE_getStdMessageFramePtr(x)\
		(&((x)->StdMessageFrame))
#define	I2O_SCSI_SCB_ABORT_MESSAGE_getTransactionContext(x)\
		(x)->TransactionContext
#define	I2O_SCSI_SCB_ABORT_MESSAGE_setTransactionContext(x,y)\
		((x)->TransactionContext = (y))
#define	I2O_SCSI_SCB_ABORT_MESSAGE_getTransactionContextToAbort(x)\
		(x)->TransactionContextToAbort
#define	I2O_SCSI_SCB_ABORT_MESSAGE_setTransactionContextToAbort(x,y)\
		((x)->TransactionContextToAbort = (y))


/*
 *  I2O_DPT_DEVICE_INFO_SCALAR
 */
#define	I2O_DPT_DEVICE_INFO_SCALAR_getDeviceType(x)\
		getU1((&(x)->DeviceType),0)
#define	I2O_DPT_DEVICE_INFO_SCALAR_setDeviceType(x,y)\
		setU1((&(x)->DeviceType),0,y)
#define	I2O_DPT_DEVICE_INFO_SCALAR_getFlags(x)\
		getU1((&(x)->Flags),0)
#define	I2O_DPT_DEVICE_INFO_SCALAR_setFlags(x,y)\
		setU1((&(x)->Flags),0,y)
#define	I2O_DPT_DEVICE_INFO_SCALAR_getBus(x)\
		getLU2((&(x)->Bus),0)
#define	I2O_DPT_DEVICE_INFO_SCALAR_setBus(x,y)\
		setLU2((&(x)->Bus),0,y)
#define	I2O_DPT_DEVICE_INFO_SCALAR_getIdentifier(x)\
		getLU4((&(x)->Identifier),0)
#define	I2O_DPT_DEVICE_INFO_SCALAR_setIdentifier(x,y)\
		setLU4((&(x)->Identifier),0,y)
/*     U8	  LunInfo[8]; *//* SCSI-2 8-bit scalar LUN goes into offset 1 */
#define	I2O_DPT_DEVICE_INFO_SCALAR_getLunInfo(x)\
		getU1((&(x)->LunInfo[0]),1)
#define	I2O_DPT_DEVICE_INFO_SCALAR_setLunInfo(x,y)\
		setU1((&(x)->LunInfo[0]),1,y)

/*
 *	 I2O_DPT_EXEC_IOP_BUFFERS_SCALAR
 */
#define	I2O_DPT_EXEC_IOP_BUFFERS_SCALAR_getSerialOutputOffset(x)\
		getLU4((&(x)->SerialOutputOffset),0)
#define	I2O_DPT_EXEC_IOP_BUFFERS_SCALAR_getSerialOutputSizet(x)\
		getLU4((&(x)->SerialOutputSize),0)
#define	I2O_DPT_EXEC_IOP_BUFFERS_SCALAR_getSerialHeaderSize(x)\
		getLU4((&(x)->SerialHeaderSize),0)
#define	I2O_DPT_EXEC_IOP_BUFFERS_SCALAR_getSerialFlagsSupported(x)\
		getLU4((&(x)->SerialFlagsSupported),0)

/*
 *  I2O_PRIVATE_MESSAGE_FRAME
 */
/* typedef struct _I2O_PRIVATE_MESSAGE_FRAME { */
/*    I2O_MESSAGE_FRAME		  StdMessageFrame; */
/*    I2O_TRANSACTION_CONTEXT	  TransactionContext; */
/*    U16			  XFunctionCode; */
/*    U16			  OrganizationID; */
/*				  PrivatePayload[]; */
/* } I2O_PRIVATE_MESSAGE_FRAME, *PI2O_PRIVATE_MESSAGE_FRAME; */
#define	I2O_PRIVATE_MESSAGE_FRAME_getTransactionContext(x) \
		(x)->TransactionContext
#define	I2O_PRIVATE_MESSAGE_FRAME_setTransactionContext(x,y) \
		((x)->TransactionContext = (y))
#define	I2O_PRIVATE_MESSAGE_FRAME_getXFunctionCode(x) \
		getLU2((&(x)->XFunctionCode),0)
#define	I2O_PRIVATE_MESSAGE_FRAME_setXFunctionCode(x,y) \
		setLU2((&(x)->XFunctionCode),0,y)
#define	I2O_PRIVATE_MESSAGE_FRAME_getOrganizationID(x) \
		getLU2((&(x)->OrganizationID),0)
#define	I2O_PRIVATE_MESSAGE_FRAME_setOrganizationID(x,y) \
		setLU2((&(x)->OrganizationID),0,y)
#if 0
typedef struct _PRIVATE_SCSI_SCB_EXECUTE_MESSAGE {
	I2O_PRIVATE_MESSAGE_FRAME PRIVATE_SCSI_SCB_EXECUTE_MESSAGE;
	BF			  TID:16; /* Upper four bits currently are zero */
	/* Command is interpreted by the host */
	BF			  Interpret:1;
	/* if TRUE, deal with Physical Firmware Array information */
	BF			  Physical:1;
	BF			  Reserved1:14;
	U8			  CDBLength;
	U8			  Reserved;
	I2O_SCB_FLAGS		  SCBFlags;
	U8			  CDB[	I2O_SCSI_CDB_LENGTH=16	];
	U32			  ByteCount;
	I2O_SG_ELEMENT		  SGL;
} PRIVATE_SCSI_SCB_EXECUTE_MESSAGE, * PPRIVATE_SCSI_SCB_EXECUTE_MESSAGE;
#endif
/*
 *	 PRIVATE_SCSI_SCB_EXECUTE_MESSAGE
 */
#define	PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_getPRIVATE_SCSI_SCB_EXECUTE_MESSAGEPtr(x)\
		(&((x)->PRIVATE_SCSI_SCB_EXECUTE_MESSAGE))
#define	PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_getCDBLength(x)\
		getU1((&(x)->CDBLength),0)
#define	PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setCDBLength(x,y)\
		setU1((&(x)->CDBLength),0,y)
#define	PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_getReserved(x)\
		getU1((&(x)->Reserved),0)
#define	PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setReserved(x,y)\
		setU1((&(x)->Reserved),0,y)
#define	PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_getSCBFlags(x)\
		getLU2((&(x)->SCBFlags),0)
#define	PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setSCBFlags(x,y)\
		setLU2((&(x)->SCBFlags),0,y)
#define	PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_getByteCount(x)\
		getLU4((&((x)->ByteCount)),0)
#define	PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setByteCount(x,y)\
		setLU4((&((x)->ByteCount)),0,y)
#define	PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_getTID(x)\
		_F_get16bit(x,TID,0,TID)
#define	PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setTID(x,y)\
		_F_set16bit(x,TID,0,TID,y)
#define	PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_getInterpret(x)\
		_F_get1bit(x,TID,2,Interpret)
#define	PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setInterpret(x,y)\
		_F_set1bit(x,TID,2,Interpret,y)
#define	PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_getPhysical(x)\
		_F_get1bit1(x,TID,2,Physical)
#define	PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setPhysical(x,y)\
		_F_set1bit1(x,TID,2,Physical,y)
#define	PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_getCDBPtr(x)\
		(&((x)->CDB[0]))


/*
 *  PRIVATE_FLASH_REGION_MESSAGE
 */
#define	PRIVATE_FLASH_REGION_MESSAGE_getFlashRegion(x) \
		getLU4((&((x)->FlashRegion)),0)
#define	PRIVATE_FLASH_REGION_MESSAGE_setFlashRegion(x,y) \
		setLU4((&((x)->FlashRegion)),0,y)
#define	PRIVATE_FLASH_REGION_MESSAGE_getRegionOffset(x) \
		getLU4((&((x)->RegionOffset)),0)
#define	PRIVATE_FLASH_REGION_MESSAGE_setRegionOffset(x,y) \
		setLU4((&((x)->RegionOffset)),0,y)
#define	PRIVATE_FLASH_REGION_MESSAGE_getByteCount(x) \
		getLU4((&((x)->ByteCount)),0)
#define	PRIVATE_FLASH_REGION_MESSAGE_setByteCount(x,y) \
		setLU4((&((x)->ByteCount)),0,y)

/*
 *  I2O_HBA_SCSI_CONTROLLER_INFO_SCALAR
 */
#define	I2O_HBA_SCSI_CONTROLLER_INFO_SCALAR_getSCSIType(x)\
		getU1((&(x)->SCSIType),0)
#define	I2O_HBA_SCSI_CONTROLLER_INFO_SCALAR_setSCSIType(x,y)\
		setU1((&(x)->SCSIType),0,y)
#define	I2O_HBA_SCSI_CONTROLLER_INFO_SCALAR_getProtectionManagement(x)\
		getU1((&(x)->ProtectionManagement),0)
#define	I2O_HBA_SCSI_CONTROLLER_INFO_SCALAR_setProtectionManagement(x,y)\
		setU1((&(x)->ProtectionManagement),0,y)
#define	I2O_HBA_SCSI_CONTROLLER_INFO_SCALAR_getSettings(x)\
		getU1((&(x)->Settings),0)
#define	I2O_HBA_SCSI_CONTROLLER_INFO_SCALAR_setSettings(x,y)\
		setU1((&(x)->Settings),0,y)
#define	I2O_HBA_SCSI_CONTROLLER_INFO_SCALAR_getReserved1(x)\
		getU1((&(x)->Reserved1),0)
#define	I2O_HBA_SCSI_CONTROLLER_INFO_SCALAR_setReserved1(x,y)\
		setU1((&(x)->Reserved1),0,y)
#define	I2O_HBA_SCSI_CONTROLLER_INFO_SCALAR_getInitiatorID(x)\
		getLU4((&(x)->InitiatorID),0)
#define	I2O_HBA_SCSI_CONTROLLER_INFO_SCALAR_setInitiatorID(x,y)\
		setLU4((&(x)->InitiatorID),0,y)
#define	I2O_HBA_SCSI_CONTROLLER_INFO_SCALAR_getScanLun0Only(x)\
		getLU4((&(x)->ScanLun0Only),0)
#define	I2O_HBA_SCSI_CONTROLLER_INFO_SCALAR_setScanLun0Only(x,y)\
		setLU4((&(x)->ScanLun0Only),0,y)
#define	I2O_HBA_SCSI_CONTROLLER_INFO_SCALAR_getDisableDevice(x)\
		getLU2((&(x)->DisableDevice),0)
#define	I2O_HBA_SCSI_CONTROLLER_INFO_SCALAR_setDisableDevice(x,y)\
		setLU2((&(x)->DisableDevice),0,y)
#define	I2O_HBA_SCSI_CONTROLLER_INFO_SCALAR_getMaxOffset(x)\
		getU1((&(x)->MaxOffset),0)
#define	I2O_HBA_SCSI_CONTROLLER_INFO_SCALAR_setMaxOffset(x,y)\
		setU1((&(x)->MaxOffset),0,y)
#define	I2O_HBA_SCSI_CONTROLLER_INFO_SCALAR_getMaxDataWidth(x)\
		getU1((&(x)->MaxDataWidth),0)
#define	I2O_HBA_SCSI_CONTROLLER_INFO_SCALAR_setMaxDataWidth(x,y)\
		setU1((&(x)->MaxDataWidth),0,y)
#define	I2O_HBA_SCSI_CONTROLLER_INFO_SCALAR_getMaxSyncRate(x)\
		getLU4((&(x)->MaxSyncRate),0)
#define	I2O_HBA_SCSI_CONTROLLER_INFO_SCALAR_setMaxSyncRate(x,y)\
		setLU4((&(x)->MaxSyncRate),0,y)

/*
 *  I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME
 */
#define	I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME_getStdReplyFramePtr(x)\
		(&((x)->StdReplyFrame))
#define	I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME_getTransferCount(x)\
		getLU4((&(x)->TransferCount),0)
#define	I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME_setTransferCount(x,y)\
		setLU4((&(x)->TransferCount),0,y)
#define	I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME_getAutoSenseTransferCount(x)\
		getLU4((&(x)->AutoSenseTransferCount),0)
#define	I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME_setAutoSenseTransferCount(x,y)\
		setLU4((&(x)->AutoSenseTransferCount),0,y)

/*
 *  I2O_SINGLE_REPLY_MESSAGE_FRAME
 */
#define	I2O_SINGLE_REPLY_MESSAGE_FRAME_getStdMessageFramePtr(x)\
		(&((x)->StdMessageFrame))
#define	I2O_SINGLE_REPLY_MESSAGE_FRAME_getTransactionContext(x)\
		(x)->TransactionContext
#define	I2O_SINGLE_REPLY_MESSAGE_FRAME_setTransactionContext(x,y)\
		((x)->TransactionContext = (y))
#define	I2O_SINGLE_REPLY_MESSAGE_FRAME_getDetailedStatusCode(x)\
		getLU2((&((x)->DetailedStatusCode)),0)
#define	I2O_SINGLE_REPLY_MESSAGE_FRAME_setDetailedStatusCode(x,y)\
		setLU2((&((x)->DetailedStatusCode)),0,y)
#define	I2O_SINGLE_REPLY_MESSAGE_FRAME_getreserved(x)\
		getU1((&((x)->reserved)),0)
#define	I2O_SINGLE_REPLY_MESSAGE_FRAME_setreserved(x,y)\
		setU1((&((x)->reserved)),0,y)
#define	I2O_SINGLE_REPLY_MESSAGE_FRAME_getReqStatus(x)\
		getU1((&((x)->ReqStatus)),0)
#define	I2O_SINGLE_REPLY_MESSAGE_FRAME_setReqStatus(x,y)\
		setU1((&((x)->ReqStatus)),0,y)


/*
 *  I2O_SCSI_SCB_EXECUTE_MESSAGE
 */
#define	I2O_SCSI_SCB_EXECUTE_MESSAGE_getStdMessageFramePtr(x)\
		(&((x)->StdMessageFrame))
#define	I2O_SCSI_SCB_EXECUTE_MESSAGE_getTransactionContext(x)\
		(x)->TransactionContext
#define	I2O_SCSI_SCB_EXECUTE_MESSAGE_setTransactionContext(x,y)\
		((x)->TransactionContext = (y))
#define	I2O_SCSI_SCB_EXECUTE_MESSAGE_getCDBLength(x)\
		getU1((&((x)->CDBLength)),0)
#define	I2O_SCSI_SCB_EXECUTE_MESSAGE_setCDBLength(x,y)\
		setU1((&((x)->CDBLength)),0,y)
#define	I2O_SCSI_SCB_EXECUTE_MESSAGE_getReserved(x)\
		getU1((&((x)->Reserved)),0)
#define	I2O_SCSI_SCB_EXECUTE_MESSAGE_setReserved(x,y)\
		setU1((&((x)->Reserved)),0,y)
#define	I2O_SCSI_SCB_EXECUTE_MESSAGE_getSCBFlags(x)\
		getLU2((&((x)->SCBFlags)),0)
#define	I2O_SCSI_SCB_EXECUTE_MESSAGE_setSCBFlags(x,y)\
		setLU2((&((x)->SCBFlags)),0,y)
#define	I2O_SCSI_SCB_EXECUTE_MESSAGE_getByteCount(x)\
		getLU2((&((x)->ByteCount)),0)
#define	I2O_SCSI_SCB_EXECUTE_MESSAGE_setByteCount(x,y)\
		setLU2((&((x)->ByteCount)),0,y)
/*  define for these */
/*     U8		       CDB[16]; */
/*     I2O_SG_ELEMENT	       SGL;	*/


/*
 *  I2O_FAILURE_REPLY_MESSAGE_FRAME
 */
#define	I2O_FAILURE_REPLY_MESSAGE_FRAME_getStdMessageFramePtr(x)\
		(&((x)->StdMessageFrame))
#define	I2O_FAILURE_REPLY_MESSAGE_FRAME_getTransactionContext(x)\
		(x)->TransactionContext
#define	I2O_FAILURE_REPLY_MESSAGE_FRAME_setTransactionContext(x,y)\
		((x)->TransactionContext = (y))
#define	I2O_FAILURE_REPLY_MESSAGE_FRAME_getLowestVersion(x)\
		getU1((&((x)->LowestVersion)),0)
#define	I2O_FAILURE_REPLY_MESSAGE_FRAME_setLowestVersion(x,y)\
		setU1((&((x)->LowestVersion)),0,y)
#define	I2O_FAILURE_REPLY_MESSAGE_FRAME_getHighestVersion(x)\
		getU1((&((x)->HighestVersion)),0)
#define	I2O_FAILURE_REPLY_MESSAGE_FRAME_setHighestVersion(x,y)\
		setU1((&((x)->HighestVersion)),0,y)
#define	I2O_FAILURE_REPLY_MESSAGE_FRAME_getAgeLimit(x)\
		getLU4((&((x)->AgeLimit)),0)
#define	I2O_FAILURE_REPLY_MESSAGE_FRAME_setAgeLimit(x,y)\
		setLU4((&((x)->AgeLimit)),0,y)
#define	I2O_FAILURE_REPLY_MESSAGE_FRAME_getSeverity(x)\
		_F_get8bit(x,Severity,0,Severity)
#define	I2O_FAILURE_REPLY_MESSAGE_FRAME_setSeverity(x,y)\
		_F_set8bit(x,Severity,0,Severity,y)
#define	I2O_FAILURE_REPLY_MESSAGE_FRAME_getFailureCode(x)\
		_F_get8bit(x,Severity,1,FailureCode)
#define	I2O_FAILURE_REPLY_MESSAGE_FRAME_setFailureCode(x,y)\
		_F_get8bit(x,Severity,1,FailureCode,y)
/*
 * #define	I2O_FAILURE_REPLY_MESSAGE_FRAME_getFailingHostUnitID(x)\
 *		 _F_get16bit(x,reserved,1,FailingHostUnitID)
 * #define	I2O_FAILURE_REPLY_MESSAGE_FRAME_setFailingHostUnitID(x,y)\
 *		 _F_set16bit(x,reserved,1,FailingHostUnitID,y)
 */
#define	I2O_FAILURE_REPLY_MESSAGE_FRAME_getPreservedMFA(x)\
		getLU4((&((x)->PreservedMFA)),0)
#define	I2O_FAILURE_REPLY_MESSAGE_FRAME_setPreservedMFA(x,y)\
		setLU4((&((x)->PreservedMFA)),0,y)



/*
 *  I2O_EXEC_STATUS_GET_REPLY
 */
#define	I2O_EXEC_STATUS_GET_REPLY_getOrganizationID(x)\
		getLU2((&(x)->OrganizationID),0)
#define	I2O_EXEC_STATUS_GET_REPLY_setOrganizationID(x,y)\
		setLU2((&(x)->OrganizationID),0,y)
/* #define	I2O_EXEC_STATUS_GET_REPLY_getreserved; */
#define	I2O_EXEC_STATUS_GET_REPLY_getIOP_ID(x)\
		_F_get12bit(x,IOP_ID,0,IOP_ID)
#define	I2O_EXEC_STATUS_GET_REPLY_setIOP_ID(x,y)\
		_F_set12bit(x,IOP_ID,0,IOP_ID,y)
/* #define	I2O_EXEC_STATUS_GET_REPLY_getreserved1(x) */
#define	I2O_EXEC_STATUS_GET_REPLY_getHostUnitID(x)\
		_F_get16bit(x,IOP_ID,2,HostUnitID)
#define	I2O_EXEC_STATUS_GET_REPLY_setHostUnitID(x,y)\
		_F_set16bit(x,IOP_ID,2,HostUnitID,y)
#define	I2O_EXEC_STATUS_GET_REPLY_getSegmentNumber(x)\
		_F_get12bit(x,SegmentNumber,0,SegmentNumber)
#define	I2O_EXEC_STATUS_GET_REPLY_setSegmentNumber(x,y)\
		_F_set12bit(x,SegmentNumber,0,SegmentNumber,y)
#define	I2O_EXEC_STATUS_GET_REPLY_getI2oVersion(x)\
		_F_get4bit4(x,SegmentNumber,1,I2oVersion)
#define	I2O_EXEC_STATUS_GET_REPLY_setI2oVersion(x,y)\
		_F_set4bit4(x,SegmentNumber,1,I2oVersion,y)
#define	I2O_EXEC_STATUS_GET_REPLY_getIopState(x)\
		_F_get8bit(x,SegmentNumver,2,IopState)
#define	I2O_EXEC_STATUS_GET_REPLY_setIopState(x,y)\
		_F_set8bit(x,SegmentNumver,2,IopState,y)
#define	I2O_EXEC_STATUS_GET_REPLY_getMessengerType(x)\
		_F_get8bit(x,SegmentNumber,3,MessengerType)
#define	I2O_EXEC_STATUS_GET_REPLY_setMessengerType(x,y)\
		_F_get8bit(x,SegmentNumber,3,MessengerType,y)
#define	I2O_EXEC_STATUS_GET_REPLY_getInboundMFrameSize(x)\
		getLU2((&(x)->InboundMFrameSize),0)
#define	I2O_EXEC_STATUS_GET_REPLY_setInboundMFrameSize(x,y)\
		setLU2((&(x)->InboundMFrameSize),0,y)
#define	I2O_EXEC_STATUS_GET_REPLY_getInitCode(x)\
		getU1((&(x)->InitCode),0)
#define	I2O_EXEC_STATUS_GET_REPLY_setInitCode(x,y)\
		setU1((&(x)->InitCode),0,y)
/* #define	I2O_EXEC_STATUS_GET_REPLY_getreserved2(x) */
#define	I2O_EXEC_STATUS_GET_REPLY_getMaxInboundMFrames(x)\
		getLU4((&(x)->MaxInboundMFrames),0)
#define	I2O_EXEC_STATUS_GET_REPLY_setMaxInboundMFrames(x,y)\
		setLU4((&(x)->MaxInboundMFrames),0,y)
#define	I2O_EXEC_STATUS_GET_REPLY_getCurrentInboundMFrames(x)\
		getLU4((&(x)->CurrentInboundMFrames),0)
#define	I2O_EXEC_STATUS_GET_REPLY_setCurrentInboundMFrames(x,y)\
		setLU4((&(x)->CurrentInboundMFrames),0,y)
#define	I2O_EXEC_STATUS_GET_REPLY_getMaxOutboundMFrames(x)\
		getLU4((&(x)->MaxOutboundMFrames),0)
#define	I2O_EXEC_STATUS_GET_REPLY_setMaxOutboundMFrames(x,y)\
		setLU4((&(x)->MaxOutboundMFrames),0,y)
/* #define	I2O_EXEC_STATUS_GET_REPLY_getProductIDString(x) */
#define	I2O_EXEC_STATUS_GET_REPLY_getExpectedLCTSize(x)\
		getLU4((&(x)->ExpectedLCTSize),0)
#define	I2O_EXEC_STATUS_GET_REPLY_setExpectedLCTSize(x,y)\
		setLU4((&(x)->ExpectedLCTSize),0,y)
#define	I2O_EXEC_STATUS_GET_REPLY_getIopCapabilities(x)\
		getLU4((&(x)->IopCapabilities),0)
#define	I2O_EXEC_STATUS_GET_REPLY_setIopCapabilities(x,y)\
		setLU4((&(x)->IopCapabilities),0,y)
#define	I2O_EXEC_STATUS_GET_REPLY_getDesiredPrivateMemSize(x)\
		getLU4((&(x)->DesiredPrivateMemSize),0)
#define	I2O_EXEC_STATUS_GET_REPLY_setDesiredPrivateMemSize(x,y)\
		setLU4((&(x)->DesiredPrivateMemSize),0,y)
#define	I2O_EXEC_STATUS_GET_REPLY_getCurrentPrivateMemSize(x)\
		getLU4((&(x)->CurrentPrivateMemSize),0)
#define	I2O_EXEC_STATUS_GET_REPLY_setCurrentPrivateMemSize(x,y)\
		setLU4((&(x)->CurrentPrivateMemSize),0,y)
#define	I2O_EXEC_STATUS_GET_REPLY_getCurrentPrivateMemBase(x)\
		getLU4((&(x)->CurrentPrivateMemBase),0)
#define	I2O_EXEC_STATUS_GET_REPLY_setCurrentPrivateMemBase(x,y)\
		setLU4((&(x)->CurrentPrivateMemBase),0,y)
#define	I2O_EXEC_STATUS_GET_REPLY_getDesiredPrivateIOSize(x)\
		getLU4((&(x)->DesiredPrivateIOSize),0)
#define	I2O_EXEC_STATUS_GET_REPLY_setDesiredPrivateIOSize(x,y)\
		setLU4((&(x)->DesiredPrivateIOSize),0,y)
#define	I2O_EXEC_STATUS_GET_REPLY_getCurrentPrivateIOSize(x)\
		getLU4((&(x)->CurrentPrivateIOSize),0)
#define	I2O_EXEC_STATUS_GET_REPLY_setCurrentPrivateIOSize(x,y)\
		setLU4((&(x)->CurrentPrivateIOSize),0,y)
#define	I2O_EXEC_STATUS_GET_REPLY_getCurrentPrivateIOBase(x)\
		getLU4((&(x)->CurrentPrivateIOBase),0)
#define	I2O_EXEC_STATUS_GET_REPLY_setCurrentPrivateIOBase(x,y)\
		setLU4((&(x)->CurrentPrivateIOBase),0,y)
/* #define	I2O_EXEC_STATUS_GET_REPLY_getreserved3(x) */
#define	I2O_EXEC_STATUS_GET_REPLY_getSyncByte(x)\
		getU1((&(x)->SyncByte),0)
#define	I2O_EXEC_STATUS_GET_REPLY_setSyncByte(x,y)\
		setU1((&(x)->SyncByte),0,y)



/*
 *  I2O_HBA_BUS_QUIESCE_MESSAGE
 */
#define	I2O_HBA_BUS_QUIESCE_MESSAGE_getStdMessageFramePtr(x)\
		(&((x)->StdMessageFrame))
#define	I2O_HBA_BUS_QUIESCE_MESSAGE_getTransactionContext(x)\
		getBU4((&((x)->TransactionContext)),0)
#define	I2O_HBA_BUS_QUIESCE_MESSAGE_setTransactionContext(x,y)\
		setBU4((&((x)->TransactionContext)),0,y)
#define	I2O_HBA_BUS_QUIESCE_MESSAGE_getFlags(x)\
		getLU4((&(x)->Flags),0)
#define	I2O_HBA_BUS_QUIESCE_MESSAGE_setFlags(x,y)\
		setLU4((&(x)->Flags),0,y)


#endif /* __INCi2odeph */
