/* varargs.h. */
#ifndef __va_list__
#define __va_list__
typedef char *va_list;
#endif
#define va_dcl int va_alist;
#define va_start(list) list = (char *) &va_alist
#define va_end(list)
#define va_arg(list,mode) ((mode *)(list += sizeof(mode)))[-1]
