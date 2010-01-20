/* 
 * sht.c - Testprogram for shared memory refclock
 * read/write shared memory segment; see usage
 */
#ifndef SYS_WINNT
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#else
#include <windows.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream.h>
#define sleep(x) Sleep(x*1000)
#endif
#include <assert.h>
struct shmTime {
	int    mode; /* 0 - if valid set
		      *       use values, 
		      *       clear valid
		      * 1 - if valid set 
		      *       if count before and after read of values is equal,
		      *         use values 
		      *       clear valid
		      */
	int    count;
	time_t clockTimeStampSec;
	int    clockTimeStampUSec;
	time_t receiveTimeStampSec;
	int    receiveTimeStampUSec;
	int    leap;
	int    precision;
	int    nsamples;
	int    valid;
};

struct shmTime *
getShmTime (
	int unit
	)
{
#ifndef SYS_WINNT
	int shmid=shmget (0x4e545030+unit, sizeof (struct shmTime), IPC_CREAT|0777);
	if (shmid==-1) {
		perror ("shmget");
		exit (1);
	}
	else {
		struct shmTime *p=(struct shmTime *)shmat (shmid, 0, 0);
		if ((int)(long)p==-1) {
			perror ("shmat");
			p=0;
		}
		assert (p!=0);
		return p;
	}
#else
	char buf[10];
	LPSECURITY_ATTRIBUTES psec=0;
	sprintf (buf,"NTP%d",unit);
	SECURITY_DESCRIPTOR sd;
	SECURITY_ATTRIBUTES sa;
	HANDLE shmid;

	assert (InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION));
	assert (SetSecurityDescriptorDacl(&sd,1,0,0));
	sa.nLength=sizeof (SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor=&sd;
	sa.bInheritHandle=0;
	shmid=CreateFileMapping ((HANDLE)0xffffffff, 0, PAGE_READWRITE,
				 psec, sizeof (struct shmTime),buf);
	if (!shmid) {
		shmid=CreateFileMapping ((HANDLE)0xffffffff, 0, PAGE_READWRITE,
					 0, sizeof (struct shmTime),buf);
		cout <<"CreateFileMapping with psec!=0 failed"<<endl;
	}

	if (!shmid) {
		char mbuf[1000];
		FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM,
			       0, GetLastError (), 0, mbuf, sizeof (mbuf), 0);
		int x=GetLastError ();
		cout <<"CreateFileMapping "<<buf<<":"<<mbuf<<endl;
		exit (1);
	}
	else {
		struct shmTime *p=(struct shmTime *) MapViewOfFile (shmid, 
								    FILE_MAP_WRITE, 0, 0, sizeof (struct shmTime));
		if (p==0) {
			char mbuf[1000];
			FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM,
				       0, GetLastError (), 0, mbuf, sizeof (mbuf), 0);
			cout <<"MapViewOfFile "<<buf<<":"<<mbuf<<endl;
			exit (1);
		}
		return p;
	}
	return 0;
#endif
}


int
main (
	int argc,
	char *argv[]
	)
{
	volatile struct shmTime *p=getShmTime(2);
	if (argc<=1) {
		printf ("usage: %s r[c][l]|w|snnn\n",argv[0]);
		printf ("       r read shared memory\n");
		printf ("        c clear valid-flag\n");
		printf ("        l loop (so, rcl will read and clear in a loop\n");
		printf ("       w write shared memory with current time\n");
		printf ("       snnnn set nsamples to nnn\n");
		printf ("       lnnnn set leap to nnn\n");
		printf ("       pnnnn set precision to -nnn\n");
		exit (0);
	}
	switch (argv[1][0]) {
	    case 's': {
		    p->nsamples=atoi(&argv[1][1]);
	    }
	    break;
	    case 'l': {
		    p->leap=atoi(&argv[1][1]);
	    }
	    break;
	    case 'p': {
		    p->precision=-atoi(&argv[1][1]);
	    }
	    break;
	    case 'r': {
		    char *ap=&argv[1][1];
		    int clear=0;
		    int loop=0;
		    printf ("reader\n");
		    while (*ap) {
			    switch (*ap) {
				case 'l' : loop=1; break;
				case 'c' : clear=1; break;
			    }
			    ap++;
		    }
		    do {
			    printf ("mode=%d, count=%d, clock=%d.%d, rec=%d.%d,\n",
				    p->mode,p->count,p->clockTimeStampSec,p->clockTimeStampUSec,
				    p->receiveTimeStampSec,p->receiveTimeStampUSec);
			    printf ("  leap=%d, precision=%d, nsamples=%d, valid=%d\n",
				    p->leap, p->precision, p->nsamples, p->valid);
			    if (!p->valid)
				printf ("***\n");
			    if (clear) {
				    p->valid=0;
				    printf ("cleared\n");
			    }
			    if (loop)
				sleep (1);
		    } while (loop);
	    }
	    break;
	    case 'w': {
		    printf ("writer\n");
		    p->mode=0;
		    if (!p->valid) {
			    p->clockTimeStampSec=time(0)-20;
			    p->clockTimeStampUSec=0;
			    p->receiveTimeStampSec=time(0)-1;
			    p->receiveTimeStampUSec=0;
			    printf ("%d %d\n",p->clockTimeStampSec, p->receiveTimeStampSec);
			    p->valid=1;
		    }
		    else {
			    printf ("p->valid still set\n"); /* not an error! */
		    }
	    }
	    break;
	}
}
