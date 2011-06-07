/*
  This software is available to you under a choice of one of two
  licenses.  You may choose to be licensed under the terms of the GNU
  General Public License (GPL) Version 2, available at
  <http://www.fsf.org/copyleft/gpl.html>, or the OpenIB.org BSD
  license, available in the LICENSE.TXT file accompanying this
  software.  These details are also available at
  <http://openib.org/license.html>.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
  ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

  Copyright (c) 2004 Topspin Communications.  All rights reserved.
  Copyright (c) 2006 Mellanox Technologies Ltd. All rights reserved.

  $Id$
*/

/*
 * system includes
 */
#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <limits.h>

/*
 * SDP specific includes
 */
#include "libsdp.h"

extern char *program_invocation_short_name;

typedef enum
{
	SDP_LOG_FILE,
	SDP_LOG_SYSLOG,
} __sdp_log_type_t;

/* --------------------------------------------------------------------- */
/* library static and global variables                                   */
/* --------------------------------------------------------------------- */
int __sdp_min_level = 9;
static __sdp_log_type_t __sdp_log_type = SDP_LOG_FILE;
static FILE *__sdp_log_file = NULL;

void
__sdp_log(
	int level,
	char *format,
	... )
{
	va_list ap;
	char extra_format[512];
	time_t timeval;
	char timestr[32];

	if ( level < __sdp_min_level ) {
		return;
	}

	va_start( ap, format );
	switch ( __sdp_log_type ) {
	case SDP_LOG_SYSLOG:
		sprintf( extra_format, "%s[%d] libsdp %s ",
					program_invocation_short_name, getpid(  ), format );
		vsyslog( LOG_USER | LOG_NOTICE, extra_format, ap );
		break;
	case SDP_LOG_FILE:
		timeval = time(NULL);
#ifdef SOLARIS_BUILD
		ctime_r(&timeval, timestr, sizeof timestr);
#else
                ctime_r(&timeval, timestr);
#endif
		timestr[strlen(timestr)-1] = '\0';
		sprintf( extra_format, "%s %s[%d] libsdp %s ",
					timestr, program_invocation_short_name,
					getpid(  ), format );
		if ( __sdp_log_file == NULL ) {
			vfprintf( stderr, extra_format, ap );
#if 0									  /* might slow everything too much? */
			( void )fflush( stderr );
#endif
		} else {
			vfprintf( __sdp_log_file, extra_format, ap );
#if 0									  /* might slow everything too much? */
			( void )fflush( __sdp_log_file );
#endif
		}
		break;
	}
	va_end( ap );
}

int
__sdp_log_get_level(
	void )
{
	return ( __sdp_min_level );
}

void
__sdp_log_set_min_level(
	int level )
{
	__sdp_min_level = level;
}

static void
__sdp_log_set_log_type(
	__sdp_log_type_t type )
{
	if ( __sdp_log_file != NULL ) {
		fclose( __sdp_log_file );
		__sdp_log_file = NULL;
	}

	__sdp_log_type = type;
}

int
__sdp_log_set_log_stderr(
	void )
{
	__sdp_log_set_log_type( SDP_LOG_FILE );
	/* NULL means stderr */

	return 1;
}

int
__sdp_log_set_log_syslog(
	void )
{
	__sdp_log_set_log_type( SDP_LOG_SYSLOG );

	return 1;
}

int
__sdp_log_set_log_file(
	char *filename )
{
	FILE *f;
	uid_t uid;
	struct stat lstat_res;
	int status;

	char *p, tfilename[PATH_MAX + 1];

	/* Strip off any paths from the filename */
	p = strrchr( filename, '/' );
	
	/* 
		base on the active user ID we either use /var/log for root or
		append the uid to the name
	*/
	uid = geteuid();
	if (uid == 0) {
		if ( p ) 
			filename = p + 1;
		snprintf( tfilename, sizeof(tfilename), "/var/log/%s", filename );
	} else {
		char tdir[PATH_MAX + 1];
		/* 
			for regular user, allow log file to be placed in a user
			requested path. If no path is requested the log file is
			placed in /tmp/
		*/ 
		if ( p ) 
			snprintf(tdir, sizeof(tdir), "%s.%d", filename, uid );
		else
			snprintf(tdir, sizeof(tdir ), "/tmp/%s.%d", filename, uid );

		if (mkdir(tdir, 0700)) {
			struct stat stat;

			if (errno != EEXIST) {
				__sdp_log( 9, "Couldn't create directory '%s' for logging (%m)\n", tdir );
				return 0;
			}

			if (lstat(tdir, &stat)) {
				__sdp_log(9, "Couldn't lstat directory %s\n", tdir);
				return 0;
			}

			if (!S_ISDIR(stat.st_mode) || stat.st_uid != uid ||
					(stat.st_mode & ~(S_IFMT | S_IRWXU))) {
				__sdp_log( 9, "Cowardly refusing to log into directory:'%s'. " 
					  "Make sure it is not: (1) link, (2) other uid, (3) bad permissions."
					  "thus is a security issue.\n", tdir );
				return 0;
			}
		}

		snprintf(tfilename, sizeof(tfilename), "%s/log", tdir);
		printf("dir: %s file: %s\n", tdir, tfilename);
	}

	/* double check the file is not a link */
	status = lstat(tfilename, &lstat_res);
	if ( (status == 0) && S_ISLNK(lstat_res.st_mode) ) {
		__sdp_log( 9, "Cowardly refusing to log into:'%s'. " 
					  "It is a link - thus is a security issue.\n", tfilename );
		return 0; 
	}
		
	f = fopen( tfilename, "a" );
	if ( !f ) {
		__sdp_log( 9, "Couldn't open '%s' for logging (%m)\n", tfilename );
		return 0;
	}

	__sdp_log_set_log_type( SDP_LOG_FILE );
	__sdp_log_file = f;

	return 1;
}
