/*
 * Copyright (c) 1999
 *	Philipp Mergenthaler <philipp.mergenthaler@stud.uni-karlsruhe.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include "sysinstall.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/param.h>
#include <netdb.h>

extern const char *ftp_dirs[]; /* defined in ftp.c */

Boolean
checkAccess(Boolean proxyCheckOnly)
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
    bool el, found=FALSE;		    /* end of header line */
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
	variable_unset(VAR_HTTP_HOST);
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
	variable_unset(VAR_HTTP_HOST);
	return FALSE;
    }
    if (proxyCheckOnly) {
       close(s);
       return TRUE;
    }

    msgNotify("Checking access to\n %s", variable_get(VAR_HTTP_PATH));
    sprintf(req,"GET %s/ HTTP/1.0\r\n\r\n", variable_get(VAR_HTTP_PATH));
    write(s,req,strlen(req));
/*
 *  scan the headers of the response
 *  this is extremely quick'n dirty
 *
 */
    bzero(buf, PATH_MAX);
    cp=buf;
    el=FALSE;
    rv=read(s,cp,1);
    variable_set2(VAR_HTTP_FTP_MODE,"",0);
    while (rv>0) {
	if ((*cp == '\012') && el) { 
	    /* reached end of a header line */
	    if (!strncmp(buf,"HTTP",4)) {
		if (strtol((char *)(buf+9),0,0) == 200) {
		    found = TRUE;
		}
	    }

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
    return found;
} 

Boolean
mediaInitHTTP(Device *dev)
{
    bool found=FALSE;		    /* end of header line */
    char *rel, req[BUFSIZ];
    int fdir;

    /* 
     * First verify the proxy access
     */
    checkAccess(TRUE);
    while (variable_get(VAR_HTTP_HOST) == NULL) {
        if (DITEM_STATUS(mediaSetHTTP(NULL)) == DITEM_FAILURE)
            return FALSE;
        checkAccess(TRUE);
    }
again:
    /* If the release is specified as "__RELEASE" or "any", then just
     * assume that the path the user gave is ok.
     */
    rel = variable_get(VAR_RELNAME);
    /*
    msgConfirm("rel: -%s-", rel);
    */

    if (strcmp(rel, "__RELEASE") && strcmp(rel, "any"))  {
        for (fdir = 0; ftp_dirs[fdir]; fdir++) {
            sprintf(req, "%s/%s/%s", variable_get(VAR_FTP_PATH),
                ftp_dirs[fdir], rel);
            variable_set2(VAR_HTTP_PATH, req, 0);
            if (checkAccess(FALSE)) {
                found = TRUE;
                break;
            }
        }
    } else {
        variable_set2(VAR_HTTP_PATH, variable_get(VAR_FTP_PATH), 0);
        found = checkAccess(FALSE);
    }
    if (!found) {
    	msgConfirm("No such directory: %s\n"
		   "please check the URL and try again.", variable_get(VAR_HTTP_PATH));
        variable_unset(VAR_HTTP_PATH);
        dialog_clear_norefresh();
        clear();
        if (DITEM_STATUS(mediaSetHTTP(NULL)) != DITEM_FAILURE) goto again;
    }
    return found;
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
						   
    sprintf(req,"GET %s/%s%s HTTP/1.0\r\n\r\n",
	    variable_get(VAR_HTTP_PATH), file, variable_get(VAR_HTTP_FTP_MODE));

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
