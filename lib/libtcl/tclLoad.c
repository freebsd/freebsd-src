/*
 * XXX: We don't have dlopen & friends in statically linked programs
 * XXX: so we avoid using them.
 */
#ifdef PIC
#include "../../../contrib/tcl/generic/tclLoad.c"
#else
#include "../../../contrib/tcl/generic/tclLoadNone.c"
#endif

