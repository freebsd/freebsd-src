/*
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 * Include file for the Kerberos administration server.
 *
 *	from: admin_server.h,v 4.7 89/01/11 11:59:42 steiner Exp $
 *	$FreeBSD$
 */

#ifndef ADMIN_SERVER_DEFS
#define ADMIN_SERVER_DEFS

#define PW_SRV_VERSION		 2	/* version number */

#define INSTALL_NEW_PW		(1<<0)	/*
					 * ver, cmd, name, password,
					 * old_pass, crypt_pass, uid
					 */

#define ADMIN_NEW_PW		(2<<1)	/*
					 * ver, cmd, name, passwd,
					 * old_pass
					 * (grot), crypt_pass (grot)
					 */

#define ADMIN_SET_KDC_PASSWORD	(3<<1)	/* ditto */
#define ADMIN_ADD_NEW_KEY	(4<<1)	/* ditto */
#define ADMIN_ADD_NEW_KEY_ATTR	(5<<1)  /*
					 * ver, cmd, name, passwd,
					 * inst, attr (grot)
					 */
#define INSTALL_REPLY		(1<<1)	/* ver, cmd, name, password */
#define	RETRY_LIMIT		 1
#define	TIME_OUT		30
#define USER_TIMEOUT		90
#define MAX_KPW_LEN		40

#define KADM	"changepw"		/* service name */

#endif /* ADMIN_SERVER_DEFS */
