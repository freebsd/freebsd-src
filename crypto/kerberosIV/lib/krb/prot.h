/*
 * $Id: prot.h,v 1.9 1999/11/30 18:57:46 bg Exp $
 *
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 *
 * Include file with authentication protocol information.
 */

#ifndef PROT_DEFS
#define PROT_DEFS

#include <krb.h>

#define		KRB_SERVICE		"kerberos-iv"
#define		KRB_PORT		750	/* PC's don't have
						 * /etc/services */
#define		KRB_PROT_VERSION 	4
#define 	MAX_PKT_LEN		1000
#define		MAX_TXT_LEN		1000

/* Routines to create and read packets may be found in prot.c */

KTEXT create_auth_reply(char *pname, char *pinst, char *prealm, 
			int32_t time_ws, int n, u_int32_t x_date, 
			int kvno, KTEXT cipher);
#ifdef DEBUG
KTEXT krb_create_death_packet(char *a_name);
#endif

/* Message types , always leave lsb for byte order */

#define		AUTH_MSG_KDC_REQUEST			 (1<<1)
#define 	AUTH_MSG_KDC_REPLY			 (2<<1)
#define		AUTH_MSG_APPL_REQUEST			 (3<<1)
#define		AUTH_MSG_APPL_REQUEST_MUTUAL		 (4<<1)
#define		AUTH_MSG_ERR_REPLY			 (5<<1)
#define		AUTH_MSG_PRIVATE			 (6<<1)
#define		AUTH_MSG_SAFE				 (7<<1)
#define		AUTH_MSG_APPL_ERR			 (8<<1)
#define		AUTH_MSG_KDC_FORWARD			 (9<<1)
#define		AUTH_MSG_KDC_RENEW			(10<<1)
#define 	AUTH_MSG_DIE				(63<<1)

/* values for kerb error codes */

#define		KERB_ERR_OK				 0
#define		KERB_ERR_NAME_EXP			 1
#define		KERB_ERR_SERVICE_EXP			 2
#define		KERB_ERR_AUTH_EXP			 3
#define		KERB_ERR_PKT_VER			 4
#define		KERB_ERR_NAME_MAST_KEY_VER		 5
#define		KERB_ERR_SERV_MAST_KEY_VER		 6
#define		KERB_ERR_BYTE_ORDER			 7
#define		KERB_ERR_PRINCIPAL_UNKNOWN		 8
#define		KERB_ERR_PRINCIPAL_NOT_UNIQUE		 9
#define		KERB_ERR_NULL_KEY			10
#define		KERB_ERR_TIMEOUT			11

/* sendauth - recvauth */

/*
 * If the protocol changes, you will need to change the version string
 * be sure to support old versions of krb_sendauth!
 */

#define	KRB_SENDAUTH_VERS "AUTHV0.1" /* MUST be KRB_SENDAUTH_VLEN chars */

#endif /* PROT_DEFS */
