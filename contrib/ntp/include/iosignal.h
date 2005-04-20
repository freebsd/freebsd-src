#if !defined _ntp_iosignaled_h
#define _ntp_iosignaled_h

#include "ntp_refclock.h"

#if defined(HAVE_SIGNALED_IO)
extern void			block_sigio     P((void));
extern void			unblock_sigio   P((void));
extern int			init_clock_sig	P((struct refclockio *));
extern void			init_socket_sig P((int));
extern void			set_signal		P((void));
RETSIGTYPE	sigio_handler	P((int));

# define BLOCKIO()	 ((void) block_sigio())
# define UNBLOCKIO() ((void) unblock_sigio())

#else

# define BLOCKIO()
# define UNBLOCKIO()
#endif /* HAVE_SIGNALED_IO */

#endif
