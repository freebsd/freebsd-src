#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <rune.h>
#include <stdlib.h>

_RuneLocale *
_Read_RuneMagi(fp)
	FILE *fp;
{
	char *data;
	void *lastp;
	_RuneLocale *rl;
	_RuneEntry *rr;
	struct stat sb;
	int x;

	if (fstat(fileno(fp), &sb) < 0)
		return(0);

	if (sb.st_size < sizeof(_RuneLocale))
		return(0);

	if ((data = malloc(sb.st_size)) == NULL)
		return(0);

	rewind(fp); /* Someone might have read the magic number once already */

	if (fread(data, sb.st_size, 1, fp) != 1) {
		free(data);
		return(0);
	}

	rl = (_RuneLocale *)data;
	lastp = data + sb.st_size;

	rl->variable = rl + 1;

	if (memcmp(rl->magic, _RUNE_MAGIC_1, sizeof(rl->magic))) {
		free(data);
		return(0);
	}

	rl->invalid_rune = ntohl(rl->invalid_rune);
	rl->variable_len = ntohl(rl->variable_len);
	rl->runetype_ext.nranges = ntohl(rl->runetype_ext.nranges);
	rl->maplower_ext.nranges = ntohl(rl->maplower_ext.nranges);
	rl->mapupper_ext.nranges = ntohl(rl->mapupper_ext.nranges);

	for (x = 0; x < _CACHED_RUNES; ++x) {
		rl->runetype[x] = ntohl(rl->runetype[x]);
		rl->maplower[x] = ntohl(rl->maplower[x]);
		rl->mapupper[x] = ntohl(rl->mapupper[x]);
	}

	rl->runetype_ext.ranges = (_RuneEntry *)rl->variable;
	rl->variable = rl->runetype_ext.ranges + rl->runetype_ext.nranges;
	if (rl->variable > lastp) {
		free(data);
		return(0);
	}

	rl->maplower_ext.ranges = (_RuneEntry *)rl->variable;
	rl->variable = rl->maplower_ext.ranges + rl->maplower_ext.nranges;
	if (rl->variable > lastp) {
		free(data);
		return(0);
	}

	rl->mapupper_ext.ranges = (_RuneEntry *)rl->variable;
	rl->variable = rl->mapupper_ext.ranges + rl->mapupper_ext.nranges;
	if (rl->variable > lastp) {
		free(data);
		return(0);
	}

	for (x = 0; x < rl->runetype_ext.nranges; ++x) {
		rr = rl->runetype_ext.ranges;

		rr[x].min = ntohl(rr[x].min);
		rr[x].max = ntohl(rr[x].max);
		if ((rr[x].map = ntohl(rr[x].map)) == 0) {
			int len = rr[x].max - rr[x].min + 1;
			rr[x].types = rl->variable;
			rl->variable = rr[x].types + len;
			if (rl->variable > lastp) {
				free(data);
				return(0);
			}
			while (len-- > 0)
				rr[x].types[len] = ntohl(rr[x].types[len]);
		} else
			rr[x].types = 0;
	}

	for (x = 0; x < rl->maplower_ext.nranges; ++x) {
		rr = rl->maplower_ext.ranges;

		rr[x].min = ntohl(rr[x].min);
		rr[x].max = ntohl(rr[x].max);
		rr[x].map = ntohl(rr[x].map);
	}

	for (x = 0; x < rl->mapupper_ext.nranges; ++x) {
		rr = rl->mapupper_ext.ranges;

		rr[x].min = ntohl(rr[x].min);
		rr[x].max = ntohl(rr[x].max);
		rr[x].map = ntohl(rr[x].map);
	}
	if (((char *)rl->variable) + rl->variable_len > (char *)lastp) {
		free(data);
		return(0);
	}

	/*
	 * Go out and zero pointers that should be zero.
	 */
	if (!rl->variable_len)
		rl->variable = 0;

	if (!rl->runetype_ext.nranges)
		rl->runetype_ext.ranges = 0;

	if (!rl->maplower_ext.nranges)
		rl->maplower_ext.ranges = 0;

	if (!rl->mapupper_ext.nranges)
		rl->mapupper_ext.ranges = 0;

	return(rl);
}

