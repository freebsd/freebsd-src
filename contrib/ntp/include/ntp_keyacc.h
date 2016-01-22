/*
 *  ntp_keyacc.h - key access stuff
 */
#ifndef NTP_KEYACC_H
#define NTP_KEYACC_H

typedef struct keyaccess KeyAccT;
struct keyaccess {
	KeyAccT *	next;
	sockaddr_u	addr;
};

#endif	/* NTP_KEYACC_H */
