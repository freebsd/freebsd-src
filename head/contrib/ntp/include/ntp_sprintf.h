/*
 * Handle ancient char* *s*printf*() systems
 */

#ifdef SPRINTF_CHAR
# define SPRINTF(x)	strlen(sprintf/**/x)
# define SNPRINTF(x)	strlen(snprintf/**/x)
# define VSNPRINTF(x)	strlen(vsnprintf/**/x)
#else
# define SPRINTF(x)	((size_t)sprintf x)
# define SNPRINTF(x)	((size_t)snprintf x)
# define VSNPRINTF(x)	((size_t)vsnprintf x)
#endif
