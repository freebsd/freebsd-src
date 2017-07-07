/*!	\file hesiod.h
 *	WSHelper DNS/Hesiod Library
 *
 *	This file contains the function declaration for: \n
 *	hes_to_bind()	\n
 *	hes_resolve()	\n
 *	hes_error()		\n
 *	hes_free()		\n
 *	hes_getmailhost()	\n
 *	hes_getservbyname()	\n
 *	hes_getpwnam()	\n
 *	hes_getpwuid()	\n
*/

#ifndef _HESIOD_
#define _HESIOD_


#include <windows.h>

/*! \def HESIOD_CONF
 *	name of the hesiod configuration file. We will look at the file to determine the RHS AND LHS value before using the default.
 *  Here is a sample hesiod.cfg file: \n
 *	lhs .ns \n
 *	rhs .ATHENA.MIT.EDU \n
 */
#define HESIOD_CONF     "c:\\net\\tcp\\hesiod.cfg"

/*! \def DEF_RHS
 *	default RHS value is the hesiod configuration file is not present
 */
#define DEF_RHS         ".Athena.MIT.EDU"

/*! \def DEF_LHS
 *	default LHS value is the hesiod configuration file is not present
 */
#define DEF_LHS         ".ns"

/*! \def HES_ER_UNINIT
 *	HES error code: uninitialized
 */
#define HES_ER_UNINIT   -1

/*! \def HES_ER_OK
 *	HES error code: no error
 */
#define HES_ER_OK       0

/*! \def HES_ER_NOTFOUND
 *	HES error code: Hesiod name not found by server
 */
#define HES_ER_NOTFOUND 1

/*! \def HES_ER_CONFIG
 *	HES error code: local problem (no config file?)
 */
#define HES_ER_CONFIG   2

/*! \def HES_ER_NET
 *	HES error code: network problem
 */
#define HES_ER_NET      3


#ifdef __cplusplus
extern "C" {
#endif

/*!	\fn LPSTR WINAPI hes_to_bind(LPSTR HesiodName, LPSTR HesiodNameType)
 *	hes_to_bind function use the LHS and RHS values and
 *	binds them with the parameters so that a well formed DNS query may
 *	be performed.
 *
 *	defined in hesiod.c
 *
 *	\param[in]	HesiodName		The Hesiod name such as a username or service name
 *	\param[in]	HesiodNameType	The Hesiod name type such as pobox, passwd, or sloc
 *	\retval		Returns NULL if there was an error. Otherwise the pointer to a string containing a valid query is returned.
 *
 */
LPSTR WINAPI
hes_to_bind(
    LPSTR HesiodName,
    LPSTR HesiodNameType
    );


/*!	\fn LPSTR * WINAPI hes_resolve(LPSTR HesiodName, LPSTR HesiodNameType)
 *	This function calls hes_to_bind to form a valid hesiod query, then queries the dns database.
 *
 *	defined in hesiod.c
 *
 *	\param[in]	HesiodName		The Hesiod name such as a username or service name
 *	\param[in]	HesiodNameType	The Hesiod name type such as pobox, passwd, or sloc
 *	\retval		returns a NULL terminated vector of strings (a la argv),
 *				one for each resource record containing Hesiod data, or NULL if
 *				there is any error. If there is an error call hes_error() to get
 *				further information. You will need to call hes_free to free the result
 *
 */

LPSTR * WINAPI
hes_resolve(
    LPSTR HesiodName,
    LPSTR HesiodNameType
    );

/*! \fn  int WINAPI hes_error(void)
 *	The  function  hes_error may be called to determine the
 *	source of the error.  It does not take an argument.
 *
 *	defined in hesiod.c
 *
 *	\retval		return one of the HES_ER_* codes defined in hesiod.h.
 */

int WINAPI
hes_error(
    void
    );


/*! \fn void WINAPI hes_free(LPSTR* hesinfo)
 * The function hes_free should be called to free up memeory returned by hes_resolve
 *
 * defined in hesiod.c
 *
 * \param[in]	hesinfo		a NULL terminiated array of strings returned by hes_resolve
 */
void WINAPI
hes_free(
    LPSTR* hesinfo
    );


/*! \struct hes_postoffice
 * For use in getting post-office information.
 */
struct hes_postoffice {
	/*! The post office type, e.g. POP, IMAP */
    LPSTR   po_type;
	/*! The post office host, e.g. PO10.MIT.EDU */
    LPSTR   po_host;
	/*! The account name on the post office, e.g. tom */
    LPSTR   po_name;
};

/*! \fn struct hes_postoffice  * WINAPI hes_getmailhost(LPSTR user)
 * This call is used to obtain a user's type of mail account and the location of that
 *	account. E.g. POP PO10.MIT.EDU or IMAP IMAP-TEST.MIT.EDU
 *
 *	defined in hesmailh.c
 *
 *	\param[in]	user	The username to be used when querying for the Hesiod Name Type POBOX.
 *	\retval				NULL if there was an error or if there was no entry for the
 *						username. Otherwise a pointer to a hes_postoffice structure is
 *						returned. The caller must never attempt to modify this structure or to free
 *						any of its components. Furthermore, only one copy of this structure is allocated per call per thread, so the application should copy any information it needs before
 *						issuing another getmailhost call
 */
struct hes_postoffice  * WINAPI hes_getmailhost(LPSTR user);

/*!	\fn struct servent  * WINAPI hes_getservbyname(LPSTR name, LPSTR proto)
 *	This function will query a Hesiod server for a servent structure given
 *	a service name and protocol. This is a replacement for the Winsock
 *	getservbyname function which normally just uses a local services
 *	file. This allows a site to use a centralized database for adding new
 *	services.
 *
 *	defined in hesservb.c
 *
 *	\param[in]	name	pointer to the official name of the service, eg "POP3".
 *	\param[in]	proto	pointer to the protocol to use when contacting the service, e.g. "TCP"
 *	\retval				NULL if there was an error or a pointer to a servent structure. The caller must
 *						never attempt to modify this structure or to free any of its components.
 *						Furthermore, only one copy of this structure is allocated per call per thread, so the application should copy any information it needs before
 *						issuing another hes_getservbyname call
 *
 */
struct servent  * WINAPI hes_getservbyname(LPSTR name,
                                              LPSTR proto);

/*! \fn struct passwd  * WINAPI hes_getpwnam(LPSTR nam)
 *	Given a username this function will return the pwd information, eg
 *	username, uid, gid, fullname, office location, phone number, home
 *	directory, and default shell
 *
 *	defined in hespwnam.c
 *
 *	\param	nam			a pointer to the username
 *	\retval				NULL if there was an error or a pointer to the passwd structure. The caller must
 *						never attempt to modify this structure or to free any of its components.
 *						Furthermore, only one copy of this structure is allocated per call per thread, so the application should copy any information it needs before
 *						issuing another hes_getpwnam call
 *
 */
struct passwd  * WINAPI hes_getpwnam(LPSTR nam);

/*!  struct passwd  * WINAPI hes_getpwuid(int uid)
 * 	Given a UID this function will return the pwd information, eg username, uid,
 *	gid, fullname, office location, phone number, home directory, and default shell
 *
 *	defined in hespwnam.c
 *
 *	\param	uid			The user ID
 *	\retval				NULL if there was an error or a pointer to the passwd structure. The caller must
 *						never attempt to modify this structure or to free any of its components.
 *						Furthermore, only one copy of this structure is allocated per call per thread, so the application should copy any information it needs before
 *						issuing another hes_getpwuid call
 */
struct passwd  * WINAPI hes_getpwuid(int uid);

#ifdef __cplusplus
}
#endif

#endif /* _HESIOD_ */
