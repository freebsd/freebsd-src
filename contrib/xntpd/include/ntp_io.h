/*
 * POSIX says use <fnct.h> to get O_* symbols and 
 * SEEK_SET symbol form <untisd.h>.
 */
#if defined(NTP_POSIX_SOURCE)

/*
 * POSIX way
 */
#include <stdio.h>
#if defined(HAVE_SIGNALED_IO) && (defined(SYS_AUX2) || defined(SYS_AUX3) || defined(SYS_PTX))
#include <sys/file.h>
#endif
#include <unistd.h>
#include <fcntl.h>
#else
/*
 * BSD way
 */
#include <sys/file.h>
#include <fcntl.h>
#if !defined(SEEK_SET) && defined(L_SET)
#define SEEK_SET L_SET
#endif
#endif
