#include <atf-c.h>
#include <alias.h>
#include <stdio.h>
#include <stdlib.h>

#include "util.h"

/* common ip ranges */
static struct in_addr masq = { htonl(0x01020304) };
static struct in_addr pub  = { htonl(0x0102dead) };
static struct in_addr prv1 = { htonl(0x0a00dead) };
static struct in_addr prv2 = { htonl(0xac10dead) };
static struct in_addr prv3 = { htonl(0xc0a8dead) };
static struct in_addr cgn  = { htonl(0x6440dead) };
static struct in_addr ext  = { htonl(0x12345678) };

#define NAT_CHECK(pip, src, msq)	do {	\
	int res;				\
	int len = ntohs(pip->ip_len);		\
	struct in_addr dst = pip->ip_dst;	\
	pip->ip_src = src;			\
	res = LibAliasOut(la, pip, len);	\
	ATF_CHECK_MSG(res == PKT_ALIAS_OK,	\
	    ">%d< not met PKT_ALIAS_OK", res);	\
	ATF_CHECK(addr_eq(msq, pip->ip_src));	\
	ATF_CHECK(addr_eq(dst, pip->ip_dst));	\
} while(0)

#define NAT_FAIL(pip, src, dst)	do {		\
	int res;				\
	int len = ntohs(pip->ip_len);		\
	pip->ip_src = src;			\
	pip->ip_dst = dst;			\
	res = LibAliasOut(la, pip, len);	\
	ATF_CHECK_MSG(res != PKT_ALIAS_OK),	\
	    ">%d< not met !PKT_ALIAS_OK", res);	\
	ATF_CHECK(addr_eq(src, pip->ip_src));	\
	ATF_CHECK(addr_eq(dst, pip->ip_dst));	\
} while(0)

#define UNNAT_CHECK(pip, src, dst, rel)	do {	\
	int res;				\
	int len = ntohs(pip->ip_len);		\
	pip->ip_src = src;			\
	pip->ip_dst = dst;			\
	res = LibAliasIn(la, pip, len);		\
	ATF_CHECK_MSG(res == PKT_ALIAS_OK,	\
	    ">%d< not met PKT_ALIAS_OK", res);	\
	ATF_CHECK(addr_eq(src, pip->ip_src));	\
	ATF_CHECK(addr_eq(rel, pip->ip_dst));	\
} while(0)

#define UNNAT_FAIL(pip, src, dst)	do {	\
	int res;				\
	int len = ntohs(pip->ip_len);		\
	pip->ip_src = src;			\
	pip->ip_dst = dst;			\
	res = LibAliasIn(la, pip, len);		\
	ATF_CHECK_MSG(res != PKT_ALIAS_OK,	\
	    ">%d< not met !PKT_ALIAS_OK", res);	\
	ATF_CHECK(addr_eq(src, pip->ip_src));	\
	ATF_CHECK(addr_eq(dst, pip->ip_dst));	\
} while(0)

ATF_TC_WITHOUT_HEAD(1_simplemasq);
ATF_TC_BODY(1_simplemasq, dummy)
{
	struct libalias *la = LibAliasInit(NULL);
	struct ip *pip;

	ATF_REQUIRE(la != NULL);
	LibAliasSetAddress(la, masq);
	LibAliasSetMode(la, 0, ~0);

	pip = ip_packet(prv1, ext, 254, 64);
	NAT_CHECK(pip, prv1, masq);
	NAT_CHECK(pip, prv2, masq);
	NAT_CHECK(pip, prv3, masq);
	NAT_CHECK(pip, cgn,  masq);
	NAT_CHECK(pip, pub,  masq);

	free(pip);
	LibAliasUninit(la);
}

ATF_TC_WITHOUT_HEAD(2_unregistered);
ATF_TC_BODY(2_unregistered, dummy)
{
	struct libalias *la = LibAliasInit(NULL);
	struct ip *pip;

	ATF_REQUIRE(la != NULL);
	LibAliasSetAddress(la, masq);
	LibAliasSetMode(la, PKT_ALIAS_UNREGISTERED_ONLY, ~0);

	pip = ip_packet(prv1, ext, 254, 64);
	NAT_CHECK(pip, prv1, masq);
	NAT_CHECK(pip, prv2, masq);
	NAT_CHECK(pip, prv3, masq);
	NAT_CHECK(pip, cgn,  cgn);
	NAT_CHECK(pip, pub,  pub);

	/*
	 * State is only for new connections
	 * Because they are now active,
	 * the mode setting should be ignored
	 */
	LibAliasSetMode(la, 0, PKT_ALIAS_UNREGISTERED_ONLY);
	NAT_CHECK(pip, prv1, masq);
	NAT_CHECK(pip, prv2, masq);
	NAT_CHECK(pip, prv3, masq);
	NAT_CHECK(pip, cgn,  cgn);
	NAT_CHECK(pip, pub,  pub);

	free(pip);
	LibAliasUninit(la);
}

ATF_TC_WITHOUT_HEAD(3_cgn);
ATF_TC_BODY(3_cgn, dummy)
{
	struct libalias *la = LibAliasInit(NULL);
	struct ip *pip;

	ATF_REQUIRE(la != NULL);
	LibAliasSetAddress(la, masq);
	LibAliasSetMode(la, PKT_ALIAS_UNREGISTERED_CGN, ~0);

	pip = ip_packet(prv1, ext, 254, 64);
	NAT_CHECK(pip, prv1, masq);
	NAT_CHECK(pip, prv2, masq);
	NAT_CHECK(pip, prv3, masq);
	NAT_CHECK(pip, cgn,  masq);
	NAT_CHECK(pip, pub,  pub);

	/*
	 * State is only for new connections
	 * Because they are now active,
	 * the mode setting should be ignored
	 */
	LibAliasSetMode(la, 0, PKT_ALIAS_UNREGISTERED_CGN);
	NAT_CHECK(pip, prv1, masq);
	NAT_CHECK(pip, prv2, masq);
	NAT_CHECK(pip, prv3, masq);
	NAT_CHECK(pip, cgn,  masq);
	NAT_CHECK(pip, pub,  pub);

	free(pip);
	LibAliasUninit(la);
}

ATF_TC_WITHOUT_HEAD(4_udp);
ATF_TC_BODY(4_udp, dummy)
{
	struct libalias *la = LibAliasInit(NULL);
	struct ip  *po, *pi;
	struct udphdr *ui, *uo;
	uint16_t sport = 0x1234;
	uint16_t dport = 0x5678;
	uint16_t aport;

	ATF_REQUIRE(la != NULL);
	LibAliasSetAddress(la, masq);
	LibAliasSetMode(la, 0, ~0);

	/* Query from prv1 */
	po = ip_packet(prv1, ext, 0, 64);
	uo = set_udp(po, sport, dport);
	NAT_CHECK(po, prv1, masq);
	ATF_CHECK(uo->uh_dport == htons(dport));
	ATF_CHECK(addr_eq(po->ip_dst, ext));
	aport = ntohs(uo->uh_sport);
	/* should use a different external port */
	ATF_CHECK(aport != sport);

	/* Response */
	pi = ip_packet(po->ip_dst, po->ip_src, 0, 64);
	ui = set_udp(pi, ntohs(uo->uh_dport), ntohs(uo->uh_sport));
	UNNAT_CHECK(pi, ext, masq, prv1);
	ATF_CHECK(ui->uh_sport == htons(dport));
	ATF_CHECK(ui->uh_dport == htons(sport));

	/* Query from different source with same ports */
	uo = set_udp(po, sport, dport);
	NAT_CHECK(po, prv2, masq);
	ATF_CHECK(uo->uh_dport == htons(dport));
	ATF_CHECK(addr_eq(po->ip_dst, ext));
	/* should use a different external port */
	ATF_CHECK(uo->uh_sport != htons(aport));

	/* Response to prv2 */
	ui->uh_dport = uo->uh_sport;
	UNNAT_CHECK(pi, ext, masq, prv2);
	ATF_CHECK(ui->uh_sport == htons(dport));
	ATF_CHECK(ui->uh_dport == htons(sport));

	/* Response to prv1 again */
	ui->uh_dport = htons(aport);
	UNNAT_CHECK(pi, ext, masq, prv1);
	ATF_CHECK(ui->uh_sport == htons(dport));
	ATF_CHECK(ui->uh_dport == htons(sport));

	free(pi);
	free(po);
	LibAliasUninit(la);
}

ATF_TC_WITHOUT_HEAD(5_sameport);
ATF_TC_BODY(5_sameport, dummy)
{
	struct libalias *la = LibAliasInit(NULL);
	struct ip  *p;
	struct udphdr *u;
	uint16_t sport = 0x1234;
	uint16_t dport = 0x5678;
	uint16_t aport;

	ATF_REQUIRE(la != NULL);
	LibAliasSetAddress(la, masq);
	LibAliasSetMode(la, PKT_ALIAS_SAME_PORTS, ~0);

	/* Query from prv1 */
	p = ip_packet(prv1, ext, 0, 64);
	u = set_udp(p, sport, dport);
	NAT_CHECK(p, prv1, masq);
	ATF_CHECK(u->uh_dport == htons(dport));
	ATF_CHECK(addr_eq(p->ip_dst, ext));
	aport = ntohs(u->uh_sport);
	/* should use the same external port */
	ATF_CHECK(aport == sport);

	/* Query from different source with same ports */
	u = set_udp(p, sport, dport);
	NAT_CHECK(p, prv2, masq);
	ATF_CHECK(u->uh_dport == htons(dport));
	ATF_CHECK(addr_eq(p->ip_dst, ext));
	/* should use a different external port */
	ATF_CHECK(u->uh_sport != htons(aport));

	free(p);
	LibAliasUninit(la);
}

ATF_TC_WITHOUT_HEAD(6_cleartable);
ATF_TC_BODY(6_cleartable, dummy)
{
	struct libalias *la = LibAliasInit(NULL);
	struct ip  *po, *pi;
	struct udphdr *ui, *uo;
	uint16_t sport = 0x1234;
	uint16_t dport = 0x5678;
	uint16_t aport;

	ATF_REQUIRE(la != NULL);
	LibAliasSetAddress(la, masq);
	LibAliasSetMode(la, PKT_ALIAS_RESET_ON_ADDR_CHANGE, ~0);
	LibAliasSetMode(la, PKT_ALIAS_SAME_PORTS, PKT_ALIAS_SAME_PORTS);
	LibAliasSetMode(la, PKT_ALIAS_DENY_INCOMING, PKT_ALIAS_DENY_INCOMING);

	/* Query from prv1 */
	po = ip_packet(prv1, ext, 0, 64);
	uo = set_udp(po, sport, dport);
	NAT_CHECK(po, prv1, masq);
	ATF_CHECK(uo->uh_dport == htons(dport));
	ATF_CHECK(addr_eq(po->ip_dst, ext));
	aport = ntohs(uo->uh_sport);
	/* should use the same external port */
	ATF_CHECK(aport == sport);

	/* Response */
	pi = ip_packet(po->ip_dst, po->ip_src, 0, 64);
	ui = set_udp(pi, ntohs(uo->uh_dport), ntohs(uo->uh_sport));
	UNNAT_CHECK(pi, ext, masq, prv1);
	ATF_CHECK(ui->uh_sport == htons(dport));
	ATF_CHECK(ui->uh_dport == htons(sport));

	/* clear table by keeping the address */
	LibAliasSetAddress(la, ext);
	LibAliasSetAddress(la, masq);

	/* Response to prv1 again -> DENY_INCOMING */
	ui->uh_dport = htons(aport);
	UNNAT_FAIL(pi, ext, masq);

	/* Query from different source with same ports */
	uo = set_udp(po, sport, dport);
	NAT_CHECK(po, prv2, masq);
	ATF_CHECK(uo->uh_dport == htons(dport));
	ATF_CHECK(addr_eq(po->ip_dst, ext));
	/* should use the same external port, because it's free */
	ATF_CHECK(uo->uh_sport == htons(aport));

	/* Response to prv2 */
	ui->uh_dport = uo->uh_sport;
	UNNAT_CHECK(pi, ext, masq, prv2);
	ATF_CHECK(ui->uh_sport == htons(dport));
	ATF_CHECK(ui->uh_dport == htons(sport));

	free(pi);
	free(po);
	LibAliasUninit(la);
}

ATF_TC_WITHOUT_HEAD(7_stress);
ATF_TC_BODY(7_stress, dummy)
{
	struct libalias *la = LibAliasInit(NULL);
	struct ip *p;
	struct udphdr *u;
	struct {
		struct in_addr src, dst;
		uint16_t sport, dport, aport;
	} *batch;
	size_t const batch_size = 1200;
	size_t const rounds = 25;
	size_t i, j;

	ATF_REQUIRE(la != NULL);
	LibAliasSetAddress(la, masq);

	p = ip_packet(prv1, ext, 0, 64);
	u = set_udp(p, 0, 0);

	batch = calloc(batch_size, sizeof(*batch));
	ATF_REQUIRE(batch != NULL);
	for (j = 0; j < rounds; j++) {
		for (i = 0; i < batch_size; i++) {
			struct in_addr s, d;
			switch (i&3) {
			case 0: s = prv1; d = ext; break;
			case 1: s = prv2; d = pub; break;
			case 2: s = prv3; d = ext; break;
			case 3: s = cgn;  d = pub; break;
			}
			s.s_addr &= htonl(0xffff0000);
			d.s_addr &= htonl(0xffff0000);
			batch[i].src.s_addr = s.s_addr | htonl(rand_range(0, 0xffff));
			batch[i].dst.s_addr = d.s_addr | htonl(rand_range(0, 0xffff));
			batch[i].sport = rand_range(1000, 60000);
			batch[i].dport = rand_range(1000, 60000);
		}

		for (i = 0; i < batch_size; i++) {
			p->ip_dst = batch[i].dst;
			u = set_udp(p, batch[i].sport, batch[i].dport);
			NAT_CHECK(p, batch[i].src, masq);
			ATF_CHECK(u->uh_dport == htons(batch[i].dport));
			ATF_CHECK(addr_eq(p->ip_dst, batch[i].dst));
			batch[i].aport = htons(u->uh_sport);
		}

		qsort(batch, batch_size, sizeof(*batch), randcmp);

		for (i = 0; i < batch_size; i++) {
			u = set_udp(p, batch[i].dport, batch[i].aport);
			UNNAT_CHECK(p, batch[i].dst, masq, batch[i].src);
			ATF_CHECK(u->uh_dport == htons(batch[i].sport));
			ATF_CHECK(u->uh_sport == htons(batch[i].dport));
		}
	}

	free(batch);
	free(p);
	LibAliasUninit(la);
}

ATF_TP_ADD_TCS(natout)
{
	/* Use "dd if=/dev/random bs=2 count=1 | od -x" to reproduce */
	srand(0x0b61);

	ATF_TP_ADD_TC(natout, 1_simplemasq);
	ATF_TP_ADD_TC(natout, 2_unregistered);
	ATF_TP_ADD_TC(natout, 3_cgn);
	ATF_TP_ADD_TC(natout, 4_udp);
	ATF_TP_ADD_TC(natout, 5_sameport);
	ATF_TP_ADD_TC(natout, 6_cleartable);
	ATF_TP_ADD_TC(natout, 7_stress);

	return atf_no_error();
}
