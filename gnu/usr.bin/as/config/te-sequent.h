/*
 * This file is te-sequent.h and is intended to set up emulation with
 * sequent's development tools.
 *
 */

#define TE_SEQUENT 1

 /* sequent has a "special" header. */
#define H_GET_HEADER_SIZE(h)	(128)

#ifdef TC_I386
 /* zmagic is 0x22eb */
#define DEFAULT_MAGIC_NUMBER_FOR_OBJECT_FILE (0x12eb)
#endif /* TC_I386 */

#ifdef TC_NS32K
 /* zmagic is 0x10ea */
#define DEFAULT_MAGIC_NUMBER_FOR_OBJECT_FILE (0x00ea)
#endif /* TC_NS32K */

/* these define interfaces */
#include "obj-format.h"

/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */

/* end of te-sequent.h */
