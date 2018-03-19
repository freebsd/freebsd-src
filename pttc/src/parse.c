/*
 * Copyright (c) 2013-2018, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "errcode.h"
#include "parse.h"
#include "util.h"
#include "pt_compiler.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if defined(_MSC_VER) && (_MSC_VER < 1900)
#  define snprintf _snprintf_c
#endif


static const char *pt_suffix = ".pt";
static const char *exp_suffix = ".exp";

#if defined(FEATURE_SIDEBAND)
static const char *sb_suffix = ".sb";
#endif

enum {
	pd_len = 1024
};

#if defined(FEATURE_SIDEBAND)

static void sb_rename_file(struct sb_file *sb)
{
	char filename[FILENAME_MAX];

	/* We encode the configuration in the sideband filename. */
	switch (sb->format) {
	case sbf_raw:
		strncpy(filename, sb->name, sizeof(filename));

		/* Make sure @filename is terminated. */
		filename[sizeof(filename) - 1] = 0;
		break;

#if defined(FEATURE_PEVENT)
	case sbf_pevent: {
		const struct pev_config *config;
		size_t base_len, ext_len, suffix_len, total_len;
		int errcode, printed;
		char extension[256];

		config = &sb->variant.pevent.config;

		printed = snprintf(extension, sizeof(extension),
				   ",sample-type=0x%" PRIx64 ",time-zero=0x%"
				   PRIx64 ",time-shift=0x%u" ",time-mult=0x%u",
				   config->sample_type, config->time_zero,
				   config->time_shift, config->time_mult);
		if (printed < 0) {
			fprintf(stderr, "error renaming %s.\n", sb->name);
			return;
		}

		ext_len = (size_t) printed;
		suffix_len = strnlen(sb_suffix, sizeof(filename));

		base_len = strnlen(sb->name, sizeof(filename));
		base_len -= suffix_len;

		total_len = base_len + ext_len + suffix_len + 1;
		if (sizeof(filename) <= total_len) {
			fprintf(stderr, "warning: %s could not be renamed.\n",
				sb->name);
			return;
		}

		strncpy(filename, sb->name, base_len);

		printed = snprintf(filename + base_len,
				   sizeof(filename) - base_len, "%s%s",
				   extension, sb_suffix);
		if (printed < 0) {
			fprintf(stderr, "error renaming %s.\n", sb->name);
			return;
		}

		errno = 0;
		errcode = rename(sb->name, filename);
		if (errcode < 0)
			fprintf(stderr, "error renaming %s: %s.\n",
				sb->name, strerror(errno));
	}
		break;
#endif /* defined(FEATURE_PEVENT) */
	}

	/* Print the name of the sideband file for test.bash. */
	printf("%s\n", filename);
}

#endif /* defined(FEATURE_SIDEBAND) */

/* Deallocates the memory used by @p, closes all files, clears and
 * zeroes the fields.
 */
static void p_free(struct parser *p)
{
	if (!p)
		return;

	yasm_free(p->y);
	pd_free(p->pd);
	l_free(p->pt_labels);
	free(p->ptfilename);

#if defined(FEATURE_SIDEBAND)
	{
		struct sb_filelist *sb;

		sb = p->sbfiles;
		while (sb) {
			struct sb_filelist *trash;

			trash = sb;
			sb = sb->next;

			fclose(trash->sbfile.file);

			sb_rename_file(&trash->sbfile);

			free(trash->sbfile.name);
			free(trash);
		}
	}
#endif /* defined(FEATURE_SIDEBAND) */

	free(p);
}

/* Initializes @p with @pttfile and @conf.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 * Returns -err_internal if @p is the NULL pointer.
 */
static struct parser *p_alloc(const char *pttfile, const struct pt_config *conf)
{
	size_t n;
	struct parser *p;

	if (!conf)
		return NULL;

	if (!pttfile)
		return NULL;

	p = calloc(1, sizeof(*p));
	if (!p)
		return NULL;

	p->y = yasm_alloc(pttfile);
	if (!p->y)
		goto error;

	n = strlen(p->y->fileroot) + 1;

	p->ptfilename = malloc(n+strlen(pt_suffix));
	if (!p->ptfilename)
		goto error;

	strcpy(p->ptfilename, p->y->fileroot);
	strcat(p->ptfilename, pt_suffix);

	p->pd = pd_alloc(pd_len);
	if (!p->pd)
		goto error;

	p->pt_labels = l_alloc();
	if (!p->pt_labels)
		goto error;

	p->conf = conf;

#if defined(FEATURE_SIDEBAND)
	p->sbfiles = NULL;
	p->current_sbfile = NULL;
#endif

	return p;

error:
	p_free(p);
	return NULL;
}

/* Generates an .exp filename following the scheme:
 *	<fileroot>[-<extra>].exp
 */
static char *expfilename(struct parser *p, const char *extra)
{
	char *filename;
	/* reserve enough space to hold the string
	 *   "-cpu_fffff_mmm_sss" + 1 for the trailing null character.
	 */
	char cpu_suffix[19];
	size_t n;

	if (!extra)
		extra = "";
	*cpu_suffix = '\0';

	/* determine length of resulting filename, which looks like:
	 *   <fileroot>[-<extra>][-cpu_<f>_<m>_<s>].exp
	 */
	n = strlen(p->y->fileroot);

	if (*extra != '\0')
		/* the extra string is prepended with a -.  */
		n += 1 + strlen(extra);

	if (p->conf->cpu.vendor != pcv_unknown) {
		struct pt_cpu cpu;

		cpu = p->conf->cpu;
		if (cpu.stepping)
			n += sprintf(cpu_suffix,
				     "-cpu_%" PRIu16 "_%" PRIu8 "_%" PRIu8 "",
				     cpu.family, cpu.model, cpu.stepping);
		else
			n += sprintf(cpu_suffix,
				     "-cpu_%" PRIu16 "_%" PRIu8 "", cpu.family,
				     cpu.model);
	}

	n += strlen(exp_suffix);

	/* trailing null character.  */
	n += 1;

	filename = malloc(n);
	if (!filename)
		return NULL;

	strcpy(filename, p->y->fileroot);
	if (*extra != '\0') {
		strcat(filename, "-");
		strcat(filename, extra);
	}
	strcat(filename, cpu_suffix);
	strcat(filename, exp_suffix);

	return filename;
}

/* Returns true if @c is part of a label; false otherwise.  */
static int islabelchar(int c)
{
	if (isalnum(c))
		return 1;

	switch (c) {
	case '_':
		return 1;
	}

	return 0;
}

/* Generates the content of the .exp file by printing all lines with
 * everything up to and including the first comment semicolon removed.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 * Returns -err_internal if @p is the NULL pointer.
 * Returns -err_file_write if the .exp file could not be fully written.
 */
static int p_gen_expfile(struct parser *p)
{
	int errcode;
	enum { slen = 1024 };
	char s[slen];
	struct pt_directive *pd;
	char *filename;
	FILE *f;

	if (bug_on(!p))
		return -err_internal;

	pd = p->pd;

	/* the directive in the current line must be the .exp directive.  */
	errcode = yasm_pd_parse(p->y, pd);
	if (bug_on(errcode < 0))
		return -err_internal;

	if (bug_on(strcmp(pd->name, ".exp") != 0))
		return -err_internal;

	filename = expfilename(p, pd->payload);
	if (!filename)
		return -err_no_mem;
	f = fopen(filename, "w");
	if (!f) {
		free(filename);
		return -err_file_open;
	}

	for (;;) {
		int i;
		char *line, *comment;

		errcode = yasm_next_line(p->y, s, slen);
		if (errcode < 0)
			break;

		errcode = yasm_pd_parse(p->y, pd);
		if (errcode < 0 && errcode != -err_no_directive)
			break;

		if (errcode == 0 && strcmp(pd->name, ".exp") == 0) {
			fclose(f);
			printf("%s\n", filename);
			free(filename);
			filename = expfilename(p, pd->payload);
			if (!filename)
				return -err_no_mem;
			f = fopen(filename, "w");
			if (!f) {
				free(filename);
				return -err_file_open;
			}
			continue;
		}

		line = strchr(s, ';');
		if (!line)
			continue;

		line += 1;

		comment = strchr(line, '#');
		if (comment)
			*comment = '\0';

		/* remove trailing spaces.  */
		for (i = (int) strlen(line)-1; i >= 0 && isspace(line[i]); i--)
			line[i] = '\0';

		for (;;) {
			char *tmp, label[256];
			uint64_t addr;
			int zero_padding, qmark_padding, qmark_size, status;

			zero_padding = 0;
			qmark_padding = 0;
			qmark_size = 0;
			status = 0;

			/* find the label character in the string.
			 * if there is no label character, we just print
			 * the rest of the line and end.
			 */
			tmp = strchr(line, '%');
			if (!tmp) {
				if (fprintf(f, "%s", line) < 0) {
					errcode = -err_file_write;
					goto error;
				}
				break;
			}

			/* make the label character a null byte and
			 * print the first portion, which does not
			 * belong to the label into the file.
			 */
			*tmp = '\0';
			if (fprintf(f, "%s", line) < 0) {
				errcode = -err_file_write;
				goto error;
			}

			/* test if there is a valid label name after the %.  */
			line = tmp+1;
			if (*line == '\0' || isspace(*line)) {
				errcode = -err_no_label;
				goto error;
			}

			/* check if zero padding is requested.  */
			if (*line == '0') {
				zero_padding = 1;
				line += 1;
			}
			/* chek if ? padding is requested.  */
			else if (*line == '?') {
				qmark_padding = 1;
				zero_padding = 1;
				qmark_size = 0;
				line += 1;
			}

			/* advance i to the first non alpha-numeric
			 * character. all characters everything from
			 * line[0] to line[i-1] belongs to the label
			 * name.
			 */
			for (i = 0; islabelchar(line[i]); i++)
				;

			if (i > 255) {
				errcode = -err_label_name;
				goto error;
			}
			strncpy(label, line, i);
			label[i] = '\0';

			/* advance to next character.  */
			line = &line[i];

			/* lookup the label name and print it to the
			 * output file.
			 */
			errcode = yasm_lookup_label(p->y, &addr, label);
			if (errcode < 0) {
				errcode = l_lookup(p->pt_labels, &addr, label);
				if (errcode < 0)
					goto error;

				if (zero_padding)
					status = fprintf(f, "%016" PRIx64, addr);
				else
					status = fprintf(f, "%" PRIx64, addr);

				if (status < 0) {
					errcode = -err_file_write;
					goto error;
				}

				continue;
			}

			/* check if masking is requested.  */
			if (*line == '.') {
				char *endptr;
				unsigned long int n;

				line += 1;

				n = strtoul(line, &endptr, 0);
				/* check if strtol made progress and
				 * stops on a space or null byte.
				 * otherwise the int could not be
				 * parsed.
				 */
				if (line == endptr ||
				    (*endptr != '\0' && !isspace(*endptr)
				     && !ispunct(*endptr))) {
					errcode = -err_parse_int;
					goto error;
				}
				if (8 < n) {
					errcode = -err_parse_int;
					goto error;
				}

				addr &= (1ull << (n << 3)) - 1ull;
				line = endptr;

				qmark_size = (int) (8 - n);
			}

			if (qmark_padding) {
				for (i = 0; i < qmark_size; ++i) {
					status = fprintf(f, "??");
					if (status < 0) {
						errcode = -err_file_write;
						goto error;
					}
				}

				for (; i < 8; ++i) {
					uint8_t byte;

					byte = (uint8_t)(addr >> ((7 - i) * 8));

					status = fprintf(f, "%02" PRIx8, byte);
					if (status < 0) {
						errcode = -err_file_write;
						goto error;
					}
				}
			} else if (zero_padding)
				status = fprintf(f, "%016" PRIx64, addr);
			else
				status = fprintf(f, "%" PRIx64, addr);

			if (status < 0) {
				errcode = -err_file_write;
				goto error;
			}

		}

		if (fprintf(f, "\n") < 0) {
			errcode = -err_file_write;
			goto error;
		}
	}

error:

	fclose(f);
	if (errcode < 0 && errcode != -err_out_of_range) {
		fprintf(stderr, "fatal: %s could not be created:\n", filename);
		yasm_print_err(p->y, "", errcode);
		remove(filename);
	} else
		printf("%s\n", filename);
	free(filename);

	/* If there are no lines left, we are done.  */
	if (errcode == -err_out_of_range)
		return 0;

	return errcode;
}

static void p_close_files(struct parser *p)
{
	if (p->ptfile) {
		fclose(p->ptfile);
		p->ptfile = NULL;
	}
}

static int p_open_files(struct parser *p)
{
	p->ptfile = fopen(p->ptfilename, "wb");
	if (!p->ptfile) {
		fprintf(stderr, "open %s failed\n", p->ptfilename);
		goto error;
	}
	return 0;

error:
	p_close_files(p);
	return -err_file_open;
}

static int parse_mwait(uint32_t *hints, uint32_t *ext, char *payload)
{
	char *endptr;
	unsigned long i;

	if (bug_on(!hints || !ext))
		return -err_internal;

	payload = strtok(payload, ",");
	if (!payload || *payload == '\0')
		return -err_parse_no_args;

	i = strtoul(payload, &endptr, 0);
	if (payload == endptr || *endptr != '\0')
		return -err_parse_int;

	if (UINT32_MAX < i)
		return -err_parse_int_too_big;

	*hints = (uint32_t)i;

	payload = strtok(NULL, " ,");
	if (!payload)
		return -err_parse_no_args;

	i = strtoul(payload, &endptr, 0);
	if (payload == endptr || *endptr != '\0')
		return -err_parse_int;

	if (UINT32_MAX < i)
		return -err_parse_int_too_big;

	*ext = (uint32_t)i;

	/* no more tokens left.  */
	payload = strtok(NULL, " ");
	if (payload)
		return -err_parse_trailing_tokens;

	return 0;
}

static int parse_c_state(uint8_t *state, uint8_t *sub_state, const char *input)
{
	unsigned int maj, min;
	int matches;

	if (!input)
		return -err_parse_no_args;

	maj = 0;
	min = 0;
	matches = sscanf(input, " c%u.%u", &maj, &min);
	switch (matches) {
	case 0:
		return -err_parse_no_args;

	case 2:
		if (!sub_state)
			return -err_parse_c_state_sub;

		if (0xf <= min)
			return -err_parse_c_state_invalid;

		fallthrough;
	case 1:
		if (!state)
			return -err_internal;

		if (0xf <= maj)
			return -err_parse_c_state_invalid;

		break;
	}

	*state = (uint8_t) ((maj - 1) & 0xf);
	if (sub_state)
		*sub_state = (uint8_t) ((min - 1) & 0xf);

	return 0;
}

/* Processes the current directive.
 * If the encoder returns an error, a message including current file and
 * line number together with the pt error string is printed on stderr.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 * Returns -err_internal if @p or @e is the NULL pointer.
 * Returns -err_pt_lib if the pt encoder returned an error.
 * Returns -err_parse if a general parsing error was encountered.
 * Returns -err_parse_unknown_directive if there was an unknown pt directive.
 */
static int p_process_pt(struct parser *p, struct pt_encoder *e)
{
	struct pt_directive *pd;
	struct pt_packet packet;
	char *directive, *payload;
	int bytes_written, errcode;

	if (bug_on(!p))
		return -err_internal;

	if (bug_on(!e))
		return -err_internal;

	pd = p->pd;
	if (!pd)
		return -err_internal;

	directive = pd->name;
	payload = pd->payload;

	if (strcmp(directive, "psb") == 0) {
		errcode = parse_empty(payload);
		if (errcode < 0) {
			yasm_print_err(p->y, "psb: parsing failed", errcode);
			return errcode;
		}
		packet.type = ppt_psb;
	} else if (strcmp(directive, "psbend") == 0) {
		errcode = parse_empty(payload);
		if (errcode < 0) {
			yasm_print_err(p->y, "psbend: parsing failed", errcode);
			return errcode;
		}
		packet.type = ppt_psbend;
	} else if (strcmp(directive, "pad") == 0) {
		errcode = parse_empty(payload);
		if (errcode < 0) {
			yasm_print_err(p->y, "pad: parsing failed", errcode);
			return errcode;
		}
		packet.type = ppt_pad;
	} else if (strcmp(directive, "ovf") == 0) {
		errcode = parse_empty(payload);
		if (errcode < 0) {
			yasm_print_err(p->y, "ovf: parsing failed", errcode);
			return errcode;
		}
		packet.type = ppt_ovf;
	} else if (strcmp(directive, "stop") == 0) {
		errcode = parse_empty(payload);
		if (errcode < 0) {
			yasm_print_err(p->y, "stop: parsing failed", errcode);
			return errcode;
		}
		packet.type = ppt_stop;
	} else if (strcmp(directive, "tnt") == 0) {
		errcode = parse_tnt(&packet.payload.tnt.payload,
				    &packet.payload.tnt.bit_size, payload);
		if (errcode < 0) {
			yasm_print_err(p->y, "tnt: parsing failed", errcode);
			return errcode;
		}
		packet.type = ppt_tnt_8;
	} else if (strcmp(directive, "tnt64") == 0) {
		errcode = parse_tnt(&packet.payload.tnt.payload,
				    &packet.payload.tnt.bit_size, payload);
		if (errcode < 0) {
			yasm_print_err(p->y, "tnt64: parsing failed", errcode);
			return errcode;
		}
		packet.type = ppt_tnt_64;
	} else if (strcmp(directive, "tip") == 0) {
		errcode = parse_ip(p, &packet.payload.ip.ip,
				   &packet.payload.ip.ipc, payload);
		if (errcode < 0) {
			yasm_print_err(p->y, "tip: parsing failed", errcode);
			return errcode;
		}
		packet.type = ppt_tip;
	} else if (strcmp(directive, "tip.pge") == 0) {
		errcode = parse_ip(p, &packet.payload.ip.ip,
				   &packet.payload.ip.ipc, payload);
		if (errcode < 0) {
			yasm_print_err(p->y, "tip.pge: parsing failed",
				       errcode);
			return errcode;
		}
		packet.type = ppt_tip_pge;
	} else if (strcmp(directive, "tip.pgd") == 0) {
		errcode = parse_ip(p, &packet.payload.ip.ip,
				   &packet.payload.ip.ipc, payload);
		if (errcode < 0) {
			yasm_print_err(p->y, "tip.pgd: parsing failed",
				       errcode);
			return errcode;
		}
		packet.type = ppt_tip_pgd;
	} else if (strcmp(directive, "fup") == 0) {
		errcode = parse_ip(p, &packet.payload.ip.ip,
				   &packet.payload.ip.ipc, payload);
		if (errcode < 0) {
			yasm_print_err(p->y, "fup: parsing failed", errcode);
			return errcode;
		}
		packet.type = ppt_fup;
	} else if (strcmp(directive, "mode.exec") == 0) {
		if (strcmp(payload, "16bit") == 0) {
			packet.payload.mode.bits.exec.csl = 0;
			packet.payload.mode.bits.exec.csd = 0;
		} else if (strcmp(payload, "64bit") == 0) {
			packet.payload.mode.bits.exec.csl = 1;
			packet.payload.mode.bits.exec.csd = 0;
		} else if (strcmp(payload, "32bit") == 0) {
			packet.payload.mode.bits.exec.csl = 0;
			packet.payload.mode.bits.exec.csd = 1;
		} else {
			errcode = yasm_print_err(p->y,
						 "mode.exec: argument must be one of \"16bit\", \"64bit\" or \"32bit\"",
						 -err_parse);
			return errcode;
		}
		packet.payload.mode.leaf = pt_mol_exec;
		packet.type = ppt_mode;
	} else if (strcmp(directive, "mode.tsx") == 0) {
		if (strcmp(payload, "begin") == 0) {
			packet.payload.mode.bits.tsx.intx = 1;
			packet.payload.mode.bits.tsx.abrt = 0;
		} else if (strcmp(payload, "abort") == 0) {
			packet.payload.mode.bits.tsx.intx = 0;
			packet.payload.mode.bits.tsx.abrt = 1;
		} else if (strcmp(payload, "commit") == 0) {
			packet.payload.mode.bits.tsx.intx = 0;
			packet.payload.mode.bits.tsx.abrt = 0;
		} else {
			errcode = yasm_print_err(p->y,
						 "mode.tsx: argument must be one of \"begin\", \"abort\" or \"commit\"",
						 -err_parse);
			return errcode;
		}
		packet.payload.mode.leaf = pt_mol_tsx;
		packet.type = ppt_mode;
	} else if (strcmp(directive, "pip") == 0) {
		const char *modifier;

		errcode = parse_uint64(&packet.payload.pip.cr3, payload);
		if (errcode < 0) {
			yasm_print_err(p->y, "pip: parsing failed", errcode);
			return errcode;
		}
		packet.type = ppt_pip;
		packet.payload.pip.nr = 0;

		modifier = strtok(NULL, " ,");
		if (modifier) {
			if (strcmp(modifier, "nr") == 0)
				packet.payload.pip.nr = 1;
			else {
				yasm_print_err(p->y, "pip: parsing failed",
					       -err_parse_trailing_tokens);
				return errcode;
			}
		}
	} else if (strcmp(directive, "tsc") == 0) {
		errcode = parse_uint64(&packet.payload.tsc.tsc, payload);
		if (errcode < 0) {
			yasm_print_err(p->y, "tsc: parsing failed", errcode);
			return errcode;
		}
		packet.type = ppt_tsc;
	} else if (strcmp(directive, "cbr") == 0) {
		errcode = parse_uint8(&packet.payload.cbr.ratio, payload);
		if (errcode < 0) {
			yasm_print_err(p->y, "cbr: parsing cbr failed",
				       errcode);
			return errcode;
		}
		packet.type = ppt_cbr;
	} else if (strcmp(directive, "tma") == 0) {
		errcode = parse_tma(&packet.payload.tma.ctc,
				    &packet.payload.tma.fc, payload);
		if (errcode < 0) {
			yasm_print_err(p->y, "tma: parsing tma failed",
				       errcode);
			return errcode;
		}
		packet.type = ppt_tma;
	} else if (strcmp(directive, "mtc") == 0) {
		errcode = parse_uint8(&packet.payload.mtc.ctc, payload);
		if (errcode < 0) {
			yasm_print_err(p->y, "mtc: parsing mtc failed",
				       errcode);
			return errcode;
		}
		packet.type = ppt_mtc;
	} else if (strcmp(directive, "cyc") == 0) {
		errcode = parse_uint64(&packet.payload.cyc.value, payload);
		if (errcode < 0) {
			yasm_print_err(p->y, "cyc: parsing cyc failed",
				       errcode);
			return errcode;
		}
		packet.type = ppt_cyc;
	} else if (strcmp(directive, "vmcs") == 0) {
		errcode = parse_uint64(&packet.payload.vmcs.base, payload);
		if (errcode < 0) {
			yasm_print_err(p->y, "vmcs: parsing failed", errcode);
			return errcode;
		}
		packet.type = ppt_vmcs;
	} else if (strcmp(directive, "mnt") == 0) {
		errcode = parse_uint64(&packet.payload.mnt.payload, payload);
		if (errcode < 0) {
			yasm_print_err(p->y, "mnt: parsing failed", errcode);
			return errcode;
		}
		packet.type = ppt_mnt;
	} else if (strcmp(directive, "exstop") == 0) {
		packet.type = ppt_exstop;
		memset(&packet.payload.exstop, 0,
		       sizeof(packet.payload.exstop));

		if (strcmp(payload, "ip") == 0)
			packet.payload.exstop.ip = 1;
		else if (*payload) {
			yasm_print_err(p->y, "exstop: parsing failed",
				       -err_parse_trailing_tokens);
			return -err_parse_trailing_tokens;
		}
	} else if (strcmp(directive, "mwait") == 0) {
		errcode = parse_mwait(&packet.payload.mwait.hints,
				      &packet.payload.mwait.ext, payload);
		if (errcode < 0) {
			yasm_print_err(p->y, "mwait: parsing failed", errcode);
			return errcode;
		}

		packet.type = ppt_mwait;
	} else if (strcmp(directive, "pwre") == 0) {
		char *token;

		packet.type = ppt_pwre;
		memset(&packet.payload.pwre, 0, sizeof(packet.payload.pwre));

		token = strtok(payload, " , ");
		errcode = parse_c_state(&packet.payload.pwre.state,
					&packet.payload.pwre.sub_state, token);
		if (errcode < 0) {
			yasm_print_err(p->y, "pwre: bad C-state", errcode);
			return errcode;
		}

		token = strtok(NULL, " ,");
		if (token) {
			if (strcmp(token, "hw") == 0)
				packet.payload.pwre.hw = 1;
			else {
				yasm_print_err(p->y, "pwre: parsing failed",
					       -err_parse_trailing_tokens);
				return -err_parse_trailing_tokens;
			}
		}
	} else if (strcmp(directive, "pwrx") == 0) {
		char *token;

		packet.type = ppt_pwrx;
		memset(&packet.payload.pwrx, 0, sizeof(packet.payload.pwrx));

		token = strtok(payload, ":");
		if (!token) {
			yasm_print_err(p->y, "pwrx: parsing failed",
				       -err_parse_no_args);
			return -err_parse_no_args;
		}

		if (strcmp(token, "int") == 0)
			packet.payload.pwrx.interrupt = 1;
		else if (strcmp(token, "st") == 0)
			packet.payload.pwrx.store = 1;
		else if (strcmp(token, "hw") == 0)
			packet.payload.pwrx.autonomous = 1;
		else {
			yasm_print_err(p->y, "pwrx: bad wake reason",
				       -err_parse);
			return -err_parse;
		}

		token = strtok(NULL, " ,");
		errcode = parse_c_state(&packet.payload.pwrx.last, NULL, token);
		if (errcode < 0) {
			yasm_print_err(p->y, "pwrx: bad last C-state", errcode);
			return errcode;
		}

		token = strtok(NULL, " ,");
		errcode = parse_c_state(&packet.payload.pwrx.deepest, NULL,
					token);
		if (errcode < 0) {
			yasm_print_err(p->y, "pwrx: bad deepest C-state",
				       errcode);
			return errcode;
		}
	} else if (strcmp(directive, "ptw") == 0) {
		char *token;

		packet.type = ppt_ptw;
		memset(&packet.payload.ptw, 0, sizeof(packet.payload.ptw));

		token = strtok(payload, ":");
		if (!token) {
			yasm_print_err(p->y, "ptw: parsing failed",
				       -err_parse_no_args);
			return -err_parse_no_args;
		}

		errcode = str_to_uint8(token, &packet.payload.ptw.plc, 0);
		if (errcode < 0) {
			yasm_print_err(p->y, "ptw: bad payload size", errcode);
			return errcode;
		}

		token = strtok(NULL, ", ");
		if (!token) {
			yasm_print_err(p->y, "ptw: no payload",
				       -err_parse_no_args);
			return -err_parse_no_args;
		}

		errcode = str_to_uint64(token, &packet.payload.ptw.payload, 0);
		if (errcode < 0) {
			yasm_print_err(p->y, "ptw: bad payload", errcode);
			return errcode;
		}

		token = strtok(NULL, " ");
		if (token) {
			if (strcmp(token, "ip") != 0) {
				yasm_print_err(p->y, "ptw: parse error",
					       -err_parse_trailing_tokens);
				return -err_parse_trailing_tokens;
			}

			packet.payload.ptw.ip = 1;
		}
	} else {
		errcode = yasm_print_err(p->y, "invalid syntax",
					 -err_parse_unknown_directive);
		return errcode;
	}

	bytes_written = pt_enc_next(e, &packet);
	if (bytes_written < 0) {
		char msg[128];

		snprintf(msg, sizeof(msg),
			 "encoder error in directive %s (status %s)", directive,
			 pt_errstr(pt_errcode(bytes_written)));

		yasm_print_err(p->y, msg, -err_pt_lib);
	} else
		p->pt_bytes_written += bytes_written;

	return bytes_written;
}

#if defined(FEATURE_SIDEBAND)

static int sb_open(struct parser *p, const char *fmt, const char *src,
		   const char *prio)
{
	struct sb_filelist *sbfiles;
	const char *root;
	char name[FILENAME_MAX];
	FILE *file;

	if (bug_on(!p) || bug_on(!p->y) || bug_on(!prio))
		return -err_internal;

	root = p->y->fileroot;
	if (!root) {
		yasm_print_err(p->y, "open - name root", -err_internal);
		return -err_internal;
	}

	if (src && *src)
		snprintf(name, sizeof(name), "%s-%s-%s-%s%s", root, src, fmt,
			 prio, sb_suffix);
	else
		snprintf(name, sizeof(name), "%s-%s-%s%s", root, fmt, prio,
			 sb_suffix);

	for (sbfiles = p->sbfiles; sbfiles; sbfiles = sbfiles->next) {
		if (strncmp(sbfiles->sbfile.name, name, sizeof(name)) == 0)
			break;
	}

	if (!sbfiles) {
		file = fopen(name, "w");
		if (!file) {
			yasm_print_err(p->y, name, -err_file_open);
			return -err_file_open;
		}

		sbfiles = malloc(sizeof(*sbfiles));
		if (!sbfiles) {
			yasm_print_err(p->y, "open", -err_no_mem);
			fclose(file);
			return -err_no_mem;
		}

		memset(&sbfiles->sbfile, 0, sizeof(sbfiles->sbfile));

		sbfiles->sbfile.name = duplicate_str(name);
		if (!sbfiles->sbfile.name) {
			yasm_print_err(p->y, "open", -err_no_mem);
			fclose(file);
			free(sbfiles);
			return -err_no_mem;
		}

		sbfiles->sbfile.file = file;
		sbfiles->sbfile.format = sbf_raw;

		sbfiles->next = p->sbfiles;
		p->sbfiles = sbfiles;
	}

	p->current_sbfile = &sbfiles->sbfile;
	return 0;
}

static struct sb_file *p_get_current_sbfile(struct parser *p)
{
	struct sb_file *sb;

	if (bug_on(!p))
		return NULL;

	sb = p->current_sbfile;
	if (!sb) {
		yasm_print_err(p->y, "no sideband file", -err_sb_missing);
		return NULL;
	}

	if (bug_on(!sb->file)) {
		yasm_print_err(p->y, "corrupt sideband file", -err_internal);
		return NULL;
	}

	return sb;
}

static int sb_set_format(struct parser *p, struct sb_file *sb,
			 enum sb_format format)
{
	if (bug_on(!p))
		return -err_internal;

	if (!sb)
		return -err_sb_missing;

	switch (format) {
	case sbf_raw:
		/* Raw sideband directives are allowed for all formats. */
		return 0;

#if defined(FEATURE_PEVENT)
	case sbf_pevent:
		switch (sb->format) {
		case sbf_pevent:
			return 0;

		case sbf_raw:
			sb->format = sbf_pevent;

			memset(&sb->variant.pevent, 0,
			       sizeof(sb->variant.pevent));
			sb->variant.pevent.config.size =
				sizeof(sb->variant.pevent.config);
			sb->variant.pevent.config.time_shift = 0;
			sb->variant.pevent.config.time_mult = 1;
			sb->variant.pevent.config.time_zero = 0ull;
			return 0;

		default:
			yasm_print_err(p->y, "mixing sideband formats",
				       -err_sb_mix);
			return -err_sb_mix;
		}
#endif /* defined(FEATURE_PEVENT) */
	}

	yasm_print_err(p->y, "unknown sideband format", -err_internal);
	return -err_internal;
}

static int sb_raw(struct parser *p, const void *buffer, size_t size)
{
	struct sb_file *sb;
	size_t written;
	int errcode;

	if (bug_on(!p))
		return -err_internal;

	sb = p_get_current_sbfile(p);
	if (!sb)
		return -err_sb_missing;

	errcode = sb_set_format(p, sb, sbf_raw);
	if (errcode < 0)
		return errcode;

	written = fwrite(buffer, size, 1, sb->file);
	if (written != 1) {
		yasm_print_err(p->y, "write failed", -err_file_write);
		return -err_file_write;
	}

	sb->bytes_written += (int) size;
	return 0;
}

static int sb_raw_8(struct parser *p, uint8_t value)
{
	return sb_raw(p, &value, sizeof(value));
}

static int sb_raw_16(struct parser *p, uint16_t value)
{
	return sb_raw(p, &value, sizeof(value));
}

static int sb_raw_32(struct parser *p, uint32_t value)
{
	return sb_raw(p, &value, sizeof(value));
}

static int sb_raw_64(struct parser *p, uint64_t value)
{
	return sb_raw(p, &value, sizeof(value));
}

#if defined(FEATURE_PEVENT)

/* A buffer to hold sample values to which a pev_event can point. */

struct pev_sample_buffer {
	uint32_t pid;
	uint32_t tid;
	uint64_t time;
	uint64_t id;
	uint64_t stream_id;
	uint32_t cpu;
	uint64_t identifier;
};

static int pevent_sample_type(struct parser *p, uint64_t sample_type)
{
	struct sb_file *sb;
	int errcode;

	sb = p_get_current_sbfile(p);
	if (!sb)
		return -err_sb_missing;

	errcode = sb_set_format(p, sb, sbf_pevent);
	if (errcode < 0)
		return errcode;

	if (sb->variant.pevent.is_final) {
		yasm_print_err(p->y,
			       "the sideband configuration can no longer be "
			       "modified", -err_sb_final);
		return -err_sb_final;
	}

	sb->variant.pevent.config.sample_type = sample_type;
	return 0;
}

static int pevent_process_samples(struct pev_event *event,
				  struct pev_sample_buffer *samples,
				  struct parser *p,
				  const struct pev_config *config,
				  char *payload)
{
	char *token;

	if (bug_on(!event) || bug_on(!samples) || bug_on(!config))
		return -err_internal;

	if (config->sample_type & PERF_SAMPLE_TID) {
		int errcode;

		token = strtok(payload, " ,");
		if (!token) {
			yasm_print_err(p->y, "pid missing", -err_parse);
			return -err_parse;
		}

		payload = NULL;

		errcode = str_to_uint32(token, &samples->pid, 0);
		if (errcode < 0) {
			yasm_print_err(p->y, "bad pid", errcode);
			return errcode;
		}

		token = strtok(payload, " ,");
		if (!token) {
			yasm_print_err(p->y, "tid missing", -err_parse);
			return -err_parse;
		}

		errcode = str_to_uint32(token, &samples->tid, 0);
		if (errcode < 0) {
			yasm_print_err(p->y, "bad tid", errcode);
			return errcode;
		}

		event->sample.pid = &samples->pid;
		event->sample.tid = &samples->tid;
	}

	if (config->sample_type & PERF_SAMPLE_TIME) {
		int errcode;

		token = strtok(payload, " ,");
		if (!token) {
			yasm_print_err(p->y, "tsc missing", -err_parse);
			return -err_parse;
		}

		payload = NULL;

		errcode = str_to_uint64(token, &event->sample.tsc, 0);
		if (errcode < 0) {
			yasm_print_err(p->y, "bad tsc", errcode);
			return errcode;
		}

		errcode = pev_time_from_tsc(&samples->time, event->sample.tsc,
					    config);
		if (errcode < 0) {
			fprintf(stderr, "error converting tsc %"PRIx64": %s\n",
				event->sample.tsc,
				pt_errstr(pt_errcode(errcode)));
			return -err_pt_lib;
		}

		event->sample.time = &samples->time;
	}

	if (config->sample_type & PERF_SAMPLE_ID) {
		int errcode;

		token = strtok(payload, " ,");
		if (!token) {
			yasm_print_err(p->y, "id missing", -err_parse);
			return -err_parse;
		}

		payload = NULL;

		errcode = str_to_uint64(token, &samples->id, 0);
		if (errcode < 0) {
			yasm_print_err(p->y, "bad id", errcode);
			return errcode;
		}

		event->sample.id = &samples->id;
	}

	if (config->sample_type & PERF_SAMPLE_CPU) {
		int errcode;

		token = strtok(payload, " ,");
		if (!token) {
			yasm_print_err(p->y, "cpu missing", -err_parse);
			return -err_parse;
		}

		payload = NULL;

		errcode = str_to_uint32(token, &samples->cpu, 0);
		if (errcode < 0) {
			yasm_print_err(p->y, "bad cpu", errcode);
			return errcode;
		}

		event->sample.cpu = &samples->cpu;
	}

	if (config->sample_type & PERF_SAMPLE_STREAM_ID) {
		int errcode;

		token = strtok(payload, " ,");
		if (!token) {
			yasm_print_err(p->y, "stream missing", -err_parse);
			return -err_parse;
		}

		payload = NULL;

		errcode = str_to_uint64(token, &samples->stream_id, 0);
		if (errcode < 0) {
			yasm_print_err(p->y, "bad stream", errcode);
			return errcode;
		}

		event->sample.stream_id = &samples->stream_id;
	}

	if (config->sample_type & PERF_SAMPLE_IDENTIFIER) {
		int errcode;

		token = strtok(payload, " ,");
		if (!token) {
			yasm_print_err(p->y, "identifier missing", -err_parse);
			return -err_parse;
		}

		payload = NULL;

		errcode = str_to_uint64(token, &samples->identifier, 0);
		if (errcode < 0) {
			yasm_print_err(p->y, "bad identifier", errcode);
			return errcode;
		}

		event->sample.identifier = &samples->identifier;
	}

	token = strtok(payload, " ,");
	if (token) {
		yasm_print_err(p->y, "unexpected samples", -err_parse);
		return -err_parse;
	}

	return 0;
}

static int sb_pevent(struct parser *p, struct pev_event *event, char *payload)
{
	const struct pev_config *config;
	struct pev_sample_buffer samples;
	struct sb_file *sb;
	uint8_t raw[FILENAME_MAX];
	int errcode, size;

	memset(raw, 0, sizeof(raw));

	sb = p_get_current_sbfile(p);
	if (!sb)
		return -err_sb_missing;

	errcode = sb_set_format(p, sb, sbf_pevent);
	if (errcode < 0)
		return errcode;

	config = &sb->variant.pevent.config;

	errcode = pevent_process_samples(event, &samples, p, config, payload);
	if (errcode < 0)
		return errcode;

	size = pev_write(event, raw, raw + sizeof(raw), config);
	if (size < 0) {
		fprintf(stderr, "error writing pevent sample: %s\n",
			pt_errstr(pt_errcode(size)));
		return -err_pt_lib;
	}

	/* Emitting a pevent sideband event finalizes the configuration. */
	sb->variant.pevent.is_final = 1;

	return sb_raw(p, raw, size);
}

static int pevent_mmap_section(struct parser *p, const char *section,
			       const char *pid, const char *tid)
{
	union {
		struct pev_record_mmap mmap;
		uint8_t buffer[FILENAME_MAX];
	} record;
	struct pev_event event;
	const char *filename;
	uint64_t start, org;
	int errcode;

	if (bug_on(!p) || bug_on(!p->y))
		return -err_internal;

	memset(record.buffer, 0, sizeof(record.buffer));
	memset(&event, 0, sizeof(event));

	filename = p->y->binfile;
	if (!filename) {
		yasm_print_err(p->y, "pevent-mmap-section - filename",
			       -err_internal);
		return -err_internal;
	}

	strncpy(record.mmap.filename, filename,
		sizeof(record.buffer) - sizeof(record.mmap));

	errcode = str_to_uint32(pid, &record.mmap.pid, 0);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-mmap-section - pid", errcode);
		return errcode;
	}

	errcode = str_to_uint32(tid, &record.mmap.tid, 0);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-mmap-section - tid", errcode);
		return errcode;
	}

	errcode = yasm_lookup_section_label(p->y, section, "vstart",
					    &record.mmap.addr);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-mmap-section - section vstart",
			       errcode);
		return errcode;
	}

	errcode = yasm_lookup_section_label(p->y, section, "length",
					    &record.mmap.len);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-mmap-section - section length",
			       errcode);
		return errcode;
	}

	errcode = yasm_lookup_section_label(p->y, section, "start",
					    &start);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-mmap-section - section start",
			       errcode);
		return errcode;
	}

	errcode = yasm_lookup_label(p->y, &org, "org");
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-mmap-section - org",
			       errcode);
		return errcode;
	}

	if (start < org) {
		yasm_print_err(p->y, "corrupt section labels", -err_internal);
		return -err_internal;
	}

	record.mmap.pgoff = start - org;

	event.type = PERF_RECORD_MMAP;
	event.record.mmap = &record.mmap;

	return sb_pevent(p, &event, NULL);
}

static int pevent_mmap(struct parser *p, const char *pid, const char *tid,
		       const char *addr, const char *len, const char *pgoff,
		       const char *filename)
{
	union {
		struct pev_record_mmap mmap;
		uint8_t buffer[FILENAME_MAX];
	} record;
	struct pev_event event;
	int errcode;

	if (bug_on(!p) || bug_on(!p->y))
		return -err_internal;

	memset(record.buffer, 0, sizeof(record.buffer));
	memset(&event, 0, sizeof(event));

	errcode = str_to_uint32(pid, &record.mmap.pid, 0);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-mmap-section - pid", errcode);
		return errcode;
	}

	errcode = str_to_uint32(tid, &record.mmap.tid, 0);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-mmap-section - tid", errcode);
		return errcode;
	}

	errcode = str_to_uint64(addr, &record.mmap.addr, 0);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-mmap-section - addr", errcode);
		return errcode;
	}

	errcode = str_to_uint64(len, &record.mmap.len, 0);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-mmap-section - len", errcode);
		return errcode;
	}

	errcode = str_to_uint64(pgoff, &record.mmap.pgoff, 0);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-mmap-section - pgoff", errcode);
		return errcode;
	}

	strncpy(record.mmap.filename, filename,
		sizeof(record.buffer) - sizeof(record.mmap));

	event.type = PERF_RECORD_MMAP;
	event.record.mmap = &record.mmap;

	return sb_pevent(p, &event, NULL);
}

static int pevent_lost(struct parser *p, const char *id, const char *lost)
{
	union {
		struct pev_record_lost lost;
		uint8_t buffer[1024];
	} record;
	struct pev_event event;
	int errcode;

	if (bug_on(!p) || bug_on(!p->y))
		return -err_internal;

	memset(record.buffer, 0, sizeof(record.buffer));
	memset(&event, 0, sizeof(event));

	errcode = str_to_uint64(id, &record.lost.id, 0);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-lost - id", errcode);
		return errcode;
	}

	errcode = str_to_uint64(lost, &record.lost.lost, 0);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-lost - lost", errcode);
		return errcode;
	}

	event.type = PERF_RECORD_LOST;
	event.record.lost = &record.lost;

	return sb_pevent(p, &event, NULL);
}

static int pevent_comm(struct parser *p, const char *pid, const char *tid,
		       const char *comm, uint16_t misc)
{
	union {
		struct pev_record_comm comm;
		uint8_t buffer[FILENAME_MAX];
	} record;
	struct pev_event event;
	int errcode;

	if (bug_on(!p) || bug_on(!p->y))
		return -err_internal;

	memset(record.buffer, 0, sizeof(record.buffer));
	memset(&event, 0, sizeof(event));

	errcode = str_to_uint32(pid, &record.comm.pid, 0);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-comm - pid", errcode);
		return errcode;
	}

	errcode = str_to_uint32(tid, &record.comm.tid, 0);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-comm - tid", errcode);
		return errcode;
	}

	strcpy(record.comm.comm, comm);

	event.type = PERF_RECORD_COMM;
	event.misc = misc;
	event.record.comm = &record.comm;

	return sb_pevent(p, &event, NULL);
}

static int pevent_exit(struct parser *p, const char *pid, const char *ppid,
		       const char *tid, const char *ptid, const char *time)
{
	union {
		struct pev_record_exit exit;
		uint8_t buffer[1024];
	} record;
	struct pev_event event;
	int errcode;

	if (bug_on(!p) || bug_on(!p->y))
		return -err_internal;

	memset(record.buffer, 0, sizeof(record.buffer));
	memset(&event, 0, sizeof(event));

	errcode = str_to_uint32(pid, &record.exit.pid, 0);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-exit - pid", errcode);
		return errcode;
	}

	errcode = str_to_uint32(ppid, &record.exit.ppid, 0);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-exit - ppid", errcode);
		return errcode;
	}

	errcode = str_to_uint32(tid, &record.exit.tid, 0);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-exit - tid", errcode);
		return errcode;
	}

	errcode = str_to_uint32(ptid, &record.exit.ptid, 0);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-exit - ptid", errcode);
		return errcode;
	}


	errcode = str_to_uint64(time, &record.exit.time, 0);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-exit - time", errcode);
		return errcode;
	}

	event.type = PERF_RECORD_EXIT;
	event.record.exit = &record.exit;

	return sb_pevent(p, &event, NULL);
}

static int pevent_fork(struct parser *p, const char *pid, const char *ppid,
		       const char *tid, const char *ptid, const char *time)
{
	union {
		struct pev_record_fork fork;
		uint8_t buffer[1024];
	} record;
	struct pev_event event;
	int errcode;

	if (bug_on(!p) || bug_on(!p->y))
		return -err_internal;

	memset(record.buffer, 0, sizeof(record.buffer));
	memset(&event, 0, sizeof(event));

	errcode = str_to_uint32(pid, &record.fork.pid, 0);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-fork - pid", errcode);
		return errcode;
	}

	errcode = str_to_uint32(ppid, &record.fork.ppid, 0);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-fork - ppid", errcode);
		return errcode;
	}

	errcode = str_to_uint32(tid, &record.fork.tid, 0);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-fork - tid", errcode);
		return errcode;
	}

	errcode = str_to_uint32(ptid, &record.fork.ptid, 0);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-fork - ptid", errcode);
		return errcode;
	}


	errcode = str_to_uint64(time, &record.fork.time, 0);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-fork - time", errcode);
		return errcode;
	}

	event.type = PERF_RECORD_FORK;
	event.record.fork = &record.fork;

	return sb_pevent(p, &event, NULL);
}

static int pevent_aux(struct parser *p, const char *offset, const char *size,
		      const char *flags)
{
	union {
		struct pev_record_aux aux;
		uint8_t buffer[1024];
	} record;
	struct pev_event event;
	int errcode;

	if (bug_on(!p) || bug_on(!p->y))
		return -err_internal;

	memset(record.buffer, 0, sizeof(record.buffer));
	memset(&event, 0, sizeof(event));

	errcode = str_to_uint64(offset, &record.aux.aux_offset, 0);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-aux - offset", errcode);
		return errcode;
	}

	errcode = str_to_uint64(size, &record.aux.aux_size, 0);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-aux - size", errcode);
		return errcode;
	}

	errcode = str_to_uint64(flags, &record.aux.flags, 0);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-aux - flags", errcode);
		return errcode;
	}

	event.type = PERF_RECORD_AUX;
	event.record.aux = &record.aux;

	return sb_pevent(p, &event, NULL);
}

static int pevent_itrace_start(struct parser *p, const char *pid,
			       const char *tid)
{
	union {
		struct pev_record_itrace_start itrace_start;
		uint8_t buffer[1024];
	} record;
	struct pev_event event;
	int errcode;

	if (bug_on(!p) || bug_on(!p->y))
		return -err_internal;

	memset(record.buffer, 0, sizeof(record.buffer));
	memset(&event, 0, sizeof(event));

	errcode = str_to_uint32(pid, &record.itrace_start.pid, 0);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-itrace-start - pid", errcode);
		return errcode;
	}

	errcode = str_to_uint32(tid, &record.itrace_start.tid, 0);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-itrace-start - tid", errcode);
		return errcode;
	}

	event.type = PERF_RECORD_ITRACE_START;
	event.record.itrace_start = &record.itrace_start;

	return sb_pevent(p, &event, NULL);
}

static int pevent_lost_samples(struct parser *p, const char *lost)
{
	union {
		struct pev_record_lost_samples lost_samples;
		uint8_t buffer[1024];
	} record;
	struct pev_event event;
	int errcode;

	if (bug_on(!p) || bug_on(!p->y))
		return -err_internal;

	memset(record.buffer, 0, sizeof(record.buffer));
	memset(&event, 0, sizeof(event));

	errcode = str_to_uint64(lost, &record.lost_samples.lost, 0);
	if (errcode < 0) {
		yasm_print_err(p->y, "pevent-lost-samples - lost", errcode);
		return errcode;
	}

	event.type = PERF_RECORD_LOST_SAMPLES;
	event.record.lost_samples = &record.lost_samples;

	return sb_pevent(p, &event, NULL);
}

static int pevent_switch(struct parser *p, uint16_t misc, char *payload)
{
	struct pev_event event;

	if (bug_on(!p) || bug_on(!p->y))
		return -err_internal;

	memset(&event, 0, sizeof(event));

	event.type = PERF_RECORD_SWITCH;
	event.misc = misc;

	return sb_pevent(p, &event, payload);
}

static int pevent_switch_cpu_wide(struct parser *p, const char *pid,
				  const char *tid, uint16_t misc)
{
	union {
		struct pev_record_switch_cpu_wide switch_cpu_wide;
		uint8_t buffer[1024];
	} record;
	struct pev_event event;
	int errcode;

	if (bug_on(!p) || bug_on(!p->y))
		return -err_internal;

	memset(record.buffer, 0, sizeof(record.buffer));
	memset(&event, 0, sizeof(event));

	event.misc = misc;

	errcode = str_to_uint32(pid, &record.switch_cpu_wide.next_prev_pid, 0);
	if (errcode < 0) {
		yasm_print_err(p->y,
			       "pevent-switch-cpu-wide - next_prev_pid",
			       errcode);
		return errcode;
	}

	errcode = str_to_uint32(tid, &record.switch_cpu_wide.next_prev_tid, 0);
	if (errcode < 0) {
		yasm_print_err(p->y,
			       "pevent-switch-cpu-wide - next_prev_tid",
			       errcode);
		return errcode;
	}

	event.type = PERF_RECORD_SWITCH_CPU_WIDE;
	event.record.switch_cpu_wide = &record.switch_cpu_wide;

	return sb_pevent(p, &event, NULL);
}
#endif /* defined(FEATURE_PEVENT) */

/* Process a @sb directive.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 * Returns -err_internal if @p is the NULL pointer.
 * Returns -err_parse if a general parsing error was encountered.
 * Returns -err_parse_unknown_directive if there was an unknown pt directive.
 */
static int p_process_sb(struct parser *p)
{
	struct pt_directive *pd;
	char *directive, *payload;

	if (bug_on(!p))
		return -err_internal;

	pd = p->pd;
	if (!pd)
		return -err_internal;

	directive = pd->name;
	payload = pd->payload;

	if (strcmp(directive, "primary") == 0) {
		char *fmt, *src;

		fmt = strtok(payload, " ,");
		if (!fmt) {
			yasm_print_err(p->y, "primary - format missing",
				       -err_parse_no_args);
			return -err_parse_no_args;
		}

		src = strtok(NULL, " ");

		return sb_open(p, fmt, src, "primary");
	} else if (strcmp(directive, "secondary") == 0) {
		char *fmt, *src;

		fmt = strtok(payload, " ,");
		if (!fmt) {
			yasm_print_err(p->y, "secondary - format missing",
				       -err_parse_no_args);
			return -err_parse_no_args;
		}

		src = strtok(NULL, " ");

		return sb_open(p, fmt, src, "secondary");
	} else if (strcmp(directive, "raw-8") == 0) {
		uint8_t value;
		int errcode;

		errcode = parse_uint8(&value, payload);
		if (errcode < 0) {
			yasm_print_err(p->y, payload, errcode);
			return errcode;
		}

		return sb_raw_8(p, value);
	} else if (strcmp(directive, "raw-16") == 0) {
		uint16_t value;
		int errcode;

		errcode = parse_uint16(&value, payload);
		if (errcode < 0) {
			yasm_print_err(p->y, payload, errcode);
			return errcode;
		}

		return sb_raw_16(p, value);
	} else if (strcmp(directive, "raw-32") == 0) {
		uint32_t value;
		int errcode;

		errcode = parse_uint32(&value, payload);
		if (errcode < 0) {
			yasm_print_err(p->y, payload, errcode);
			return errcode;
		}

		return sb_raw_32(p, value);
	} else if (strcmp(directive, "raw-64") == 0) {
		uint64_t value;
		int errcode;

		errcode = parse_uint64(&value, payload);
		if (errcode < 0) {
			yasm_print_err(p->y, payload, errcode);
			return errcode;
		}

		return sb_raw_64(p, value);
#if defined(FEATURE_PEVENT)
	} else if (strcmp(directive, "pevent-sample_type") == 0) {
		uint64_t sample_type;
		char *token;

		sample_type = 0ull;

		for (token = strtok(payload, " ,"); token;
		     token = strtok(NULL, " ,")) {

			if (strcmp(token, "tid") == 0)
				sample_type |= (uint64_t) PERF_SAMPLE_TID;
			else if (strcmp(token, "time") == 0)
				sample_type |= (uint64_t) PERF_SAMPLE_TIME;
			else if (strcmp(token, "id") == 0)
				sample_type |= (uint64_t) PERF_SAMPLE_ID;
			else if (strcmp(token, "stream") == 0)
				sample_type |= (uint64_t) PERF_SAMPLE_STREAM_ID;
			else if (strcmp(token, "cpu") == 0)
				sample_type |= (uint64_t) PERF_SAMPLE_CPU;
			else if (strcmp(token, "identifier") == 0)
				sample_type |=
					(uint64_t) PERF_SAMPLE_IDENTIFIER;
			else {
				uint64_t value;
				int errcode;

				errcode = parse_uint64(&value, payload);
				if (errcode < 0) {
					yasm_print_err(p->y, token, errcode);
					return errcode;
				}

				sample_type |= value;
			}
		}

		return pevent_sample_type(p, sample_type);

	} else if (strcmp(directive, "pevent-mmap-section") == 0) {
		char *section, *pid, *tid;

		section = strtok(payload, " ,");
		if (!section) {
			yasm_print_err(p->y, "section missing", -err_parse);
			return -err_parse;
		}

		pid = strtok(NULL, " ,");
		if (!pid) {
			yasm_print_err(p->y, "pid missing", -err_parse);
			return -err_parse;
		}

		tid = strtok(NULL, " ,");
		if (!tid) {
			yasm_print_err(p->y, "tid missing", -err_parse);
			return -err_parse;
		}

		return pevent_mmap_section(p, section, pid, tid);
	} else if (strcmp(directive, "pevent-mmap") == 0) {
		char *pid, *tid, *addr, *len, *pgoff, *filename;

		pid = strtok(payload, " ,");
		if (!pid) {
			yasm_print_err(p->y, "pid missing", -err_parse);
			return -err_parse;
		}

		tid = strtok(NULL, " ,");
		if (!tid) {
			yasm_print_err(p->y, "tid missing", -err_parse);
			return -err_parse;
		}

		addr = strtok(NULL, " ,");
		if (!addr) {
			yasm_print_err(p->y, "addr missing", -err_parse);
			return -err_parse;
		}

		len = strtok(NULL, " ,");
		if (!len) {
			yasm_print_err(p->y, "len missing", -err_parse);
			return -err_parse;
		}

		pgoff = strtok(NULL, " ,");
		if (!pgoff) {
			yasm_print_err(p->y, "pgoff missing", -err_parse);
			return -err_parse;
		}

		filename = strtok(NULL, " ,");
		if (!filename) {
			yasm_print_err(p->y, "filename missing", -err_parse);
			return -err_parse;
		}

		return pevent_mmap(p, pid, tid, addr, len, pgoff, filename);
	} else if (strcmp(directive, "pevent-lost") == 0) {
		char *id, *lost;

		id = strtok(payload, " ,");
		if (!id) {
			yasm_print_err(p->y, "id missing", -err_parse);
			return -err_parse;
		}

		lost = strtok(NULL, " ,");
		if (!lost) {
			yasm_print_err(p->y, "lost missing", -err_parse);
			return -err_parse;
		}

		return pevent_lost(p, id, lost);
	} else if (strcmp(directive, "pevent-comm") == 0) {
		char *pid, *tid, *comm;

		pid = strtok(payload, " ,");
		if (!pid) {
			yasm_print_err(p->y, "pid missing", -err_parse);
			return -err_parse;
		}

		tid = strtok(NULL, " ,");
		if (!tid) {
			yasm_print_err(p->y, "tid missing", -err_parse);
			return -err_parse;
		}

		comm = strtok(NULL, " ,");
		if (!comm) {
			yasm_print_err(p->y, "comm missing", -err_parse);
			return -err_parse;
		}

		return pevent_comm(p, pid, tid, comm, 0);
	} else if (strcmp(directive, "pevent-comm.exec") == 0) {
		char *pid, *tid, *comm;

		pid = strtok(payload, " ,");
		if (!pid) {
			yasm_print_err(p->y, "pid missing", -err_parse);
			return -err_parse;
		}

		tid = strtok(NULL, " ,");
		if (!tid) {
			yasm_print_err(p->y, "tid missing", -err_parse);
			return -err_parse;
		}

		comm = strtok(NULL, " ,");
		if (!comm) {
			yasm_print_err(p->y, "comm missing", -err_parse);
			return -err_parse;
		}

		return pevent_comm(p, pid, tid, comm,
				   PERF_RECORD_MISC_COMM_EXEC);
	} else if (strcmp(directive, "pevent-exit") == 0) {
		char *pid, *ppid, *tid, *ptid, *time;

		pid = strtok(payload, " ,");
		if (!pid) {
			yasm_print_err(p->y, "pid missing", -err_parse);
			return -err_parse;
		}

		ppid = strtok(NULL, " ,");
		if (!ppid) {
			yasm_print_err(p->y, "ppid missing", -err_parse);
			return -err_parse;
		}

		tid = strtok(NULL, " ,");
		if (!tid) {
			yasm_print_err(p->y, "tid missing", -err_parse);
			return -err_parse;
		}

		ptid = strtok(NULL, " ,");
		if (!ptid) {
			yasm_print_err(p->y, "ptid missing", -err_parse);
			return -err_parse;
		}

		time = strtok(NULL, " ,");
		if (!time) {
			yasm_print_err(p->y, "time missing", -err_parse);
			return -err_parse;
		}

		return pevent_exit(p, pid, ppid, tid, ptid, time);
	} else if (strcmp(directive, "pevent-fork") == 0) {
		char *pid, *ppid, *tid, *ptid, *time;

		pid = strtok(payload, " ,");
		if (!pid) {
			yasm_print_err(p->y, "pid missing", -err_parse);
			return -err_parse;
		}

		ppid = strtok(NULL, " ,");
		if (!ppid) {
			yasm_print_err(p->y, "ppid missing", -err_parse);
			return -err_parse;
		}

		tid = strtok(NULL, " ,");
		if (!tid) {
			yasm_print_err(p->y, "tid missing", -err_parse);
			return -err_parse;
		}

		ptid = strtok(NULL, " ,");
		if (!ptid) {
			yasm_print_err(p->y, "ptid missing", -err_parse);
			return -err_parse;
		}

		time = strtok(NULL, " ,");
		if (!time) {
			yasm_print_err(p->y, "time missing", -err_parse);
			return -err_parse;
		}

		return pevent_fork(p, pid, ppid, tid, ptid, time);
	} else if (strcmp(directive, "pevent-aux") == 0) {
		char *offset, *size, *flags;

		offset = strtok(payload, " ,");
		if (!offset) {
			yasm_print_err(p->y, "offset missing", -err_parse);
			return -err_parse;
		}

		size = strtok(NULL, " ,");
		if (!size) {
			yasm_print_err(p->y, "size missing", -err_parse);
			return -err_parse;
		}

		flags = strtok(NULL, " ,");
		if (!flags) {
			yasm_print_err(p->y, "flags missing", -err_parse);
			return -err_parse;
		}

		return pevent_aux(p, offset, size, flags);
	} else if (strcmp(directive, "pevent-itrace-start") == 0) {
		char *pid, *tid;

		pid = strtok(payload, " ,");
		if (!pid) {
			yasm_print_err(p->y, "pid missing", -err_parse);
			return -err_parse;
		}

		tid = strtok(NULL, " ,");
		if (!tid) {
			yasm_print_err(p->y, "tid missing", -err_parse);
			return -err_parse;
		}

		return pevent_itrace_start(p, pid, tid);
	} else if (strcmp(directive, "pevent-lost-samples") == 0) {
		char *lost;

		lost = strtok(payload, " ,");
		if (!lost) {
			yasm_print_err(p->y, "lost missing", -err_parse);
			return -err_parse;
		}

		return pevent_lost_samples(p, lost);
	} else if (strcmp(directive, "pevent-switch.in") == 0)
		return pevent_switch(p, 0u, payload);
	else if (strcmp(directive, "pevent-switch.out") == 0)
		return pevent_switch(p, PERF_RECORD_MISC_SWITCH_OUT, payload);
	else if (strcmp(directive, "pevent-switch-cpu-wide.in") == 0) {
		char *pid, *tid;

		pid = strtok(payload, " ,");
		if (!pid) {
			yasm_print_err(p->y, "pid missing", -err_parse);
			return -err_parse;
		}

		tid = strtok(NULL, " ,");
		if (!tid) {
			yasm_print_err(p->y, "tid missing", -err_parse);
			return -err_parse;
		}

		return pevent_switch_cpu_wide(p, pid, tid, 0u);
	} else if (strcmp(directive, "pevent-switch-cpu-wide.out") == 0) {
		char *pid, *tid;

		pid = strtok(payload, " ,");
		if (!pid) {
			yasm_print_err(p->y, "pid missing", -err_parse);
			return -err_parse;
		}

		tid = strtok(NULL, " ,");
		if (!tid) {
			yasm_print_err(p->y, "tid missing", -err_parse);
			return -err_parse;
		}

		return pevent_switch_cpu_wide(p, pid, tid,
					      PERF_RECORD_MISC_SWITCH_OUT);
#endif /* defined(FEATURE_PEVENT) */
	} else {
		yasm_print_err(p->y, "syntax error",
			       -err_parse_unknown_directive);
		return -err_parse_unknown_directive;
	}
}

#endif /* defined(FEATURE_SIDEBAND) */

/* Processes the current directive.
 * If the encoder returns an error, a message including current file and
 * line number together with the pt error string is printed on stderr.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 * Returns -err_internal if @p or @e is the NULL pointer.
 * Returns -err_parse_missing_directive if there was a pt directive marker,
 * but no directive.
 * Returns -stop_process if the .exp directive was encountered.
 * Returns -err_pt_lib if the pt encoder returned an error.
 * Returns -err_parse if a general parsing error was encountered.
 * Returns -err_parse_unknown_directive if there was an unknown pt directive.
 */
static int p_process(struct parser *p, struct pt_encoder *e)
{
	char *directive, *tmp;
	struct pt_directive *pd;

	if (bug_on(!p))
		return -err_internal;

	pd = p->pd;
	if (!pd)
		return -err_internal;

	directive = pd->name;

	/* We must have a directive. */
	if (!directive || (strcmp(directive, "") == 0))
		return yasm_print_err(p->y, "invalid syntax",
				      -err_parse_missing_directive);

	/* Check for special directives - they won't contain labels. */
	if (strcmp(directive, ".exp") == 0) {
		int errcode;

		/* this is the end of processing pt directives, so we
		 * add a p_last label to the pt directive labels.
		 */
		errcode = l_append(p->pt_labels, "eos", p->pt_bytes_written);
		if (errcode < 0)
			return yasm_print_err(p->y, "append label", errcode);

		return -stop_process;
	}

	/* find a label name.  */
	tmp = strchr(directive, ':');
	if (tmp) {
		char *pt_label_name;
		uint64_t x;
		int errcode, bytes_written;
		size_t len;

		pt_label_name = directive;
		directive = tmp+1;
		*tmp = '\0';

		/* ignore whitespace between label and directive. */
		while (isspace(*directive))
			directive += 1;

		/* we must have a directive, not just a label. */
		if (strcmp(directive, "") == 0)
			return yasm_print_err(p->y, "invalid syntax",
					      -err_parse_missing_directive);

		/* if we can lookup a yasm label with the same name, the
		 * current pt directive label is invalid.  */
		errcode = yasm_lookup_label(p->y, &x, pt_label_name);
		if (errcode == 0)
			errcode = -err_label_not_unique;

		if (errcode != -err_no_label)
			return yasm_print_err(p->y, "label lookup",
					      errcode);

		/* if we can lookup a pt directive label with the same
		 * name, the current pt directive label is invalid.  */
		errcode = l_lookup(p->pt_labels, &x, pt_label_name);
		if (errcode == 0)
			errcode = -err_label_not_unique;

		if (errcode != -err_no_label)
			return yasm_print_err(p->y, "label lookup",
					      -err_label_not_unique);

		bytes_written = -pte_internal;
		switch (pd->kind) {
		case pdk_pt:
			bytes_written = p->pt_bytes_written;
			break;

#if defined(FEATURE_SIDEBAND)
		case pdk_sb: {
			struct sb_file *sb;

			sb = p_get_current_sbfile(p);
			if (!sb)
				return yasm_print_err(p->y, "sideband label",
						      -err_sb_missing);

			bytes_written = sb->bytes_written;
		}
			break;
#endif /* defined(FEATURE_SIDEBAND) */
		}

		if (bytes_written < 0)
			return bytes_written;

		errcode = l_append(p->pt_labels, pt_label_name, bytes_written);
		if (errcode < 0)
			return errcode;

		/* Update the directive name in the parser. */
		len = strlen(directive) + 1;
		memmove(pd->name, directive, len);
	}

	switch (pd->kind) {
	case pdk_pt:
		return p_process_pt(p, e);

#if defined(FEATURE_SIDEBAND)
	case pdk_sb:
		return p_process_sb(p);
#endif
	}

	return -err_internal;
}

/* Starts the parsing process.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 * Returns -err_pt_lib if the pt encoder could not be initialized.
 * Returns -err_file_write if the .pt or .exp file could not be fully
 * written.
 */
static int p_start(struct parser *p)
{
	int errcode;

	if (bug_on(!p))
		return -err_internal;

	errcode = yasm_parse(p->y);
	if (errcode < 0)
		return errcode;

	for (;;) {
		int bytes_written;
		struct pt_encoder *e;

		errcode = yasm_next_pt_directive(p->y, p->pd);
		if (errcode < 0)
			break;

		e = pt_alloc_encoder(p->conf);
		if (!e) {
			fprintf(stderr, "pt_alloc_encoder failed\n");
			errcode = -err_pt_lib;
			break;
		}

		bytes_written = p_process(p, e);

		pt_free_encoder(e);

		if (bytes_written == -stop_process) {
			errcode = p_gen_expfile(p);
			break;
		}
		if (bytes_written < 0) {
			errcode = bytes_written;
			break;
		}
		if (fwrite(p->conf->begin, 1, bytes_written, p->ptfile)
		    != (size_t)bytes_written) {
			fprintf(stderr, "write %s failed", p->ptfilename);
			errcode = -err_file_write;
			break;
		}
	}

	/* If there is no directive left, there's nothing more to do.  */
	if (errcode == -err_no_directive)
		return 0;

	return errcode;
}

int parse(const char *pttfile, const struct pt_config *conf)
{
	int errcode;
	struct parser *p;

	p = p_alloc(pttfile, conf);
	if (!p)
		return -err_no_mem;

	errcode = p_open_files(p);
	if (errcode < 0)
		goto error;

	errcode = p_start(p);
	p_close_files(p);

error:
	p_free(p);
	return errcode;
}

int parse_empty(char *payload)
{
	if (!payload)
		return 0;

	strtok(payload, " ");
	if (!payload || *payload == '\0')
		return 0;

	return -err_parse_trailing_tokens;
}

int parse_tnt(uint64_t *tnt, uint8_t *size, char *payload)
{
	char c;

	if (bug_on(!size))
		return -err_internal;

	if (bug_on(!tnt))
		return -err_internal;

	*size = 0;
	*tnt = 0ull;

	if (!payload)
		return 0;

	while (*payload != '\0') {
		c = *payload;
		payload++;
		if (isspace(c) || c == '.')
			continue;
		*size += 1;
		*tnt <<= 1;
		switch (c) {
		case 'n':
			break;
		case 't':
			*tnt |= 1;
			break;
		default:
			return -err_parse_unknown_char;
		}
	}

	return 0;
}

static int ipc_from_uint32(enum pt_ip_compression *ipc, uint32_t val)
{
	switch (val) {
	case pt_ipc_suppressed:
	case pt_ipc_update_16:
	case pt_ipc_update_32:
	case pt_ipc_update_48:
	case pt_ipc_sext_48:
	case pt_ipc_full:
		*ipc = (enum pt_ip_compression) val;
		return 0;
	}
	return -err_parse_ipc;
}

int parse_ip(struct parser *p, uint64_t *ip, enum pt_ip_compression *ipc,
	     char *payload)
{
	uint32_t ipcval;
	int errcode;

	if (bug_on(!ip))
		return -err_internal;

	if (bug_on(!ipc))
		return -err_internal;

	*ipc = pt_ipc_suppressed;
	*ip = 0;

	payload = strtok(payload, " :");
	if (!payload || *payload == '\0')
		return -err_parse_no_args;

	errcode = str_to_uint32(payload, &ipcval, 0);
	if (errcode < 0)
		return errcode;

	errcode = ipc_from_uint32(ipc, ipcval);
	if (errcode < 0)
		return errcode;

	payload = strtok(NULL, " :");
	if (!payload)
		return -err_parse_ip_missing;

	/* can be resolved to a label?  */
	if (*payload == '%') {
		if (!p)
			return -err_internal;

		errcode = yasm_lookup_label(p->y, ip, payload + 1);
		if (errcode < 0)
			return errcode;
	} else {
		/* can be parsed as address?  */
		errcode = str_to_uint64(payload, ip, 0);
		if (errcode < 0)
			return errcode;
	}

	/* no more tokens left.  */
	payload = strtok(NULL, " ");
	if (payload)
		return -err_parse_trailing_tokens;

	return 0;
}

int parse_uint64(uint64_t *x, char *payload)
{
	int errcode;

	if (bug_on(!x))
		return -err_internal;

	payload = strtok(payload, " ,");
	if (!payload)
		return -err_parse_no_args;

	errcode = str_to_uint64(payload, x, 0);
	if (errcode < 0)
		return errcode;

	return 0;
}

int parse_uint32(uint32_t *x, char *payload)
{
	int errcode;

	if (bug_on(!x))
		return -err_internal;

	payload = strtok(payload, " ,");
	if (!payload)
		return -err_parse_no_args;

	errcode = str_to_uint32(payload, x, 0);
	if (errcode < 0)
		return errcode;

	return 0;
}

int parse_uint16(uint16_t *x, char *payload)
{
	int errcode;

	if (bug_on(!x))
		return -err_internal;

	payload = strtok(payload, " ,");
	if (!payload)
		return -err_parse_no_args;

	errcode = str_to_uint16(payload, x, 0);
	if (errcode < 0)
		return errcode;

	return 0;
}

int parse_uint8(uint8_t *x, char *payload)
{
	int errcode;

	if (bug_on(!x))
		return -err_internal;

	payload = strtok(payload, " ,");
	if (!payload)
		return -err_parse_no_args;

	errcode = str_to_uint8(payload, x, 0);
	if (errcode < 0)
		return errcode;

	return 0;
}

int parse_tma(uint16_t *ctc, uint16_t *fc, char *payload)
{
	char *endptr;
	long int i;

	if (bug_on(!ctc || !fc))
		return -err_internal;

	payload = strtok(payload, ",");
	if (!payload || *payload == '\0')
		return -err_parse_no_args;

	i = strtol(payload, &endptr, 0);
	if (payload == endptr || *endptr != '\0')
		return -err_parse_int;

	if (i > 0xffffl)
		return -err_parse_int_too_big;

	*ctc = (uint16_t)i;

	payload = strtok(NULL, " ,");
	if (!payload)
		return -err_parse_no_args;

	i = strtol(payload, &endptr, 0);
	if (payload == endptr || *endptr != '\0')
		return -err_parse_int;

	if (i > 0xffffl)
		return -err_parse_int_too_big;

	*fc = (uint16_t)i;

	/* no more tokens left.  */
	payload = strtok(NULL, " ");
	if (payload)
		return -err_parse_trailing_tokens;

	return 0;
}
