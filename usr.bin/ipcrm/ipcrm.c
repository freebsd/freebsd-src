/*
 * Implement the SYSV ipcrm command.
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <errno.h>

#define OMIT_SHM		/* don't handle shm stuff */

int
getnumber(char *num,long *lptr)
{
    char *end;

    if ( num == NULL ) {
	fprintf(stderr,"ipcrm:  missing numeric parameter\n");
	return(0);
    }

    *lptr = strtol(num, &end, 0);
    if ( *num == '\0' || *end != '\0' ) {
	fprintf(stderr,"ipcrm:  can't convert \"%s\" to an integer\n",num);
	return(0);
    }
    if ( *lptr == LONG_MAX || *lptr == LONG_MIN ) {
	fprintf(stderr,"ipcrm:  absurd numeric parameter \"%s\"\n",num);
	return(0);
    }
    return(1);
}

main(int argc,char **argv)
{
    char *parm;
    char tbuf[10000];
    int errcnt = 0;
    long id;
    long key;

    while ( (parm = *++argv) != NULL ) {

	if ( strcmp(parm,"-q") == 0 ) {
	    if ( getnumber(*++argv,&id) ) {
		if ( msgctl(id,IPC_RMID,NULL) == 0 ) {
		    printf("msqid %s deleted\n",*argv);
		} else if ( errno == ENOSYS ) {
		    fprintf(stderr,"ipcrm:  SYSV message passing not configured in kernel\n");
		    errcnt += 1;
		} else {
		    strcpy(tbuf,"can't remove msqid ");
		    strcat(tbuf,*argv);
		    perror(tbuf);
		    errcnt += 1;
		}
	    } else {
		errcnt += 1;
	    }
	} else if ( strcmp(parm,"-m") == 0 ) {
	    if ( getnumber(*++argv,&id) ) {
#ifndef OMIT_SHM
		%%% untested %%%
		if ( shmctl(id,IPC_RMID,NULL) == 0 ) {
		    printf("shmid %s deleted\n",*argv);
		} else if ( errno == ENOSYS ) {
		    fprintf(stderr,"ipcrm:  SYSV shared memory not configured in kernel\n");
		    errcnt += 1;
		} else {
		    strcpy(tbuf,"can't remove shmid ");
		    strcat(tbuf,*argv);
		    perror(tbuf);
		    errcnt += 1;
		}
#else
		fprintf(stderr,"ipcrm:  sorry, this version of ipcrm doesn't support shared memory\n");
		errcnt += 1;
#endif
	    } else {
		errcnt += 1;
	    }
	} else if ( strcmp(parm,"-s") == 0 ) {
	    if ( getnumber(*++argv,&id) ) {
		union semun junk;
		if ( semctl(id,0,IPC_RMID,junk) == 0 ) {
		    printf("semid %s deleted\n",*argv);
		} else if ( errno == ENOSYS ) {
		    fprintf(stderr,"ipcrm:  SYSV semaphores not configured in kernel\n");
		    errcnt += 1;
		} else {
		    strcpy(tbuf,"can't remove semid ");
		    strcat(tbuf,*argv);
		    perror(tbuf);
		    errcnt += 1;
		}
	    } else {
		errcnt += 1;
	    }
	} else if ( strcmp(parm,"-Q") == 0 ) {
	    if ( getnumber(*++argv,&key) ) {
		if ( key == IPC_PRIVATE ) {
		    fprintf(stderr,"ipcrm:  can't remove private message queues\n");
		} else {
		    strcpy(tbuf,"can't access msq key ");
		    strcat(tbuf,*argv);
		    if ( (id = msgget(key,0000)) >= 0 ) {
			strcpy(tbuf,"can't remove msq key ");
			strcat(tbuf,*argv);
			if ( msgctl(id,IPC_RMID,NULL) == 0 ) {
			    printf("msq key %s deleted\n",*argv);
			} else {
			    perror(tbuf);
			    errcnt += 1;
			}
		    } else if ( errno == ENOSYS ) {
			fprintf(stderr,"ipcrm:  SYSV message passing not configured in kernel\n");
			errcnt += 1;
		    } else {
			perror(tbuf);
			errcnt += 1;
		    }
		}
	    } else {
		errcnt += 1;
	    }
	} else if ( strcmp(parm,"-M") == 0 ) {
	    if ( getnumber(*++argv,&key) ) {
#ifndef OMIT_SHM
		%%% untested %%%
		if ( key == IPC_PRIVATE ) {
		    fprintf(stderr,"ipcrm:  can't remove private shared memory segments\n");
		} else {
		    strcpy(tbuf,"can't access shm key ");
		    strcat(tbuf,*argv);
		    if ( (id = shmget(key,0,0000)) >= 0 ) {
			strcpy(tbuf,"can't remove shm key ");
			strcat(tbuf,*argv);
			if ( shmctl(id,IPC_RMID,NULL) == 0 ) {
			    printf("shm key %s deleted\n",*argv);
			} else {
			    perror(tbuf);
			    errcnt += 1;
			}
		    } else if ( errno == ENOSYS ) {
			fprintf(stderr,"ipcrm:  SYSV shared memory not configured in kernel\n");
			errcnt += 1;
		    } else {
			perror(tbuf);
			errcnt += 1;
		    }
		}
#else
		fprintf(stderr,"ipcrm:  sorry, this version of ipcrm doesn't support shared memory\n");
		errcnt += 1;
#endif
	    } else {
		errcnt += 1;
	    }
	} else if ( strcmp(parm,"-S") == 0 ) {
	    if ( getnumber(*++argv,&key) ) {
		if ( key == IPC_PRIVATE ) {
		    fprintf(stderr,"ipcrm:  can't remove private semaphores\n");
		} else {
		    strcpy(tbuf,"can't access sem key ");
		    strcat(tbuf,*argv);
		    if ( (id = semget(key,0,0000)) >= 0 ) {
			union semun junk;
			strcpy(tbuf,"can't remove sem key ");
			strcat(tbuf,*argv);
			if ( semctl(id,0,IPC_RMID,junk) == 0 ) {
			    printf("sem key %s deleted\n",*argv);
			} else {
			    perror(tbuf);
			    errcnt += 1;
			}
		    } else if ( errno == ENOSYS ) {
			fprintf(stderr,"ipcrm:  SYSV semaphores not configured in kernel\n");
			errcnt += 1;
		    } else {
			perror(tbuf);
			errcnt += 1;
		    }
		}
	    } else {
		errcnt += 1;
	    }
	} else {
	    fprintf(stderr,"ipcrm:  illegal parameter \"%s\" - bye!\n",parm);
	    exit(1);
	}

    }

    if ( errcnt == 0 ) {
	exit(0);
    } else {
	exit(1);
    }
}
