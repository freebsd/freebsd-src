/* Definitions for an Apple Macintosh running A/UX 3.x. */

#include <sys/param.h>
#include <sys/page.h>

/* Definitions used by trad-core.c.  */
#define	NBPG			NBPP

#define	HOST_DATA_START_ADDR	u.u_exdata.ux_datorg
#define	HOST_TEXT_START_ADDR	u.u_exdata.ux_txtorg
#define	HOST_STACK_END_ADDR	0x100000000

#define	UPAGES			USIZE

#define	TRAD_UNIX_CORE_FILE_FAILING_SIGNAL(abfd) \
  (abfd->tdata.trad_core_data->u.u_arg[0])
