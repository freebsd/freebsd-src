/*-
 * Copyright (c) 1999, 2000, 2001, 2002 Robert N. M. Watson
 * Copyright (c) 2001 Ilmar S. Habibulin
 * Copyright (c) 2001, 2002, 2003 Networks Associates Technology, Inc.
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
 * $FreeBSD$
 */

/*
 * MAC Framework sysctl namespace.
 */
#ifdef SYSCTL_DECL
SYSCTL_DECL(_security);
SYSCTL_DECL(_security_mac);
#ifdef MAC_DEBUG
SYSCTL_DECL(_security_mac_debug);
SYSCTL_DECL(_security_mac_debug_counters);
#endif
#endif /* SYSCTL_DECL */

/*
 * MAC Framework global types and typedefs.
 */
LIST_HEAD(mac_policy_list_head, mac_policy_conf);
#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_MACTEMP);
#endif

/*
 * MAC Framework global variables.
 */
extern struct mac_policy_list_head	mac_policy_list;
extern struct mac_policy_list_head	mac_static_policy_list;
extern int				mac_late;
extern int				mac_enforce_process;
extern int				mac_enforce_sysv;
extern int				mac_enforce_vm;
#ifndef MAC_ALWAYS_LABEL_MBUF
extern int				mac_labelmbufs;
#endif

/*
 * MAC Framework object/access counter primitives, conditionally
 * compiled.
 */
#ifdef MAC_DEBUG
#define	MAC_DEBUG_COUNTER_INC(x)	atomic_add_int(x, 1);
#define	MAC_DEBUG_COUNTER_DEC(x)	atomic_subtract_int(x, 1);
#else
#define	MAC_DEBUG_COUNTER_INC(x)
#define	MAC_DEBUG_COUNTER_DEC(x)
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
 * MAC Framework per-object type functions.  It's not yet clear how
 * the namespaces, etc, should work for these, so for now, sort by
 * object type.
 */
struct label	*mac_pipe_label_alloc(void);
void		 mac_pipe_label_free(struct label *label);

int	mac_check_cred_relabel(struct ucred *cred, struct label *newlabel);
int	mac_externalize_cred_label(struct label *label, char *elements, 
	    char *outbuf, size_t outbuflen);
int	mac_internalize_cred_label(struct label *label, char *string);
void	mac_relabel_cred(struct ucred *cred, struct label *newlabel);

void	mac_copy_pipe_label(struct label *src, struct label *dest);
int	mac_externalize_pipe_label(struct label *label, char *elements,
	    char *outbuf, size_t outbuflen);
int	mac_internalize_pipe_label(struct label *label, char *string);

int	mac_socket_label_set(struct ucred *cred, struct socket *so,
	    struct label *label);

int	mac_externalize_vnode_label(struct label *label, char *elements,
	    char *outbuf, size_t outbuflen);
int	mac_internalize_vnode_label(struct label *label, char *string);
void	mac_check_vnode_mmap_downgrade(struct ucred *cred, struct vnode *vp,
	    int *prot);
int	vn_setlabel(struct vnode *vp, struct label *intlabel,
	    struct ucred *cred);

/*
 * MAC_CHECK performs the designated check by walking the policy module
 * list and checking with each as to how it feels about the request.
 * Note that it returns its value via 'error' in the scope of the caller.
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
 * MAC_BOOLEAN performs the designated boolean composition by walking
 * the module list, invoking each instance of the operation, and
 * combining the results using the passed C operator.  Note that it
 * returns its value via 'result' in the scope of the caller, which
 * should be initialized by the caller in a meaningful way to get
 * a meaningful result.
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
 * MAC_PERFORM performs the designated operation by walking the policy
 * module list and invoking that operation for each policy.
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
