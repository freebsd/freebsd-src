#ifndef __XSCALE_MACH_H__
#define __XSCALE_MACH_H__

/* These are predefined by new versions of GNU cpp.  */

#ifndef __USER_LABEL_PREFIX__
#define __USER_LABEL_PREFIX__ _
#endif

#ifndef __REGISTER_PREFIX__
#define __REGISTER_PREFIX__
#endif

/* ANSI concatenation macros.  */

#define CONCAT1(a, b) CONCAT2(a, b)
#define CONCAT2(a, b) a##b

/* Use the right prefix for global labels.  */

#define SYM(x) CONCAT1(__USER_LABEL_PREFIX__, x)

#define PRELOAD(X) pld	[X]
#define PRELOADSTR(X) "	pld	[" X "]"

#endif /* !__XSCALE_MACH_H__ */
