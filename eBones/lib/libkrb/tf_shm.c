/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 * Shared memory segment functions for session keys.  Derived from code
 * contributed by Dan Kolkowitz (kolk@jessica.stanford.edu).
 *
 *	from: tf_shm.c,v 4.2 89/10/25 23:26:46 qjb Exp $
 *	$FreeBSD$
 */

#if 0
#ifndef	lint
static char rcsid[] =
"$FreeBSD$";
#endif	lint
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <krb.h>
#include <des.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_BUFF sizeof(des_cblock)*1000 /* room for 1k keys */

extern int errno;
extern int krb_debug;

/*
 * krb_create_shmtkt:
 *
 * create a shared memory segment for session keys, leaving its id
 * in the specified filename.
 */

int
krb_shm_create(file_name)
char *file_name;
{
    int retval;
    int shmid;
    struct shmid_ds shm_buf;
    FILE *sfile;
    uid_t me, metoo, getuid(), geteuid();

    (void) krb_shm_dest(file_name);	/* nuke it if it exists...
					 this cleans up to make sure we
					 don't slowly lose memory. */

    shmid = shmget((long)IPC_PRIVATE,MAX_BUFF, IPC_CREAT);
    if (shmid == -1) {
	if (krb_debug)
	    perror("krb_shm_create shmget");
	return(KFAILURE);		/* XXX */
    }
    me = getuid();
    metoo = geteuid();
    /*
     * now set up the buffer so that we can modify it
     */
    shm_buf.shm_perm.uid = me;
    shm_buf.shm_perm.gid = getgid();
    shm_buf.shm_perm.mode = 0600;
    if (shmctl(shmid,IPC_SET,&shm_buf) < 0) { /*can now map it */
	if (krb_debug)
	    perror("krb_shm_create shmctl");
	(void) shmctl(shmid, IPC_RMID, 0);
	return(KFAILURE);		/* XXX */
    }
    (void) shmctl(shmid, SHM_LOCK, 0);	/* attempt to lock-in-core */
    /* arrange so the file is owned by the ruid
       (swap real & effective uid if necessary). */
    if (me != metoo) {
	if (setreuid(metoo, me) < 0) {
	    /* can't switch??? barf! */
	    if (krb_debug)
		perror("krb_shm_create: setreuid");
	    (void) shmctl(shmid, IPC_RMID, 0);
	    return(KFAILURE);
	} else
	    if (krb_debug)
		printf("swapped UID's %d and %d\n",metoo,me);
    }
    if ((sfile = fopen(file_name,"w")) == 0) {
	if (krb_debug)
	    perror("krb_shm_create file");
	(void) shmctl(shmid, IPC_RMID, 0);
	return(KFAILURE);		/* XXX */
    }
    if (fchmod(fileno(sfile),0600) < 0) {
	if (krb_debug)
	    perror("krb_shm_create fchmod");
	(void) shmctl(shmid, IPC_RMID, 0);
	return(KFAILURE);		/* XXX */
    }
    if (me != metoo) {
	if (setreuid(me, metoo) < 0) {
	    /* can't switch??? barf! */
	    if (krb_debug)
		perror("krb_shm_create: setreuid2");
	    (void) shmctl(shmid, IPC_RMID, 0);
	    return(KFAILURE);
	} else
	    if (krb_debug)
		printf("swapped UID's %d and %d\n",me,metoo);
    }

    (void) fprintf(sfile,"%d",shmid);
    (void) fflush(sfile);
    (void) fclose(sfile);
    return(KSUCCESS);
}


/*
 * krb_is_diskless:
 *
 * check / to see if file .diskless exists.  If so it is diskless.
 *     Do it this way now to avoid dependencies on a particular routine.
 *      Choose root file system since that will be private to the client.
 */

int krb_is_diskless()
{
	struct stat buf;
	if (stat("/.diskless",&buf) < 0)
		return(0);
	else return(1);
}

/*
 * krb_shm_dest: destroy shared memory segment with session keys, and remove
 * file pointing to it.
 */

int krb_shm_dest(file)
char *file;
{
    int shmid;
    FILE *sfile;
    struct stat st_buf;

    if (stat(file,&st_buf) == 0) {
	/* successful stat */
	if ((sfile = fopen(file,"r")) == 0) {
	    if (krb_debug)
		perror("cannot open shared memory file");
	    return(KFAILURE);		/* XXX */
	}
	if (fscanf(sfile,"%d",&shmid) == 1) {
		if (shmctl(shmid,IPC_RMID,0) != 0) {
		    if (krb_debug)
			perror("krb_shm_dest: cannot delete shm segment");
		    (void) fclose(sfile);
		    return(KFAILURE);	/* XXX */
		}
	} else {
	    if (krb_debug)
		fprintf(stderr, "bad format in shmid file\n");
	    (void) fclose(sfile);
	    return(KFAILURE);		/* XXX */
	}
	(void) fclose(sfile);
	(void) unlink(file);
	return(KSUCCESS);
    } else
	return(RET_TKFIL);		/* XXX */
}



