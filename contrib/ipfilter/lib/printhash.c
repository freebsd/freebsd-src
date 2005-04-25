/*	$NetBSD$	*/

/*
 * Copyright (C) 2002 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */

#include "ipf.h"

#define	PRINTF	(void)printf
#define	FPRINTF	(void)fprintf


iphtable_t *printhash(hp, copyfunc, name, opts)
iphtable_t *hp;
copyfunc_t copyfunc;
char *name;
int opts;
{
	iphtent_t *ipep, **table;
	iphtable_t iph;
	int i, printed;
	size_t sz;

	if ((*copyfunc)((char *)hp, (char *)&iph, sizeof(iph)))
		return NULL;

	if ((name != NULL) && strncmp(name, iph.iph_name, FR_GROUPLEN))
		return iph.iph_next;

	if ((opts & OPT_DEBUG) == 0) {
		if ((iph.iph_type & IPHASH_ANON) == IPHASH_ANON)
			PRINTF("# 'anonymous' table\n");
		switch (iph.iph_type & ~IPHASH_ANON)
		{
		case IPHASH_LOOKUP :
			PRINTF("table");
			break;
		case IPHASH_GROUPMAP :
			PRINTF("group-map");
			if (iph.iph_flags & FR_INQUE)
				PRINTF(" in");
			else if (iph.iph_flags & FR_OUTQUE)
				PRINTF(" out");
			else
				PRINTF(" ???");
			break;
		default :
			PRINTF("%#x", iph.iph_type);
			break;
		}
		PRINTF(" role = ");
	} else {
		PRINTF("Hash Table Number: %s", iph.iph_name);
		if ((iph.iph_type & IPHASH_ANON) == IPHASH_ANON)
			PRINTF("(anon)");
		putchar(' ');
		PRINTF("Role: ");
	}

	switch (iph.iph_unit)
	{
	case IPL_LOGNAT :
		PRINTF("nat");
		break;
	case IPL_LOGIPF :
		PRINTF("ipf");
		break;
	case IPL_LOGAUTH :
		PRINTF("auth");
		break;
	case IPL_LOGCOUNT :
		PRINTF("count");
		break;
	default :
		PRINTF("#%d", iph.iph_unit);
		break;
	}

	if ((opts & OPT_DEBUG) == 0) {
		if ((iph.iph_type & ~IPHASH_ANON) == IPHASH_LOOKUP)
			PRINTF(" type = hash");
		PRINTF(" number = %s size = %lu",
			iph.iph_name, (u_long)iph.iph_size);
		if (iph.iph_seed != 0)
			PRINTF(" seed = %lu", iph.iph_seed);
		putchar('\n');
	} else {
		PRINTF(" Type: ");
		switch (iph.iph_type & ~IPHASH_ANON)
		{
		case IPHASH_LOOKUP :
			PRINTF("lookup");
			break;
		case IPHASH_GROUPMAP :
			PRINTF("groupmap Group. %s", iph.iph_name);
			break;
		default :
			break;
		}

		putchar('\n');
		PRINTF("\t\tSize: %lu\tSeed: %lu",
			(u_long)iph.iph_size, iph.iph_seed);
		PRINTF("\tRef. Count: %d\tMasks: %#x\n", iph.iph_ref,
			iph.iph_masks);
	}

	if ((opts & OPT_DEBUG) != 0) {
		struct in_addr m;

		for (i = 0; i < 32; i++) {
			if ((1 << i) & iph.iph_masks) {
				ntomask(4, i, &m.s_addr);
				PRINTF("\t\tMask: %s\n", inet_ntoa(m));
			}
		}
	}

	if ((opts & OPT_DEBUG) == 0)
		PRINTF("\t{");

	sz = iph.iph_size * sizeof(*table);
	table = malloc(sz);
	if ((*copyfunc)((char *)iph.iph_table, (char *)table, sz))
		return NULL;

	for (i = 0, printed = 0; i < iph.iph_size; i++) {
		for (ipep = table[i]; ipep != NULL; ) {
			ipep = printhashnode(&iph, ipep, copyfunc, opts);
			printed++;
		}
	}
	if (printed == 0)
		putchar(';');

	free(table);

	if ((opts & OPT_DEBUG) == 0)
		PRINTF(" };\n");

	return iph.iph_next;
}
