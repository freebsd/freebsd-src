/*
    YPS-0.2, NIS-Server for Linux
    Copyright (C) 1994  Tobias Reber

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    Modified for use with FreeBSD 2.x by Bill Paul (wpaul@ctr.columbia.edu)

	$Id$
*/

/*
 *	$Author: root $
 *	$Log: ypclnt.c,v $
 * Revision 2.0  1994/01/06  16:58:48  root
 * Version 2.0
 *
 * Revision 0.17  1994/01/02  22:48:22  root
 * Added strict prototypes
 *
 * Revision 0.16  1994/01/02  20:09:39  root
 * Added GPL notice
 *
 * Revision 0.15  1993/12/30  22:34:57  root
 * *** empty log message ***
 *
 * Revision 0.14  1993/12/19  12:42:32  root
 * *** empty log message ***
 *
 * Revision 0.13  1993/06/12  09:39:30  root
 * Align with include-4.4
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpcsvc/yp.h>
/*
 * ypclnt.h does not have a definition for struct dom_binding,
 * although it is used there. It is defined in yp_prot.h, but
 * we cannot use it here.
 */
struct dom_binding {
	void * m;
};
#include <rpcsvc/ypclnt.h>
 
static struct sockaddr_in ServerAddress;
static CLIENT *UdpClient=NULL, *TcpClient=NULL;
 
#ifdef YPBROADCAST
static bool_t
eachresult( caddr_t resultsp, struct sockaddr_in *raddr)
{
   bcopy(raddr, &ServerAddress, sizeof(ServerAddress));
   return((bool_t) TRUE);
}
#endif
 
static struct sockaddr_in *
__do_ypbind(domainname d)
{
   static struct sockaddr_in resp;
   int rc;
   ypbind_resp r;
   CLIENT *localBindClient;
   struct sockaddr_in localAddr;
   int s;
   struct timeval t={5,0}, tott={25,0};

   s=RPC_ANYSOCK;
   bzero(&localAddr, sizeof localAddr);
   localAddr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
   localBindClient=clntudp_create(&localAddr, YPBINDPROG, YPBINDVERS, tott, &s);
   if (!localBindClient) {
      clnt_pcreateerror("");
      return NULL;
   }
   
   rc=clnt_call(localBindClient, YPBINDPROC_DOMAIN, 
      xdr_domainname, (char *)&d, xdr_ypbind_resp, (char *)&r, t);
   if (rc) {
      clnt_perrno(rc);
      return NULL;
   }

   switch (r.ypbind_status) {
   case YPBIND_FAIL_VAL:
      switch(r.ypbind_resp_u.ypbind_error) {
      case YPBIND_ERR_ERR:
         fprintf(stderr, "YPBINDPROC_DOMAIN: Internal error\n");
         break;
      case YPBIND_ERR_NOSERV:
         fprintf(stderr, "YPBINDPROC_DOMAIN: No bound server for passed domain\n");
         break;
      case YPBIND_ERR_RESC:
         fprintf(stderr, "YPBINDPROC_DOMAIN: System resource allocation failure\n");
         break;
      default:
         fprintf(stderr, "YPBINDPROC_DOMAIN: Unknown error\n");
         break;
      }
      return NULL;
   case YPBIND_SUCC_VAL:
      {
         struct ypbind_binding *y=&r.ypbind_resp_u.ypbind_bindinfo;
         bzero(&resp, sizeof resp);
         resp.sin_family=AF_INET;
         resp.sin_addr=*(struct in_addr *)(y->ypbind_binding_addr);
         return &resp;
      }
   }
   return NULL;
}

void
__yp_unbind(char *DomainName)
{
   if (UdpClient) clnt_destroy(UdpClient);
   UdpClient=NULL;
   if (TcpClient) clnt_destroy(TcpClient);
   TcpClient=NULL;
}
 
int
_yp_bind(struct sockaddr_in *ServerAddress, char *DomainName)
{
   struct sockaddr_in UdpServerAddress, TcpServerAddress;
   int UdpSockp, TcpSockp;
   static struct timeval Wait = { 5, 0 };
 
   if (UdpClient || TcpClient) __yp_unbind(DomainName);
 
   bcopy(ServerAddress, &UdpServerAddress, sizeof(*ServerAddress));
   UdpServerAddress.sin_port=0;
   UdpSockp=(RPC_ANYSOCK);
   bcopy(ServerAddress, &TcpServerAddress, sizeof(*ServerAddress));
   TcpServerAddress.sin_port=0;
   TcpSockp=(RPC_ANYSOCK);
   if ((UdpClient=clntudp_create(&UdpServerAddress, YPPROG, YPVERS,
      Wait, &UdpSockp))==NULL) {
      clnt_pcreateerror("UdpClient");
      return(YPERR_RPC);
   }
   if ((TcpClient=clnttcp_create(&TcpServerAddress, YPPROG, YPVERS,
      &TcpSockp, 0, 0))==NULL) {
      clnt_pcreateerror("TcpClient");
      return(YPERR_RPC);
   }
   return(0);
 
}
 
int
__yp_bind(char *DomainName)
{
#  ifdef YPBROADCAST
   enum clnt_stat clnt_stat;
   static bool_t res;
#  endif
   static domainname domain;
 
   domain=DomainName;
#  ifndef SOCKSERVER
#  ifdef YPBROADCAST
   bzero(&ServerAddress, sizeof ServerAddress);
   if ((clnt_stat=clnt_broadcast(YPPROG, YPVERS,
      YPPROC_DOMAIN_NONACK, xdr_domainname, (char *)&domain, xdr_bool,
      (char *)&res, eachresult))!=RPC_SUCCESS) {
      clnt_perrno(clnt_stat);
      return(YPERR_DOMAIN);
   }
#  else
   {
      struct sockaddr_in *s=__do_ypbind(DomainName);
      if (!s) return(YPERR_DOMAIN);
      ServerAddress=*s;
   }
#  endif
#  else
   bzero(&ServerAddress, sizeof ServerAddress);
   ServerAddress.sin_family=AF_INET;
   ServerAddress.sin_addr.s_addr=htonl(SOCKSERVER);
#  endif SOCKSERVER
   return (_yp_bind(&ServerAddress, DomainName));
}

int
__yp_all( char *DomainName, char *MapName, struct ypall_callback *CallBack)
{
   static ypreq_nokey req;
   ypresp_all *resp;
   extern struct ypall_callback *xdr_ypall_callback;
   int Status;
 
   do {
      if (TcpClient==NULL)
         if ((Status=__yp_bind(DomainName))) return(Status);
 
      req.domain=DomainName;
      req.map=MapName;
      xdr_ypall_callback=CallBack;
      if ((resp=ypproc_all_2(&req, TcpClient))==NULL) {
         clnt_perror(TcpClient, "ypall");
         __yp_unbind(DomainName);
      }
   } while(resp==NULL);
   switch (resp->ypresp_all_u.val.stat) {
   case YP_TRUE:
   case YP_NOMORE:
      Status=0;
      break;
   default:
      Status=ypprot_err(resp->ypresp_all_u.val.stat);
   }
   clnt_freeres(TcpClient, xdr_ypresp_all, resp);
   return(Status);
}

int
_yp_clear(char *DomainName)
{
   void *resp;
   int Status;
 
   do {
      if (UdpClient==NULL)
         if ((Status=yp_bind(DomainName))) return(Status);
      if ((resp=ypproc_clear_2(NULL, UdpClient))==NULL) {
         clnt_perror(UdpClient, "_yp_clear");
         __yp_unbind(DomainName);
      }
   } while(resp==NULL);
   return 0;
}

