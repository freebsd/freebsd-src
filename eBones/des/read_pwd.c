/* read_pwd.c */
/* Copyright (C) 1993 Eric Young - see README for more details */
/* 06-Apr-92 Luke Brennan    Support for VMS */

/*-
 *	$Id: read_pwd.c,v 1.1.1.1 1994/09/30 14:49:51 csgr Exp $
 */

#include "des_locl.h"
#include <string.h>
#include <signal.h>
#include <setjmp.h>

#include <sys/param.h>

#ifdef BSD
#include <pwd.h>
extern char * getpass(const char * prompt);
#endif

#ifndef VMS
#ifndef MSDOS
#ifndef _IRIX
#ifdef CRAY
#include <termio.h>
#define sgttyb termio
#define sg_flags c_lflag
#else /* !CRAY */
#include <sgtty.h>
#endif
#include <sys/ioctl.h>
#else /* _IRIX */
struct  sgttyb {
	char    sg_ispeed;              /* input speed */
	char    sg_ospeed;              /* output speed */
	char    sg_erase;               /* erase character */
	char    sg_kill;                /* kill character */
	short   sg_flags; /* mode flags */
	};
#endif
#else /* MSDOS */
#define fgets(a,b,c) noecho_fgets(a,b,c)
#ifndef NSIG
#define NSIG 32
#endif
#endif
#else /* VMS */
#include <ssdef.h>
#include <iodef.h>
#include <ttdef.h>
#include <descrip.h>
struct IOSB {
	short iosb$w_value;
	short iosb$w_count;
	long  iosb$l_info;
	};
#endif

static void read_till_nl();
static int read_pw();
static void recsig();
static void pushsig();
static void popsig();
#ifdef MSDOS
static int noecho_fgets();
#endif

static void (*savsig[NSIG])();
static jmp_buf save;

int des_read_password(key,prompt,verify)
des_cblock *key;
char *prompt;
int verify;
	{
	int ok;
	char buf[BUFSIZ],buff[BUFSIZ];

	if ((ok=read_pw(buf,buff,BUFSIZ,prompt,verify)) == 0)
		des_string_to_key(buf,key);
	bzero(buf,BUFSIZ);
	bzero(buff,BUFSIZ);
	return(ok);
	}

int des_read_2passwords(key1,key2,prompt,verify)
des_cblock *key1;
des_cblock *key2;
char *prompt;
int verify;
	{
	int ok;
	char buf[BUFSIZ],buff[BUFSIZ];

	if ((ok=read_pw(buf,buff,BUFSIZ,prompt,verify)) == 0)
		des_string_to_2keys(buf,key1,key2);
	bzero(buf,BUFSIZ);
	bzero(buff,BUFSIZ);
	return(ok);
	}

#if defined(BSD)
int des_read_pw_string(buf, length, prompt, verify)
	char *buf;
	int length;
	char * prompt;
	int verify;
{
	int len = MIN(_PASSWORD_LEN, length);
	char * s;
	int ok = 0;

	fflush(stdout);
	while (!ok) {
		s = getpass(prompt);
		strncpy(buf, s, len);
		if(verify) {
			printf("Verifying password\n"); fflush(stdout);
			if(strncmp(getpass(prompt), buf, len) != 0) {
				printf("\nVerify failure - try again\n");
				fflush(stdout);
				continue;
			}
		}
		ok = 1;
		buf[len-1] = '\0';
	}
	return (!ok);
}

#else /* BSD */

int des_read_pw_string(buf,length,prompt,verify)
char *buf;
int length;
char *prompt;
int verify;
	{
	char buff[BUFSIZ];
	int ret;

	ret=read_pw(buf,buff,(length>BUFSIZ)?BUFSIZ:length,prompt,verify);
	bzero(buff,BUFSIZ);
	return(ret);
	}
#endif

static void read_till_nl(in)
FILE *in;
	{
#define SIZE 4
	char buf[SIZE+1];

	do	{
		fgets(buf,SIZE,in);
		} while (index(buf,'\n') == NULL);
	}

/* return 0 if ok, 1 (or -1) otherwise */
static int read_pw(buf,buff,size,prompt,verify)
char *buf,*buff;
int size;
char *prompt;
int verify;
	{
#ifndef VMS
#ifndef MSDOS
	struct sgttyb tty_orig,tty_new;
#endif /* !MSDOS */
#else
	struct IOSB iosb;
	$DESCRIPTOR(terminal,"TT");
	long tty_orig[3], tty_new[3];
	long status;
	unsigned short channel = 0;
#endif
	int ok=0;
	char *p;
	int ps=0;
	FILE *tty;

#ifndef MSDOS
	if ((tty=fopen("/dev/tty","r")) == NULL)
		tty=stdin;
#else /* MSDOS */
	if ((tty=fopen("con","r")) == NULL)
		tty=stdin;
#endif /* MSDOS */
#ifndef VMS
#ifdef TIOCGETP
	if (ioctl(fileno(tty),TIOCGETP,(char *)&tty_orig) == -1)
		return(-1);
	bcopy(&(tty_orig),&(tty_new),sizeof(tty_orig));
#endif
#else /* VMS */
	status = SYS$ASSIGN(&terminal,&channel,0,0);
	if (status != SS$_NORMAL)
		return(-1);
	status=SYS$QIOW(0,channel,IO$_SENSEMODE,&iosb,0,0,tty_orig,12,0,0,0,0);
	if ((status != SS$_NORMAL) || (iosb.iosb$w_value != SS$_NORMAL))
		return(-1);
#endif

	if (setjmp(save))
		{
		ok=0;
		goto error;
		}
	pushsig();
	ps=1;
#ifndef VMS
#ifndef MSDOS
	tty_new.sg_flags &= ~ECHO;
#endif /* !MSDOS */
#ifdef TIOCSETP
	if (ioctl(fileno(tty),TIOCSETP,(char *)&tty_new) == -1)
		return(-1);
#endif
#else /* VMS */
	tty_new[0] = tty_orig[0];
	tty_new[1] = tty_orig[1] | TT$M_NOECHO;
	tty_new[2] = tty_orig[2];
	status = SYS$QIOW(0,channel,IO$_SETMODE,&iosb,0,0,tty_new,12,0,0,0,0);
	if ((status != SS$_NORMAL) || (iosb.iosb$w_value != SS$_NORMAL))
		return(-1);
#endif /* VMS */
	ps=2;

	fflush(stdout);
	fflush(stderr);
	while (!ok)
		{
		fputs(prompt,stderr);
		fflush(stderr);

		buf[0]='\0';
		fgets(buf,size,tty);
		if (feof(tty)) goto error;
		if ((p=(char *)index(buf,'\n')) != NULL)
			*p='\0';
		else	read_till_nl(tty);
		if (verify)
			{
			fprintf(stderr,"\nVerifying password %s",prompt);
			fflush(stderr);
			buff[0]='\0';
			fgets(buff,size,tty);
			if (feof(tty)) goto error;
			if ((p=(char *)index(buff,'\n')) != NULL)
				*p='\0';
			else	read_till_nl(tty);
				
			if (strcmp(buf,buff) != 0)
				{
				fprintf(stderr,"\nVerify failure - try again\n");
				fflush(stderr);
				continue;
				}
			}
		ok=1;
		}

error:
	fprintf(stderr,"\n");
	/* What can we do if there is an error? */
#ifndef VMS
#ifdef TIOCSETP
	if (ps >= 2) ioctl(fileno(tty),TIOCSETP,(char *)&tty_orig);
#endif
#else /* VMS */
	if (ps >= 2)
		status = SYS$QIOW(0,channel,IO$_SETMODE,&iosb,0,0
			,tty_orig,12,0,0,0,0);
#endif /* VMS */
	
	if (ps >= 1) popsig();
	if (stdin != tty) fclose(tty);
#ifdef VMS
	status = SYS$DASSGN(channel);
#endif
	return(!ok);
	}

static void pushsig()
	{
	int i;

	for (i=0; i<NSIG; i++)
		savsig[i]=signal(i,recsig);
	}

static void popsig()
	{
	int i;

	for (i=0; i<NSIG; i++)
		signal(i,savsig[i]);
	}

static void recsig()
	{
	longjmp(save,1);
	}

#ifdef MSDOS
static int noecho_fgets(buf,size,tty)
char *buf;
int size;
FILE *tty;
	{
	int i;
	char *p;

	p=buf;
	for (;;)
		{
		if (size == 0)
			{
			*p='\0';
			break;
			}
		size--;
		i=getch();
		if (i == '\r') i='\n';
		*(p++)=i;
		if (i == '\n')
			{
			*p='\0';
			break;
			}
		}
	}
#endif
