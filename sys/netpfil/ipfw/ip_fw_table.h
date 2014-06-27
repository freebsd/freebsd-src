/*-
 * Copyright (c) 2002-2009 Luigi Rizzo, Universita` di Pisa
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
 * $FreeBSD: projects/ipfw/sys/netpfil/ipfw/ip_fw_private.h 267467 2014-06-14 10:58:39Z melifaro $
 */

#ifndef _IPFW2_TABLE_H
#define _IPFW2_TABLE_H

/*
 * Internal constants and data structures used by ipfw tables
 * and not meant to be exported outside the kernel.
 */
#ifdef _KERNEL

struct table_info {
	table_lookup_t	*lookup;	/* Lookup function */
	void		*state;		/* Lookup radix/other structure */
	void		*xstate;	/* eXtended state */
	u_long		data;		/* Hints for given func */
};

struct tentry_info {
	void		*paddr;
	int		plen;		/* Total entry length		*/
	uint8_t		masklen;	/* mask length			*/
	uint8_t		spare;
	uint16_t	flags;		/* record flags			*/
	uint32_t	value;		/* value			*/
};

typedef int (ta_init)(void **ta_state, struct table_info *ti, char *data);
typedef void (ta_destroy)(void *ta_state, struct table_info *ti);
typedef int (ta_prepare_add)(struct tentry_info *tei, void *ta_buf);
typedef int (ta_prepare_del)(struct tentry_info *tei, void *ta_buf);
typedef int (ta_add)(void *ta_state, struct table_info *ti,
    struct tentry_info *tei, void *ta_buf);
typedef int (ta_del)(void *ta_state, struct table_info *ti,
    struct tentry_info *tei, void *ta_buf);
typedef void (ta_flush_entry)(struct tentry_info *tei, void *ta_buf);

typedef int ta_foreach_f(void *node, void *arg);
typedef void ta_foreach(void *ta_state, struct table_info *ti, ta_foreach_f *f,
  void *arg);
typedef int ta_dump_entry(void *ta_state, struct table_info *ti, void *e,
    ipfw_table_entry *ent);
typedef int ta_dump_xentry(void *ta_state, struct table_info *ti, void *e,
    ipfw_table_xentry *xent);

struct table_algo {
	char		name[16];
	int		idx;
	ta_init		*init;
	ta_destroy	*destroy;
	table_lookup_t	*lookup;
	ta_prepare_add	*prepare_add;
	ta_prepare_del	*prepare_del;
	ta_add		*add;
	ta_del		*del;
	ta_flush_entry	*flush_entry;
	ta_foreach	*foreach;
	ta_dump_entry	*dump_entry;
	ta_dump_xentry	*dump_xentry;
};
void ipfw_add_table_algo(struct ip_fw_chain *ch, struct table_algo *ta);
extern struct table_algo radix_cidr, radix_iface;

void ipfw_table_algo_init(struct ip_fw_chain *chain);
void ipfw_table_algo_destroy(struct ip_fw_chain *chain);


/* direct ipfw_ctl handlers */
int ipfw_listsize_tables(struct ip_fw_chain *ch, struct sockopt_data *sd);
int ipfw_list_tables(struct ip_fw_chain *ch, struct sockopt_data *sd);
int ipfw_dump_table(struct ip_fw_chain *ch, ip_fw3_opheader *op3,
    struct sockopt_data *sd);
int ipfw_describe_table(struct ip_fw_chain *ch, struct sockopt_data *sd);

int ipfw_create_table(struct ip_fw_chain *ch, struct sockopt *sopt,
    ip_fw3_opheader *op3);
int ipfw_modify_table(struct ip_fw_chain *ch, struct sockopt *sopt,
    ip_fw3_opheader *op3);

int ipfw_destroy_table(struct ip_fw_chain *ch, struct tid_info *ti);
int ipfw_flush_table(struct ip_fw_chain *ch, struct tid_info *ti);
int ipfw_add_table_entry(struct ip_fw_chain *ch, struct tid_info *ti,
    struct tentry_info *tei);
int ipfw_del_table_entry(struct ip_fw_chain *ch, struct tid_info *ti,
    struct tentry_info *tei);
int ipfw_rewrite_table_uidx(struct ip_fw_chain *chain,
    struct rule_check_info *ci);
int ipfw_rewrite_table_kidx(struct ip_fw_chain *chain, struct ip_fw *rule);
void ipfw_unbind_table_rule(struct ip_fw_chain *chain, struct ip_fw *rule);
void ipfw_unbind_table_list(struct ip_fw_chain *chain, struct ip_fw *head);

/* utility functions  */
void objheader_to_ti(struct _ipfw_obj_header *oh, struct tid_info *ti);

/* Legacy interfaces */
int ipfw_count_table(struct ip_fw_chain *ch, struct tid_info *ti,
    uint32_t *cnt);
int ipfw_count_xtable(struct ip_fw_chain *ch, struct tid_info *ti,
    uint32_t *cnt);
int ipfw_dump_table_legacy(struct ip_fw_chain *ch, struct tid_info *ti,
    ipfw_table *tbl);


#endif /* _KERNEL */
#endif /* _IPFW2_TABLE_H */
