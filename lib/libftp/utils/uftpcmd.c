#include "uftp.h"

String tmp;

jmp_buf connectstack;



Ftp_connect_hook(FTP *ftp,int code, char *msg)
{
  FtpLog(ftp->title,msg);
  longjmp(connectstack,1);
  
}
  

Ftp_connect(ARGS) 
{
  STATUS (*func1)(),(*func2)();

  if (*w2==0) w2=readline("Host:");

  if (FtpGetHost(w2)==NULL && FtpInit.IO != NULL) 
    {
      char *msg;
      extern int h_errno;
      
      switch(h_errno)
	{
	case HOST_NOT_FOUND:	  msg = "Host unknown";	  break;
	case TRY_AGAIN:	          msg = "Hostname lookup failure";break;
	default:	          msg = "gethostbyname failure";	  
	}
      return (*FtpInit.IO)(LINK,QUIT,msg);
    }
  
  newframe(0);
  
  func1 = FtpInit.error;
  func2 = FtpInit.IO;
  
  if (trymode) 
    {
      FtpSetErrorHandler(&FtpInit,Ftp_connect_hook);
      FtpSetIOHandler(&FtpInit,Ftp_connect_hook);
      setjmp(connectstack);
    }
  
  FtpConnect(&LINK,w2);
  strcpy(iftp[frame].host,FtpGetHost(w2)->h_name);
  
  FtpSetErrorHandler(LINK,func1);
  FtpSetErrorHandler(&FtpInit,func1);
  FtpSetIOHandler(LINK,func2);
  FtpSetIOHandler(&FtpInit,func2);
  
  return;
}

Ftp_user(ARGS)
{
  
  if (*w2==0)
    {
      sprintf(tmp,"login (default %s):",defaultuser);
      w2=readline(tmp);
      if (*w2==0) 
	w2=defaultuser;
    }
  strcpy(iftp[frame].user,w2);
  strcpy(iftp[frame].pass,w3);
  if (FtpUser(LINK,w2)==331) Ftp_pass("",w3,"","","","");
}

Ftp_pass(ARGS)
{
  if (*w2==0) 
    {
      String pass;
      String host;
  
      gethostname(host, sizeof host);
      strcpy(host,FtpGetHost(host)->h_name);
      sprintf(pass,"%s@%s",getpwuid(getuid())->pw_name,host);
      sprintf(tmp,"Password (default %s):",pass);
      w2=getpass(tmp);
      if (*w2==0) w2=pass;
    }
  
  strcpy(iftp[frame].pass,w2);
  FtpPassword(LINK,w2);
  strcpy(iftp[frame].pwd,FtpPwd(LINK));
}

Ftp_open(ARGS)
{
  register char *p1;
  STATUS (*err)();
  
  Ftp_connect("",w2,"","","","");
  Ftp_user("",w3,w4,"","","" );
  if (ifalias("autologin")) execute("autologin");
  if (*w5) 
    Ftp_cd("cd",w5,"","","","");
}

Ftp_reopen(ARGS)
{
  String host,user,pass,pwd;

  strcpy(host,iftp[frame].host);
  strcpy(user,iftp[frame].user);
  strcpy(pass,iftp[frame].pass);
  strcpy(pwd,iftp[frame].pwd);

  Ftp_close("","","","","","");
  Ftp_open("",host,user,pass,pwd,"");
}

Ftp_ftp(ARGS)
{
  String pass;
  String host;
  
  gethostname(host, sizeof host);
  strcpy(host,FtpGetHost(host)->h_name);
  sprintf(pass,"%s@%s",getpwuid(getuid())->pw_name,host);
  
  Ftp_open("",w2,"anonymous",pass,w3,"");
  return;
}

Ftp_quit(ARGS)
{
  exit(0);
}

Ftp_mput_handler()
{
  log("File(s) or directory not found");
  longjmp(start,1);
}


Ftp_lcd(ARGS)
{
  glob_t gl;

  bzero(&gl,sizeof gl);
 
  glob(w2,GLOB_BRACE|GLOB_TILDE|GLOB_QUOTE,
       Ftp_mput_handler, &gl);
  
  
  if (gl.gl_matchc<1 || chdir(*gl.gl_pathv))
    perror(w2);
  
  printf("Local directory is \"%s\"\n",getwd(tmp));
}

Ftp_dir(ARGS)
{
  char *cmd1;
  String cmd;
  char mode=LINK->mode;
  
  if ( !strcmp(w1,"dir") ) 
    cmd1="LIST";
  else
    cmd1="NLST";
  
  strcpy(cmd,makestr(cmd1,w2,w3,w4,w5,w6,NULL));

  if ( LINK->mode != 'A' ) FtpAscii(LINK);
  FtpRetr(LINK,cmd,"","-");
  if ( LINK->mode != mode ) FtpType(LINK,mode);
  
}

Ftp_close(ARGS)
{
  register int i;
  
  if (isdigit(*w2)) 
    {
      i=atoi(w2);
    }
  else 
    {
      i=frame;
    }
  
  FtpQuickBye(ftp[i]);
  
  iftp[i].host[0]=0;
  iftp[i].pwd[0]=0;
  ftp[i]=NULL;
  newframe(1);
}


INLINE SetLogicalVar(char arg, int * var, char *msg)
{
  switch(arg)
    {
      
    case 'y':
      
      *var=1;
      break;
      
    case 'n':
      
      *var=0;
      break;
      
    default:
      
      (*var)?(*var=0):(*var=1);
      break;
    }
  return printf("%s %s\n",msg,(*var)?"enable":"disable");
}


Ftp_set(ARGS)
{
  if (!strcmp("frame",w2))
    return atoi(w3)<NFRAMES?frame=atoi(w3):0;
  
  if (!strcmp("timeout",w2))
    {
      FtpSetTimeout(&FtpInit,atoi(w3));
      if (LINK!=0)
	FtpSetTimeout(LINK,atoi(w3));
      return;
    }
  
  if (!strcmp("sleep",w2))
    {
      sleeptime=atoi(w3);
      return;
    }
  
  if (!strcmp("debug",w2))
    {
      if ( *w3=='y' || *w3==0) 
	{ 
	  if (LINK!=NULL) FtpSetDebugHandler(LINK,FtpDebugDebug);
	  FtpSetDebugHandler(&FtpInit,FtpDebugDebug);
	  return puts("Debuging on for current and next session");
	}
      if ( *w3 == 'n')
	{
	  if (LINK!=NULL) FtpSetDebugHandler(LINK,NULL);
	  FtpSetDebugHandler(&FtpInit,NULL);
	  return puts("Debuging off for current and next session");
	}
    }
  
  if (!strcmp("bin",w2))
    {
      if ( *w3=='y' || *w3==0) 
	{ 
	  FtpInit.mode='I';
	  return puts("Binary mode enable");
	}
      if ( *w3 == 'n')
	{
	  FtpInit.mode='A';
	  return puts("Binary mode disable");
	}
    }
  
  if (!strcmp("try",w2))
    return SetLogicalVar(*w3,&trymode,"Try mode");
  if (!strcmp("hash",w2))
    return SetLogicalVar(*w3,&hashmode,"Hash mode");
  if (!strcmp("glass",w2))
    return SetLogicalVar(*w3,&glassmode,"Glass mode");
  if (!strcmp("rest",w2)||!strcmp(w2,"restore"))
    return SetLogicalVar(*w3,&restmode,"Restore mode");
  
  if (!strcmp("prompt",w2))
    {
      strcpy(prompt,w3);
      return;
    }
  
  if (!strcmp("port",w2))
    {
      if ( isdigit(*w3)) 
	return FtpSetPort(&FtpInit,atoi(w3));
      puts("Port must be number");
      fflush(stdout);
      return;
    }

  if (!strcmp("noopinterval",w2)||!strcmp("noop",w2))
    {
      if ( isdigit(*w3)) 
	return noopinterval=(time_t)atoi(w3);
      puts("Time must be number");
      fflush(stdout);
      return;
    }

  if (!strcmp("nooptimeout",w2))
    {
      if ( isdigit(*w3)) 
	return nooptimeout=(time_t)atoi(w3);
      puts("Time must be number");
      fflush(stdout);
      return;
    }

      

  if (!strcmp("user",w2)||!strcmp("login",w2))
    {
      strcpy(defaultuser,w3);
      return;
    }
  
  if (!strcmp("",w2))
    {

      printf("frime %d\n",frame);
      printf("timeout %d secs\n",FtpInit.timeout.tv_sec);
      printf("sleep time %d secs\n",sleeptime);
      printf("debug %s\n",(FtpInit.debug!=NULL)?"enable":"disable");
      printf("glass mode %s\n",glassmode?"enable":"disable");
      printf("try mode %s\n",trymode?"enable":"disable");
      printf("hash mode %s\n",hashmode?"enable":"disable");
      printf("restore mode %s\n",restmode?"enable":"disable");
      printf("automatic binary mode %s\n",(FtpInit.mode=='I')?"enable":"disable");
      printf("prompt  \"%s\"\n",prompt);
      printf("port %d\n",FtpInit.port);
      printf("noop interval %d\n",noopinterval);
      printf("noop timeout %d\n",nooptimeout);
      printf("Default login name \"%s\"\n",defaultuser);
      fflush(stdout);
      return;
    }
  return puts("arg 2 unknown"); 
  
}

jmp_buf getstack;

Ftp_get_hook(FTP *con,int code, char *msg)
{

  if ( abs(code)==550 && FtpBadReply550(msg) ) 
    {
      FtpLog(con->title,msg);
      log("Transfer cancel");
      longjmp(getstack,2);
    }
  
  if ( code == LQUIT )
    {
      log(msg);
      log("Transfer leave after I/O error with local file");
      longjmp(getstack,2);
    }


  
  FtpLog(con->title,msg);
  FtpQuickBye(LINK);
  LINK=NULL;

  log("sleeping......");
  sleep(sleeptime);
  log("try again...");
  
  longjmp(getstack,1);
  
}
  
void Ftp_get_intr(int sig)
{
  signal(SIGINT,intr);
  log("Transfer interupt");
  Ftp_abort();
  longjmp(getstack,3);
}

Ftp_get(ARGS)
{
  FTP OldInit;
  int back=0;
  int code;
  int status=0;
  
  OldInit=FtpInit;
  
  if (strstr(w1,"put")!=NULL) back=1;
  
  
  if (restmode || ((*w1=='r')&&(*(w1+1)=='e')) )
    FtpSetFlag(LINK,FTP_REST);
  else
    FtpClearFlag(LINK,FTP_REST);    
  
  
  
  if (trymode)
    {
      FtpSetErrorHandler(LINK,Ftp_get_hook);
      FtpSetIOHandler(LINK,Ftp_get_hook);
      FtpInit=*LINK;
      FTPCMD(&FtpInit)=FTPCMD(&OldInit);
      FTPDATA(&FtpInit)=FTPDATA(&OldInit);
    }
  
  signal(SIGINT,Ftp_get_intr);
  FtpSetHashHandler(LINK,NULL);
  
  
  if ((status=setjmp(getstack))==2||status==3) 
    goto done;
  
  if ((LINK==NULL)||(LINK->sock==FtpInit.sock))
    {
      FtpLogin(&LINK,iftp[frame].host,iftp[frame].user,
	       iftp[frame].pass,NULL);	  
      FtpChdir(LINK,iftp[frame].pwd);
    }
  
  if (hashmode && isatty(fileno(stdout)))
    FtpSetHashHandler(LINK,myhash);
  else
    FtpSetHashHandler(LINK,NULL);
    
  myhash(LINK,0);
  
  if (!back)
    FtpGet(LINK,w2,makefilename(w2,w3));
  else
    FtpPut(LINK,w2,makefilename(w2,w3));


  log("Transfer compliete");
  
  
done:
  
  
  FtpSetHashHandler(LINK,NULL);
  FtpSetErrorHandler(LINK,my_error);
  FtpSetIOHandler(LINK,my_error);
  FtpClearFlag(LINK,FTP_REST);    
  FtpInit=OldInit;
}

Ftp_mget(ARGS)
{
  FILE *list;
  char mode=LINK->mode;
  
  sprintf(tmp,"/tmp/uftplist-%s.XXXXXX",getpwuid(getuid())->pw_name);
  mktemp(tmp);

  if ( LINK->mode != 'A' ) FtpAscii(LINK);

  if (*w2==0)
    FtpRetr(LINK,"NLST","",tmp);
  else
    FtpRetr(LINK,"NLST %s",w2,tmp);

  if ( LINK->mode != mode ) FtpType(LINK,mode);

  if ((list=fopen(tmp,"r"))==NULL)
    return FtpLog(tmp,sys_errlist[errno]);

  while ( fgets(tmp,sizeof tmp,list)!=NULL)
    {
      tmp[strlen(tmp)-1]=0;
      Ftp_get("get",tmp,w3,"","","");
    }

  fclose(list);
}

Ftp_mput(ARGS)
{
  glob_t gl;
 
  glob(w2,GLOB_BRACE|GLOB_TILDE|GLOB_QUOTE,
       Ftp_mput_handler, &gl);
  
  while(gl.gl_matchc--)
    {
      Ftp_get("put",*gl.gl_pathv,"","","","");
      gl.gl_pathv++;
    }
}


Ftp_bget(ARGS)
{
  
  String fn,lfn;
  char *p;

  newframe(0);
  
  if (!FtpFullSyntax(w2,iftp[frame].host,iftp[frame].user,
		     iftp[frame].pass,fn))
    return puts("Filename syntax error");
  
  strcpy(iftp[frame].pwd,"/");
  
  if ((p=strrchr(fn,'/'))!=NULL) 
    strcpy(lfn,p+1);
  else
    strcpy(lfn,fn);
  
  FtpQuickBye(LINK);
  LINK=FtpCreateObject();
  LINK->sock=NULL;
  
  Ftp_get("get",fn,(*w3==0)?lfn:w3,"","","");
}

Ftp_bput(ARGS)
{
  
  String fn,lfn;
  char *p;

  if (!FtpFullSyntax((*w3==0)?w2:w3,iftp[frame].host,iftp[frame].user,
		     iftp[frame].pass,fn))
    return puts("Filename syntax error");
  
  strcpy(iftp[frame].pwd,"/");
  
  if ((p=strrchr(fn,'/'))!=NULL) 
    strcpy(lfn,p+1);
  else
    strcpy(lfn,fn);
  
  FtpQuickBye(LINK);
  LINK=FtpCreateObject();
  LINK->sock=NULL;
  
  Ftp_get("put",(*w3==0)?lfn:w2,fn,"","","");
}

Ftp_copy(ARGS)
{
  char *p;
  int in,out;
  
  if ( !*w2 || !*w3 ) return puts("Must pass two args");
  
  if ((p=strchr(w2,'!'))!=NULL) 
    {
      *p=0;
      in=atoi(w2);
      w2=p+1;
    }
  else
    in=frame;

  if ((p=strchr(w3,'!'))!=NULL) 
    {
      *p=0;
      out=atoi(w3);
      w3=p+1;
    }
  else
    in=frame;
  
  if (in==out) return puts("Files must been from different frames");
  
  FtpCopy(ftp[in],ftp[out],w2,w3);
}

Ftp_ccopy(ARGS)
{
  char *p;
  int in,out;
  
  if ( !*w2 || !*w3 ) return puts("Must pass two args");
  
  if ((p=strchr(w2,'!'))!=NULL) 
    {
      *p=0;
      in=atoi(w2);
      w2=p+1;
    }
  else
    in=frame;

  if ((p=strchr(w3,'!'))!=NULL) 
    {
      *p=0;
      out=atoi(w3);
      w3=p+1;
    }
  else
    in=frame;
  
  if (in==out) return puts("Files must been from different frames");
  
  FtpPassiveTransfer(ftp[in],ftp[out],w2,w3);
}

Ftp_bin(ARGS)
{
  FtpBinary(LINK);
}

Ftp_ascii(ARGS)
{
  FtpAscii(LINK);
}

Ftp_cd(ARGS)
{
  FtpChdir(LINK,w2);
  strcpy(iftp[frame].pwd,FtpPwd(LINK));
  if (ifalias("autocd")) execute("autocd");
}


Ftp_dup(ARGS)
{ 
  LINKINFO oldinfo;
  FTP oldftp;
  
  oldinfo=iftp[frame];
  oldftp=*LINK;
  
  newframe(0);
  puts("Make alternative connection...");
  Ftp_open("",oldinfo.host,oldinfo.user,oldinfo.pass,"","");
  if (strcmp(oldinfo.pwd,iftp[frame].pwd)) 
    Ftp_cd("",oldinfo.pwd,"","","","");
  if (LINK->mode!=oldftp.mode)
    FtpType(LINK,oldftp.mode);
  LINK -> timeout = oldftp.timeout;
  LINK -> flags = oldftp.flags;
  FtpSetDebugHandler(LINK,oldftp.debug);
  FtpSetErrorHandler(LINK,oldftp.error);
  FtpSetIOHandler(LINK,oldftp.IO);
  FtpSetHashHandler(LINK,oldftp.hash);
}
  

  
Ftp_bg(ARGS)
{
  if (fork())
    {
      
      log("Backgrounding...");
      return;
    }
  else
    {
      int i=frame;
  
      lastcmd=1;
     
      /* Ignoring keypad */

      alarm (0);
      signal(SIGALRM,SIG_IGN);
      signal(SIGURG,SIG_IGN);
      signal(SIGPIPE,SIG_IGN);
      signal(SIGTSTP,SIG_IGN);
      signal(SIGINT,SIG_IGN);
      signal(SIGQUIT,SIG_IGN);
      signal(SIGCHLD,SIG_IGN);
      signal(SIGIO,SIG_IGN);

      /* Droping output */

  
      sprintf(tmp,"/tmp/uftp-%s.XXXXXX",getpwuid(getuid())->pw_name);
      mktemp(tmp);
      close(0);close(1);close(2);
      open(tmp,O_RDWR|O_TRUNC|O_CREAT,0600);
      dup(0);dup(0);

      if (LINK!=NULL)
	{
	  Ftp_dup("","","","","","");
	  free(ftp[i]);
	  ftp[i]=NULL;
	}
      
      return executev(w2,w3,w4,w5,w6,"");
    }
}
  

Ftp_list()
{
  register int i;
      
#define _FMT "%-5s %-15s %-10s %-25s %-7s %-4s\n" 
#define  FMT "%-5d %-15s %-10s %-25s %-7d %-4d\n" 
      
  printf(_FMT,"Frame","Host name","User's name","Working directory","Timeout","Port");
  
  for ( i = 0 ; i < NFRAMES ; i++ )
    if (ftp[i]!=NULL)
      printf(FMT,i,iftp[i].host,iftp[i].user,iftp[i].pwd,
	     ftp[i]->timeout.tv_sec,ftp[i]->port);
  fflush(stdout);
  return;
}

Ftp_abort(ARGS)
{
  time_t save;

  if (LINK!=NULL)
    {
      save = LINK ->timeout.tv_sec;
      LINK->timeout.tv_sec = nooptimeout;
      FtpAbort(LINK);
      LINK->timeout.tv_sec = save;
    }
}

Ftp_type(ARGS)
{
  FtpGet(LINK,w2,"*STDOUT*");
}


Ftp_page(ARGS)
{
  register char *pager;
  String out={'|',0};
  
  if ((pager=(char *)getenv("PAGER"))==NULL) 
    pager="more";
 
  strcat(out,pager);
  FtpGet(LINK,w2,out);
}


Ftp_mkdir(ARGS)
{
  FtpMkdir(LINK,w2);
}

Ftp_rm(ARGS)
{
  FILE *list;

  sprintf(tmp,"/tmp/uftplist-%s.XXXXXX",getpwuid(getuid())->pw_name);
  mktemp(tmp);
  
  if (*w2==0) 
    {
      log("Filename specification must present");
      return;
    }
  
  FtpRetr(LINK,"NLST %s",w2,tmp);
  
  if ((list=fopen(tmp,"r"))==NULL)
    return FtpLog(tmp,sys_errlist[errno]);
  
  while ( fgets(tmp,sizeof tmp,list)!=NULL)
    {
      tmp[strlen(tmp)-1]=0;
      FtpCommand(LINK,"DELE %s",tmp,0,EOF);
    }
  
  fclose(list);
}




Ftp_move(ARGS)
{
  FtpMove(LINK,w2,w3);
}

Ftp_help(ARGS)
{
  register int i,ii;
 
  if ( !*w2 )
    {
      puts("Warrning!!!! \nPlease read general information using command \"help etc\"\n\n");

      for ( i = 0 ;1; i++)
      {
	for ( ii = 0 ; ii < 5 ; ii++)
	  {
	    if (cmds[ii+i*5].cmd==NULL) return putchar('\n');
	    printf("%-16s",cmds[ii+i*5].cmd);
	  }
	putchar ('\n');
      }
    }
  

  for ( i = 0 ; 1; i++)
    {

      if (cmds[i].cmd==NULL) return puts("Command not found");
      if (!strcmp(cmds[i].cmd,w2))
	break;
    }

  puts(cmds[i].help);
      
}

Ftp_quote(ARGS)
{
  String new;
  
  new[0]=0;

  if (*w2!=0) strcpy(new,w2);
  if (*w3!=0) strcat(new," "),strcat(new,w3);
  if (*w4!=0) strcat(new," "),strcat(new,w4);
  if (*w5!=0) strcat(new," "),strcat(new,w5);
  if (*w6!=0) strcat(new," "),strcat(new,w6);

  FtpCommand(LINK,new,"",0,EOF);
}

Ftp_alias(ARGS)
{
  ALIAS *a=firstalias;
  
  
  if ( *w2==0 )
    {
      while (a!=NULL)
	{
	  printf("%s=%s\n",a->name,a->str);
	  a=a->next;
	}
      return;
    }


  while (1) 
    {

      if ( a == NULL )
	{ 
	  firstalias = a = (ALIAS *) malloc(sizeof(ALIAS));
	  memset(a,0,sizeof(ALIAS));
	  a -> next = NULL;
	  break;
	}
      
      if (!strcmp(a->name,w2))
	break;
      
      if ( a->next == NULL)
	{
	  a -> next = (ALIAS *) malloc(sizeof(ALIAS));
	  a = a->next;
	  memset(a,0,sizeof(ALIAS));
	  a -> next = NULL;
	  break;
	}
      a=a->next;
    }
  
  strcpy(a -> name,w2);
  strcpy(a -> str,makestr(w3,w4,w5,w6,NULL));
}

Ftp_mkalias(ARGS)
{
  String new;

  if (!*w2) return puts("Arg must present\n");

  sprintf(new,"open \"%s\" \"%s\" \"%s\" \"%s\"",
	  iftp[frame].host,iftp[frame].user,
	  iftp[frame].pass,iftp[frame].pwd);
  
  Ftp_alias("alias",w2,new,"","","");
}

Ftp_unalias(ARGS)
{
  ALIAS *cur,*prev;

  cur=prev=firstalias;

  while ( cur != NULL )
    {
      if (!strcmp(cur->name,w2))
	{
	  if ( cur == firstalias )
	    {
	      firstalias = cur->next;
	      free(cur);
	      return;
	    }
	  prev -> next = cur -> next;
	  free(cur);
	}
      prev=cur;
      cur=cur->next;
    }
}


Ftp_save(ARGS)
{
  ALIAS *a=firstalias;
  String fn;
  FILE *out;

  if ((out=fopen (getaliasrcname(),"w"))==NULL)
    {
      perror(getaliasrcname());
      return;
    }

  while (a!=NULL)
    {
      fprintf(out,"alias %s '%s'\n",a->name,a->str);
      a=a->next;
    }
  fclose(out);
  chmod ( getaliasrcname(), 0600);
  puts("Aliases saved");
}

#define ARCHIE_MAX_TARGETS 20

Ftp_acd(ARGS)
{
  static int targets=0;
  static String what={0};
  static ARCHIE result[ARCHIE_MAX_TARGETS];
  
  int i, selected_target;
  String tmp;
  char *p;

  
  if ( (what[0] == 0 || strcmp(w2,what) != 0) && *w2!=0 )
    {
      if ((targets=FtpArchie(w2,result,ARCHIE_MAX_TARGETS))<1) 
	return puts("Archie failure or target not found");
      strcpy(what,w2);
    }
  
  for (i=0;i<targets;i++)
    printf("%2d %s:%s\n",i,result[i].host,result[i].file);
  
  if (strcmp(w1,"archie")==0)
    return;
  
  
  p = readline("Your selection? ");
  if (p==NULL) return;
  
  selected_target = atoi(p);
  
  if ( result[selected_target].file[strlen(result[selected_target].file)-1]
      == '/' )
    {
      Ftp_ftp("ftp",result[selected_target].host,result[selected_target].file,
	      "","","");
      return;
    }
  else
    {
      sprintf(tmp,"%s:%s",
	      result[selected_target].host,result[selected_target].file);
      Ftp_bget("bget",tmp,"","","","");
    }
}


CMDS cmds[]={
  
  "connect",		Ftp_connect,      0,
  "connect <hostname> - make new ftp connection",

  "open",		Ftp_open,         0,
  "open <hostname> <user> <pass> <directory> - login to server",
  
  "reopen",		Ftp_reopen,       1,
  "Open again connection with existing frame information",
  
  "ftp",		Ftp_ftp,          0,
  "ftp <hostname> - anonymously login to server",
  
  "close",		Ftp_close,        1,
  "Close connection",
  
  "quit",		Ftp_quit,         0,
  "Exit from uftp",
  
  "set",		Ftp_set,          0,
"Set variables: (Without args print current settings)\n\
       frame <number>    - select another session(frame)\n\
       timeout <secs>    - Set network timeout\n\
       nooptimeout <secs>- Set network timeout with clearing timeout\n\
       noop <secs>       - Set time interval for sending NOOP operator\n\
                           to server for erased delay\n\
       sleep <secs>      - Set pause beetween transfer attempt\n\
       debug <y|n>       - Set debuging ftp's protocol (Default no)\n\
       try <y|n>         - Set retransfer mode with broken network (Default yes)\n\
       hash <y|n>        - Set hashing transfer (Default no)\n\
       restore <y|n>     - Set retransfer mode (reget/reput) (Default yes!!!!)\n\
       bin <y|n>         - Set automatic turn on binary mode (Default no) \n\
       glass <y|n>       - Set glass mode (bad commands interprets as commands for FTPD)\n\
       prompt <string>   - Set prompt (See help prompt)\n\
       port <number>     - Set ftpd's port for next sessions\n\
       user <name>       - Set default user name (default you name)",

  "prompt",               NULL,         0,
  "\
prompt is a string, which may be contains %<char> 
or ^<char> combitanion, which have next interprets:

%H, %h - full and short remote host names
%M, %m - full and short local host names
%u     - remote user's name
%d     - remote current directory
%D     - local current directory
%f     - number of current frame
%p     - the ftp's port number
%t     - timeout 
%T     - current time 
%P     - uftp process id
%%     - character %
^<char>- control character
%^     - character ^
",

  "list",		Ftp_list,         0,
"List session's information",

  "user",		Ftp_user,         1,
"user <user> - send user's name",

  "pass",		Ftp_pass,         1,
"pass <pass> - send user's password",

  "bin",		Ftp_bin,          1,
"Set binary mode for current frame",

  "ascii",		Ftp_ascii,        1,
"Set ASCII mode for current frame",

  "cd",			Ftp_cd,           1,
"cd <directory> - change current remote directory ",

  "acd",		Ftp_acd,          0,
"acd <file_or_directory> - search pointed directory using archie, and setup connection to it",

  "lcd",		Ftp_lcd,          0,
"Change local directory",

  "abort",		Ftp_abort,        1,
"abort last operation",

  "mkdir",		Ftp_mkdir,        1,
"mkdir <dirname> - create new directory",

  "rm",		        Ftp_rm,           1,
"rm <filename_spec> - remove file(s)",

  "mv",		        Ftp_move,         1,
"mv <old> <new> - rename file",

  "dir",		Ftp_dir,          1,
"dir <argslist> ... - print list of files",

  "ls",		        Ftp_dir,          1,
"ls <arglist> ... - print short list of files",

  "get",		Ftp_get,          1,
"get <remote_file> [<local_file>] - receive file from server",

  "mget",		Ftp_mget,         1,
"mget <remote_file(s)> [<local_directory>] - receive file(s) from server",

  "reget",		Ftp_get,          1,
"reget <remote_file> <local_file> - receive file restarting at end of local file",

  "aget",		Ftp_acd,          0,
"aget <file> - search pointed file using archie, and retrive it",

  "put",		Ftp_get,          1,
"put <local_file> [<remote_file>] - send server to file",

  "mput",		Ftp_mput,         1,
"mput <local_file(s)>  - send file(s) from server",

  "reput",		Ftp_get,          1,
"reput <local_file> [<remote_file>] - send file restarting at end of remote file",

  "bget",		Ftp_bget,         0,
"bget <libftp_file> [<localfile>] - full session procedure (See \"help etc\")",

  "bput",		Ftp_bput,         0,
"bput [<localfile>] <libftp_file> - full session procedure (See \"help etc\")",
  
  "copy",               Ftp_copy,         1,
"copy [<frame>!]file [<frame>!]file - copy file via client cache",

  "ccopy",               Ftp_ccopy,       1,
"ccopy [<frame>!]file [<frame>!]file - copy file directly beetween servers",

  "cat",		Ftp_type,         1,
"cat <file> - print body of remote file to screen",
  
  "page",		Ftp_page,         1,
"page <file> - print body of remote file to screen via pager",

  "bg",			Ftp_bg,           0,
"bg <any_command> - run command(s) backgroundly (output redirect to file),\n\
You can also add &-char to back of line without \"bg\"",

  "archie",             Ftp_acd,       0,
"Find file using archie service and display to screen",

  "dup",		Ftp_dup,          1,
"Make new analogous frame",

  "quote",              Ftp_quote,        1,
"quote <some_string> - send command directly to server",

  "help",		Ftp_help,         0,
"help <command> - print list of commands or command description",

  "alias",              Ftp_alias,        0,
"\
alias aliasname <list> .... - make new alias, use $X for taking \n\
                  X's argument from command string, and $* for taking\n\
                  all arguments. If $<anything> on alias not present,\n\
                  the arguments appending to end of command string",

  "unalias",            Ftp_unalias,      0,
"unalias <aliasname> - remove alias",

  "mkalias",            Ftp_mkalias,      0,
"make alias for create this frame, use savealias for saving it to file",

  "savealias",               Ftp_save,         0,
  "Save aliases to file",

  "etc",               NULL,         0,
  "\
1. In any command you may use constructions <file and >file for\n\
   redirect input output.\n\
\n\
2. All local files files interprets as libftp file(s), \n\
   this support next specification:\n\
\n\
   |string - interprets string as shell command, which must be\n\
   execute and input or output take from/to it.\n\
   \n\
   hostname:filename - interprets as file, witch must be take \n\
   using ftp protocol with anonymously access\n\
  \n\
   user@hostname:filename - interprets as file accesses via ftp\n\
   with password yourname@your_host.your_domain\n\
\n\
   user/pass@hostname:filename - also ftp file.\n\
\n\
   *STDIN*, *STDOUT*, *STDERR* - opened streams.\n\
\n\
   anything - local file name.\n\
\n\
3. Command started with '!' passed to shell.\n
\n\
4. If string beetween two \" or \', its interprets as one word.\n\
\n\
5. Any string may be devide to few commands using ';'.",



  NULL

};







