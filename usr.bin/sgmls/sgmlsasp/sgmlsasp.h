/* sgmlsasp.h */

#include "config.h"
#include "std.h"

#ifdef USE_PROTOTYPES
#define P(parms) parms
#else
#define P(parms) ()
#endif

#ifdef __GNUC__
#define NO_RETURN volatile
#else
#define NO_RETURN /* as nothing */
#endif

#ifdef VARARGS
#define VP(parms) ()
#else
#define VP(parms) P(parms)
#endif

NO_RETURN void error VP((char *,...));

extern int fold_general_names;
