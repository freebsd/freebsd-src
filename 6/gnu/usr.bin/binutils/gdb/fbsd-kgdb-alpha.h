/* $FreeBSD$ */

#ifndef FBSD_KGDB_ALPHA_H
#define FBSD_KGDB_ALPHA_H

#include "alpha/tm-fbsd.h"
#include "fbsd-kgdb.h"

#undef  FRAME_SAVED_PC
#define FRAME_SAVED_PC(FRAME) \
  (kernel_debugging ? fbsd_kern_frame_saved_pc(FRAME) : \
		      alpha_saved_pc_after_call(FRAME))
 
#endif /* FBSD_KGDB_ALPHA_H */
