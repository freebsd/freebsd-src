/*
 * ldns-compare-zones compares two zone files
 * 
 * Written by Ondrej Sury in 2007
 * 
 * Modified a bit by NLnet Labs.
 * 
 * See the file LICENSE for the license
 */

#include "config.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#include <ldns/ldns.h>

#include <errno.h>

#define OP_INS '+'
#define OP_DEL '-'
#define OP_CHG '~'

static void 
usage(char *prog)
{
	printf("Usage: %s [-v] [-i] [-d] [-c] [-s] <zonefile1> <zonefile2>\n",
		prog);
	printf("       -i - print inserted\n");
	printf("       -d - print deleted\n");
	printf("       -c - print changed\n");
	printf("       -a - print all differences (-i -d -c)\n");
	printf("       -s - do not exclude SOA record from comparison\n");
	printf("       -z - do not sort zones\n");
}

int 
main(int argc, char **argv)
{
	char           *fn1, *fn2;
	FILE           *fp1, *fp2;
	ldns_zone      *z1, *z2;
	ldns_status	s;
	size_t		i      , j;
	ldns_rr_list   *rrl1, *rrl2;
	int		rr_cmp, rr_chg = 0;
	ldns_rr        *rr1 = NULL, *rr2 = NULL, *rrx = NULL;
	int		line_nr1 = 0, line_nr2 = 0;
	size_t		rrc1   , rrc2;
	size_t		num_ins = 0, num_del = 0, num_chg = 0;
	int		c;
	bool		opt_deleted = false, opt_inserted = false, opt_changed = false;
        bool		sort = true, inc_soa = false;
	char		op = 0;

	while ((c = getopt(argc, argv, "ahvdicsz")) != -1) {
		switch (c) {
		case 'h':
			usage(argv[0]);
			exit(EXIT_SUCCESS);
			break;
		case 'v':
			printf("%s version %s (ldns version %s)\n",
				  argv[0],
				  LDNS_VERSION,
				  ldns_version());
			exit(EXIT_SUCCESS);
			break;
		case 's':
			inc_soa = true;
			break;
		case 'z':
			sort = false;
                        break;
		case 'd':
			opt_deleted = true;
			break;
		case 'i':
			opt_inserted = true;
			break;
		case 'c':
			opt_changed = true;
			break;
		case 'a':
			opt_deleted = true;
			opt_inserted = true;
			opt_changed = true;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 2) {
		argc -= optind;
		argv -= optind;
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	fn1 = argv[0];
	fp1 = fopen(fn1, "r");
	if (!fp1) {
		fprintf(stderr, "Unable to open %s: %s\n", fn1, strerror(errno));
		exit(EXIT_FAILURE);
	}
	/* Read first zone */
	s = ldns_zone_new_frm_fp_l(&z1, fp1, NULL, 0, 
						  LDNS_RR_CLASS_IN, &line_nr1);
	if (s != LDNS_STATUS_OK) {
		fclose(fp1);
		fprintf(stderr, "%s: %s at %d\n",
			   fn1,
			   ldns_get_errorstr_by_id(s),
			   line_nr1);
		exit(EXIT_FAILURE);
	}
	fclose(fp1);

	fn2 = argv[1];
	fp2 = fopen(fn2, "r");
	if (!fp2) {
		fprintf(stderr, "Unable to open %s: %s\n", fn2, strerror(errno));
		exit(EXIT_FAILURE);
	}
	/* Read second zone */
	s = ldns_zone_new_frm_fp_l(&z2, fp2, NULL, 0,
						  LDNS_RR_CLASS_IN, &line_nr2);
	if (s != LDNS_STATUS_OK) {
		ldns_zone_deep_free(z1);
		fclose(fp2);
		fprintf(stderr, "%s: %s at %d\n",
			   fn2,
			   ldns_get_errorstr_by_id(s),
			   line_nr2);
		exit(EXIT_FAILURE);
	}
	fclose(fp2);

	rrl1 = ldns_zone_rrs(z1);
	rrc1 = ldns_rr_list_rr_count(rrl1);

	rrl2 = ldns_zone_rrs(z2);
	rrc2 = ldns_rr_list_rr_count(rrl2);

        if (sort) {
		/* canonicalize zone 1 */
		ldns_rr2canonical(ldns_zone_soa(z1));
                for (i = 0; i < ldns_rr_list_rr_count(ldns_zone_rrs(z1)); i++) {
                	ldns_rr2canonical(ldns_rr_list_rr(ldns_zone_rrs(z1), i));
		}
                /* sort zone 1 */
                ldns_zone_sort(z1);
		/* canonicalize zone 2 */
		ldns_rr2canonical(ldns_zone_soa(z2));
                for (i = 0; i < ldns_rr_list_rr_count(ldns_zone_rrs(z2)); i++) {
                	ldns_rr2canonical(ldns_rr_list_rr(ldns_zone_rrs(z2), i));
		}
                /* sort zone 2 */
                ldns_zone_sort(z2);
        }

	if(inc_soa) {
		ldns_rr_list* wsoa = ldns_rr_list_new();
		ldns_rr_list_push_rr(wsoa, ldns_zone_soa(z1));
		ldns_rr_list_cat(wsoa, rrl1);
		rrl1 = wsoa;
		rrc1 = ldns_rr_list_rr_count(rrl1);
		wsoa = ldns_rr_list_new();
		ldns_rr_list_push_rr(wsoa, ldns_zone_soa(z2));
		ldns_rr_list_cat(wsoa, rrl2);
		rrl2 = wsoa;
		rrc2 = ldns_rr_list_rr_count(rrl2);
		if(sort) {
			ldns_rr_list_sort(rrl1);
			ldns_rr_list_sort(rrl2);
		}
	}

	/*
	 * Walk through both zones. The previously seen resource record is
	 * kept (in the variable rrx) so that we can recognize when we are
	 * handling a new owner name. If the owner name changes, we have to
	 * set the operator again.
	 */
	for (i = 0, j = 0; i < rrc1 || j < rrc2;) {
		rr_cmp = 0;
		if (i < rrc1 && j < rrc2) {
			rr1 = ldns_rr_list_rr(rrl1, i);
			rr2 = ldns_rr_list_rr(rrl2, j);
			rr_cmp = ldns_rr_compare(rr1, rr2);

			/* Completely skip if the rrs are equal */
			if (rr_cmp == 0) {
				i++;
				j++;
				continue;
			}
			rr_chg = ldns_dname_compare(ldns_rr_owner(rr1),
								   ldns_rr_owner(rr2));
		} else if (i >= rrc1) {
			/* we have reached the end of zone 1, so the current record
			 * from zone 2 automatically sorts higher
			 */
			rr1 = NULL;
			rr2 = ldns_rr_list_rr(rrl2, j);
			rr_chg = rr_cmp = 1;
		} else if (j >= rrc2) {
			/* we have reached the end of zone 2, so the current record
			 * from zone 1 automatically sorts lower
			 */
			rr1 = ldns_rr_list_rr(rrl1, i);
			rr2 = NULL;
			rr_chg = rr_cmp = -1;
		}
		if (rr_cmp < 0) {
			i++;
			if ((rrx != NULL) && (ldns_dname_compare(ldns_rr_owner(rr1), 
											 ldns_rr_owner(rrx)
											 ) != 0)) {
				/* The owner name is different, forget previous rr */
				rrx = NULL;
			}
			if (rrx == NULL) {
				if (rr_chg == 0) {
					num_chg++;
					op = OP_CHG;
				} else {
					num_del++;
					op = OP_DEL;
				}
				rrx = rr1;
			}
			if (((op == OP_DEL) && opt_deleted) ||
			    ((op == OP_CHG) && opt_changed)) {
				printf("%c-", op);
				ldns_rr_print(stdout, rr1);
			}
		} else if (rr_cmp > 0) {
			j++;
			if ((rrx != NULL) && (ldns_dname_compare(ldns_rr_owner(rr2),
											 ldns_rr_owner(rrx)
											 ) != 0)) {
				rrx = NULL;
			}
			if (rrx == NULL) {
				if (rr_chg == 0) {
					num_chg++;
					op = OP_CHG;
				} else {
					num_ins++;
					op = OP_INS;
				}
				/* remember this rr for it's name in the next iteration */
				rrx = rr2;
			}
			if (((op == OP_INS) && opt_inserted) ||
			    ((op == OP_CHG) && opt_changed)) {
				printf("%c+", op);
				ldns_rr_print(stdout, rr2);
			}
		}
	}

	printf("\t%c%u\t%c%u\t%c%u\n", 
		  OP_INS, 
		  (unsigned int) num_ins, 
		  OP_DEL,
		  (unsigned int) num_del, 
		  OP_CHG, 
		  (unsigned int) num_chg);

	/* Free resources */
	if(inc_soa) {
		ldns_rr_list_free(rrl1);
		ldns_rr_list_free(rrl2);
	}
	ldns_zone_deep_free(z2);
	ldns_zone_deep_free(z1);

	return 0;
}
