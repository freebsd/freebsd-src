/*
 * ldexp() and scalbn() are defined to be identical, but ldexp() lives in libc
 * for backwards compatibility.
 */
#define scalbn ldexp
#include "../../msun/src/s_scalbn.c"
#undef scalbn
