/*
 * Aic7xxx SCSI host adapter firmware asssembler
 *
 * Copyright (c) 1997 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      $Id$
 */
#include <sys/types.h>
#include <sys/mman.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "aic7xxx_asm.h"
#include "symbol.h"
#include "sequencer.h"

static void usage __P((void));
static void back_patch __P((void));
static void output_code __P((FILE *ofile));
static void output_listing __P((FILE *listfile, char *ifilename,
				char *options));
static struct patch *next_patch __P((struct patch *cur_patch, int options,
				     int instrptr));

struct path_list search_path;
int includes_search_curdir;
char *appname;
FILE *ofile;
char *ofilename;

static STAILQ_HEAD(,instruction) seq_program;
static STAILQ_HEAD(, patch) patch_list;
symlist_t patch_options;

#if DEBUG
extern int yy_flex_debug;
extern int yydebug;
#endif
extern FILE *yyin;
extern int yyparse __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	int ch;
	int  retval;
	char *inputfilename;
	char *regfilename;
	FILE *regfile;
	char *listfilename;
	FILE *listfile;
	char *options;

	SLIST_INIT(&search_path);
	STAILQ_INIT(&seq_program);
	STAILQ_INIT(&patch_list);
	SLIST_INIT(&patch_options);
	includes_search_curdir = 1;
	appname = *argv;
	regfile = NULL;
	listfile = NULL;
	options = NULL;
#if DEBUG
	yy_flex_debug = 0;
#endif
	while ((ch = getopt(argc, argv, "d:l:n:o:r:I:O:")) != EOF) {
		switch(ch) {
		case 'd':
#if DEBUG
			if (strcmp(optarg, "s") == 0)
				yy_flex_debug = 1;
			else if (strcmp(optarg, "p") == 0)
				yydebug = 1;
#else
			stop("-d: Assembler not built with debugging "
			     "information", EX_SOFTWARE);
#endif
			break;
		case 'l':
			/* Create a program listing */
			if ((listfile = fopen(optarg, "w")) == NULL) {
				perror(optarg);
				stop(NULL, EX_CANTCREAT);
			}
			listfilename = optarg;
			break;
		case 'n':
			/* Don't complain about the -nostdinc directrive */
			if (strcmp(optarg, "ostdinc")) {
				fprintf(stderr, "%s: Unknown option -%c%s\n",
					appname, ch, optarg);
				usage();
				/* NOTREACHED */
			}
			break;
		case 'o':
			if ((ofile = fopen(optarg, "w")) == NULL) {
				perror(optarg);
				stop(NULL, EX_CANTCREAT);
			}
			ofilename = optarg;
			break;
		case 'O':
			/* Patches to include in the listing */
			options = optarg;
			break;
		case 'r':
			if ((regfile = fopen(optarg, "w")) == NULL) {
				perror(optarg);
				stop(NULL, EX_CANTCREAT);
			}
			regfilename = optarg;
			break;
		case 'I':
		{
			path_entry_t include_dir;

			if (strcmp(optarg, "-") == 0) {
				if (includes_search_curdir == 0) {
					fprintf(stderr, "%s: Warning - '-I-' "
							"specified multiple "
							"times\n", appname);
				}
				includes_search_curdir = 0;
				for (include_dir = search_path.slh_first;
				     include_dir != NULL;
				     include_dir = include_dir->links.sle_next)
					/*
					 * All entries before a '-I-' only
					 * apply to includes specified with
					 * quotes instead of "<>".
					 */
					include_dir->quoted_includes_only = 1;
			} else {
				include_dir =
				    (path_entry_t)malloc(sizeof(*include_dir));
				if (include_dir == NULL) {
					perror(optarg);
					stop(NULL, EX_OSERR);
				}
				include_dir->directory = strdup(optarg);
				if (include_dir->directory == NULL) {
					perror(optarg);
					stop(NULL, EX_OSERR);
				}
				include_dir->quoted_includes_only = 0;
				SLIST_INSERT_HEAD(&search_path, include_dir,
						  links);
			}
			break;
		}
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1) {
		fprintf(stderr, "%s: No input file specifiled\n", appname);
		usage();
		/* NOTREACHED */
	}

	symtable_open();
	inputfilename = *argv;
	include_file(*argv, SOURCE_FILE);
	retval = yyparse();
	if (retval == 0) {
		back_patch();
		if (ofile != NULL)
			output_code(ofile);
		if (regfile != NULL)
			symtable_dump(regfile);
		if (listfile != NULL)
			output_listing(listfile, inputfilename, options);
	}

	stop(NULL, 0);
	/* NOTREACHED */
	return (0);
}

static void
usage()
{

	(void)fprintf(stderr,
"usage: %-16s [-nostdinc] [-I-] [-I directory] [-o output_file]
			[-r register_output_file] [-l program_list_file]
			[-O option_name[|options_name2]] input_file\n",
			appname);
	exit(EX_USAGE);
}

static void
back_patch()
{
	struct instruction *cur_instr;

	for(cur_instr = seq_program.stqh_first;
	    cur_instr != NULL;
	    cur_instr = cur_instr->links.stqe_next) {
		if (cur_instr->patch_label != NULL) {
			struct ins_format3 *f3_instr;
			u_int address;

			if (cur_instr->patch_label->type != LABEL) {
				char buf[255];

				snprintf(buf, sizeof(buf),
					 "Undefined label %s",
					 cur_instr->patch_label->name);
				stop(buf, EX_DATAERR);
				/* NOTREACHED */
			}
			f3_instr = &cur_instr->format.format3;
			address = ((f3_instr->opcode_addr & ADDR_HIGH_BIT) << 8)
				| f3_instr->address;
			address += cur_instr->patch_label->info.linfo->address;
			f3_instr->opcode_addr &= ~ADDR_HIGH_BIT;
			f3_instr->opcode_addr |= (address >> 8) & ADDR_HIGH_BIT;
			f3_instr->address = address & 0xFF;
		}
	}
}

static void
output_code(ofile)
	FILE *ofile;
{
	struct instruction *cur_instr;
	patch_t *cur_patch;
	symbol_node_t *cur_node;
	int instrcount;

	instrcount = 0;
	fprintf(ofile,
"/*
  * DO NOT EDIT - This file is automatically generated.
  */\n");

	fprintf(ofile, "static u_int8_t seqprog[] = {\n");
	for(cur_instr = seq_program.stqh_first;
	    cur_instr != NULL;
	    cur_instr = cur_instr->links.stqe_next) {
		fprintf(ofile, "\t0x%02x, 0x%02x, 0x%02x, 0x%02x,\n",
			cur_instr->format.bytes[0],
			cur_instr->format.bytes[1],
			cur_instr->format.bytes[2],
			cur_instr->format.bytes[3]);
		instrcount++;
	}
	fprintf(ofile, "};\n");

	/*
	 *  Output the patch list, option definitions first.
	 */
	for(cur_node = patch_options.slh_first;
	    cur_node != NULL;
	    cur_node = cur_node->links.sle_next) {
		fprintf(ofile, "#define\t%-16s\t0x%x\n", cur_node->symbol->name,
			cur_node->symbol->info.condinfo->value);
	}

	fprintf(ofile,
"struct patch {
	int	options;
	int	negative;
	int	begin;
	int	end;
} patches[] = {\n");

	for(cur_patch = patch_list.stqh_first;
	    cur_patch != NULL;
	    cur_patch = cur_patch->links.stqe_next)

		fprintf(ofile, "\t{ 0x%08x, %d, 0x%03x, 0x%03x },\n",
			cur_patch->options, cur_patch->negative, cur_patch->begin,
			cur_patch->end);

	fprintf(ofile, "\t{ 0x%08x, %d, 0x%03x, 0x%03x }\n};\n",
		0, 0, 0, 0);
	
	fprintf(stderr, "%s: %d instructions used\n", appname, instrcount);
}

void
output_listing(listfile, ifilename, patches)
	FILE *listfile;
	char *ifilename;
	char *patches;
{
	FILE *ifile;
	int line;
	struct instruction *cur_instr;
	int instrcount;
	int instrptr;
	char buf[1024];
	patch_t *cur_patch;
	char *option_spec;
	int options;

	instrcount = 0;
	instrptr = 0;
	line = 1;
	options = 1; /* All code outside of patch blocks */
	if ((ifile = fopen(ifilename, "r")) == NULL) {
		perror(ifilename);
		stop(NULL, EX_DATAERR);
	}

	/*
	 * Determine which options to apply to this listing.
	 */
	while ((option_spec = strsep(&patches, "|")) != NULL) {
		symbol_t *symbol;

		symbol = symtable_get(option_spec);
		if (symbol->type != CONDITIONAL) {
			stop("Invalid option specified in patch list for "
			     "program listing", EX_USAGE);
			/* NOTREACHED */
		}
		options |= symbol->info.condinfo->value;
	}

	cur_patch = patch_list.stqh_first;
	for(cur_instr = seq_program.stqh_first;
	    cur_instr != NULL;
	    cur_instr = cur_instr->links.stqe_next,instrcount++) {

		cur_patch = next_patch(cur_patch, options, instrcount);
		if (cur_patch
		 && cur_patch->begin <= instrcount
		 && cur_patch->end > instrcount)
			/* Don't count this instruction as it is in a patch
			 * that was removed.
			 */
                        continue;

		while (line < cur_instr->srcline) {
			fgets(buf, sizeof(buf), ifile);
				fprintf(listfile, "\t\t%s", buf);
				line++;
		}
		fprintf(listfile, "%03x %02x%02x%02x%02x", instrptr,
			cur_instr->format.bytes[0],
			cur_instr->format.bytes[1],
			cur_instr->format.bytes[2],
			cur_instr->format.bytes[3]);
		fgets(buf, sizeof(buf), ifile);
		fprintf(listfile, "\t%s", buf);
		line++;
		instrptr++;
	}
	/* Dump the remainder of the file */
	while(fgets(buf, sizeof(buf), ifile) != NULL)
		fprintf(listfile, "\t\t%s", buf);

	fclose(ifile);
}

static struct patch *
next_patch(cur_patch, options, instrptr)
	struct patch *cur_patch;
	int	options;
	int	instrptr;
{
	while(cur_patch != NULL) {
		if (((cur_patch->options & options) != 0
		   && cur_patch->negative == FALSE)
		 || ((cur_patch->options & options) == 0
		   && cur_patch->negative == TRUE)
		 || (instrptr >= cur_patch->end)) {
			/*
			 * Either we want to keep this section of code,
			 * or we have consumed this patch. Skip to the
			 * next patch.
			 */
			cur_patch = cur_patch->links.stqe_next;
		} else
			/* Found an okay patch */
			break;
	}
	return (cur_patch);
}

/*
 * Print out error information if appropriate, and clean up before
 * terminating the program.
 */
void
stop(string, err_code)
	const char *string;
	int  err_code;
{
	if (string != NULL) {
		fprintf(stderr, "%s: ", appname);
		if (yyfilename != NULL) {
			fprintf(stderr, "Stopped at file %s, line %d - ",
				yyfilename, yylineno);
		}
		fprintf(stderr, "%s\n", string);
	}

	if (ofile != NULL) {
		fclose(ofile);
		if (err_code != 0) {
			fprintf(stderr, "%s: Removing %s due to error\n",
				appname, ofilename);
			unlink(ofilename);
		}
	}

	symlist_free(&patch_options);
	symtable_close();

	exit(err_code);
}

struct instruction *
seq_alloc()
{
	struct instruction *new_instr;

	new_instr = (struct instruction *)malloc(sizeof(struct instruction));
	if (new_instr == NULL)
		stop("Unable to malloc instruction object", EX_SOFTWARE);
	memset(new_instr, 0, sizeof(*new_instr));
	STAILQ_INSERT_TAIL(&seq_program, new_instr, links);
	new_instr->srcline = yylineno;
	return new_instr;
}

patch_t *
patch_alloc()
{
	patch_t *new_patch;

	new_patch = (patch_t *)malloc(sizeof(patch_t));
	if (new_patch == NULL)
		stop("Unable to malloc patch object", EX_SOFTWARE);
	memset(new_patch, 0, sizeof(*new_patch));
	STAILQ_INSERT_TAIL(&patch_list, new_patch, links);
	return new_patch;
}
