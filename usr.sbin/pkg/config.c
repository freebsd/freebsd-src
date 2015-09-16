/*-
 * Copyright (c) 2014 Baptiste Daroussin <bapt@FreeBSD.org>
 * Copyright (c) 2013 Bryan Drewery <bdrewery@FreeBSD.org>
 * All rights reserved.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/sbuf.h>
#include <sys/elf_common.h>
#include <sys/endian.h>
#include <sys/types.h>

#include <assert.h>
#include <dirent.h>
#include <ucl.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <gelf.h>
#include <inttypes.h>
#include <paths.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "elf_tables.h"
#include "config.h"

#define roundup2(x, y)	(((x)+((y)-1))&(~((y)-1))) /* if y is powers of two */

struct config_value {
       char *value;
       STAILQ_ENTRY(config_value) next;
};

struct config_entry {
	uint8_t type;
	const char *key;
	const char *val;
	char *value;
	STAILQ_HEAD(, config_value) *list;
	bool envset;
	bool main_only;				/* Only set in pkg.conf. */
};

static struct config_entry c[] = {
	[PACKAGESITE] = {
		PKG_CONFIG_STRING,
		"PACKAGESITE",
		URL_SCHEME_PREFIX "http://pkg.FreeBSD.org/${ABI}/latest",
		NULL,
		NULL,
		false,
		false,
	},
	[ABI] = {
		PKG_CONFIG_STRING,
		"ABI",
		NULL,
		NULL,
		NULL,
		false,
		true,
	},
	[MIRROR_TYPE] = {
		PKG_CONFIG_STRING,
		"MIRROR_TYPE",
		"SRV",
		NULL,
		NULL,
		false,
		false,
	},
	[ASSUME_ALWAYS_YES] = {
		PKG_CONFIG_BOOL,
		"ASSUME_ALWAYS_YES",
		"NO",
		NULL,
		NULL,
		false,
		true,
	},
	[SIGNATURE_TYPE] = {
		PKG_CONFIG_STRING,
		"SIGNATURE_TYPE",
		NULL,
		NULL,
		NULL,
		false,
		false,
	},
	[FINGERPRINTS] = {
		PKG_CONFIG_STRING,
		"FINGERPRINTS",
		NULL,
		NULL,
		NULL,
		false,
		false,
	},
	[REPOS_DIR] = {
		PKG_CONFIG_LIST,
		"REPOS_DIR",
		NULL,
		NULL,
		NULL,
		false,
		true,
	},
	[PUBKEY] = {
		PKG_CONFIG_STRING,
		"PUBKEY",
		NULL,
		NULL,
		NULL,
		false,
		false
	}
};

static const char *
elf_corres_to_string(struct _elf_corres *m, int e)
{
	int i;

	for (i = 0; m[i].string != NULL; i++)
		if (m[i].elf_nb == e)
			return (m[i].string);

	return ("unknown");
}

static const char *
aeabi_parse_arm_attributes(void *data, size_t length) 
{
	uint32_t sect_len;
	uint8_t *section = data;

#define	MOVE(len) do {            \
	assert(length >= (len));  \
	section += (len);         \
	length -= (len);          \
} while (0)

	if (length == 0 || *section != 'A')
		return (NULL);

	MOVE(1);

	/* Read the section length */
	if (length < sizeof(sect_len))
		return (NULL);

	memcpy(&sect_len, section, sizeof(sect_len));

	/*
	 * The section length should be no longer than the section it is within
	 */
	if (sect_len > length)
		return (NULL);

	MOVE(sizeof(sect_len));

	/* Skip the vendor name */
	while (length != 0) {
		if (*section == '\0')
			break;
		MOVE(1);
	}
	if (length == 0)
		return (NULL);
	MOVE(1);

	while (length != 0) {
		uint32_t tag_length;

	switch(*section) {
	case 1: /* Tag_File */
		MOVE(1);
		if (length < sizeof(tag_length))
			return (NULL);
		memcpy(&tag_length, section, sizeof(tag_length));
		break;
	case 2: /* Tag_Section */
	case 3: /* Tag_Symbol */
	default:
		return (NULL);
	}
	/* At least space for the tag and size */
	if (tag_length <= 5)
		return (NULL);
	tag_length--;
	/* Check the tag fits */
	if (tag_length > length)
		return (NULL);

#define  MOVE_TAG(len) do {           \
	assert(tag_length >= (len));  \
	MOVE(len);                    \
	tag_length -= (len);          \
} while(0)

		MOVE(sizeof(tag_length));
		tag_length -= sizeof(tag_length);

		while (tag_length != 0) {
			uint8_t tag;

			assert(tag_length >= length);

			tag = *section;
			MOVE_TAG(1);

			/*
			 * These tag values come from:
			 * 
			 * Addenda to, and Errata in, the ABI for the
			 * ARM Architecture. Release 2.08, section 2.3.
			 */
			if (tag == 6) { /* == Tag_CPU_arch */
				uint8_t val;

				val = *section;
				/*
				 * We don't support values that require
				 * more than one byte.
				 */
				if (val & (1 << 7))
					return (NULL);

				/* We have an ARMv4 or ARMv5 */
				if (val <= 5)
					return ("arm");
				else /* We have an ARMv6+ */
					return ("armv6");
			} else if (tag == 4 || tag == 5 || tag == 32 ||
			    tag == 65 || tag == 67) {
				while (*section != '\0' && length != 0)
					MOVE_TAG(1);
				if (tag_length == 0)
					return (NULL);
				/* Skip the last byte */
				MOVE_TAG(1);
			} else if ((tag >= 7 && tag <= 31) || tag == 34 ||
			    tag == 36 || tag == 38 || tag == 42 || tag == 44 ||
			    tag == 64 || tag == 66 || tag == 68 || tag == 70) {
				/* Skip the uleb128 data */
				while (*section & (1 << 7) && length != 0)
					MOVE_TAG(1);
				if (tag_length == 0)
					return (NULL);
				/* Skip the last byte */
				MOVE_TAG(1);
			} else
				return (NULL);
#undef MOVE_TAG
		}

		break;
	}
	return (NULL);
#undef MOVE
}

static int
pkg_get_myabi(char *dest, size_t sz)
{
	Elf *elf;
	Elf_Data *data;
	Elf_Note note;
	Elf_Scn *scn;
	char *src, *osname;
	const char *arch, *abi, *fpu, *endian_corres_str;
	const char *wordsize_corres_str;
	GElf_Ehdr elfhdr;
	GElf_Shdr shdr;
	int fd, i, ret;
	uint32_t version;

	version = 0;
	ret = -1;
	scn = NULL;
	abi = NULL;

	if (elf_version(EV_CURRENT) == EV_NONE) {
		warnx("ELF library initialization failed: %s",
		    elf_errmsg(-1));
		return (-1);
	}

	if ((fd = open(_PATH_BSHELL, O_RDONLY)) < 0) {
		warn("open()");
		return (-1);
	}

	if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
		ret = -1;
		warnx("elf_begin() failed: %s.", elf_errmsg(-1));
		goto cleanup;
	}

	if (gelf_getehdr(elf, &elfhdr) == NULL) {
		ret = -1;
		warn("getehdr() failed: %s.", elf_errmsg(-1));
		goto cleanup;
	}
	while ((scn = elf_nextscn(elf, scn)) != NULL) {
		if (gelf_getshdr(scn, &shdr) != &shdr) {
			ret = -1;
			warn("getshdr() failed: %s.", elf_errmsg(-1));
			goto cleanup;
		}

		if (shdr.sh_type == SHT_NOTE)
			break;
	}

	if (scn == NULL) {
		ret = -1;
		warn("failed to get the note section");
		goto cleanup;
	}

	data = elf_getdata(scn, NULL);
	src = data->d_buf;
	for (;;) {
		memcpy(&note, src, sizeof(Elf_Note));
		src += sizeof(Elf_Note);
		if (note.n_type == NT_VERSION)
			break;
		src += note.n_namesz + note.n_descsz;
	}
	osname = src;
	src += roundup2(note.n_namesz, 4);
	if (elfhdr.e_ident[EI_DATA] == ELFDATA2MSB)
		version = be32dec(src);
	else
		version = le32dec(src);

	for (i = 0; osname[i] != '\0'; i++)
		osname[i] = (char)tolower(osname[i]);

	wordsize_corres_str = elf_corres_to_string(wordsize_corres,
	    (int)elfhdr.e_ident[EI_CLASS]);

	arch = elf_corres_to_string(mach_corres, (int) elfhdr.e_machine);

	snprintf(dest, sz, "%s:%d",
	    osname, version / 100000);

	ret = 0;

	switch (elfhdr.e_machine) {
	case EM_ARM:
		endian_corres_str = elf_corres_to_string(endian_corres,
		    (int)elfhdr.e_ident[EI_DATA]);

		/* FreeBSD doesn't support the hard-float ABI yet */
		fpu = "softfp";
		if ((elfhdr.e_flags & 0xFF000000) != 0) {
			const char *sh_name = NULL;
			size_t shstrndx;

			/* This is an EABI file, the conformance level is set */
			abi = "eabi";
			/* Find which TARGET_ARCH we are building for. */
			elf_getshdrstrndx(elf, &shstrndx);
			while ((scn = elf_nextscn(elf, scn)) != NULL) {
				sh_name = NULL;
				if (gelf_getshdr(scn, &shdr) != &shdr) {
					scn = NULL;
					break;
				}

				sh_name = elf_strptr(elf, shstrndx,
				    shdr.sh_name);
				if (sh_name == NULL)
					continue;
				if (strcmp(".ARM.attributes", sh_name) == 0)
					break;
			}
			if (scn != NULL && sh_name != NULL) {
				data = elf_getdata(scn, NULL);
				/*
				 * Prior to FreeBSD 10.0 libelf would return
				 * NULL from elf_getdata on the .ARM.attributes
				 * section. As this was the first release to
				 * get armv6 support assume a NULL value means
				 * arm.
				 *
				 * This assumption can be removed when 9.x
				 * is unsupported.
				 */
				if (data != NULL) {
					arch = aeabi_parse_arm_attributes(
					    data->d_buf, data->d_size);
					if (arch == NULL) {
						ret = 1;
						warn("unknown ARM ARCH");
						goto cleanup;
					}
				}
			} else {
				ret = 1;
				warn("Unable to find the .ARM.attributes "
				    "section");
				goto cleanup;
			}
		} else if (elfhdr.e_ident[EI_OSABI] != ELFOSABI_NONE) {
			/*
			 * EABI executables all have this field set to
			 * ELFOSABI_NONE, therefore it must be an oabi file.
			 */
			abi = "oabi";
		} else {
			ret = 1;
			warn("unknown ARM ABI");
			goto cleanup;
		}
		snprintf(dest + strlen(dest), sz - strlen(dest),
		    ":%s:%s:%s:%s:%s", arch, wordsize_corres_str,
		    endian_corres_str, abi, fpu);
		break;
	case EM_MIPS:
		/*
		 * this is taken from binutils sources:
		 * include/elf/mips.h
		 * mapping is figured out from binutils:
		 * gas/config/tc-mips.c
		 */
		switch (elfhdr.e_flags & EF_MIPS_ABI) {
		case E_MIPS_ABI_O32:
			abi = "o32";
			break;
		case E_MIPS_ABI_N32:
			abi = "n32";
			break;
		default:
			if (elfhdr.e_ident[EI_DATA] ==
			    ELFCLASS32)
				abi = "o32";
			else if (elfhdr.e_ident[EI_DATA] ==
			    ELFCLASS64)
				abi = "n64";
			break;
		}
		endian_corres_str = elf_corres_to_string(endian_corres,
		    (int)elfhdr.e_ident[EI_DATA]);

		snprintf(dest + strlen(dest), sz - strlen(dest), ":%s:%s:%s:%s",
		    arch, wordsize_corres_str, endian_corres_str, abi);
		break;
	default:
		snprintf(dest + strlen(dest), sz - strlen(dest), ":%s:%s",
		    arch, wordsize_corres_str);
	}

cleanup:
	if (elf != NULL)
		elf_end(elf);

	close(fd);
	return (ret);
}

static void
subst_packagesite(const char *abi)
{
	struct sbuf *newval;
	const char *variable_string;
	const char *oldval;

	if (c[PACKAGESITE].value != NULL)
		oldval = c[PACKAGESITE].value;
	else
		oldval = c[PACKAGESITE].val;

	if ((variable_string = strstr(oldval, "${ABI}")) == NULL)
		return;

	newval = sbuf_new_auto();
	sbuf_bcat(newval, oldval, variable_string - oldval);
	sbuf_cat(newval, abi);
	sbuf_cat(newval, variable_string + strlen("${ABI}"));
	sbuf_finish(newval);

	free(c[PACKAGESITE].value);
	c[PACKAGESITE].value = strdup(sbuf_data(newval));
}

static int
boolstr_to_bool(const char *str)
{
	if (str != NULL && (strcasecmp(str, "true") == 0 ||
	    strcasecmp(str, "yes") == 0 || strcasecmp(str, "on") == 0 ||
	    str[0] == '1'))
		return (true);

	return (false);
}

static void
config_parse(const ucl_object_t *obj, pkg_conf_file_t conftype)
{
	struct sbuf *buf = sbuf_new_auto();
	const ucl_object_t *cur, *seq;
	ucl_object_iter_t it = NULL, itseq = NULL;
	struct config_entry *temp_config;
	struct config_value *cv;
	const char *key;
	int i;
	size_t j;

	/* Temporary config for configs that may be disabled. */
	temp_config = calloc(CONFIG_SIZE, sizeof(struct config_entry));

	while ((cur = ucl_iterate_object(obj, &it, true))) {
		key = ucl_object_key(cur);
		if (key == NULL)
			continue;
		sbuf_clear(buf);

		if (conftype == CONFFILE_PKG) {
			for (j = 0; j < strlen(key); ++j)
				sbuf_putc(buf, key[j]);
			sbuf_finish(buf);
		} else if (conftype == CONFFILE_REPO) {
			if (strcasecmp(key, "url") == 0)
				sbuf_cpy(buf, "PACKAGESITE");
			else if (strcasecmp(key, "mirror_type") == 0)
				sbuf_cpy(buf, "MIRROR_TYPE");
			else if (strcasecmp(key, "signature_type") == 0)
				sbuf_cpy(buf, "SIGNATURE_TYPE");
			else if (strcasecmp(key, "fingerprints") == 0)
				sbuf_cpy(buf, "FINGERPRINTS");
			else if (strcasecmp(key, "pubkey") == 0)
				sbuf_cpy(buf, "PUBKEY");
			else if (strcasecmp(key, "enabled") == 0) {
				if ((cur->type != UCL_BOOLEAN) ||
				    !ucl_object_toboolean(cur))
					goto cleanup;
			} else
				continue;
			sbuf_finish(buf);
		}

		for (i = 0; i < CONFIG_SIZE; i++) {
			if (strcmp(sbuf_data(buf), c[i].key) == 0)
				break;
		}

		/* Silently skip unknown keys to be future compatible. */
		if (i == CONFIG_SIZE)
			continue;

		/* env has priority over config file */
		if (c[i].envset)
			continue;

		/* Parse sequence value ["item1", "item2"] */
		switch (c[i].type) {
		case PKG_CONFIG_LIST:
			if (cur->type != UCL_ARRAY) {
				warnx("Skipping invalid array "
				    "value for %s.\n", c[i].key);
				continue;
			}
			temp_config[i].list =
			    malloc(sizeof(*temp_config[i].list));
			STAILQ_INIT(temp_config[i].list);

			while ((seq = ucl_iterate_object(cur, &itseq, true))) {
				if (seq->type != UCL_STRING)
					continue;
				cv = malloc(sizeof(struct config_value));
				cv->value =
				    strdup(ucl_object_tostring(seq));
				STAILQ_INSERT_TAIL(temp_config[i].list, cv,
				    next);
			}
			break;
		case PKG_CONFIG_BOOL:
			temp_config[i].value =
			    strdup(ucl_object_toboolean(cur) ? "yes" : "no");
			break;
		default:
			/* Normal string value. */
			temp_config[i].value = strdup(ucl_object_tostring(cur));
			break;
		}
	}

	/* Repo is enabled, copy over all settings from temp_config. */
	for (i = 0; i < CONFIG_SIZE; i++) {
		if (c[i].envset)
			continue;
		/* Prevent overriding ABI, ASSUME_ALWAYS_YES, etc. */
		if (conftype != CONFFILE_PKG && c[i].main_only == true)
			continue;
		switch (c[i].type) {
		case PKG_CONFIG_LIST:
			c[i].list = temp_config[i].list;
			break;
		default:
			c[i].value = temp_config[i].value;
			break;
		}
	}

cleanup:
	free(temp_config);
	sbuf_delete(buf);
}

/*-
 * Parse new repo style configs in style:
 * Name:
 *   URL:
 *   MIRROR_TYPE:
 * etc...
 */
static void
parse_repo_file(ucl_object_t *obj)
{
	ucl_object_iter_t it = NULL;
	const ucl_object_t *cur;
	const char *key;

	while ((cur = ucl_iterate_object(obj, &it, true))) {
		key = ucl_object_key(cur);

		if (key == NULL)
			continue;

		if (cur->type != UCL_OBJECT)
			continue;

		config_parse(cur, CONFFILE_REPO);
	}
}


static int
read_conf_file(const char *confpath, pkg_conf_file_t conftype)
{
	struct ucl_parser *p;
	ucl_object_t *obj = NULL;

	p = ucl_parser_new(0);

	if (!ucl_parser_add_file(p, confpath)) {
		if (errno != ENOENT)
			errx(EXIT_FAILURE, "Unable to parse configuration "
			    "file %s: %s", confpath, ucl_parser_get_error(p));
		ucl_parser_free(p);
		/* no configuration present */
		return (1);
	}

	obj = ucl_parser_get_object(p);
	if (obj->type != UCL_OBJECT) 
		warnx("Invalid configuration format, ignoring the "
		    "configuration file %s", confpath);
	else {
		if (conftype == CONFFILE_PKG)
			config_parse(obj, conftype);
		else if (conftype == CONFFILE_REPO)
			parse_repo_file(obj);
	}

	ucl_object_unref(obj);
	ucl_parser_free(p);

	return (0);
}

static int
load_repositories(const char *repodir)
{
	struct dirent *ent;
	DIR *d;
	char *p;
	size_t n;
	char path[MAXPATHLEN];
	int ret;

	ret = 0;

	if ((d = opendir(repodir)) == NULL)
		return (1);

	while ((ent = readdir(d))) {
		/* Trim out 'repos'. */
		if ((n = strlen(ent->d_name)) <= 5)
			continue;
		p = &ent->d_name[n - 5];
		if (strcmp(p, ".conf") == 0) {
			snprintf(path, sizeof(path), "%s%s%s",
			    repodir,
			    repodir[strlen(repodir) - 1] == '/' ? "" : "/",
			    ent->d_name);
			if (access(path, F_OK) == 0 &&
			    read_conf_file(path, CONFFILE_REPO)) {
				ret = 1;
				goto cleanup;
			}
		}
	}

cleanup:
	closedir(d);

	return (ret);
}

int
config_init(void)
{
	char *val;
	int i;
	const char *localbase;
	char *env_list_item;
	char confpath[MAXPATHLEN];
	struct config_value *cv;
	char abi[BUFSIZ];

	for (i = 0; i < CONFIG_SIZE; i++) {
		val = getenv(c[i].key);
		if (val != NULL) {
			c[i].envset = true;
			switch (c[i].type) {
			case PKG_CONFIG_LIST:
				/* Split up comma-separated items from env. */
				c[i].list = malloc(sizeof(*c[i].list));
				STAILQ_INIT(c[i].list);
				for (env_list_item = strtok(val, ",");
				    env_list_item != NULL;
				    env_list_item = strtok(NULL, ",")) {
					cv =
					    malloc(sizeof(struct config_value));
					cv->value =
					    strdup(env_list_item);
					STAILQ_INSERT_TAIL(c[i].list, cv,
					    next);
				}
				break;
			default:
				c[i].val = val;
				break;
			}
		}
	}

	/* Read LOCALBASE/etc/pkg.conf first. */
	localbase = getenv("LOCALBASE") ? getenv("LOCALBASE") : _LOCALBASE;
	snprintf(confpath, sizeof(confpath), "%s/etc/pkg.conf",
	    localbase);

	if (access(confpath, F_OK) == 0 && read_conf_file(confpath,
	    CONFFILE_PKG))
		goto finalize;

	/* Then read in all repos from REPOS_DIR list of directories. */
	if (c[REPOS_DIR].list == NULL) {
		c[REPOS_DIR].list = malloc(sizeof(*c[REPOS_DIR].list));
		STAILQ_INIT(c[REPOS_DIR].list);
		cv = malloc(sizeof(struct config_value));
		cv->value = strdup("/etc/pkg");
		STAILQ_INSERT_TAIL(c[REPOS_DIR].list, cv, next);
		cv = malloc(sizeof(struct config_value));
		if (asprintf(&cv->value, "%s/etc/pkg/repos", localbase) < 0)
			goto finalize;
		STAILQ_INSERT_TAIL(c[REPOS_DIR].list, cv, next);
	}

	STAILQ_FOREACH(cv, c[REPOS_DIR].list, next)
		if (load_repositories(cv->value))
			goto finalize;

finalize:
	if (c[ABI].val == NULL && c[ABI].value == NULL) {
		if (pkg_get_myabi(abi, BUFSIZ) != 0)
			errx(EXIT_FAILURE, "Failed to determine the system "
			    "ABI");
		c[ABI].val = abi;
	}

	subst_packagesite(c[ABI].value != NULL ? c[ABI].value : c[ABI].val);

	return (0);
}

int
config_string(pkg_config_key k, const char **val)
{
	if (c[k].type != PKG_CONFIG_STRING)
		return (-1);

	if (c[k].value != NULL)
		*val = c[k].value;
	else
		*val = c[k].val;

	return (0);
}

int
config_bool(pkg_config_key k, bool *val)
{
	const char *value;

	if (c[k].type != PKG_CONFIG_BOOL)
		return (-1);

	*val = false;

	if (c[k].value != NULL)
		value = c[k].value;
	else
		value = c[k].val;

	if (boolstr_to_bool(value))
		*val = true;

	return (0);
}

void
config_finish(void) {
	int i;

	for (i = 0; i < CONFIG_SIZE; i++)
		free(c[i].value);
}
