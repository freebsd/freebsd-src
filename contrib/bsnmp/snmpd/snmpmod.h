/*
 * Copyright (c) 2001-2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 *	All rights reserved.
 *
 * Author: Harti Brandt <harti@freebsd.org>
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Begemot: bsnmp/snmpd/snmpmod.h,v 1.28 2005/05/23 09:03:59 brandt_h Exp $
 *
 * SNMP daemon data and functions exported to modules.
 */
#ifndef snmpmod_h_
#define snmpmod_h_

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include "asn1.h"
#include "snmp.h"
#include "snmpagent.h"

#define MAX_MOD_ARGS	16

/*
 * These macros help to handle object lists for SNMP tables. They use
 * tail queues to hold the objects in ascending order in the list.
 * ordering can be done either on an integer/unsigned field or and asn_oid.
 */
#define INSERT_OBJECT_OID_LINK_INDEX(PTR, LIST, LINK, INDEX) do {	\
	__typeof (PTR) _lelem;						\
									\
	TAILQ_FOREACH(_lelem, (LIST), LINK)				\
		if (asn_compare_oid(&_lelem->INDEX, &(PTR)->INDEX) > 0)	\
			break;						\
	if (_lelem == NULL)						\
		TAILQ_INSERT_TAIL((LIST), (PTR), LINK);			\
	else								\
		TAILQ_INSERT_BEFORE(_lelem, (PTR), LINK);		\
    } while(0)

#define INSERT_OBJECT_INT_LINK_INDEX(PTR, LIST, LINK, INDEX) do {	\
	__typeof (PTR) _lelem;						\
									\
	TAILQ_FOREACH(_lelem, (LIST), LINK)				\
		if ((asn_subid_t)_lelem->INDEX > (asn_subid_t)(PTR)->INDEX)\
			break;						\
	if (_lelem == NULL)						\
		TAILQ_INSERT_TAIL((LIST), (PTR), LINK);			\
	else								\
		TAILQ_INSERT_BEFORE(_lelem, (PTR), LINK);		\
    } while(0)

#define FIND_OBJECT_OID_LINK_INDEX(LIST, OID, SUB, LINK, INDEX) ({	\
	__typeof (TAILQ_FIRST(LIST)) _lelem;				\
									\
	TAILQ_FOREACH(_lelem, (LIST), LINK)				\
		if (index_compare(OID, SUB, &_lelem->INDEX) == 0)	\
			break;						\
	(_lelem);							\
    })

#define NEXT_OBJECT_OID_LINK_INDEX(LIST, OID, SUB, LINK, INDEX) ({	\
	__typeof (TAILQ_FIRST(LIST)) _lelem;				\
									\
	TAILQ_FOREACH(_lelem, (LIST), LINK)				\
		if (index_compare(OID, SUB, &_lelem->INDEX) < 0)	\
			break;						\
	(_lelem);							\
    })

#define FIND_OBJECT_INT_LINK_INDEX(LIST, OID, SUB, LINK, INDEX) ({	\
	__typeof (TAILQ_FIRST(LIST)) _lelem;				\
									\
	if ((OID)->len - SUB != 1)					\
		_lelem = NULL;						\
	else								\
		TAILQ_FOREACH(_lelem, (LIST), LINK)			\
			if ((OID)->subs[SUB] == (asn_subid_t)_lelem->INDEX)\
				break;					\
	(_lelem);							\
    })

#define NEXT_OBJECT_INT_LINK_INDEX(LIST, OID, SUB, LINK, INDEX) ({	\
	__typeof (TAILQ_FIRST(LIST)) _lelem;				\
									\
	if ((OID)->len - SUB == 0)					\
		_lelem = TAILQ_FIRST(LIST);				\
	else								\
		TAILQ_FOREACH(_lelem, (LIST), LINK)			\
			if ((OID)->subs[SUB] < (asn_subid_t)_lelem->INDEX)\
				break;					\
	(_lelem);							\
    })

/*
 * Macros for the case where the index field is called 'index'
 */
#define INSERT_OBJECT_OID_LINK(PTR, LIST, LINK)				\
    INSERT_OBJECT_OID_LINK_INDEX(PTR, LIST, LINK, index)

#define INSERT_OBJECT_INT_LINK(PTR, LIST, LINK) do {			\
    INSERT_OBJECT_INT_LINK_INDEX(PTR, LIST, LINK, index)

#define FIND_OBJECT_OID_LINK(LIST, OID, SUB, LINK)			\
    FIND_OBJECT_OID_LINK_INDEX(LIST, OID, SUB, LINK, index)

#define NEXT_OBJECT_OID_LINK(LIST, OID, SUB, LINK)			\
    NEXT_OBJECT_OID_LINK_INDEX(LIST, OID, SUB, LINK, index)

#define FIND_OBJECT_INT_LINK(LIST, OID, SUB, LINK)			\
    FIND_OBJECT_INT_LINK_INDEX(LIST, OID, SUB, LINK, index)

#define NEXT_OBJECT_INT_LINK(LIST, OID, SUB, LINK)			\
    NEXT_OBJECT_INT_LINK_INDEX(LIST, OID, SUB, LINK, index)

/*
 * Macros for the case where the index field is called 'index' and the
 * link field 'link'.
 */
#define INSERT_OBJECT_OID(PTR, LIST)					\
    INSERT_OBJECT_OID_LINK_INDEX(PTR, LIST, link, index)

#define INSERT_OBJECT_INT(PTR, LIST)					\
    INSERT_OBJECT_INT_LINK_INDEX(PTR, LIST, link, index)

#define FIND_OBJECT_OID(LIST, OID, SUB)					\
    FIND_OBJECT_OID_LINK_INDEX(LIST, OID, SUB, link, index)

#define FIND_OBJECT_INT(LIST, OID, SUB)					\
    FIND_OBJECT_INT_LINK_INDEX(LIST, OID, SUB, link, index)

#define NEXT_OBJECT_OID(LIST, OID, SUB)					\
    NEXT_OBJECT_OID_LINK_INDEX(LIST, OID, SUB, link, index)

#define NEXT_OBJECT_INT(LIST, OID, SUB)					\
    NEXT_OBJECT_INT_LINK_INDEX(LIST, OID, SUB, link, index)

struct lmodule;

/* The tick when the program was started. This is the absolute time of
 * the start in 100th of a second. */
extern uint64_t start_tick;

/* The tick when the current packet was received. This is the absolute
 * time in 100th of second. */
extern uint64_t this_tick;

/* Get the current absolute time in 100th of a second. */
uint64_t get_ticks(void);

/*
 * Return code for proxy function
 */
enum snmpd_proxy_err {
	/* proxy code will process the PDU */
	SNMPD_PROXY_OK,
	/* proxy code does not process PDU */
	SNMPD_PROXY_REJ,
	/* drop this PDU */
	SNMPD_PROXY_DROP,
	/* drop because of bad community */
	SNMPD_PROXY_BADCOMM,
	/* drop because of bad community use */
	SNMPD_PROXY_BADCOMMUSE
};

/*
 * Input handling
 */
enum snmpd_input_err {
	/* proceed with packet */
	SNMPD_INPUT_OK,
	/* fatal error in packet, ignore it */
	SNMPD_INPUT_FAILED,
	/* value encoding has wrong length in a SET operation */
	SNMPD_INPUT_VALBADLEN,
	/* value encoding is out of range */
	SNMPD_INPUT_VALRANGE,
	/* value has bad encoding */
	SNMPD_INPUT_VALBADENC,
	/* need more data (truncated packet) */
	SNMPD_INPUT_TRUNC,
	/* unknown community */
	SNMPD_INPUT_BAD_COMM,
};

/*
 * Every loadable module must have one of this structures with
 * the external name 'config'.
 */
struct snmp_module {
	/* a comment describing what this module implements */
	const char *comment;

	/* the initialisation function */
	int (*init)(struct lmodule *, int argc, char *argv[]);

	/* the finalisation function */
	int (*fini)(void);

	/* the idle function */
	void (*idle)(void);

	/* the dump function */
	void (*dump)(void);

	/* re-configuration function */
	void (*config)(void);

	/* start operation */
	void (*start)(void);

	/* proxy a PDU */
	enum snmpd_proxy_err (*proxy)(struct snmp_pdu *, void *,
	    const struct asn_oid *, const struct sockaddr *, socklen_t,
	    enum snmpd_input_err, int32_t, int);

	/* the tree this module is going to server */
	const struct snmp_node *tree;
	u_int tree_size;

	/* function called, when another module was unloaded/loaded */
	void (*loading)(const struct lmodule *, int);
};

/*
 * Stuff exported to modules
 */

/*
 * The system group.
 */
struct systemg {
	u_char		*descr;
	struct asn_oid	object_id;
	u_char		*contact;
	u_char		*name;
	u_char		*location;
	u_int32_t	services;
	u_int32_t	or_last_change;
};
extern struct systemg systemg;

/*
 * Community support.
 *
 * We have 2 fixed communities for SNMP read and write access. Modules
 * can create their communities dynamically. They are deleted automatically
 * if the module is unloaded.
 */
#define COMM_INITIALIZE	0
#define COMM_READ	1
#define COMM_WRITE	2

u_int comm_define(u_int, const char *descr, struct lmodule *, const char *str);
const char * comm_string(u_int);

/* community for current packet */
extern u_int community;

/* 
 * Well known OIDs
 */
extern const struct asn_oid oid_zeroDotZero;

/*
 * Request ID ranges.
 *
 * A module can request a range of request ids and associate them with a
 * type field. All ranges are deleted if a module is unloaded.
 */
u_int reqid_allocate(int size, struct lmodule *);
int32_t reqid_next(u_int type);
int32_t reqid_base(u_int type);
int reqid_istype(int32_t reqid, u_int type);
u_int reqid_type(int32_t reqid);

/*
 * Timers.
 */
void *timer_start(u_int, void (*)(void *), void *, struct lmodule *);
void timer_stop(void *);

/*
 * File descriptors
 */
void *fd_select(int, void (*)(int, void *), void *, struct lmodule *);
void fd_deselect(void *);
void fd_suspend(void *);
int fd_resume(void *);

/*
 * Object resources
 */
u_int or_register(const struct asn_oid *, const char *, struct lmodule *);
void or_unregister(u_int);

/*
 * Buffers
 */
void *buf_alloc(int tx);
size_t buf_size(int tx);

/* decode PDU and find community */
enum snmpd_input_err snmp_input_start(const u_char *, size_t, const char *,
    struct snmp_pdu *, int32_t *, size_t *);

/* process the pdu. returns either _OK or _FAILED */
enum snmpd_input_err snmp_input_finish(struct snmp_pdu *, const u_char *,
    size_t, u_char *, size_t *, const char *, enum snmpd_input_err, int32_t,
    void *);

void snmp_output(struct snmp_pdu *, u_char *, size_t *, const char *);
void snmp_send_port(void *, const struct asn_oid *, struct snmp_pdu *,
	const struct sockaddr *, socklen_t);

/* sending traps */
void snmp_send_trap(const struct asn_oid *, ...);

/*
 * Action support
 */
int string_save(struct snmp_value *, struct snmp_context *, ssize_t, u_char **);
void string_commit(struct snmp_context *);
void string_rollback(struct snmp_context *, u_char **);
int string_get(struct snmp_value *, const u_char *, ssize_t);
void string_free(struct snmp_context *);

int ip_save(struct snmp_value *, struct snmp_context *, u_char *);
void ip_rollback(struct snmp_context *, u_char *);
void ip_commit(struct snmp_context *);
int ip_get(struct snmp_value *, u_char *);

int oid_save(struct snmp_value *, struct snmp_context *, struct asn_oid *);
void oid_rollback(struct snmp_context *, struct asn_oid *);
void oid_commit(struct snmp_context *);
int oid_get(struct snmp_value *, const struct asn_oid *);

int index_decode(const struct asn_oid *oid, u_int sub, u_int code, ...);
int index_compare(const struct asn_oid *, u_int, const struct asn_oid *);
int index_compare_off(const struct asn_oid *, u_int, const struct asn_oid *,
    u_int);
void index_append(struct asn_oid *, u_int, const struct asn_oid *);
void index_append_off(struct asn_oid *, u_int, const struct asn_oid *, u_int);

#endif
