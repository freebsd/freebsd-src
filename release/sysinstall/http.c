#include "sysinstall.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/param.h>
#include <netdb.h>

Boolean
mediaInitHTTP(Device *dev)
{
/* 
 * Some proxies fetch files with certain extensions in "ascii mode" instead
 * of "binary mode" for FTP. The FTP server then translates all LF to CRLF.
 *
 * You can force Squid to use binary mode by appending ";type=i" to the URL,
 * which is what I do here. For other proxies, the LF->CRLF substitution
 * is reverted in distExtract().
 */

    int rv, s, af;
    bool el;		    /* end of header line */
    char *cp, buf[PATH_MAX], req[BUFSIZ];
    struct addrinfo hints, *res, *res0;

    af = variable_cmp(VAR_IPV6_ENABLE, "YES") ? AF_INET : AF_UNSPEC;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = af;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    if ((rv = getaddrinfo(variable_get(VAR_HTTP_HOST),
			  variable_get(VAR_HTTP_PORT), &hints, &res0)) != 0) {
	msgConfirm("%s", gai_strerror(rv));
	return FALSE;
    }
    s = -1;
    for (res = res0; res; res = res->ai_next) {
	if ((s = socket(res->ai_family, res->ai_socktype,
			res->ai_protocol)) < 0)
	    continue;
	if (connect(s, res->ai_addr, res->ai_addrlen) >= 0)
	    break;
	close(s);
	s = -1;
    }
    freeaddrinfo(res0);
    if (s == -1) {
	msgConfirm("Couldn't connect to proxy %s:%s",
		    variable_get(VAR_HTTP_HOST),variable_get(VAR_HTTP_PORT));
	return FALSE;
    }

    sprintf(req,"GET / HTTP/1.0\r\n\r\n");
    write(s,req,strlen(req));
/*
 *  scan the headers of the response
 *  this is extremely quick'n dirty
 *
 */
    cp=buf;
    el=FALSE;
    rv=read(s,cp,1);
    variable_set2(VAR_HTTP_FTP_MODE,"",0);
    while (rv>0) {
	if ((*cp == '\012') && el) { 
	    /* reached end of a header line */
	    if (!strncmp(buf,"Server: ",8)) {
		if (!strncmp(buf,"Server: Squid",13)) {
		    variable_set2(VAR_HTTP_FTP_MODE,";type=i",0);
		} else {
		    variable_set2(VAR_HTTP_FTP_MODE,"",0);
		}
	    }
	    /* ignore other headers */
	    /* check for "\015\012" at beginning of line, i.e. end of headers */
	    if ((cp-buf) == 1)
		break;
	    cp=buf;
	    rv=read(s,cp,1);
	} else {
	    el=FALSE;
	    if (*cp == '\015')
		el=TRUE;
	    cp++;
	    rv=read(s,cp,1);
	}
    }
    close(s);
    return TRUE;
} 


FILE *
mediaGetHTTP(Device *dev, char *file, Boolean probe)
{
    FILE *fp;
    int rv, s, af;
    bool el;			/* end of header line */
    char *cp, buf[PATH_MAX], req[BUFSIZ];
    struct addrinfo hints, *res, *res0;

    af = variable_cmp(VAR_IPV6_ENABLE, "YES") ? AF_INET : AF_UNSPEC;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = af;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    if ((rv = getaddrinfo(variable_get(VAR_HTTP_HOST),
			  variable_get(VAR_HTTP_PORT), &hints, &res0)) != 0) {
	msgConfirm("%s", gai_strerror(rv));
	return NULL;
    }
    s = -1;
    for (res = res0; res; res = res->ai_next) {
	if ((s = socket(res->ai_family, res->ai_socktype,
			res->ai_protocol)) < 0)
	    continue;
	if (connect(s, res->ai_addr, res->ai_addrlen) >= 0)
	    break;
	close(s);
	s = -1;
    }
    freeaddrinfo(res0);
    if (s == -1) {
	msgConfirm("Couldn't connect to proxy %s:%s",
		    variable_get(VAR_HTTP_HOST),variable_get(VAR_HTTP_PORT));
	return NULL;
    }
						   
    sprintf(req,"GET %s/%s/%s%s HTTP/1.0\r\n\r\n",
	    variable_get(VAR_FTP_PATH), variable_get(VAR_RELNAME),
	    file, variable_get(VAR_HTTP_FTP_MODE));

    if (isDebug()) {
	msgDebug("sending http request: %s",req);
    }
    write(s,req,strlen(req));

/*
 *  scan the headers of the response
 *  this is extremely quick'n dirty
 *
 */
    cp=buf;
    el=FALSE;
    rv=read(s,cp,1);
    while (rv>0) {
	if ((*cp == '\012') && el) {
  	    /* reached end of a header line */
  	    if (!strncmp(buf,"HTTP",4)) {
		rv=strtol((char *)(buf+9),0,0);
		*(cp-1)='\0';		/* chop the CRLF off */
		if (probe && (rv != 200)) {
		    return NULL;
		} else if (rv >= 500) {
		    msgConfirm("Server error %s when sending %s, you could try an other server",buf, req);
		    return NULL;
		} else if (rv == 404) {
		    msgConfirm("%s was not found, maybe directory or release-version are wrong?",req);
		    return NULL;
		} else if (rv >= 400) {
		    msgConfirm("Client error %s, you could try an other server",buf);
		    return NULL;
		} else if (rv >= 300) {
		    msgConfirm("Error %s,",buf);
		    return NULL;
		} else if (rv != 200) {
		    msgConfirm("Error %s when sending %s, you could try an other server",buf, req);
		    return NULL;
		}
	    }
	    /* ignore other headers */
	    /* check for "\015\012" at beginning of line, i.e. end of headers */
	    if ((cp-buf) == 1) 
		break;
	    cp=buf;
	    rv=read(s,cp,1);
	} else {
	    el=FALSE;
	    if (*cp == '\015')
		el=TRUE;
	    cp++;
	    rv=read(s,cp,1);
	}
    }
    fp=fdopen(s,"r");
    return fp;
}
