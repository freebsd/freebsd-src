/*-
 * Copyright (c) 1999-2002, 2006 Robert N. M. Watson
 * Copyright (c) 2001 Ilmar S. Habibulin
 * Copyright (c) 2001-2004 Networks Associates Technology, Inc.
 * Copyright (c) 2006 nCircle Network Security, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert Watson and Ilmar Habibulin for the
 * TrustedBSD Project.
 *
 * This software was developed for the FreeBSD Project in part by Network
 * Associates Laboratories, the Security Research Division of Network
 * Associates, Inc. under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"),
 * as part of the DARPA CHATS research program.
 *
 * This software was developed by Robert N. M. Watson for the TrustedBSD
 * Project under contract to nCircle Network Security, Inc.
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
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/security/mac/mac_internal.h,v 1.121.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _SECURITY_MAC_MAC_INTERNAL_H_
#define	_SECURITY_MAC_MAC_INTERNAL_H_

#ifndef _KERNEL
#error "no user-serviceable parts inside"
#endif

/*
 * MAC Framework sysctl namespace.
 */
#ifdef SYSCTL_DECL
SYSCTL_DECL(_security_mac);
#endif /* SYSCTL_DECL */

/*
 * MAC Framework global types and typedefs.
 */
LIST_HEAD(mac_policy_list_head, mac_policy_conf);
#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_MACTEMP);
#endif

/*
 * MAC labels -- in-kernel storage format.
 *
 * In general, struct label pointers are embedded in kernel data structures
 * representing objects that may be labeled (and protected).  Struct label is
 * opaque to both kernel services that invoke the MAC Framework and MAC
 * policy modules.  In particular, we do not wish to encode the layout of the
 * label structure into any ABIs.  Historically, the slot array contained
 * unions of {long, void} but now contains uintptr_t.
 */
#define	MAC_MAX_SLOTS	4
#define	MAC_FLAG_INITIALIZED	0x0000001	/* Is initialized for use. */
struct label {
	int		l_flags;
	intptr_t	l_perpolicy[MAC_MAX_SLOTS];
};

/*
 * MAC Framework global variables.
 */
extern struct mac_policy_list_head	mac_policy_list;
extern struct mac_policy_list_head	mac_static_policy_list;
#ifndef MAC_ALWAYS_LABEL_MBUF
extern int				mac_labelmbufs;
#endif

/*
 * MAC Framework infrastructure functions.
 */
int	mac_error_select(int error1, int error2);

void	mac_policy_grab_exclusive(void);
void	mac_policy_assert_exclusive(void);
void	mac_policy_release_exclusive(void);
void	mac_policy_list_busy(void);
int	mac_policy_list_conditional_busy(void);
void	mac_policy_list_unbusy(void);

struct label	*mac_labelzone_alloc(int flags);
void		 mac_labelzone_free(struct label *label);
void		 mac_labelzone_init(void);

void	mac_init_label(struct label *label);
void	mac_destroy_label(struct label *label);
int	mac_check_structmac_consistent(struct mac *mac);
int	mac_allocate_slot(void);

/*
 * MAC Framework per-object type functions.  It's not yet clear how the
 * namespaces, etc, should work for these, so for now, sort by object type.
 */
struct label	*mac_pipe_label_alloc(void);
void		 mac_pipe_label_free(struct label *label);
struct label	*mac_socket_label_alloc(int flag);
void		 mac_socket_label_free(struct label *label);

int	mac_check_cred_relabel(struct ucred *cred, struct label *newlabel);
int	mac_externalize_cred_label(struct label *label, char *elements,
	    char *outbuf, size_t outbuflen);
int	mac_internalize_cred_label(struct label *label, char *string);
void	mac_relabel_cred(struct ucred *cred, struct label *newlabel);

struct label	*mac_mbuf_to_label(struct mbuf *m);

void	mac_copy_pipe_label(struct label *src, struct label *dest);
int	mac_externalize_pipe_label(struct label *label, char *elements,
	    char *outbuf, size_t outbuflen);
int	mac_internalize_pipe_label(struct label *label, char *string);

int	mac_socket_label_set(struct ucred *cred, struct socket *so,
	    struct label *label);
void	mac_copy_socket_label(struct label *src, struct label *dest);
int	mac_externalize_socket_label(struct label *label, char *elements,
	    char *outbuf, size_t outbuflen);
int	mac_internalize_socket_label(struct label *label, char *string);

int	mac_externalize_vnode_label(struct label *label, char *elements,
	    char *outbuf, size_t outbuflen);
int	mac_internalize_vnode_label(struct label *label, char *string);
void	mac_check_vnode_mmap_downgrade(struct ucred *cred, struct vnode *vp,
	    int *prot);
int	vn_setlabel(struct vnode *vp, struct label *intlabel,
	    struct ucred *cred);

/*
 * MAC_CHECK performs the designated check by walking the policy module list
 * and checking with each as to how it feels about the request.  Note that it
 * returns its value via 'error' in the scope of the caller.
 */
#define	MAC_CHECK(check, args...) do {					\
	struct mac_policy_conf *mpc;					\
	int entrycount;							\
									\
	error = 0;							\
	LIST_FOREACH(mpc, &mac_static_policy_list, mpc_list) {		\
		if (mpc->mpc_ops->mpo_ ## check != NULL)		\
			error = mac_error_select(			\
			    mpc->mpc_ops->mpo_ ## check (args),		\
			    error);					\
	}								\
	if ((entrycount = mac_policy_list_conditional_busy()) != 0) {	\
		LIST_FOREACH(mpc, &mac_policy_list, mpc_list) {		\
			if (mpc->mpc_ops->mpo_ ## check != NULL)	\
				error = mac_error_select(		\
				    mpc->mpc_ops->mpo_ ## check (args),	\
				    error);				\
		}							\
		mac_policy_list_unbusy();				\
	}								\
} while (0)

/*
 * MAC_GRANT performs the designated check by walking the policy module list
 * and checking with each as to how it feels about the request.  Unlike
 * MAC_CHECK, it grants if any policies return '0', and otherwise returns
 * EPERM.  Note that it returns its value via 'error' in the scope of the
 * caller.
 */
#define	MAC_GRANT(check, args...) do {					\
	struct mac_policy_conf *mpc;					\
	int entrycount;							\
									\
	error = EPERM;							\
	LIST_FOREACH(mpc, &mac_static_policy_list, mpc_list) {		\
		if (mpc->mpc_ops->mpo_ ## check != NULL) {		\
			if (mpc->mpc_ops->mpo_ ## check(args) == 0)	\
				error = 0;				\
		}							\
	}								\
	if ((entrycount = mac_policy_list_conditional_busy()) != 0) {	\
		LIST_FOREACH(mpc, &mac_policy_list, mpc_list) {		\
			if (mpc->mpc_ops->mpo_ ## check != NULL) {	\
				if (mpc->mpc_ops->mpo_ ## check (args)	\
				    == 0)				\
					error = 0;			\
			}						\
		}							\
		mac_policy_list_unbusy();				\
	}								\
} while (0)

/*
 * MAC_BOOLEAN performs the designated boolean composition by walking the
 * module list, invoking each instance of the operation, and combining the
 * results using the passed C operator.  Note that it returns its value via
 * 'result' in the scope of the caller, which should be initialized by the
 * caller in a meaningful way to get a meaningful result.
 */
#define	MAC_BOOLEAN(operation, composition, args...) do {		\
	struct mac_policy_conf *mpc;					\
	int entrycount;							\
									\
	LIST_FOREACH(mpc, &mac_static_policy_list, mpc_list) {		\
		if (mpc->mpc_ops->mpo_ ## operation != NULL)		\
			result = result composition			\
			    mpc->mpc_ops->mpo_ ## operation (args);	\
	}								\
	if ((entrycount = mac_policy_list_conditional_busy()) != 0) {	\
		LIST_FOREACH(mpc, &mac_policy_list, mpc_list) {		\
			if (mpc->mpc_ops->mpo_ ## operation != NULL)	\
				result = result composition		\
				    mpc->mpc_ops->mpo_ ## operation	\
				    (args);				\
		}							\
		mac_policy_list_unbusy();				\
	}								\
} while (0)

/*
 * MAC_EXTERNALIZE queries each policy to see if it can generate an
 * externalized version of a label element by name.  Policies declare whether
 * they have matched a particular element name, parsed from the string by
 * MAC_EXTERNALIZE, and an error is returned if any element is matched by no
 * policy.
 */
#define	MAC_EXTERNALIZE(type, label, elementlist, outbuf, 		\
    outbuflen) do {							\
	int claimed, first, ignorenotfound, savedlen;			\
	char *element_name, *element_temp;				\
	struct sbuf sb;							\
									\
	error = 0;							\
	first = 1;							\
	sbuf_new(&sb, outbuf, outbuflen, SBUF_FIXEDLEN);		\
	element_temp = elementlist;					\
	while ((element_name = strsep(&element_temp, ",")) != NULL) {	\
		if (element_name[0] == '?') {				\
			element_name++;					\
			ignorenotfound = 1;				\
		 } else							\
			ignorenotfound = 0;				\
		savedlen = sbuf_len(&sb);				\
		if (first)						\
			error = sbuf_printf(&sb, "%s/", element_name);	\
		else							\
			error = sbuf_printf(&sb, ",%s/", element_name);	\
		if (error == -1) {					\
			error = EINVAL;	/* XXX: E2BIG? */		\
			break;						\
		}							\
		claimed = 0;						\
		MAC_CHECK(externalize_ ## type ## _label, label,	\
		    element_name, &sb, &claimed);			\
		if (error)						\
			break;						\
		if (claimed == 0 && ignorenotfound) {			\
			/* Revert last label name. */			\
			sbuf_setpos(&sb, savedlen);			\
		} else if (claimed != 1) {				\
			error = EINVAL;	/* XXX: ENOLABEL? */		\
			break;						\
		} else {						\
			first = 0;					\
		}							\
	}								\
	sbuf_finish(&sb);						\
} while (0)

/*
 * MAC_INTERNALIZE presents parsed element names and data to each policy to
 * see if any is willing to claim it and internalize the label data.  If no
 * policies match, an error is returned.
 */
#define	MAC_INTERNALIZE(type, label, instring) do {			\
	char *element, *element_name, *element_data;			\
	int claimed;							\
									\
	error = 0;							\
	element = instring;						\
	while ((element_name = strsep(&element, ",")) != NULL) {	\
		element_data = element_name;				\
		element_name = strsep(&element_data, "/");		\
		if (element_data == NULL) {				\
			error = EINVAL;					\
			break;						\
		}							\
		claimed = 0;						\
		MAC_CHECK(internalize_ ## type ## _label, label,	\
		    element_name, element_data, &claimed);		\
		if (error)						\
			break;						\
		if (claimed != 1) {					\
			/* XXXMAC: Another error here? */		\
			error = EINVAL;					\
			break;						\
		}							\
	}								\
} while (0)

/*
 * MAC_PERFORM performs the designated operation by walking the policy module
 * list and invoking that operation for each policy.
 */
#define	MAC_PERFORM(operation, args...) do {				\
	struct mac_policy_conf *mpc;					\
	int entrycount;							\
									\
	LIST_FOREACH(mpc, &mac_static_policy_list, mpc_list) {		\
		if (mpc->mpc_ops->mpo_ ## operation != NULL)		\
			mpc->mpc_ops->mpo_ ## operation (args);		\
	}								\
	if ((entrycount = mac_policy_list_conditional_busy()) != 0) {	\
		LIST_FOREACH(mpc, &mac_policy_list, mpc_list) {		\
			if (mpc->mpc_ops->mpo_ ## operation != NULL)	\
				mpc->mpc_ops->mpo_ ## operation (args);	\
		}							\
		mac_policy_list_unbusy();				\
	}								\
} while (0)

#endif /* !_SECURITY_MAC_MAC_INTERNAL_H_ */
