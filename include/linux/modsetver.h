/* Symbol versioning nastiness.  */

#define __SYMBOL_VERSION(x)       __ver_ ## x
#define __VERSIONED_SYMBOL2(x,v)  x ## _R ## v
#define __VERSIONED_SYMBOL1(x,v)  __VERSIONED_SYMBOL2(x,v)
#define __VERSIONED_SYMBOL(x)     __VERSIONED_SYMBOL1(x,__SYMBOL_VERSION(x))

#ifndef _set_ver
#define _set_ver(x)		  __VERSIONED_SYMBOL(x)
#endif
