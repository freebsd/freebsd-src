/* File Transfer Protocol Toolkit based on libftp */

#include "uftp.h"
#include <varargs.h>


FTP *ftp[NFRAMES];
LINKINFO iftp[NFRAMES];
int frame=0;


int status;
jmp_buf start;
int lastcmd=0;
int glassmode=0;
int trymode=1;
int restmode=1;
int hashmode=0;
int sleeptime=30;
time_t noopinterval=0;
time_t nooptimeout=1;
time_t prevtime=0;

String cmd;
String prompt="%T %u@%H:%d> ";
String defaultuser;

ALIAS *firstalias=NULL;

/* if main have any arguments, interprets each it as command with args */


main(int argc, char **argv)

{
  register int i;
  register char *p1;
  FILE *fp;
  String tmp;
   
  if (setjmp(start)!=0) 
    goto main_loop;
  
  setsignals();

  
  
  FtpSetErrorHandler(&FtpInit,my_error);
  FtpSetIOHandler(&FtpInit,my_error);
  
  strcpy(defaultuser,getpwuid(getuid())->pw_name);

  
  memset(ftp,0,sizeof(FTP*)*NFRAMES);
  memset(iftp,0,sizeof(LINKINFO)*NFRAMES);
  

  
  batch(SYSTEMRC);

  if (access(getrcname(),F_OK))
    {
      FILE *out=fdopen(open(getrcname(),O_WRONLY|O_CREAT|O_TRUNC,0700),"w");

      printf("Create default rc-file \"%s\"\n",getrcname());

      if (out==NULL)
	  perror(getrcname());

      else
	{
	  
	  fprintf(out,"set timeout 120\nset hash\nset debug\nset bin\n");
	  fprintf(out,"set prompt \"%%T %%u@%%h:%%d\\> \"\n");
	  fprintf(out,"alias a alias\na ed ! emacs\nalias tn ! telnet\n");
	  
	  fclose(out);
	}
    }

      
  batch(getrcname());
  batch(getaliasrcname());


  for (i=1, tmp[0]=0; i< argc; i++)
    {
      strcat(tmp,argv[i]);
      if (i+1!=argc) strcat(tmp," ");
    }
  
  if (tmp[0]!=0)
    {
      String new;

/*
      if (!strcmp(defaultuser,"ftp") || !strcmp(defaultuser,"anonymous"))
	strcpy(new,"ftp ");
      else
*/
      strcpy(new,"open ");
      
      if (ifalias(tmp)) 
	execute (tmp);
      else
	strcat(new,tmp),
	execute(new);
    }


main_loop:

  setsignals();
  
  while (1)
    {
      
      setjmp(start);
      if (lastcmd) exit(0);
      

      if (isatty(fileno(stdin)))
	p1=readline(getprompt());
      else
	p1=gets(cmd);
      
      if (p1==NULL) 
	{
	  putchar('\n');
	  exit(0);
	}
      
      strcpy(cmd,p1);

      if (cmd[0]) add_history(cmd);
      execute(cmd);
    }
}

INLINE char *findspace(char *str)
{
  while ( !isspace(*str) && *str != '\0' ) str++;
  return str;
}


    
char *word(char *str, int n)
{
  String new;
  register char *p1, *p2;
  register int i;
  
  strcpy(new,str);
  
  p1=new;

  while (isspace(*p1)) p1++;

  if (n>1 )
    for (i=0;i<n-1;i++) /* Skip n-1 words */
      {
	if ((*p1=='"')||(*p1=='\'')) 
	  {
	    p1=strchr(p1+1,*p1);
	    if (p1==NULL) return "";
	    p1++;
	    while ( isspace(*p1) ) p1++;
	    continue;
	  }
	p1=findspace(p1);
	if ( *p1=='\0' ) return "";
	p1++;
	while ( isspace(*p1) ) p1++;
      }

  if ((*p1=='"')|(*p1=='\''))
    {
      p2=strchr(p1+1,*p1);
      if (p2==NULL) return p1+1;
      *p2=0;
      return p1+1;
    }
  
  if  ((p2=findspace(p1)) != NULL )
    {
      *p2=0;
      return p1;
    }
  return "";
}


/* Exacute few command separated by ';' . The character ' must use for mark complex 
   works*/  

execute (char *cmd)
{
  String w1,w2,w3,w4,w5,w6;
  String newcmd;
  char *p;

  if (!*cmd || *cmd=='#' ) return;

  for ( p=newcmd ; *cmd; cmd++)
    {
      if ( *cmd == '\'' )
	{
	  *p++=*cmd++;
	  while ( *cmd != '\'' && *cmd != 0 ) *p++=*cmd++;
	  if ( *cmd == 0 ) 
	    return puts("Unbalanced \', please corrected!\n");
	  *p++=*cmd;
	  continue;
	}
      
      if ( *cmd == ';' ) 
	{
	  *p=0;
	  execute(newcmd);
	  p=newcmd;
	  continue;
	}
      *p++=*cmd;
    }

  
  *p=0;
  cmd=newcmd;
  
  if ( *cmd=='\\' ) 
    cmd++;
  else
    {
      String new;
      strcpy(new,"\\");
      strcat(new,expandalias(cmd));
      return execute(new);
    }

  if ( *cmd == '!' ) 
    {
      int pid,_pid;
      union wait status;
      
      if (!(pid=fork()))
	{
	  execlp((getenv("SHELL")==NULL)?"/bin/sh":(char *)getenv("SHELL"),
		 "shell","-c",cmd+1,NULL);
	}
      
      while(1)
	{
	  _pid=wait(&status);
	  if (_pid==pid)
	    return;
	}
    }
  
      
  redir(cmd);

  if (cmd[strlen(cmd)-1]=='&')
    {
      String tmp;

      cmd[strlen(cmd)-1]=0;

      strcpy(tmp,"bg ");
      strcat(tmp,cmd);

      strcpy(cmd,tmp);
    }
  
  strcpy(w1,word(cmd,1));
  strcpy(w2,word(cmd,2));
  strcpy(w3,word(cmd,3));
  strcpy(w4,word(cmd,4));
  strcpy(w5,word(cmd,5));
  strcpy(w6,word(cmd,6));
  
  return executev(w1,w2,w3,w4,w5,w6);
}

executev(ARGS)
{
  CMDS *xcmd=&cmds[0];
  String tmp;
  
  if (isdigit(*w1))
    return 
      atoi(w1)<NFRAMES?frame=atoi(w1):0,
      executev(w2,w3,w4,w5,w6,"");
  
  while ( xcmd -> cmd != NULL )
    {
      if ( !strcmp(xcmd->cmd,w1) && (xcmd -> func != NULL) )
	{
	  int status;
	  
	  if ( xcmd -> need && LINK == NULL)
	    return puts("Need connection to server");
	  iftp[frame].lock=1; unsetsignals();
	  status = (*xcmd->func)(w1,w2,w3,w4,w5,w6);
	  iftp[frame].lock=0; setsignals();
	  redirback();
	  return status;
	}
      xcmd++;
    }
  
  
  if (LINK!=NULL && glassmode)
    return FtpCommand(LINK,cmd,"",0,EOF);
  
  printf("%s: unknown command\n",w1);
  fflush(stdout);
  return -1;
}


void intr(int sig)
{
  printf("Interupted by signal %d\n",sig);
  if (LINK!=NULL) FtpSetHashHandler(LINK,NULL);
  setsignals();
  reset_termio(); /* From readline */
  prevtime = time((time_t *)0);
  longjmp(start,1);
}

newframe(int connecteble)
{
  register int i;
  
  if (connecteble)
    for (i=0; i<NFRAMES; i++) if (ftp[i]!=NULL) return frame=i;
  for (i=0; i<NFRAMES; i++) if (ftp[i]==NULL) return frame=i;
  return -1;
}

STATUS my_error(FTP *ftp, int code, char *msg)
{

  if (code==LQUIT||(ftp==NULL)) log(msg);
  else
    FtpLog(ftp->title,msg);
  
  if ( abs(code) == 530 && (strstr(msg,"anonymous")!=NULL))
    {
      Ftp_reopen();
      longjmp(start,1);
    }
  longjmp(start,1);
}

char *getrcname()
{
  static String rcpath;
  struct passwd *pwd=getpwuid(getuid());
  
  sprintf(rcpath,"%s/.uftprc",pwd->pw_dir);
  return rcpath;
}

char *getaliasrcname()
{
  static String rcpath;
  struct passwd *pwd=getpwuid(getuid());
  
  sprintf(rcpath,"%s/.uftp_aliases",pwd->pw_dir);
  return rcpath;
}

char *makestr(va_alist)
 va_dcl
{
  char *p1;
  va_list args;
  String new={0};
  
  va_start(args);

  while(1)
    {
      p1=va_arg(args,char *);
      if (p1==NULL) break;
      if (*p1!=0) 
	{
	  if (new[0]!=0) strcat(new," ");
	  strcat(new,p1);
	}
    }
  va_end(args);
  return new;
}

  
#define ADD(str,chr) (*str++=chr,*str=0)

INLINE ADDSTR(char **str, char *str1)
{
  while (*str1) *(*str)++=*str1++;
}

char *expandalias(char *str)
{
  ALIAS *a=firstalias;
  String new={0},w1={0};
  char *p,*p1=new,*args;
  int dollar=0;
  
  strcpy(w1,word(str,1));

  if ( (p=strchr(str,' '))!=NULL )
    args=p+1;
  else
    args="";
  
  while (a) 
    {
      if (!strcmp(a->name,w1))
	break;
      a=a->next;
    }
  
  if (!a) 
    return str;
  
  for ( p=a->str; *p; p++)
    {
      if ( *p != '$' ) 
	{
	  ADD(p1,*p);
	  continue;
	}
      
      dollar=1;
      p++;
      
      if (isdigit(*p)) 
	{
	  ADDSTR(&p1,word(str,(*p)-'0'+1));
	  continue;
	}

      switch (*p) 
	{ 
	  
	case '\0':
	case '$':  ADD(p1,'$');continue;
	case '*':  ADDSTR(&p1,args);continue;
	default:   ADD(p1,'$');ADD(p1,*p);continue;
	}
    }
  
  if (!dollar) 
    {
      ADD(p1,' ');
      ADDSTR(&p1,args);
    }

  *p=0;
  
  return new;
}

ifalias(char *cmd)
{
  String what;
  ALIAS *a=firstalias;


  strcpy(what,word(cmd,1));

  while ( a!=NULL)
    {
      if (!strcmp(a->name,what))
	return 1;
      a=a->next;
    }
  return 0;
}
  


char *getprompt()
{
  
  static String _prompt;
  String tmp;
  char *s;

  _prompt[0]=0;

  for(s=prompt;*s;s++)
    switch (*s)
      {
      case '%':
	switch (*++s)
	  {
	  
	  case 'H': 
	    strcat(_prompt,iftp[frame].host);
	    break;
	    
	  case 'h':
	    strcpy(tmp,iftp[frame].host);
	    if (strchr(tmp,'.')!=NULL) *(char *)strchr(tmp,'.')=0;
	    strcat(_prompt,tmp);
	    break;

	  case 'M': 
	    gethostname(tmp, sizeof tmp);
	    strcat(_prompt,gethostbyname(tmp)->h_name);
	    break;
	    
	  case 'm':
	    gethostname(tmp, sizeof tmp);
	    strcpy(tmp,gethostbyname(tmp)->h_name);
	    if (strchr(tmp,'.')!=NULL) *(char *)strchr(tmp,'.')=0;
	    strcat(_prompt,tmp);
	    break;

	  case 'u':
	    strcat(_prompt,iftp[frame].user);
	    break;
	    
	  case 'd':
	    strcat(_prompt,iftp[frame].pwd);
	    break;

	  case 'D':
	    strcat(_prompt,(char *)getcwd(tmp,sizeof(tmp)));
	    break;

	  case 'f':
	    sprintf(tmp,"%d",frame);
	    strcat(_prompt,tmp);
	    break;

	  case 'p':
	    sprintf(tmp,"%d",(LINK==NULL)?0:LINK->port);
	    strcat(_prompt,tmp);
	    break;
	    
	  case 't':
	    
	    sprintf(tmp,"%d",(LINK==NULL)?0:LINK->timeout.tv_sec);
	    strcat(_prompt,tmp);
	    break;
	    

	  case 'T':
	    
	    {
	      time_t t=time((time_t *)0);
	      struct tm *lt=localtime(&t);
	      sprintf(tmp,"%02d:%02d:%02d",lt->tm_hour,
		      lt->tm_min,lt->tm_sec);
	      strcat(_prompt,tmp);
	    }
	    break;

	  case 'P':

	    sprintf(tmp,"%d",getpid());
	    strcat(_prompt,tmp);
	    break;
	    
	  default:
	    sprintf(tmp,"%%%c",*s);
	    strcat(_prompt,tmp);
	    break;
	  }
	break;

      case '^':
	
	++s;
	if (isalpha(*s))
	  {
	    sprintf(tmp,"%c",toupper(*s)-'A'+1);
	    strcat(_prompt,tmp);
	  }
	break;
	
      default:
	
	sprintf(tmp,"%c",*s);
	strcat(_prompt,tmp);
	break;
      }
  return _prompt;
}
	
	    
void noop()
{
  int i;
  time_t curtime,save;
  STATUS (*func1)(),(*func2)(),(*func3)();


  if (noopinterval==0) return;
  
  curtime = time((time_t *)0);
  
  signal(SIGALRM,noop);
  
  if (prevtime==0) 
    {
      prevtime=curtime;
      alarm(noopinterval);
      return;
    }
  
  if (curtime-prevtime < noopinterval) 
    {
      alarm(prevtime+noopinterval-curtime);
      return;
    }
 
  printf("Waiting...");fflush(stdout);
 
  for (i=0;i<NFRAMES;i++)
    {
      if ( ftp[i]==NULL || FTPCMD(ftp[i]) == NULL || iftp[i].lock )
	continue;

      func1=ftp[i]->debug; ftp[i]->debug=NULL;
      func2=ftp[i]->error; ftp[i]->error=NULL;
      func3=ftp[i]->IO; ftp[i]->IO=NULL;
      save = ftp[i]->timeout.tv_sec;
      ftp[i]->timeout.tv_sec = nooptimeout;
      
      FtpCommand(ftp[i],"NOOP","",0,EOF);

      ftp[i]->timeout.tv_sec = save;
      ftp[i]->debug=func1;
      ftp[i]->error=func1;
      ftp[i]->IO=func1;

    }
  
  alarm(noopinterval);
  prevtime=curtime;
  
  for (i=0;i<10;i++) putchar(8),putchar(' '),putchar(8);
  fflush(stdout);
}


setsignals()
{
  signal(SIGINT,intr);
  signal(SIGQUIT,intr);
  noop();
}

unsetsignals()
{
  signal(SIGALRM,SIG_IGN);
  alarm(0);
}


int myhash(FTP *ftp,unsigned int chars)
{
  
  if (hashmode)
    {
      if (chars==0) return ftp -> counter=0;
      ftp -> counter += chars;
      fprintf(stdout,"%10u bytes transfered\r",ftp -> counter);
      fflush(stdout);
    }

  if (!lastcmd) 
    {
      noop();
      alarm(0);
    }
}



char *makefilename(char *f1, char *f2)
{
  char *p;
  
  if (*f2!=0)
    return f2;

  if ( (p=strrchr(f1,'/'))!=NULL)
    return p+1;
  return f1;
}

redir(char *cmdline)
{
  char *p=cmdline;
  String result;
  char *r=result;
  
  for ( ; *p ; p++ , r++ )
    {
      if ( *p == '\\' ) 
	{
	  *r = * ++ p ;
	  continue;
	}
      
      if ( *p == '>' || *p == '<' )
	{
	  String filename;
	  char *q=filename;
	  char c=*p;
	  
	  for (p++;isspace(*p)&&*p!=0;p++);
	  if (*p=='"')
	    {
	      for (p++; *p!='"' && *p!=0 ; p++,q++) *q=*p;
	      if (*p!='"') p++;
	    }
	  else
	    for (; !isspace(*p) && *p!=0 ; p++,q++) *q=*p;
	    
	  *q=0;
	  
	  if ( c == '>' ) 
	    output(filename);
	  else
	    input(filename);
	}
      *r=*p;
    }
  *r=0;
  strcpy(cmdline,result);
}

int itty=-1,otty=-1;
FILE *is=NULL,*os=NULL;


input(char *filename)
{
  
  if ((is=Ftpfopen(filename,"r"))==NULL)
    {
      perror(filename);
      return;
    }

  fflush(stdin);
  itty=dup(0);
  close(0);
  dup2(fileno(is),0);

}

output(char *filename)
{
  
  if ((os=Ftpfopen(filename,"w"))==NULL)
    {
      perror(filename);
      return;
    }
  
  fflush(stdout);
  otty=dup(1);
  close(1);
  dup2(fileno(os),1);
}

redirback()
{

  if (itty!=-1)
    {
      fflush(stdin);
      close(0);
      Ftpfclose(is);
      dup2(itty,0);
      is=NULL;
      itty=-1;
    }

  if (otty!=-1)
    {
      fflush(stdout);
      close(1);
      Ftpfclose(os);
      dup2(otty,1);
      os=NULL;
      otty=-1;
    }
}


batch(char *filename)
{
  FILE *fp;
  String tmp;
  
  if ((fp=fopen(filename,"r"))!=NULL)
    {
      
      while ( fgets(tmp, sizeof tmp, fp) != NULL)
	{
	  tmp[strlen(tmp)-1]=0;
	  execute(tmp);
	  if (tmp[0]) add_history(tmp);
	}
      fclose(fp);
    }
}
  





