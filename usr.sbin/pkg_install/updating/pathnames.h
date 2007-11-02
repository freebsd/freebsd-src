/*-
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <beat@chruetertee.ch> wrote this file. As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.          Beat Gätzi
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 *
 */

/* Copy from ../version/version.h */

/* Where the ports lives by default */
#define DEF_PORTS_DIR "/usr/ports/UPDATING"
/* just in case we change the environment variable name */
#define PORTSDIR  "PORTSDIR"
/* macro to get name of directory where we put logging information */
#define UPDATING (getenv(PORTSDIR) ? strcat(getenv(PORTSDIR), \
	"/UPDATING") : DEF_PORTS_DIR)

/* Where we put logging information by default, else ${PKG_DBDIR} if set */
#define DEF_LOG_DIR	"/var/db/pkg"
/* just in case we change the environment variable name */
#define PKG_DBDIR	"PKG_DBDIR"
/* macro to get name of directory where we put logging information */
#define LOG_DIR		(getenv(PKG_DBDIR) ? getenv(PKG_DBDIR) : DEF_LOG_DIR)
