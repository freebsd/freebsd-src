/*
 * $Id: inf.c,v 1.3 2003/11/30 21:58:16 winter Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <sys/queue.h>

#include "inf.h"

extern FILE *yyin;
int yyparse (void);

const char *words[W_MAX];	/* More than we'll need. */
int idx;

static struct section_head sh;
static struct reg_head rh;
static struct assign_head ah;

static char	*sstrdup	(const char *);
static struct assign
		*find_assign	(const char *, const char *);
static struct section
		*find_section	(const char *);
static void	dump_deviceids	(void);
static void	dump_pci_id	(const char *);
static void	dump_regvals	(void);
static void	dump_paramreg	(const struct section *,
				const struct reg *, int);

static FILE	*ofp;

int
inf_parse (FILE *fp, FILE *outfp)
{
	TAILQ_INIT(&sh);
	TAILQ_INIT(&rh);
	TAILQ_INIT(&ah);

	ofp = outfp;
	yyin = fp;
	yyparse();

	dump_deviceids();
	dump_regvals();

	return (0);
}

void
section_add (const char *s)
{
	struct section *sec;

	sec = malloc(sizeof(struct section));
	bzero(sec, sizeof(struct section));
	sec->name = s;
	TAILQ_INSERT_TAIL(&sh, sec, link);

	return;
}

static struct assign *
find_assign (const char *s, const char *k)
{
	struct assign *assign;
	char newkey[256];

	/* Deal with string section lookups. */

	if (k != NULL && k[0] == '%') {
		bzero(newkey, sizeof(newkey));
		strncpy(newkey, k + 1, strlen(k) - 2);
		k = newkey;
	}

	TAILQ_FOREACH(assign, &ah, link) {
		if (strcasecmp(assign->section->name, s) == 0) {
			if (k == NULL)
				return(assign);
			else
				if (strcasecmp(assign->key, k) == 0)
					return(assign);
		}
	}
	return(NULL);
}

static const char *
stringcvt(const char *s)
{
	struct assign *manf;

	manf = find_assign("strings", s);
	if (manf == NULL)
		return(s);
	return(manf->vals[0]);
}

struct section *
find_section (const char *s)
{
	struct section *section;

	TAILQ_FOREACH(section, &sh, link) {
		if (strcasecmp(section->name, s) == 0)
			return(section);
	}
	return(NULL);
}

static void
dump_pci_id(const char *s)
{
	char *p;
	char vidstr[7], didstr[7], subsysstr[14];

	p = strcasestr(s, "VEN_");
	if (p == NULL)
		return;
	p += 4;
	strcpy(vidstr, "0x");
	strncat(vidstr, p, 4);
	p = strcasestr(s, "DEV_");
	if (p == NULL)
		return;
	p += 4;
	strcpy(didstr, "0x");
	strncat(didstr, p, 4);
	if (p == NULL)
		return;
	p = strcasestr(s, "SUBSYS_");
	if (p == NULL)
		strcpy(subsysstr, "0x00000000");
	else {
		p += 7;
		strcpy(subsysstr, "0x");
		strncat(subsysstr, p, 8);
	}

	fprintf(ofp, "\t\\\n\t{ %s, %s, %s, ", vidstr, didstr, subsysstr);
	return;
}

static void
dump_deviceids()
{
	struct assign *manf, *dev;
	struct section *sec;
	struct assign *assign;
	char xpsec[256];

	/* Find manufacturer name */
	manf = find_assign("Manufacturer", NULL);

	/* Find manufacturer section */
	if (manf->vals[1] != NULL &&
	    (strcasecmp(manf->vals[1], "NT.5.1") == 0 ||
	    strcasecmp(manf->vals[1], "NTx86.5.1") == 0)) {
		/* Handle Windows XP INF files. */
		snprintf(xpsec, sizeof(xpsec), "%s.%s",
		    manf->vals[0], manf->vals[1]);
		sec = find_section(xpsec);
	} else
		sec = find_section(manf->vals[0]);

	/* Emit start of device table */
	fprintf (ofp, "#define NDIS_DEV_TABLE");

	/*
	 * Now run through all the device names listed
	 * in the manufacturer section and dump out the
	 * device descriptions and vendor/device IDs.
	 */

	TAILQ_FOREACH(assign, &ah, link) {
		if (assign->section == sec) {
			dev = find_assign("strings", assign->key);
			/* Emit device IDs. */
			if (strcasestr(assign->vals[1], "PCI") != NULL)
				dump_pci_id(assign->vals[1]);
#ifdef notdef
			else if (strcasestr(assign->vals[1], "PCMCIA") != NULL)
				dump_pcmcia_id(assign->vals[1]);
#endif
			/* Emit device description */
			fprintf (ofp, "\t\\\n\t\"%s\" },", dev->vals[0]);
		}
	}

	/* Emit end of table */

	fprintf(ofp, "\n\n");

}

static void
dump_addreg(const char *s, int devidx)
{
	struct section *sec;
	struct reg *reg;

	/* Find the addreg section */
	sec = find_section(s);

	/* Dump all the keys defined in it. */
	TAILQ_FOREACH(reg, &rh, link) {
		/*
		 * Keys with an empty subkey are very easy to parse,
		 * so just deal with them here. If a parameter key
		 * of the same name also exists, prefer that one and
		 * skip this one.
		 */
		if (reg->section == sec) {
			if (reg->subkey == NULL) {
				fprintf(ofp, "\n\t{ \"%s\",", reg->key);
				fprintf(ofp,"\n\t\"%s \",", reg->key);
				fprintf(ofp, "\n\t{ \"%s\" }, %d },",
				    reg->value == NULL ? "" :
				    stringcvt(reg->value), devidx);
			} else if (strcasestr(reg->subkey,
			    "Ndi\\params") != NULL &&
			    strcasecmp(reg->key, "ParamDesc") == 0)
				dump_paramreg(sec, reg, devidx);
		}
	}

	return;
}

static void
dump_enumreg(const struct section *s, const struct reg *r)
{
	struct reg *reg;
	char enumkey[256];

	sprintf(enumkey, "%s\\enum", r->subkey);
	TAILQ_FOREACH(reg, &rh, link) {
		if (reg->section != s)
			continue;
		if (reg->subkey == NULL || strcasecmp(reg->subkey, enumkey))
			continue;
		fprintf(ofp, " [%s=%s]", reg->key,
		    stringcvt(reg->value));
	}
	return;
}

static void
dump_editreg(const struct section *s, const struct reg *r)
{
	struct reg *reg;

	TAILQ_FOREACH(reg, &rh, link) {
		if (reg->section != s)
			continue;
		if (reg->subkey == NULL || strcasecmp(reg->subkey, r->subkey))
			continue;
		if (strcasecmp(reg->key, "LimitText") == 0)
			fprintf(ofp, " [maxchars=%s]", reg->value);
		if (strcasecmp(reg->key, "Optional") == 0 &&
		    strcmp(reg->value, "1") == 0)
			fprintf(ofp, " [optional]");
	}
	return;
}

/* Use this for int too */
static void
dump_dwordreg(const struct section *s, const struct reg *r)
{
	struct reg *reg;

	TAILQ_FOREACH(reg, &rh, link) {
		if (reg->section != s)
			continue;
		if (reg->subkey == NULL || strcasecmp(reg->subkey, r->subkey))
			continue;
		if (strcasecmp(reg->key, "min") == 0)
			fprintf(ofp, " [min=%s]", reg->value);
		if (strcasecmp(reg->key, "max") == 0)
			fprintf(ofp, " [max=%s]", reg->value);
	}
	return;
}

static void
dump_defaultinfo(const struct section *s, const struct reg *r, int devidx)
{
	struct reg *reg;
	TAILQ_FOREACH(reg, &rh, link) {
		if (reg->section != s)
			continue;
		if (reg->subkey == NULL || strcasecmp(reg->subkey, r->subkey))
			continue;
		if (strcasecmp(reg->key, "Default"))
			continue;
		fprintf(ofp, "\n\t{ \"%s\" }, %d },", reg->value == NULL ? "" :
		    stringcvt(reg->value), devidx);
			break;
	}
	return;
}

static void
dump_paramdesc(const struct section *s, const struct reg *r)
{
	struct reg *reg;
	TAILQ_FOREACH(reg, &rh, link) {
		if (reg->section != s)
			continue;
		if (reg->subkey == NULL || strcasecmp(reg->subkey, r->subkey))
			continue;
		if (strcasecmp(reg->key, "ParamDesc"))
			continue;
		fprintf(ofp, "\n\t\"%s", stringcvt(r->value));
			break;
	}
	return;
}

static void
dump_typeinfo(const struct section *s, const struct reg *r)
{
	struct reg *reg;
	TAILQ_FOREACH(reg, &rh, link) {
		if (reg->section != s)
			continue;
		if (reg->subkey == NULL || strcasecmp(reg->subkey, r->subkey))
			continue;
		if (strcasecmp(reg->key, "type"))
			continue;
		if (strcasecmp(reg->value, "dword") == 0 ||
		    strcasecmp(reg->value, "int") == 0)
			dump_dwordreg(s, r);
		if (strcasecmp(reg->value, "enum") == 0)
			dump_enumreg(s, r);
		if (strcasecmp(reg->value, "edit") == 0)
			dump_editreg(s, r);
	}
	return;
}

static void
dump_paramreg(const struct section *s, const struct reg *r, int devidx)
{
	const char *keyname;

	keyname = r->subkey + strlen("Ndi\\params\\");
	fprintf(ofp, "\n\t{ \"%s\",", keyname);
	dump_paramdesc(s, r);
	dump_typeinfo(s, r);
	fprintf(ofp, "\",");
	dump_defaultinfo(s, r, devidx);

	return;
}

static void
dump_regvals(void)
{
	struct assign *manf, *dev;
	struct section *sec;
	struct assign *assign;
	char sname[256];
	int i, is_winxp = 0, devidx = 0;

	/* Find manufacturer name */
	manf = find_assign("Manufacturer", NULL);

	/* Find manufacturer section */
	if (manf->vals[1] != NULL &&
	    (strcasecmp(manf->vals[1], "NT.5.1") == 0 ||
	    strcasecmp(manf->vals[1], "NTx86.5.1") == 0)) {
		is_winxp++;
		/* Handle Windows XP INF files. */
		snprintf(sname, sizeof(sname), "%s.%s",
		    manf->vals[0], manf->vals[1]);
		sec = find_section(sname);
	} else
		sec = find_section(manf->vals[0]);

	/* Emit start of block */
	fprintf (ofp, "ndis_cfg ndis_regvals[] = {");

	TAILQ_FOREACH(assign, &ah, link) {
		if (assign->section == sec) {
			/*
			 * Find all the AddReg sections.
			 * Look for section names with .NT, unless
			 * this is a WinXP .INF file.
			 */
			if (is_winxp) {
				sprintf(sname, "%s.NTx86", assign->vals[0]);
				dev = find_assign(sname, "AddReg");
				if (dev == NULL)
					dev = find_assign(assign->vals[0],
					    "AddReg");
			} else {
				sprintf(sname, "%s.NT", assign->vals[0]);
				dev = find_assign(sname, "AddReg");
			}
			/* Section not found. */
			if (dev == NULL)
				continue;
			for (i = 0; i < W_MAX; i++) {
				if (dev->vals[i] != NULL)
					dump_addreg(dev->vals[i], devidx);
			}
			devidx++;
		}
	}

	fprintf(ofp, "\n\t{ NULL, NULL, { 0 }, 0 }\n};\n\n");

	return;
}

void
assign_add (const char *a)
{
	struct assign *assign;
	int i;

	assign = malloc(sizeof(struct assign));
	bzero(assign, sizeof(struct assign));
	assign->section = TAILQ_LAST(&sh, section_head);
	assign->key = sstrdup(a);
	for (i = 0; i < idx; i++)
		assign->vals[(idx - 1) - i] = sstrdup(words[i]);
	TAILQ_INSERT_TAIL(&ah, assign, link);

	clear_words();
	return;
}

void
define_add (const char *d __unused)
{
#ifdef notdef
	fprintf(stderr, "define \"%s\"\n", d);
#endif
	return;
}

static char *
sstrdup(const char *str)
{
	if (str != NULL && strlen(str))
		return (strdup(str));
	return (NULL);
}

static int
satoi (const char *nptr)
{
	if (nptr != NULL && strlen(nptr))
		return (atoi(nptr));
	return (0);
}

void
regkey_add (const char *r)
{
	struct reg *reg;

	reg = malloc(sizeof(struct reg));
	bzero(reg, sizeof(struct reg));
	reg->section = TAILQ_LAST(&sh, section_head);
	reg->root = sstrdup(r);
	reg->subkey = sstrdup(words[3]);
	reg->key = sstrdup(words[2]);
	reg->flags = satoi(words[1]);
	reg->value = sstrdup(words[0]);
	TAILQ_INSERT_TAIL(&rh, reg, link);

	free(__DECONST(char *, r));
	clear_words();
	return;
}

void
push_word (const char *w)
{
	if (w && strlen(w))
		words[idx++] = w;
	else
		words[idx++] = NULL;
	return;
}

void
clear_words (void)
{
	int i;

	for (i = 0; i < idx; i++) {
		if (words[i]) {
			free(__DECONST(char *, words[i]));
		}
	}
	idx = 0;
	bzero(words, sizeof(words));
	return;
}
