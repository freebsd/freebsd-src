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

    Modified for use with FreeBSd 2.x by Bill Paul (wpaul@ctr.columbia.edu)

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
 
#if 0
#define SOCKSERVER 0x7f000001
#endif

static struct sockaddr_in ServerAddress;
static CLIENT *UdpClient=NULL, *TcpClient=NULL;
 
void
_yp_unbind(char *DomainName)
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
 
   if (UdpClient || TcpClient) yp_unbind(DomainName);
 
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
_yp_clear(char *DomainName)
{
   void *resp;
   int Status;

   do {
      if (UdpClient==NULL)
         if ((Status=yp_bind(DomainName))) return(Status);
      if ((resp=ypproc_clear_2(NULL, UdpClient))==NULL) {
         clnt_perror(UdpClient, "_yp_clear");
         _yp_unbind(DomainName);
      }
   } while(resp==NULL);
   return 0;
}
