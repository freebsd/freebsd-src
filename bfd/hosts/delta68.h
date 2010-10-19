/* Definitions for a Motorola Delta 3300 box running System V R3.0.
   Contributed by manfred@lts.sel.alcatel.de.  */

#include <sys/param.h>

/* Definitions used by trad-core.c.  */
#define	NBPG			NBPC
#define	HOST_DATA_START_ADDR	u.u_exdata.ux_datorg
#define	HOST_TEXT_START_ADDR	u.u_exdata.ux_txtorg
/* User's stack, copied from sys/param.h  */
#define HOST_STACK_END_ADDR	USRSTACK
#define	UPAGES			USIZE
#define	TRAD_UNIX_CORE_FILE_FAILING_SIGNAL(abfd) \
  abfd->tdata.trad_core_data->u.u_abort
