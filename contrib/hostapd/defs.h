#ifndef DEFS_H
#define DEFS_H

#ifdef CONFIG_NATIVE_WINDOWS
#ifdef FALSE
#undef FALSE
#endif
#ifdef TRUE
#undef TRUE
#endif
#endif /* CONFIG_NATIVE_WINDOWS */
typedef enum { FALSE = 0, TRUE = 1 } Boolean;

#endif /* DEFS_H */
