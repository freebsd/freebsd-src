/*	simpleHTTPd (C) 1998 netSTOR Technologies, Inc. ("netSTOR")
	FreeBSD port and additional work by Marc Nicholas <marc@netstor.com>
	Based on work by:-
	Thierry Leconte & Yury Shimanovsky
	My Russian webserver writing friends :-)

	This is an HTTP daemon that will serve up HTML, text files, JPEGs,
	GIFs and do simple CGI work.

	You may use this code for non-commercial distribution only. Commercial
	distribution requires the express, written permission of netSTOR. No
	warranty is implied or given -- use at your own risk!
*/

/*
 * $Id: simple_httpd.c,v 1.1 1998/08/19 16:24:06 abial Exp $
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>

int             http_sock, con_sock;
int             http_port = 80;
struct sockaddr_in source;
char           homedir[100];
char           *adate();
struct hostent *hst;

void
init_servconnection(void)
{
	struct sockaddr_in server;

	/* Create a socket */
	http_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (http_sock < 0) {
		perror("socket");
		exit(1);
	}
	server.sin_family = AF_INET;
	server.sin_port = htons(http_port);
	server.sin_addr.s_addr = INADDR_ANY;
	if (bind(http_sock, (struct sockaddr *) & server, sizeof(server)) < 0) {
		perror("bind socket");
		exit(1);
	}
        printf("simpleHTTPd running on %d port\n",http_port);
}

attenteconnection(void)
{
	int lg;

	lg = sizeof(struct sockaddr_in);

	con_sock = accept(http_sock, (struct sockaddr *) & source, &lg);
	if (con_sock <= 0) {
		perror("accept");
		exit(1);
	}
}

outdate()
{
	time_t	tl;
	char	buff[50];

	tl = time(NULL);
	strftime(buff, 50, "Date: %a, %d %h %Y %H:%M:%S %Z\r\n", gmtime(&tl));
	write(con_sock, buff, strlen(buff));
}

char           *rep_err_nget[2] = {"<HTML><HEAD><TITLE>Error</TITLE></HEAD><BODY><H1>Error 405</H1>\
This server is supports only GET and HEAD  requests\n</BODY></HTML>\r\n",
"HTTP/1.0 405 Method Not allowed\r\nAllow: GET,HEAD\r\nServer: jhttpd\r\n"};

char           *rep_err_acc[2] = {"<HTML><HEAD><TITLE>Error</TITLE></HEAD><BODY><H1>Error 404</H1>\
Not found - file doesn't exist or is read protected\n</BODY></HTML>\r\n",
"HTTP/1.0 404 Not found\r\nServer: jhttpd\r\n"};

outerror(char **rep, int http1) /* Выдыча ошибки клиенту в html- виде */
{

	if (http1) {
		write(con_sock, rep[1], strlen(rep[1]));
		outdate();
		write(con_sock, "\r\n", 2);
	}
	write(con_sock, rep[0], strlen(rep[0]));
}

char            rep_head[] = "HTTP/1.0 200 OK\r\nServer: simpleHTTPD\r\n";

traite_req()
{
	char            buff[8192];
	int             fd, lg, cmd, http1, i;
	char           *filename, *c;
	struct stat     statres;
	char            req[1024];
        char            logfile[80];
        char            msg[1024];
        char           *p,
                       *par;
        long            addr;
        FILE           *log;

	lg = read(con_sock, req, 1024);

        if (p=strstr(req,"\n")) *p=0;
        if (p=strstr(req,"\r")) *p=0;

       if (geteuid())
          {
          strcpy(logfile,getenv("HOME"));
          strcat(logfile,"/");
          strcat(logfile,"jhttp.log");
          }
       else strcpy(logfile,"/usr/adm/jhttpd.log");

       if ( access(logfile,W_OK))
            { 
            lg=creat (logfile,O_WRONLY);         
            chmod (logfile,00600);
            close(lg);
            }

        strcpy(buff,inet_ntoa(source.sin_addr));

        addr=inet_addr(buff);
        
        strcpy(msg,adate());
        strcat(msg,"    ");                 
        hst=gethostbyaddr((char*) &addr, 4, AF_INET);
        if (hst) strcat(msg,hst->h_name);
        strcat(msg," (");
        strcat(msg,buff);
        strcat(msg,")   ");
        strcat(msg,req);

        log=fopen(logfile,"a");
        fprintf(log,"%s\n",msg);
        fclose(log);

	c = strtok(req, " ");
	if (c == NULL) {
		outerror(rep_err_nget, 0);
		goto error;
	}
	cmd = 0;
	if (strncmp(c, "GET", 3) == 0)
		cmd = 1;
	if (strncmp(c, "HEAD", 4) == 0) {
		cmd = 2;
	}

	filename = strtok(NULL, " ");

	http1 = 0;
	c = strtok(NULL, " ");
	if (c != NULL && strncmp(c, "HTTP", 4) == 0)
		http1 = 1;

	if (cmd == 0) {
		outerror(rep_err_nget, http1);
		goto error;
	}
   
	if (filename == NULL || 
            strlen(filename)==1) filename="/index.html"; 

         while (filename[0]== '/') filename++;        

        /**/
        if (!strncmp(filename,"cgi-bin/",8))           
           {
           par=0;
           if (par=strstr(filename,"?"))                        
              {
               *par=0;            
                par++;      
              } 
           if (access(filename,X_OK)) goto conti;
           stat (filename,&statres);
           if (setuid(statres.st_uid)) return(0);
           if (seteuid(statres.st_uid)) return(0);
           if (!fork())
              {
               close(1);
               dup(con_sock);
               printf("HTTP/1.0 200 OK\nContent-type: text/html\n\n\n");
               execlp (filename,filename,par,0);
              } 
            wait(&i);
            return(0);
            }
        conti:
	if (filename == NULL) {
		outerror(rep_err_acc, http1);
		goto error;
	}
	/* interdit les .. dans le path */
	c = filename;
	while (*c != '\0')
		if (c[0] == '.' && c[1] == '.') {
			outerror(rep_err_acc, http1);
			goto error;
		} else
			c++;

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		outerror(rep_err_acc, http1);
		goto error;
	}
	if (fstat(fd, &statres) < 0) {
		outerror(rep_err_acc, http1);
		goto error;
	}
	if (!S_ISREG(statres.st_mode))
	    {
	    outerror(rep_err_acc, http1);
            goto error;
	    }
	if (http1) {
		char            buff[50];
		time_t          tl;
     
		write(con_sock, rep_head, strlen(rep_head));
		sprintf(buff, "Content-length: %d\r\n", statres.st_size);
		write(con_sock, buff, strlen(buff));
		outdate();

                if (strstr(filename,"."))
                   {
                   strcpy(buff,"Content-type: ");
                   strcat(buff,strstr(filename,".")+1);
                   strcat(buff,"\r\n");
                   write(con_sock,buff,strlen(buff));
                   }

                if (strstr(filename,".txt"))
                   {
                   strcpy(buff,"Content-type: text/plain\r\n");
                   write(con_sock, buff, strlen(buff));
                   }

                if (strstr(filename,".html") ||
                    strstr(filename,".htm"))
                   {
                   strcpy(buff,"Content-type: text/html\r\n");
                   write(con_sock, buff, strlen(buff));
                   }

                if (strstr(filename,".gif"))
                   {
                   strcpy(buff,"Content-type: image/gif\r\n");
                   write(con_sock, buff, strlen(buff));
                   }

                if (strstr(filename,".jpg"))
                   {
                   strcpy(buff,"Content-type: image/jpeg\r\n");
                   write(con_sock, buff, strlen(buff));
                   } 

		strftime(buff, 50, "Last-Modified: %a, %d %h %Y %H:%M:%S %Z\r\n\r\n", gmtime(&statres.st_mtime));
		write(con_sock, buff, strlen(buff));
	}
	if (cmd == 1) {
		while (lg = read(fd, buff, 8192))
			write(con_sock, buff, lg);
	} 

error:
	close(fd);
	close(con_sock);

}


main(int argc, char **argv)
{
	int             lg;
        char            hello[100];

        if (argc<2 && geteuid())
           {
           printf("Usage: simple_htppd <port>\n");
           exit(1);
           }

	if (argc>=2) http_port = atoi(argv[1]);
 
	strcpy (homedir,getenv("HOME"));
        if (!geteuid()) strcpy (homedir,"/httphome");
           else         strcat (homedir,"/httphome");

        strcpy(hello,homedir);
        strcat(hello,"/0hello.html");

	if (chdir(homedir)) 
           {
	   perror("chdir");
           puts(homedir);
           exit(1);
	   }
        init_servconnection();                  
                
        if (fork()) exit(0);

        setpgrp(0,65534);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

        if (listen(http_sock,100) < 0) exit(1);

        label:	
	attenteconnection();
        if (fork())
           {
           close(con_sock);
           goto label;
           }
        alarm(1800);
	traite_req();
        exit(0);
}



char *adate()
{
        static char out[50];
        long now;
        struct tm *t;
        time(&now);
        t = localtime(&now);
        sprintf(out, "%02d:%02d:%02d %02d/%02d/%02d",
                     t->tm_hour, t->tm_min, t->tm_sec,
                     t->tm_mday, t->tm_mon+1, t->tm_year );
        return out;
}
