/*
 * Simplified implementation of SYSV ipcs.
 */

#include <nlist.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <paths.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h>
#define KERNEL
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>

static kmem_fd;

getsymbol(struct nlist *symbols, char *symname, void *dptr, int len)
{
    int i, rlen;

    for ( i = 0; symbols[i].n_name != NULL; i += 1 ) {
	if ( strcmp(symbols[i].n_name,symname) == 0 ) {
	    break;
	}
    }

    if ( symbols[i].n_name == NULL ) {
	fprintf(stderr,"ipcs(getsymbol):  symbol %s not in local symbols list\n",
	symname);
	exit(1);
    }

    if ( symbols[i].n_value == NULL ) {
	fprintf(stderr,"ipcs(getsymbol):  symbol %s not in %s\n",
	symname,_PATH_UNIX);
	return(0);
    }

    if ( kmem_fd == 0 ) {
	kmem_fd = open("/dev/kmem",0);
	if ( kmem_fd < 0 ) {
	    perror("ipcs(getsymbol(open /dev/kmem))");
	    exit(1);
	}
    }

    lseek(kmem_fd,symbols[i].n_value,SEEK_SET);
    if ( (rlen = read(kmem_fd,dptr,len)) != len ) {
	fprintf(stderr,"ipcs(getsymbol):  can't fetch symbol %s from /dev/kmem\n",symname);
	exit(1);
    }
    return(1);
}

void
getlocation(void *addr, void *dptr, int len)
{
    int i, rlen;

    if ( kmem_fd == 0 ) {
	kmem_fd = open("/dev/kmem",0);
	if ( kmem_fd < 0 ) {
	    perror("ipcs(getlocation(open /dev/kmem))");
	    exit(1);
	}
    }

    lseek(kmem_fd,(long)addr,SEEK_SET);
    if ( (rlen = read(kmem_fd,dptr,len)) != len ) {
	fprintf(stderr,"ipcs(getlocation):  can't fetch location %08x from /dev/kmem\n",addr);
	exit(1);
    }
}

char *
fmt_perm(ushort mode)
{
    static char buffer[100];

    buffer[0]  = '-';
    buffer[1]  = '-';
    buffer[2]  = ((mode & 0400) ? 'r' : '-');
    buffer[3]  = ((mode & 0200) ? 'w' : '-');
    buffer[4]  = ((mode & 0100) ? 'a' : '-');
    buffer[5]  = ((mode & 0040) ? 'r' : '-');
    buffer[6]  = ((mode & 0020) ? 'w' : '-');
    buffer[7]  = ((mode & 0010) ? 'a' : '-');
    buffer[8]  = ((mode & 0004) ? 'r' : '-');
    buffer[9]  = ((mode & 0002) ? 'w' : '-');
    buffer[10] = ((mode & 0001) ? 'a' : '-');
    buffer[11] = '\0';
    return(&buffer[0]);
}

void
cvt_time(time_t t,char *buf)
{
    struct tm tms;
    static char *months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    if ( t == 0 ) {
	strcpy(buf,"<not set>");
    } else {
	tms = *localtime(&t);
	if ( t > time(0) - 6 * 30 * 24 * 3600 ) {	/* less than about 6 months ago? */
	    sprintf(buf,"%s %2d %2d:%2d",
	    months[tms.tm_mon],tms.tm_mday,tms.tm_hour,tms.tm_min);
	} else {
	    sprintf(buf,"%s %2d %5d",
	    months[tms.tm_mon],tms.tm_mday,tms.tm_year+1900);
	}
    }
}

main()
{
    static struct nlist symbols[] = {
	{ "_sema" },
	{ "_seminfo" },
	{ "_semu" },
	{ "_msginfo" },
	{ "_msqids" },
	{ NULL }
    };
    int i;
    int show_sem_values = 1;
    int show_undo_values = 1;

    switch ( nlist(_PATH_UNIX,&symbols[0]) ) {
    case 0: break;
    case -1:
	fprintf(stderr,"ipcs:  can't open %s - bye!\n",_PATH_UNIX);
	exit(1);
    default:
	fprintf(stderr,"ipcs:  nlist failed\n");
	for ( i = 0; symbols[i].n_name != NULL; i += 1 ) {
	    if ( symbols[i].n_value == 0 ) {
		fprintf(stderr,"\tsymbol %s not found\n",symbols[i].n_name);
	    }
	}
	break;
    }

    /*
    for ( i = 0; symbols[i].n_name != NULL; i += 1 ) {
	fprintf(stderr,"\t%s : %08x\n",symbols[i].n_name,symbols[i].n_value);
    }
    */

    if ( getsymbol(symbols,"_seminfo",&seminfo,sizeof(seminfo)) ) {
	struct semid_ds *xsema;

	printf("seminfo:\n");
	    printf("\tsemmap: %6d\t(# of entries in semaphore map)\n",seminfo.semmap);
	    printf("\tsemmni: %6d\t(# of semaphore identifiers)\n",seminfo.semmni);
	    printf("\tsemmns: %6d\t(# of semaphores in system)\n",seminfo.semmns);
	    printf("\tsemmnu: %6d\t(# of undo structures in system)\n",seminfo.semmnu);
	    printf("\tsemmsl: %6d\t(max # of semaphores per id)\n",seminfo.semmsl);
	    printf("\tsemopm: %6d\t(max # of operations per semop call)\n",seminfo.semopm);
	    printf("\tsemume: %6d\t(max # of undo entries per process)\n",seminfo.semume);
	    printf("\tsemusz: %6d\t(size in bytes of undo structure)\n",seminfo.semusz);
	    printf("\tsemvmx: %6d\t(semaphore maximum value)\n",seminfo.semvmx);
	    printf("\tsemaem: %6d\t(adjust on exit max value)\n",seminfo.semaem);

	/*
	 * Lock out other users of the semaphore facility
	 */

	if ( semconfig(SEM_CONFIG_FREEZE) != 0 ) {
	    perror("semconfig");
	    fprintf(stderr,"Can't lock semaphore facility - winging it...\n");
	}

	getsymbol(symbols,"_sema",&sema,sizeof(sema));
	xsema = malloc(sizeof(struct semid_ds) * seminfo.semmni);
	getlocation(sema,xsema,sizeof(struct semid_ds) * seminfo.semmni);

	for ( i = 0; i < seminfo.semmni; i += 1 ) {
	    if ( (xsema[i].sem_perm.mode & SEM_ALLOC) != 0 ) {
		char ctime_buf[100], otime_buf[100];
		struct semid_ds *semaptr = &xsema[i];
		cvt_time(semaptr->sem_ctime,ctime_buf);
		cvt_time(semaptr->sem_otime,otime_buf);

		printf("\nsema id:  %d  key:  0x%08x:\n",
		IXSEQ_TO_IPCID(i,semaptr->sem_perm),
		semaptr->sem_perm.key);

		printf("     cuid:  %6d    cgid:  %6d    ctime:  %s\n",
		semaptr->sem_perm.cuid,semaptr->sem_perm.cgid,ctime_buf);

		printf("     uid:   %6d    gid:   %6d    otime:  %s\n",
		semaptr->sem_perm.uid,semaptr->sem_perm.gid,otime_buf);

		printf("     nsems: %6d                     perm:   %s\n",
		semaptr->sem_nsems,fmt_perm(semaptr->sem_perm.mode));

		if ( show_sem_values ) {
		    int j, value;
		    union semun junk;
		    for ( j = 0; j < semaptr->sem_nsems; j += 1 ) {
			if ( (value = semctl( IXSEQ_TO_IPCID(i,semaptr->sem_perm), j, GETVAL, junk )) < 0 ) {
			    printf("can't get semaphore values\n");
			    break;
			}
			if ( j % 5 == 0 ) {
			    if ( j == 0 ) {
				printf("     values: {");
			    } else {
				printf("\n");
				printf("              ");
			    }
			}
			printf(" %d",value);
			if ( j == semaptr->sem_nsems - 1 ) {
			    printf(" }\n");
			} else {
			    printf(", ");
			}
		    }
		}

	    }

	}

	if ( show_undo_values ) {
	    int j;
	    int *ksemu, *semu;
	    int semu_size;
	    int got_one_undo = 0;

	    semu = 0;
	    semu_size = (int)SEMU(seminfo.semmnu);
	    semu = (int *)malloc( semu_size );
	    getsymbol(symbols,"_semu",&ksemu,sizeof(ksemu));
	    getlocation(ksemu,semu,semu_size);

	    printf("\nsem undos:\n");
	    for ( j = 0; j < seminfo.semmnu; j += 1 ) {
		struct sem_undo *suptr;
		int k;

		suptr = SEMU(j);
		if ( suptr->un_proc != NULL ) {
		    struct proc proc;
		    getlocation(suptr->un_proc,&proc,sizeof(proc));
		    got_one_undo = 1;
		    printf("     pid %d:  semid  semnum  adjval\n",proc.p_pid);
		    for ( k = 0; k < suptr->un_cnt; k += 1 ) {
			printf("          %10d   %5d  %6d\n",
			IXSEQ_TO_IPCID(suptr->un_ent[k].un_id,xsema[suptr->un_ent[k].un_id].sem_perm),
			suptr->un_ent[k].un_num,
			suptr->un_ent[k].un_adjval);
		    }
		}
	    }
	    if ( !got_one_undo ) {
		printf("     none allocated\n");
	    }
	}

	(void)semconfig(SEM_CONFIG_THAW);

    } else {
	fprintf(stderr,"SVID semaphores facility not configured in the system\n");
    }

    if ( getsymbol(symbols,"_msginfo",&msginfo,sizeof(msginfo)) ) {
	struct msqid_ds *xmsqids;

	printf("\nmsginfo:\n");
	    printf("\tmsgmax: %6d\t(max characters in a message)\n",msginfo.msgmax);
	    printf("\tmsgmni: %6d\t(# of message queues)\n",msginfo.msgmni);
	    printf("\tmsgmnb: %6d\t(max characters in a message queue)\n",msginfo.msgmnb);
	    printf("\tmsgtql: %6d\t(max # of messages in system)\n",msginfo.msgtql);
	    printf("\tmsgssz: %6d\t(size of a message segment)\n",msginfo.msgssz);
	    printf("\tmsgseg: %6d\t(# of message segments in system)\n",msginfo.msgseg);

	getsymbol(symbols,"_msqids",&msqids,sizeof(msqids));
	xmsqids = malloc(sizeof(struct msqid_ds) * msginfo.msgmni);
	getlocation(msqids,xmsqids,sizeof(struct msqid_ds) * msginfo.msgmni);

	for ( i = 0; i < msginfo.msgmni; i += 1 ) {
	    if ( xmsqids[i].msg_qbytes != 0 ) {
		char stime_buf[100], rtime_buf[100], ctime_buf[100];
		struct msqid_ds *msqptr = &xmsqids[i];

		cvt_time(msqptr->msg_stime,stime_buf);
		cvt_time(msqptr->msg_rtime,rtime_buf);
		cvt_time(msqptr->msg_ctime,ctime_buf);

		printf("\nmsgq id:  %d  key:  0x%08x\n",
		IXSEQ_TO_IPCID(i,msqptr->msg_perm),
		msqptr->msg_perm.key);

		printf("     cuid:  %6d    cgid:  %6d    ctime:  %s\n",
		msqptr->msg_perm.cuid,msqptr->msg_perm.cgid,ctime_buf);

		printf("     uid:   %6d    gid:   %6d\n",
		msqptr->msg_perm.uid,msqptr->msg_perm.gid);

		printf("     lspid: %6d                     stime:  %s\n",
		msqptr->msg_lspid,stime_buf);

		printf("     lrpid: %6d    qnum:  %6d    rtime:  %s\n",
		msqptr->msg_lrpid,msqptr->msg_qnum,rtime_buf);

		printf("     cbytes:%6d    qbytes:%6d    perm:   %s\n",
		msqptr->msg_cbytes,msqptr->msg_qbytes,fmt_perm(msqptr->msg_perm.mode));

	    }
	}

    } else {
	fprintf(stderr,"SVID messages facility not configured in the system\n");
    }

    exit(0);
}
