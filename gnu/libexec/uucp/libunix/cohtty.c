/* Coherent tty locking support.  This file was contributed by Bob
   Hemedinger <bob@dalek.mwc.com> of Mark Williams Corporation and
   lightly edited by Ian Lance Taylor.  */

/* The bottom part of this file is lock.c.
 * This is a hacked lock.c. A full lock.c can be found in the libmisc sources 
 * under /usr/src/misc.tar.Z.
 *
 * These are for checking for the existence of locks:
 * lockexist(resource)
 * lockttyexist(ttyname)
 */

#include "uucp.h"

#if HAVE_COHERENT_LOCKFILES

/* cohtty.c:	Given a serial device name, read /etc/ttys and determine if
 *		the device is already enabled. If it is, disable the
 *		device and return a string so that it can be re-enabled
 * 		at the completion of the uucico session as part of the
 *		function that resets the serial device before uucico
 *		terminates.
 *
 */

#include "uudefs.h"
#include "sysdep.h"

#include <ctype.h>
#include <access.h>

/* fscoherent_disable_tty() is a COHERENT specific function. It takes the name
 * of a serial device and then scans /etc/ttys for a match. If it finds one,
 * it checks the first field of the entry. If it is a '1', then it will disable
 * the port and set a flag. The flag will be checked later when uucico wants to
 * reset the serial device to see if the device needs to be re-enabled.
 */

boolean
fscoherent_disable_tty (zdevice, pzenable)
     const char *zdevice;
     char **pzenable;
{


struct ttyentry{			/* this is an /etc/ttys entry */
	char enable_disable[1];
	char remote_local[1];
	char baud_rate[1];
	char tty_device[16];
};

struct ttyentry sought_tty;

int x,y,z;				/* dummy */
FILE *	infp;				/* this will point to /etc/ttys */
char disable_command[66];		/* this will be the disable command
					 * passed to the system.
					 */
char enable_device[16];			/* this will hold our device name
					 * to enable.
					 */

	*pzenable = NULL;

	strcpy(enable_device,"");	/* initialize our strings */
	strcpy(sought_tty.tty_device,"");

	if( (infp = fopen("/etc/ttys","r")) == NULL){
		ulog(LOG_ERROR,"Error: check_disable_tty: failed to open /etc/ttys\n");
		return FALSE;
	}

	while (NULL !=(fgets(&sought_tty, sizeof (sought_tty), infp ))){
		sought_tty.tty_device[strlen(sought_tty.tty_device) -1] = '\0';
		strcpy(enable_device,sought_tty.tty_device);

		/* we must strip away the suffix to the com port name or
		 * we will never find a match. For example, if we are passed
		 * /dev/com4l to call out with and the port is already enabled,
		 * 9/10 the port enabled will be com4r. After we strip away the
		 * suffix of the port found in /etc/ttys, then we can test
		 * if the base port name appears in the device name string
		 * passed to us.
		 */

		for(z = strlen(sought_tty.tty_device) ; z > 0 ; z--){
			if(isdigit(sought_tty.tty_device[z])){
				break;
			}
		}
		y = strlen(sought_tty.tty_device);
		for(x = z+1 ; x <= y; x++){
			sought_tty.tty_device[x] = '\0';
		}


/*		ulog(LOG_NORMAL,"found device {%s}\n",sought_tty.tty_device); */
		if(strstr(zdevice, sought_tty.tty_device)){
			if(sought_tty.enable_disable[0] == '1'){
				ulog(LOG_NORMAL, "coh_tty: Disabling device %s {%s}\n",
					zdevice, sought_tty.tty_device);
					sprintf(disable_command, "/etc/disable %s",enable_device);
				{
				  pid_t ipid;
				  const char *azargs[3];
				  int aidescs[3];

				  azargs[0] = "/etc/disable";
				  azargs[1] = enable_device;
				  azargs[2] = NULL;
				  aidescs[0] = SPAWN_NULL;
				  aidescs[1] = SPAWN_NULL;
				  aidescs[2] = SPAWN_NULL;
				  ipid = ixsspawn (azargs, aidescs, TRUE,
						   FALSE,
						   (const char *) NULL, TRUE,
						   TRUE,
						   (const char *) NULL,
						   (const char *) NULL,
						   (const char *) NULL);
				  if (ipid < 0)
				    x = 1;
				  else
				    x = ixswait ((unsigned long) ipid,
						 (const char *) NULL);
				}
				*pzenable = zbufalc (sizeof "/dev/"
						     + strlen (enable_device));
				sprintf(*pzenable,"/dev/%s", enable_device);
/*				ulog(LOG_NORMAL,"Enable string is {%s}",*pzenable); */
				return(x==0? TRUE : FALSE); /* disable either failed
							   or succeded */
			}else{
				return FALSE;	/* device in tty entry not enabled */
			}
		}
	}
	return FALSE;	/* no ttys entry found */
}

/* The following is COHERENT 4.0 specific. It is used to test for any
 * existing lockfiles on a port which would have been created by init
 * when a user logs into a port.
 */

#define LOCKSIG		9	/* Significant Chars of Lockable Resources.  */
#define LOKFLEN		64	/* Max Length of UUCP Lock File Name.	     */

#define	LOCKPRE	"LCK.."
#define PIDLEN	6	/* Maximum length of string representing a pid.  */

#ifndef LOCKDIR
#define LOCKDIR SPOOLDIR
#endif

/* There is a special version of DEVMASK for the PE multiport driver
 * because of the peculiar way it uses the minor device number.  For
 * all other drivers, the lower 5 bits describe the physical port--
 * the upper 3 bits give attributes for the port.
 */

#define PE_DRIVER 21	/* Major device number for the PE driver.  */
#define PE_DEVMASK 0x3f	/* PE driver minor device mask.  */
#define DEVMASK 0x1f	/* Minor device mask.  */

/*
 * Generates a resource name for locking, based on the major number
 * and the lower 4 bits of the minor number of the tty device.
 *
 * Builds the name in buff as two "." separated decimal numbers.
 * Returns NULL on failure, buff on success.
 */
static char *
gen_res_name(path, buff)
char *path;
char *buff;
{
	struct stat sbuf;
	int status;
	
	if (0 != (status = stat(path, &sbuf))) {
		/* Can't stat the file.  */
		return (NULL);
	}

	if (PE_DRIVER == major(sbuf.st_rdev)) {
		sprintf(buff, "%d.%d", major(sbuf.st_rdev),
				       PE_DEVMASK & minor(sbuf.st_rdev));
	} else {
		sprintf(buff, "%d.%d", major(sbuf.st_rdev),
				       DEVMASK & minor(sbuf.st_rdev));
	}

	return(buff);
} /* gen_res_name */

/*
 *  lockexist(resource)  char *resource;
 *
 *  Test for existance of a lock on the given resource.
 *
 *  Returns:  (1)  Resource is locked.
 *	      (0)  Resource is not locked.
 */

static boolean
lockexist(resource)
const char	*resource;
{
	char lockfn[LOKFLEN];

	if ( resource == NULL )
		return(0);
	sprintf(lockfn, "%s/%s%.*s", LOCKDIR, LOCKPRE, LOCKSIG, resource);

	return (!access(lockfn, AEXISTS));
} /* lockexist() */

/*
 *  lockttyexist(ttyname)  char *ttyname;
 *
 *  Test for existance of a lock on the given tty.
 *
 *  Returns:  (1)  Resource is locked.
 *	      (0)  Resource is not locked.
 */
boolean
lockttyexist(ttyn)
const char *ttyn;
{
	char resource[LOKFLEN];
	char filename[LOKFLEN];

	sprintf(filename, "/dev/%s", ttyn);
	if (NULL == gen_res_name(filename, resource)){
		return(0);	/* Non-existent tty can not be locked :-) */
	}

	return(lockexist(resource));
} /* lockttyexist() */

#endif /* HAVE_COHERENT_LOCKFILES */
