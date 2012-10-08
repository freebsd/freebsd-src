/*	$FreeBSD$	*/

#ifndef _UTIL_H_
#define _UTIL_H_

#include <libutil.h>

char	*flags_to_string(u_long flags, const char *def);
int	 string_to_flags(char **stringp, u_long *setp, u_long *clrp);

#endif	/* _UTIL_H_ */
