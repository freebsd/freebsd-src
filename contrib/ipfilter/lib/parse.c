/*	$FreeBSD$	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: parse.c,v 1.34.2.1 2004/12/09 19:41:21 darrenr Exp $
 */
#include <ctype.h>
#include "ipf.h"
#include "opts.h"

static frentry_t *fp = NULL;

/* parse()
 *
 * parse a line read from the input filter rule file
 */
struct	frentry	*parse(line, linenum)
char	*line;
int     linenum;
{
	static fripf_t fip;
	char *cps[31], **cpp, *endptr, *proto = NULL, *s;
	struct protoent	*p = NULL;
	int i, cnt = 1, j;
	u_int k;

	if (fp == NULL) {
		fp = malloc(sizeof(*fp));
		if (fp == NULL)
			return NULL;
	}

	while (*line && ISSPACE(*line))
		line++;
	if (!*line)
		return NULL;

	bzero((char *)fp, sizeof(*fp));
	bzero((char *)&fip, sizeof(fip));
	fp->fr_v = use_inet6 ? 6 : 4;
	fp->fr_ipf = &fip;
	fp->fr_dsize = sizeof(fip);
	fp->fr_ip.fi_v = fp->fr_v;
	fp->fr_mip.fi_v = 0xf;
	fp->fr_type = FR_T_NONE;
	fp->fr_loglevel = 0xffff;
	fp->fr_isc = (void *)-1;
	fp->fr_tag = FR_NOTAG;

	/*
	 * break line up into max of 20 segments
	 */
	if (opts & OPT_DEBUG)
		fprintf(stderr, "parse [%s]\n", line);
	for (i = 0, *cps = strtok(line, " \b\t\r\n"); cps[i] && i < 30; cnt++)
		cps[++i] = strtok(NULL, " \b\t\r\n");
	cps[i] = NULL;

	if (cnt < 3) {
		fprintf(stderr, "%d: not enough segments in line\n", linenum);
		return NULL;
	}

	cpp = cps;
	/*
	 * The presence of an '@' followed by a number gives the position in
	 * the current rule list to insert this one.
	 */
	if (**cpp == '@')
		fp->fr_hits = (U_QUAD_T)atoi(*cpp++ + 1) + 1;

	/*
	 * Check the first keyword in the rule and any options that are
	 * expected to follow it.
	 */
	if (!strcasecmp("block", *cpp)) {
		fp->fr_flags |= FR_BLOCK;
		if (!strncasecmp(*(cpp+1), "return-icmp-as-dest", 19) &&
		    (i = 19))
			fp->fr_flags |= FR_FAKEICMP;
		else if (!strncasecmp(*(cpp+1), "return-icmp", 11) && (i = 11))
			fp->fr_flags |= FR_RETICMP;
		if (fp->fr_flags & FR_RETICMP) {
			cpp++;
			if (strlen(*cpp) == i) {
				if (*(cpp + 1) && **(cpp +1) == '(') {
					cpp++;
					i = 0;
				} else
					i = -1;
			}

			/*
			 * The ICMP code is not required to follow in ()'s
			 */
			if ((i >= 0) && (*(*cpp + i) == '(')) {
				i++;
				j = icmpcode(*cpp + i);
				if (j == -1) {
					fprintf(stderr,
					"%d: unrecognised icmp code %s\n",
						linenum, *cpp + 20);
					return NULL;
				}
				fp->fr_icode = j;
			}
		} else if (!strncasecmp(*(cpp+1), "return-rst", 10)) {
			fp->fr_flags |= FR_RETRST;
			cpp++;
		}
	} else if (!strcasecmp("count", *cpp)) {
		fp->fr_flags |= FR_ACCOUNT;
	} else if (!strcasecmp("pass", *cpp)) {
		fp->fr_flags |= FR_PASS;
	} else if (!strcasecmp("auth", *cpp)) {
		 fp->fr_flags |= FR_AUTH;
	} else if (fp->fr_arg != 0) {
		printf("skip %u", fp->fr_arg);
	} else if (!strcasecmp("preauth", *cpp)) {
		 fp->fr_flags |= FR_PREAUTH;
	} else if (!strcasecmp("nomatch", *cpp)) {
		 fp->fr_flags |= FR_NOMATCH;
	} else if (!strcasecmp("skip", *cpp)) {
		cpp++;
		if (ratoui(*cpp, &k, 0, UINT_MAX))
			fp->fr_arg = k;
		else {
			fprintf(stderr, "%d: integer must follow skip\n",
				linenum);
			return NULL;
		}
	} else if (!strcasecmp("log", *cpp)) {
		fp->fr_flags |= FR_LOG;
		if (!strcasecmp(*(cpp+1), "body")) {
			fp->fr_flags |= FR_LOGBODY;
			cpp++;
		}
		if (!strcasecmp(*(cpp+1), "first")) {
			fp->fr_flags |= FR_LOGFIRST;
			cpp++;
		}
		if (*cpp && !strcasecmp(*(cpp+1), "or-block")) {
			fp->fr_flags |= FR_LOGORBLOCK;
			cpp++;
		}
		if (!strcasecmp(*(cpp+1), "level")) {
			cpp++;
			if (loglevel(cpp, &fp->fr_loglevel, linenum) == -1)
				return NULL;
			cpp++;
		}
	} else {
		/*
		 * Doesn't start with one of the action words
		 */
		fprintf(stderr, "%d: unknown keyword (%s)\n", linenum, *cpp);
		return NULL;
	}
	if (!*++cpp) {
		fprintf(stderr, "%d: missing 'in'/'out' keyword\n", linenum);
		return NULL;
	}

	/*
	 * Get the direction for filtering.  Impose restrictions on direction
	 * if blocking with returning ICMP or an RST has been requested.
	 */
	if (!strcasecmp("in", *cpp))
		fp->fr_flags |= FR_INQUE;
	else if (!strcasecmp("out", *cpp)) {
		fp->fr_flags |= FR_OUTQUE;
		if (fp->fr_flags & FR_RETICMP) {
			fprintf(stderr,
				"%d: Can only use return-icmp with 'in'\n",
				linenum);
			return NULL;
		} else if (fp->fr_flags & FR_RETRST) {
			fprintf(stderr,
				"%d: Can only use return-rst with 'in'\n",
				linenum);
			return NULL;
		}
	}
	if (!*++cpp) {
		fprintf(stderr, "%d: missing source specification\n", linenum);
		return NULL;
	}

	if (!strcasecmp("log", *cpp)) {
		if (!*++cpp) {
			fprintf(stderr, "%d: missing source specification\n",
				linenum);
			return NULL;
		}
		if (FR_ISPASS(fp->fr_flags))
			fp->fr_flags |= FR_LOGP;
		else if (FR_ISBLOCK(fp->fr_flags))
			fp->fr_flags |= FR_LOGB;
		if (*cpp && !strcasecmp(*cpp, "body")) {
			fp->fr_flags |= FR_LOGBODY;
			cpp++;
		}
		if (*cpp && !strcasecmp(*cpp, "first")) {
			fp->fr_flags |= FR_LOGFIRST;
			cpp++;
		}
		if (*cpp && !strcasecmp(*cpp, "or-block")) {
			if (!FR_ISPASS(fp->fr_flags)) {
				fprintf(stderr,
					"%d: or-block must be used with pass\n",
					linenum);
				return NULL;
			}
			fp->fr_flags |= FR_LOGORBLOCK;
			cpp++;
		}
		if (*cpp && !strcasecmp(*cpp, "level")) {
			if (loglevel(cpp, &fp->fr_loglevel, linenum) == -1)
				return NULL;
			cpp++;
			cpp++;
		}
	}

	if (*cpp && !strcasecmp("quick", *cpp)) {
		if (fp->fr_arg != 0) {
			fprintf(stderr, "%d: cannot use skip with quick\n",
				linenum);
			return NULL;
		}
		cpp++;
		fp->fr_flags |= FR_QUICK;
	}

	/*
	 * Parse rule options that are available if a rule is tied to an
	 * interface.
	 */
	*fp->fr_ifname = '\0';
	*fp->fr_oifname = '\0';
	if (*cpp && !strcasecmp(*cpp, "on")) {
		if (!*++cpp) {
			fprintf(stderr, "%d: interface name missing\n",
				linenum);
			return NULL;
		}
		(void)strncpy(fp->fr_ifname, *cpp, IFNAMSIZ-1);
		fp->fr_ifname[IFNAMSIZ-1] = '\0';
		cpp++;
		if (!*cpp) {
			if ((fp->fr_flags & FR_RETMASK) == FR_RETRST) {
				fprintf(stderr,
					"%d: %s can only be used with TCP\n",
					linenum, "return-rst");
				return NULL;
			}
			return fp;
		}

		if (!strcasecmp(*cpp, "out-via")) {
			if (fp->fr_flags & FR_OUTQUE) {
				fprintf(stderr,
					"out-via must be used with in\n");
				return NULL;
			}
			cpp++;
			(void)strncpy(fp->fr_oifname, *cpp, IFNAMSIZ-1);
			fp->fr_oifname[IFNAMSIZ-1] = '\0';
			cpp++;
		} else if (!strcasecmp(*cpp, "in-via")) {
			if (fp->fr_flags & FR_INQUE) {
				fprintf(stderr,
					"in-via must be used with out\n");
				return NULL;
			}
			cpp++;
			(void)strncpy(fp->fr_oifname, *cpp, IFNAMSIZ-1);
			fp->fr_oifname[IFNAMSIZ-1] = '\0';
			cpp++;
		}

		if (!strcasecmp(*cpp, "dup-to") && *(cpp + 1)) {
			cpp++;
			if (to_interface(&fp->fr_dif, *cpp, linenum))
				return NULL;
			cpp++;
		}
		if (*cpp && !strcasecmp(*cpp, "to") && *(cpp + 1)) {
			cpp++;
			if (to_interface(&fp->fr_tif, *cpp, linenum))
				return NULL;
			cpp++;
		} else if (*cpp && !strcasecmp(*cpp, "fastroute")) {
			if (!(fp->fr_flags & FR_INQUE)) {
				fprintf(stderr,
					"can only use %s with 'in'\n",
					"fastroute");
				return NULL;
			}
			fp->fr_flags |= FR_FASTROUTE;
			cpp++;
		}

		/*
		 * Set the "other" interface name.  Lets you specify both
		 * inbound and outbound interfaces for state rules.  Do not
		 * prevent both interfaces from being the same.
		 */
		strcpy(fp->fr_ifnames[3], "*");
		if ((*cpp != NULL) && (*(cpp + 1) != NULL) &&
		    ((((fp->fr_flags & FR_INQUE) != 0) &&
		      (strcasecmp(*cpp, "out-via") == 0)) ||
		     (((fp->fr_flags & FR_OUTQUE) != 0) &&
		      (strcasecmp(*cpp, "in-via") == 0)))) {
			cpp++;

			s = strchr(*cpp, ',');
			if (s != NULL) {
				*s++ = '\0';
				(void)strncpy(fp->fr_ifnames[3], s,
					      IFNAMSIZ - 1);
				fp->fr_ifnames[3][IFNAMSIZ - 1] = '\0';
			}

                        (void)strncpy(fp->fr_ifnames[2], *cpp, IFNAMSIZ - 1);
                        fp->fr_ifnames[2][IFNAMSIZ - 1] = '\0';
                        cpp++;
                } else
                        strcpy(fp->fr_ifnames[2], "*");

	}

	if (*cpp && !strcasecmp(*cpp, "tos")) {
		if (!*++cpp) {
			fprintf(stderr, "%d: tos missing value\n", linenum);
			return NULL;
		}
		fp->fr_tos = strtol(*cpp, NULL, 0);
		fp->fr_mip.fi_tos = 0xff;
		cpp++;
	}

	if (*cpp && !strcasecmp(*cpp, "ttl")) {
		if (!*++cpp) {
			fprintf(stderr, "%d: ttl missing hopcount value\n",
				linenum);
			return NULL;
		}
		if (ratoi(*cpp, &i, 0, 255))
			fp->fr_ttl = i;
		else {
			fprintf(stderr, "%d: invalid ttl (%s)\n",
				linenum, *cpp);
			return NULL;
		}
		fp->fr_mip.fi_ttl = 0xff;
		cpp++;
	}

	/*
	 * check for "proto <protoname>" only decode udp/tcp/icmp as protoname
	 */
	if (*cpp && !strcasecmp(*cpp, "proto")) {
		if (!*++cpp) {
			fprintf(stderr, "%d: protocol name missing\n", linenum);
			return NULL;
		}
		fp->fr_type = FR_T_IPF;
		proto = *cpp++;
		if (!strcasecmp(proto, "tcp/udp")) {
			fp->fr_flx |= FI_TCPUDP;
			fp->fr_mflx |= FI_TCPUDP;
		} else if (use_inet6 && !strcasecmp(proto, "icmp")) {
			fprintf(stderr,
"%d: use proto ipv6-icmp with IPv6 (or use proto 1 if you really mean icmp)\n",
				linenum);
			return NULL;
		} else {
			fp->fr_proto = getproto(proto);
			fp->fr_mip.fi_p = 0xff;
		}
	}
	if ((fp->fr_proto != IPPROTO_TCP) &&
	    ((fp->fr_flags & FR_RETMASK) == FR_RETRST)) {
		fprintf(stderr, "%d: %s can only be used with TCP\n",
			linenum, "return-rst");
		return NULL;
	}

	/*
	 * get the from host and bit mask to use against packets
	 */

	if (!*cpp) {
		fprintf(stderr, "%d: missing source specification\n", linenum);
		return NULL;
	}
	if (!strcasecmp(*cpp, "all")) {
		cpp++;
		if (!*cpp) {
			if (fp->fr_type == FR_T_NONE) {
				fp->fr_dsize = 0;
				fp->fr_data = NULL;
			}
			return fp;
		}
		fp->fr_type = FR_T_IPF;
#ifdef	IPFILTER_BPF
	} else if (!strcmp(*cpp, "{")) {
		struct bpf_program bpf;
		struct pcap *p;
		char **cp;
		u_32_t l;

		if (fp->fr_type != FR_T_NONE) {
			fprintf(stderr,
				"%d: cannot mix BPF/ipf matching\n", linenum);
			return NULL;
		}
		fp->fr_type = FR_T_BPFOPC;
		cpp++;
		if (!strncmp(*cpp, "0x", 2)) {
			fp->fr_data = malloc(4);
			for (cp = cpp, i = 0; *cp; cp++, i++) {
				if (!strcmp(*cp, "}"))
					break;
				fp->fr_data = realloc(fp->fr_data,
						      (i + 1) * 4);
				l = strtoul(*cp, NULL, 0);
				((u_32_t *)fp->fr_data)[i] = l;
			}
			if (!*cp) {
				fprintf(stderr, "Missing closing '}'\n");
				return NULL;
			}
			fp->fr_dsize = i * sizeof(l);
			bpf.bf_insns = fp->fr_data;
			bpf.bf_len = fp->fr_dsize / sizeof(struct bpf_insn);
		} else {
			for (cp = cpp; *cp; cp++) {
				if (!strcmp(*cp, "}"))
					break;
				(*cp)[-1] = ' ';
			}
			if (!*cp) {
				fprintf(stderr, "Missing closing '}'\n");
				return NULL;
			}

			bzero((char *)&bpf, sizeof(bpf));
			p = pcap_open_dead(DLT_RAW, 1);
			if (!p) {
				fprintf(stderr, "pcap_open_dead failed\n");
				return NULL;
			}

			if (pcap_compile(p, &bpf, *cpp, 1, 0xffffffff)) {
				pcap_perror(p, "ipf");
				pcap_close(p);
				fprintf(stderr, "pcap parsing failed\n");
				return NULL;
			}
			pcap_close(p);
			fp->fr_dsize = bpf.bf_len * sizeof(struct bpf_insn);
			fp->fr_data = bpf.bf_insns;
			if (!bpf_validate(fp->fr_data, bpf.bf_len)) {
				fprintf(stderr, "BPF validation failed\n");
				return NULL;
			}
			if (opts & OPT_DEBUG)
				bpf_dump(&bpf, 0);
		}
		cpp = cp;
		(*cpp)++;
#endif
	} else {
		fp->fr_type = FR_T_IPF;

		if (strcasecmp(*cpp, "from")) {
			fprintf(stderr, "%d: unexpected keyword (%s) - from\n",
				linenum, *cpp);
			return NULL;
		}
		if (!*++cpp) {
			fprintf(stderr, "%d: missing host after from\n",
				linenum);
			return NULL;
		}
		if (**cpp == '!') {
			fp->fr_flags |= FR_NOTSRCIP;
			(*cpp)++;
		} else if (!strcmp(*cpp, "!")) {
			fp->fr_flags |= FR_NOTSRCIP;
			cpp++;
		}

		s = *cpp;
		i = hostmask(&cpp, proto, fp->fr_ifname, (u_32_t *)&fp->fr_src,
			     (u_32_t *)&fp->fr_smsk, linenum);
		if (i == -1)
			return NULL;
		if (*fp->fr_ifname && !strcasecmp(s, fp->fr_ifname))
			fp->fr_satype = FRI_DYNAMIC;
		if (i == 1) {
			if (fp->fr_v == 6) {
				fprintf(stderr,
					"can only use pools with ipv4\n");
				return NULL;
			}
			fp->fr_satype = FRI_LOOKUP;
		}

		if (ports(&cpp, proto, &fp->fr_sport, &fp->fr_scmp,
			  &fp->fr_stop, linenum))
			return NULL;

		if (!*cpp) {
			fprintf(stderr, "%d: missing to fields\n", linenum);
			return NULL;
		}

		/*
		 * do the same for the to field (destination host)
		 */
		if (strcasecmp(*cpp, "to")) {
			fprintf(stderr, "%d: unexpected keyword (%s) - to\n",
				linenum, *cpp);
			return NULL;
		}
		if (!*++cpp) {
			fprintf(stderr, "%d: missing host after to\n", linenum);
			return NULL;
		}

		if (**cpp == '!') {
			fp->fr_flags |= FR_NOTDSTIP;
			(*cpp)++;
		} else if (!strcmp(*cpp, "!")) {
			fp->fr_flags |= FR_NOTDSTIP;
			cpp++;
		}

		s = *cpp;
		i = hostmask(&cpp, proto, fp->fr_ifname, (u_32_t *)&fp->fr_dst,
			     (u_32_t *)&fp->fr_dmsk, linenum);
		if (i == -1)
			return NULL;
		if (*fp->fr_ifname && !strcasecmp(s, fp->fr_ifname))
			fp->fr_datype = FRI_DYNAMIC;
		if (i == 1) {
			if (fp->fr_v == 6) {
				fprintf(stderr,
					"can only use pools with ipv4\n");
				return NULL;
			}
			fp->fr_datype = FRI_LOOKUP;
		}

		if (ports(&cpp, proto, &fp->fr_dport, &fp->fr_dcmp,
			  &fp->fr_dtop, linenum))
			return NULL;
	}

	if (fp->fr_type == FR_T_IPF) {
		/*
		 * check some sanity, make sure we don't have icmp checks
		 * with tcp or udp or visa versa.
		 */
		if (fp->fr_proto && (fp->fr_dcmp || fp->fr_scmp) &&
		    fp->fr_proto != IPPROTO_TCP &&
		    fp->fr_proto != IPPROTO_UDP) {
			fprintf(stderr,
				"%d: port operation on non tcp/udp\n",linenum);
			return NULL;
		}
		if (fp->fr_icmp && fp->fr_proto != IPPROTO_ICMP) {
			fprintf(stderr,
				"%d: icmp comparisons on wrong protocol\n",
				linenum);
			return NULL;
		}

		if (!*cpp)
			return fp;

		if (*cpp && (fp->fr_type == FR_T_IPF) &&
		    !strcasecmp(*cpp, "flags")) {
			if (!*++cpp) {
				fprintf(stderr, "%d: no flags present\n",
					linenum);
				return NULL;
			}
			fp->fr_tcpf = tcp_flags(*cpp, &fp->fr_tcpfm, linenum);
			cpp++;
		}

		/*
		 * extras...
		 */
		if ((fp->fr_v == 4) && *cpp && (!strcasecmp(*cpp, "with") ||
		     !strcasecmp(*cpp, "and")))
			if (extras(&cpp, fp, linenum))
				return NULL;

		/*
		 * icmp types for use with the icmp protocol
		 */
		if (*cpp && !strcasecmp(*cpp, "icmp-type")) {
			if (fp->fr_proto != IPPROTO_ICMP &&
			    fp->fr_proto != IPPROTO_ICMPV6) {
				fprintf(stderr,
					"%d: icmp with wrong protocol (%d)\n",
					linenum, fp->fr_proto);
				return NULL;
			}
			if (addicmp(&cpp, fp, linenum))
				return NULL;
			fp->fr_icmp = htons(fp->fr_icmp);
			fp->fr_icmpm = htons(fp->fr_icmpm);
		}
	}

	/*
	 * Keep something...
	 */
	while (*cpp && !strcasecmp(*cpp, "keep"))
		if (addkeep(&cpp, fp, linenum))
			return NULL;

	/*
	* This is here to enforce the old interface binding behaviour.
	* That is, "on X" is equivalent to "<dir> on X <!dir>-via -,X"
	*/
	if (fp->fr_flags & FR_KEEPSTATE) {
		if (*fp->fr_ifnames[0] && !*fp->fr_ifnames[3]) {
			bcopy(fp->fr_ifnames[0], fp->fr_ifnames[3],
			      sizeof(fp->fr_ifnames[3]));
			strncpy(fp->fr_ifnames[2], "*",
				sizeof(fp->fr_ifnames[3]));
		}
	}

	/*
	 * head of a new group ?
	 */
	if (*cpp && !strcasecmp(*cpp, "head")) {
		if (fp->fr_arg != 0) {
			fprintf(stderr, "%d: cannot use skip with head\n",
				linenum);
			return NULL;
		}
		if (!*++cpp) {
			fprintf(stderr, "%d: head without group #\n", linenum);
			return NULL;
		}
		if (strlen(*cpp) > FR_GROUPLEN) {
			fprintf(stderr, "%d: head name too long #\n", linenum);
			return NULL;
		}
		strncpy(fp->fr_grhead, *cpp, FR_GROUPLEN);
		cpp++;
	}

	/*
	 * reference to an already existing group ?
	 */
	if (*cpp && !strcasecmp(*cpp, "group")) {
		if (!*++cpp) {
			fprintf(stderr, "%d: group without group #\n",
				linenum);
			return NULL;
		}
		if (strlen(*cpp) > FR_GROUPLEN) {
			fprintf(stderr, "%d: group name too long #\n", linenum);
			return NULL;
		}
		strncpy(fp->fr_group, *cpp, FR_GROUPLEN);
		cpp++;
	}

	if (*cpp && !strcasecmp(*cpp, "tag")) {
		if (!*++cpp) {
			fprintf(stderr, "%d: tag id missing value\n", linenum);
			return NULL;
		}
		fp->fr_tag = strtol(*cpp, NULL, 0);
		cpp++;
	}

	/*
	 * pps counter
	 */
	if (*cpp && !strcasecmp(*cpp, "pps")) {
		if (!*++cpp) {
			fprintf(stderr, "%d: pps without rate\n", linenum);
			return NULL;
		}
		if (ratoui(*cpp, &k, 0, INT_MAX))
			fp->fr_pps = k;
		else {
			fprintf(stderr, "%d: invalid pps rate (%s)\n",
				linenum, *cpp);
			return NULL;
		}
		cpp++;
	}

	/*
	 * leftovers...yuck
	 */
	if (*cpp && **cpp) {
		fprintf(stderr, "%d: unknown words at end: [", linenum);
		for (; *cpp; cpp++)
			fprintf(stderr, "%s ", *cpp);
		fprintf(stderr, "]\n");
		return NULL;
	}

	/*
	 * lazy users...
	 */
	if (fp->fr_type == FR_T_IPF) {
		if ((fp->fr_tcpf || fp->fr_tcpfm) &&
		    (fp->fr_proto != IPPROTO_TCP)) {
			fprintf(stderr,
				"%d: TCP protocol not specified\n", linenum);
			return NULL;
		}
		if (!(fp->fr_flx & FI_TCPUDP) &&
		    (fp->fr_proto != IPPROTO_TCP) &&
		    (fp->fr_proto != IPPROTO_UDP) &&
		    (fp->fr_dcmp || fp->fr_scmp)) {
			if (!fp->fr_proto) {
				fp->fr_flx |= FI_TCPUDP;
				fp->fr_mflx |= FI_TCPUDP;
			} else {
				fprintf(stderr,
					"%d: port check for non-TCP/UDP\n",
					linenum);
				return NULL;
			}
		}
	}
	if (*fp->fr_oifname && strcmp(fp->fr_oifname, "*") &&
	    !(fp->fr_flags & FR_KEEPSTATE)) {
		fprintf(stderr, "%d: *-via <if> must be used %s\n",
			linenum, "with keep-state");
		return NULL;
	}
	return fp;
}
