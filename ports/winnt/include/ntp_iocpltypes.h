/*
 * ntp_iocpltypes.h - data structures for overlapped IO
 *
 * Written by Juergen Perlinger (perlinger@ntp.org) for the NTP project.
 * The contents of 'html/copyright.html' apply.
 *
 * --------------------------------------------------------------------
 */
#ifndef NTP_IOCPLTYPES_H
#define NTP_IOCPLTYPES_H

#include <stdlib.h>
#include <Windows.h>
#include "ntp.h"

/* ---------------------------------------------------------------------
 * forward declarations to avoid deep header nesting
 */
typedef struct IoCtx		IoCtx_t;
typedef struct refclockio	RIO_t;
typedef struct interface	endpt;
typedef struct recvbuf		recvbuf_t;

/* ---------------------------------------------------------------------
 * shared control structure for IO. Removal of communication handles
 * or other detach-like operations must be done exclusively by the IO
 * thread, or Bad Things (tm) are bound to happen!
 */
typedef struct IoHndPad IoHndPad_T;
typedef const struct IoHndPad CIoHndPad_T;
struct IoHndPad {
	volatile u_long		refc_count;
	union {
		RIO_t *		 rio;	/*  RIO back-link (for offload)	*/
		endpt *		 ept;	/*  interface backlink		*/
		ULONG_PTR	 key;	/*  as key for IOCPL queue	*/
		void *		 any;
	}			rsrc;	/* registered source		*/
	HANDLE			handles[2]; /* 0->COM/SOCK 1->BCASTSOCK	*/

	/* COMPORT specific stuff */
	int			riofd;	/* FD for comports		*/
	unsigned int		flDropEmpty : 1;	/* no empty line*/
	unsigned int		flFirstSeen : 1;
};

typedef BOOL(__fastcall * IoPreCheck_T)(CIoHndPad_T*);

extern IoHndPad_T* __fastcall	iohpCreate(void * rsrc);
extern IoHndPad_T* __fastcall	iohpAttach(IoHndPad_T*);
extern IoHndPad_T* __fastcall	iohpDetach(IoHndPad_T*);

extern BOOL __fastcall	iohpRefClockOK(CIoHndPad_T*);
extern BOOL __fastcall	iohpEndPointOK(CIoHndPad_T*);

extern BOOL	iohpQueueLocked(CIoHndPad_T*, IoPreCheck_T, recvbuf_t*);


/* ---------------------------------------------------------------------
 * storage type for PPS data (DCD change counts & times)
 * ---------------------------------------------------------------------
 */
typedef struct PpsData PPSData_t;
struct PpsData {
	u_long	cc_assert;
	u_long	cc_clear;
	l_fp	ts_assert;
	l_fp	ts_clear;
};

typedef volatile struct PpsDataEx PPSDataEx_t;
struct PpsDataEx {
	u_long		cov_count;
	PPSData_t	data;
};

/* ---------------------------------------------------------------------
 * device context; uses reference counting to avoid nasty surprises.
 * Currently this stores only the PPS time stamps, but it could be
 * easily extended.
 * ---------------------------------------------------------------------
 */
#define PPS_QUEUE_LEN	8u		  /* must be power of two! */
#define PPS_QUEUE_MSK	(PPS_QUEUE_LEN-1) /* mask for easy MOD ops */

typedef struct DeviceContext DevCtx_t;
struct DeviceContext {
	volatile u_long	ref_count;
	volatile u_long	cov_count;
	PPSData_t	pps_data;
	PPSDataEx_t	pps_buff[PPS_QUEUE_LEN];
};

extern DevCtx_t* __fastcall DevCtxAlloc(void);
extern DevCtx_t* __fastcall DevCtxAttach(DevCtx_t*);
extern DevCtx_t* __fastcall DevCtxDetach(DevCtx_t*);

/* ---------------------------------------------------------------------
 * I/O context structure
 *
 * This is an extended overlapped structure. Some fields are only used
 * for serial I/O, others are used for all operations. The serial I/O is
 * more interesting since the same context object is used for waiting,
 * actual I/O and possibly offload processing in a worker thread until
 * a complete operation cycle is done.
 *
 * In this case the I/O context is used to gather all the bits that are
 * finally needed for the processing of the buffer.
 * ---------------------------------------------------------------------
 */

typedef void(*IoCompleteFunc)(ULONG_PTR, IoCtx_t *);

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable : 201)		/* nonstd extension nameless union */
#endif
struct IoCtx {
	OVERLAPPED		ol;		/* 'kernel' part of the context	*/
	union {
		recvbuf_t *	recv_buf;	/* incoming -> buffer structure	*/
		void *		trans_buf;	/* outgoing -> char array	*/
		PPSData_t *	pps_buf;	/* for reading PPS seq/stamps	*/
		HANDLE		ppswake;	/* pps wakeup for attach	*/
	};
	union {
		HANDLE		 hnd;		/*  IO handle (the real McCoy)	*/
		SOCKET		 sfd;		/*  socket descriptor		*/
	}			io;		/* the IO resource used		*/
	IoCompleteFunc		onIoDone;	/* HL callback to execute	*/
	IoHndPad_T *		iopad;
	DevCtx_t *		devCtx;
	DWORD			errCode;	/* error code of last I/O	*/
	DWORD			byteCount;	/* byte count     "             */
	DWORD			ioFlags;	/* in/out flags for recvfrom()	*/
	u_int			flRawMem : 1;	/* buffer is raw memory -> free */
	struct {
		l_fp		DCDSTime;	/* PPS-hack: time of DCD ON	*/
		l_fp		FlagTime;	/* time stamp of flag/event char*/
		l_fp		RecvTime;	/* time stamp of callback	*/
		DWORD		com_events;	/* buffer for COM events	*/
		u_int		flTsDCDS : 1;	/* DCDSTime valid?		*/
		u_int		flTsFlag : 1;	/* FlagTime valid?		*/
	} aux;
};
#ifdef _MSC_VER
# pragma warning(pop)
#endif

typedef BOOL (__fastcall *IoCtxStarterT)(IoCtx_t*, recvbuf_t*);

extern IoCtx_t* __fastcall IoCtxAlloc(IoHndPad_T*, DevCtx_t*);
extern void	__fastcall IoCtxFree(IoCtx_t*);
extern void	__fastcall IoCtxRelease(IoCtx_t*);

extern BOOL	IoCtxStartChecked(IoCtx_t*, IoCtxStarterT, recvbuf_t*);

#endif /*!defined(NTP_IOCPLTYPES_H)*/
