/*  $Revision: 1.1 $
**
**  Editline system header file for Unix.
*/

#define CRLF		"\r\n"
#define FORWARD		STATIC

#include <sys/types.h>
#include <sys/stat.h>

#if	defined(USE_DIRENT)
#include <dirent.h>
typedef struct dirent	DIRENTRY;
#else
#include <sys/dir.h>
typedef struct direct	DIRENTRY;
#endif	/* defined(USE_DIRENT) */

#if	!defined(S_ISDIR)
#define S_ISDIR(m)		(((m) & S_IFMT) == S_IFDIR)
#endif	/* !defined(S_ISDIR) */
