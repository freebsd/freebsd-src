/*
 * XXX: We don't have dlopen & friends in statically linked programs
 * XXX: so we avoid using them.
 */
#ifdef PIC
#include "../../../contrib/tcl/unix/tclLoadDl2.c"
#endif

