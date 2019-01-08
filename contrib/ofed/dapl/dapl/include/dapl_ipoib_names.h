/*
 * Copyright (c) 2002-2003, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a
 *    copy of which is available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/gpl-license.php.
 *
 * Licensee has the right to choose one of the above licenses.
 *
 * Redistributions of source code must retain the above copyright
 * notice and one of the license notices.
 *
 * Redistributions in binary form must reproduce both the above copyright
 * notice, one of the license notices in the documentation
 * and/or other materials provided with the distribution.
 */

/**********************************************************************
 * 
 * MODULE: ipoib_naming.h
 *
 * PURPOSE: Defines flags and prototypes for IPoIB API
 *
 * Description: 
 *	This defines a simple naming interface for discovering
 *      the IP addresses available to a provider, then a set
 *      of query mechanisms useful to map an IP address to
 *      a provider specific address; a GID in InfiniBand.
 *
 *      NOTE: As implementations mature this may not be necessary.
 *
 * $Id:$
 **********************************************************************/

#ifndef _IPOIB_NAMING_H_
#define _IPOIB_NAMING_H_

typedef enum _ipoib_port_num {
	HCA_PORT_1= 1,
	HCA_PORT_2,
	HCA_PORT_ANY
} IPOIB_PORT_NUM;

typedef struct if_query_info
{
	uint64_t	      guid;
	uint32_t	      port_num;
	uint32_t	      state;
}IF_QUERY_INFO;

/***********************************************************************
 * ipoib_enum_if()
 *
 *    PURPOSE
 *    Returns count of IP interfaces. 
 *
 *    ARGUMENTS
 *    hca_index: index of HCA in the provider library. In general
 *    terms, the index represents the HCA number, e.g. 
 *    1 == First HCA, 2 == Second HCA, etc.
 *
 *    port: an enum of
 *        HCA_PORT_0
 *        HCA_PORT_1
 *        HCA_PORT_ANY
 *    HCA_PORT_ANY enum value returns all IP instances assigned to the HCA.
 *
 *    RETURNS
 *    count of IP interfaces supported on physical port
 *
 ***********************************************************************/
int
ipoib_enum_if(
	IN  uint32_t		hca_index, 
	IN  IPOIB_PORT_NUM	port);

 
/***********************************************************************
 * ipoib_get_if()
 *
 *    PURPOSE
 *    Returns array of IP Addresses of all instances. Port parameter may
 *    restrict instances of interest.
 * 
 *    ARGUMENTS
 *    hca_index: index of HCA in the provider library.
 *
 *    port: IPOIB_PORT_NUM as described above
 *
 *    ip_addr_list: pointer to user-allocated space in which an array of
 *    IP addresses found for this hca and port will be returned
 *
 *    ip_addr_count: number of returned addresses
 *
 *    RETURNS
 *    0 for SUCCESS
 *    !0 for failure
 *
 ***********************************************************************/
int
ipoib_get_if(
	IN  uint32_t			hca_index,
	IN  IPOIB_PORT_NUM		port,
	OUT struct sockaddr		**ip_addr_list,
	OUT int				*ip_addr_count);

/***********************************************************************
 *
 *    PURPOSE 
 *    Returns a handle to this interface, to be used for subsequent
 *    operations
 * 
 *    ARGUMENTS
 *    ip_address: input IP address
 *
 *    ipoib_handle: handle to be used in subsequent operations.
 *
 *    RETURNS
 *    0 for SUCCESS
 *    !0 for failure
 *
 ***********************************************************************/
int
ipoib_open_if(
	IN  struct sockaddr		*ip_address,
	OUT void			*ipoib_handle);

/***********************************************************************
 * ipoib_query_if()
 *
 *    PURPOSE
 *    if_query_if returns information on local ipoib_handle such as GID,
 *    Port number, IPoIB state, anything interesting
 *
 *    ARGUMENTS
 *    ipoib_handle: handle for instance
 *
 *    if_qry_info: info struct. Looks like:
 *
 *    RETURNS
 *    0 for SUCCESS
 *    !0 for failure
 *
 ***********************************************************************/
int
ipoib_query_if(
	IN  void			*ipoib_handle, 
	OUT IF_QUERY_INFO		*if_qry_info);

/***********************************************************************
 *
 *
 *    PURPOSE
 *    Obtain a GID from an IP Address. Used by the active side of
 *    a connection.
 *
 *    The behavior of this routine is specified to provide control
 *    over the underlying implementation.
 *    Returns immediately if the remote information is available. If
 *    callback_routine_ptr is NULL then it will block until information is
 *    available or known to be unavailable. If callback_routine_ptr is
 *    specified then it will be invoked when remote information is
 *    available or known to be unavailable. Remote_Addr_info contains
 *    remote GID information.
 *
 *    ARGUMENTS
 *    ipoib_handle: handle for instance
 *
 *    remote_ip_address: IP address of remote instance
 *
 *    callback_routine_ptr: routine to invoke for asynch callback. If
 *    NULL ipoib_getaddrinfo() will block.
 *
 *    context: argument to pass to asynch callback_routine.
 *
 *    Remote_Addr_info: Remote GID
 *
 *    RETURNS
 *    0 for SUCCESS
 *    !0 for failure
 *
 ***********************************************************************/
int
ipoib_getaddrinfo(
	IN  void				*ipoib_handle,
	IN  struct sockaddr			*remote_ip_address,
	IN  void				*callback_routine_ptr,
	IN  void				*context,
	OUT void				*Remote_Addr_info );

/***********************************************************************
 *
 *
 *    PURPOSE
 *    Obtain an IP Address from a GID. Used by the passive side of a
 *    connection.
 *
 *    The behavior of this routine is specified to provide control over
 *    the underlying implementation.  Returns immediately if the remote
 *    information is available. If callback_routine_ptr is NULL then it
 *    will block until information is available or known to be
 *    unavailable. If callback_routine_ptr is specified then it will be
 *    invoked when remote information is available or known to be
 *    unavailable.
 *
 *    ARGUMENTS
 *    ipoib_handle:	handle for instance
 *
 *    remote_gidAddr: 	Remote GID. It is not defined on how the application
 *		 	will obtain this GID from the connection manager.
 *
 *    callback_routine_ptr: 
 *			routine to invoke for async callback. If NULL
 *			ipoib_getgidinfo() will block.
 *
 *    context: 		argument to pass to asynch callback_routine.
 *
 *    remote_ip_address:
 *			 IP address of remote instance
 *
 *    RETURNS
 *    0 for SUCCESS
 *    !0 for failure
 *
 ***********************************************************************/
int
ipoib_getgidinfo(
	IN  void				*ipoib_handle,
	IN  GID					*remote_gid,
	IN  void				*callback_routine_ptr,
	IN  void				*context,
	OUT struct sockaddr			*remote_ip_address);

/***********************************************************************
 *
 *    PURPOSE
 *    Release handle.
 *
 *    ARGUMENTS
 *    ipoib_handle: handle for instance
 *
 *    RETURNS
 *    0 for SUCCESS
 *    !0 for failure
 *
 ***********************************************************************/
int
ipoib_close(
	IN  void				*ipoib_handle);
 

#endif /* _IPOIB_NAMING_H_ */
