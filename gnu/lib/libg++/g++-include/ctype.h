#include <_G_config.h>
extern "C" {
#include_next <ctype.h>
#ifndef toupper
extern int toupper _G_ARGS((int));
#endif
#ifndef tolower
extern int tolower _G_ARGS((int));
#endif
}
