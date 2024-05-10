#ifndef NTP_IOCPMPLETIONPORT_H
#define NTP_IOCPMPLETIONPORT_H

#include "ntp_fp.h"
#include "ntp.h"
#include "clockstuff.h"
#include "ntp_worker.h"

#if defined(HAVE_IO_COMPLETION_PORT)

/* NotifyIpInterfaceChange() is available on Windows Vista and later. */
typedef enum ENUM_DONTCARE_MIB_NOTIF_TYPE {
	SomeType_DontCare
} MIB_NOTIF_TYPE;

typedef void
(WINAPI* PMYIPINTERFACE_CHANGE_CALLBACK) (
	PVOID		CallerContext,
	PVOID		Row,
	MIB_NOTIF_TYPE	NotificationType
	);

typedef DWORD (WINAPI* NotifyIpInterfaceChange_ptr)(
	ADDRESS_FAMILY			Family,
	PMYIPINTERFACE_CHANGE_CALLBACK	Callback,
	PVOID				CallerContext,
	BOOLEAN				InitialNotification,
	HANDLE*				NotificationHandle
	);

extern	NotifyIpInterfaceChange_ptr	pNotifyIpInterfaceChange;


struct refclockio;	/* in ntp_refclock.h but inclusion here triggers problems */


extern	void	init_io_completion_port(void);
extern	void	uninit_io_completion_port(void);

extern	BOOL	io_completion_port_add_interface(endpt *);
extern	void	io_completion_port_remove_interface(endpt *);

extern	BOOL	io_completion_port_add_socket(SOCKET fd, endpt *, BOOL bcast);
extern	void	io_completion_port_remove_socket(SOCKET fd, endpt *);

extern	int	io_completion_port_sendto(endpt*, SOCKET, void *, size_t, sockaddr_u *);

extern	BOOL	io_completion_port_add_clock_io(struct refclockio *rio);
extern	void	io_completion_port_remove_clock_io(struct refclockio *rio);

extern	int	GetReceivedBuffers(void);
extern	void WINAPI	IpInterfaceChangedCallback(PVOID ctx, PVOID row,
						   MIB_NOTIF_TYPE type);


extern	HANDLE	WaitableExitEventHandle;

#endif /*!defined(HAVE_IO_COMPLETION_PORT)*/
#endif /*!defined(NTP_IOCPMPLETIONPORT_H)*/
