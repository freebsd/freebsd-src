/* Definitions that are needed for core files.  Core section sizes for
   the DPX2 are in bytes.  */

#include <sys/param.h>
#define NBPG 1
#define UPAGES (USIZE * NBPP)
#define HOST_DATA_START_ADDR (u.u_exdata.ux_datorg)
#define HOST_STACK_END_ADDR (USERSTACK)
