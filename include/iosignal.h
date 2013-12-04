#if !defined _ntp_iosignaled_h
#define _ntp_iosignaled_h

#include "ntp_refclock.h"

#if defined(HAVE_SIGNALED_IO)
extern void		block_sigio	(void);
extern void		unblock_sigio	(void);
extern int		init_clock_sig	(struct refclockio *);
extern void		init_socket_sig	(int);
extern void		set_signal	(void);
RETSIGTYPE		sigio_handler	(int);

# define BLOCKIO()	block_sigio()
# define UNBLOCKIO()	unblock_sigio()

#else

# define BLOCKIO()
# define UNBLOCKIO()
#endif /* HAVE_SIGNALED_IO */

#endif
