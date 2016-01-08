#if !defined __ntp_iocompletionport_h
# define __ntp_iocompletionport_h

#include "ntp_fp.h"
#include "ntp.h"
#include "clockstuff.h"
#include "ntp_worker.h"

# if defined(HAVE_IO_COMPLETION_PORT)

extern	void	init_io_completion_port(void);
extern	void	uninit_io_completion_port(void);

extern	int	io_completion_port_add_socket(SOCKET fd, struct interface *);

struct refclockio; /* in ntp_refclock.h but inclusion here triggers problems */
extern	int	io_completion_port_add_clock_io(struct refclockio *rio);
extern	void	io_completion_port_remove_clock_io(struct refclockio *rio);
extern	int	io_completion_port_sendto(SOCKET, void *, size_t, sockaddr_u *);

extern	int	GetReceivedBuffers(void);

extern	HANDLE	WaitableExitEventHandle;

# endif

#endif
