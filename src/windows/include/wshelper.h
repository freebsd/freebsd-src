/*! \file wshelper.h
 * WSHelper DNS/Hesiod Library
 *
 * This file contains the function declaration for:	\n
 *	rgethostbyname()	\n
 *	rgethostbyaddr()	\n
 *  rgetservbyname()	\n
 *  inet_aton()			\n
 *	wsh_gethostname()	\n
 *	wsh_getdomainname()	\n \n
 *  and unsupported functions: \n
 *	gethinfobyname()	\n
 *	getmxbyname()		\n
 *	getrecordbyname()	\n
 *	rrhost()			\n
 */

#ifndef _WSHELPER_
#define _WSHELPER_

#include <winsock.h>
#include <mitwhich.h>
#include <resolv.h>
#include <hesiod.h>

#ifdef __cplusplus
extern "C" {
#endif
/*!  \fn struct hostent * WINAPI rgethostbyname(char  *name)
 *	retrieves host information corresponding to a host name in the DNS database
 *
 *	defined in gethna.c
 *
 *	\param[in]	name	Pointer to the null-terminated name of the host to resolve. It can be a fully qualified host name such as x.mit.edu
 *						or it can be a simple host name such as x. If it is a simple host name, the default domain name is
 *						appended to do the search.
 *	\retval		a pointer to the structure hostent. a structure allocated by the library. The hostent structure contains
 *				the results of a successful search for the host specified in the name parameter. The caller must never
 *				attempt to modify this structure or to free any of its components. Furthermore, only one copy of this
 *				structure is allocated per call per thread, so the application should copy any information it needs before
 *				issuing another rgethostbyname.
 *				NULL if the search has failed
 *
*/
struct hostent * WINAPI rgethostbyname(char  *name);

/*! \fn struct hostent * WINAPI rgethostbyaddr(char  *addr, int len, int type)
 *	retrieves the host information corresponding to a network address in the DNS database
 *
 *	defined in gethna.c
 *
 *	\param[in]	addr	Pointer to an address in network byte order
 *	\param[in]	len		Length of the address, in bytes
 *	\param[in]  type	Type of the address, such as the AF_INET address family type (defined as TCP,
 *						UDP, and other associated Internet protocols). Address family types and their corresponding
 *						values are defined in the Winsock2.h header file.
 *	\retval		returns a pointer to the hostent structure that contains the name and address corresponding
 *				to the given network address. The structure is allocated by the library.  The caller must never
 *				attempt to modify this structure or to free any of its components. Furthermore, only one copy of this
 *				structure is allocated per call per thread, so the application should copy any information it needs before
 *				issuing another rgethostbyaddr.
 *				NULL if the search has failed
 *
*/
struct hostent * WINAPI rgethostbyaddr(char  *addr, int len, int type);

/*! \fn	struct servent * WINAPI rgetservbyname(LPSTR name, LPSTR proto)
 *	retrieves service information corresponding to a service name and protocol.
 *
 *	defined in gethna.c
 *
 *	\param[in]	name	Pointer to a null-terminated service name.
 *	\param[in]  proto	pointer to a null-terminated protocol name. getservbyname should match both
 *						the name and the proto.
 *	\retval		a pointer to the servent structure containing the name(s) and service number that match the name and proto
 *				parameters. The structure is allocated by the library.  The caller must never
 *				attempt to modify this structure or to free any of its components. Furthermore, only one copy of this
 *				structure is allocated per call per thread, so the application should copy any information it needs before
 *				issuing another rgetservbyname.
 *				NULL if the search has failed
 *
 */
struct servent * WINAPI rgetservbyname(LPSTR name, LPSTR proto);

/*! \fn LPSTR WINAPI gethinfobyname(LPSTR name)
 *	unsupported
 */
LPSTR WINAPI gethinfobyname(LPSTR name);

/*! \fn LPSTR WINAPI getmxbyname(LPSTR name)
 *	unsupported
 */
LPSTR WINAPI getmxbyname(LPSTR name);

/*! \fn LPSTR WINAPI getrecordbyname(LPSTR name, int rectype)
 *	unsupported
 */
LPSTR WINAPI getrecordbyname(LPSTR name, int rectype);

/*! \fn  DWORD WINAPI rrhost( LPSTR lpHost )
 *	unsupported
 */
DWORD WINAPI rrhost( LPSTR lpHost );

/*! \fn  unsigned long WINAPI inet_aton(register const char *cp, struct in_addr *addr)
 *	converts a string containing an (Ipv4) Internet Protocol dotted address into a proper address for the in_addr structure
 *
 *	defined in inetaton.c
 *
 *	\param[in]		cp		Null-terminated character string representing a number expressed in the
 *							Internet standard ".'' (dotted) notation.
 *	\param[in, out]	addr	pointer to the in_addr structure. The s_addr memeber will be populated
 *	\retval Returns 1 if the address is valid, 0 if not.
 */
unsigned long WINAPI inet_aton(register const char *cp, struct in_addr *addr);


/*! \fn int WINAPI wsh_gethostname(char* name, int size)
 *	Gets the base part of the hostname
 *
 *	defined in res_init.c
 *
 *	\param[in, out]	name	pointer to a buffer that receives a null-terminated string containing the computer name
 *	\param[in]		size	specifies the size of the buffer, in chars (must be large
 *							enough to hold NULL-terminated host name)
 *	\retval			return 0 ifsuccess,  -1 on error.
*/
int WINAPI wsh_gethostname(char* name, int size);

/*!	\fn int WINAPI wsh_getdomainname(char* name, int size)
 *	Gets the machine's domain name
 *
 *	defined in res_init.c
 *
 *	\param[in, out]	name	pointer to a buffer that receives a null-terminated string containing the domain name
 *	\param[in]		size	specifies the size of the buffer, in chars (must be large
 *							enough to hold NULL-terminated domain name)
 *
 *	\retval			return 0 ifsuccess,  -1 on error.
 */
int WINAPI wsh_getdomainname(char* name, int size);


#ifdef __cplusplus
}
#endif

#endif  /* _WSHELPER_ */
