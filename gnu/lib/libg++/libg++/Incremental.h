#ifndef Incremental_h
#ifdef __GNUG__
#pragma interface
#endif
#define Incremental_h
#define DECLARE_INIT_FUNCTION(USER_INIT_FUNCTION) \
static void USER_INIT_FUNCTION (); extern void (*_initfn)(); \
static struct xyzzy { xyzzy () {_initfn = USER_INIT_FUNCTION;}; \
~xyzzy () {};} __2xyzzy;
#else
#error Incremental.h was not the first file included in this module
#endif
