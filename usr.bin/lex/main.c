/* flex - tool to generate fast lexical analyzers */

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Vern Paxson.
 *
 * The United States Government has rights in this work pursuant
 * to contract no. DE-AC03-76SF00098 between the United States
 * Department of Energy and the University of California.
 *
 * Redistribution and use in source and binary forms are permitted provided
 * that: (1) source distributions retain this entire copyright notice and
 * comment, and (2) distributions including binaries display the following
 * acknowledgement:  ``This product includes software developed by the
 * University of California, Berkeley and its contributors'' in the
 * documentation or other materials provided with the distribution and in
 * all advertising materials mentioning features or use of this software.
 * Neither the name of the University nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1990 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

/* $Header: /pub/FreeBSD/FreeBSD-CVS/src/usr.bin/lex/main.c,v 1.2 1995/05/30 06:31:15 rgrimes Exp $ */


#include "flexdef.h"
#include "version.h"

static char flex_version[] = FLEX_VERSION;


/* declare functions that have forward references */

void flexinit PROTO((int, char**));
void readin PROTO((void));
void set_up_initial_allocations PROTO((void));


/* these globals are all defined and commented in flexdef.h */
int printstats, syntaxerror, eofseen, ddebug, trace, nowarn, spprdflt;
int interactive, caseins, lex_compat, useecs, fulltbl, usemecs;
int fullspd, gen_line_dirs, performance_report, backing_up_report;
int C_plus_plus, long_align, use_read, yytext_is_array, csize;
int yymore_used, reject, real_reject, continued_action;
int yymore_really_used, reject_really_used;
int datapos, dataline, linenum;
FILE *skelfile = NULL;
int skel_ind = 0;
char *action_array;
int action_size, defs1_offset, prolog_offset, action_offset, action_index;
char *infilename = NULL;
int onestate[ONE_STACK_SIZE], onesym[ONE_STACK_SIZE];
int onenext[ONE_STACK_SIZE], onedef[ONE_STACK_SIZE], onesp;
int current_mns, num_rules, num_eof_rules, default_rule;
int current_max_rules, lastnfa;
int *firstst, *lastst, *finalst, *transchar, *trans1, *trans2;
int *accptnum, *assoc_rule, *state_type;
int *rule_type, *rule_linenum, *rule_useful;
int current_state_type;
int variable_trailing_context_rules;
int numtemps, numprots, protprev[MSP], protnext[MSP], prottbl[MSP];
int protcomst[MSP], firstprot, lastprot, protsave[PROT_SAVE_SIZE];
int numecs, nextecm[CSIZE + 1], ecgroup[CSIZE + 1], nummecs, tecfwd[CSIZE + 1];
int tecbck[CSIZE + 1];
int lastsc, current_max_scs, *scset, *scbol, *scxclu, *sceof, *actvsc;
char **scname;
int current_max_dfa_size, current_max_xpairs;
int current_max_template_xpairs, current_max_dfas;
int lastdfa, *nxt, *chk, *tnxt;
int *base, *def, *nultrans, NUL_ec, tblend, firstfree, **dss, *dfasiz;
union dfaacc_union *dfaacc;
int *accsiz, *dhash, numas;
int numsnpairs, jambase, jamstate;
int lastccl, current_maxccls, *cclmap, *ccllen, *cclng, cclreuse;
int current_max_ccl_tbl_size;
Char *ccltbl;
char nmstr[MAXLINE];
int sectnum, nummt, hshcol, dfaeql, numeps, eps2, num_reallocs;
int tmpuses, totnst, peakpairs, numuniq, numdup, hshsave;
int num_backing_up, bol_needed;
FILE *backing_up_file;
int end_of_buffer_state;
char **input_files;
int num_input_files;
char *program_name;

#ifndef SHORT_FILE_NAMES
static char *outfile_template = "lex.%s.%s";
#else
static char *outfile_template = "lex%s.%s";
#endif
static char outfile_path[64];

static int outfile_created = 0;
static int use_stdout;
static char *skelname = NULL;
static char *prefix = "yy";


int main( argc, argv )
int argc;
char **argv;
	{
	int i;

	flexinit( argc, argv );

	readin();

	ntod();

	for ( i = 1; i <= num_rules; ++i )
		if ( ! rule_useful[i] && i != default_rule )
			line_warning( "rule cannot be matched",
					rule_linenum[i] );

	if ( spprdflt && ! reject && rule_useful[default_rule] )
		line_warning( "-s option given but default rule can be matched",
			rule_linenum[default_rule] );

	/* Generate the C state transition tables from the DFA. */
	make_tables();

	/* Note, flexend does not return.  It exits with its argument
	 * as status.
	 */
	flexend( 0 );

	return 0;	/* keep compilers/lint happy */
	}


/* flexend - terminate flex
 *
 * note
 *    This routine does not return.
 */

void flexend( exit_status )
int exit_status;

	{
	int tblsiz;
	int unlink();

	if ( skelfile != NULL )
		{
		if ( ferror( skelfile ) )
			flexfatal(
				"error occurred when reading skeleton file" );

		else if ( fclose( skelfile ) )
			flexfatal(
				"error occurred when closing skeleton file" );
		}

	if ( exit_status != 0 && outfile_created )
		{
		if ( ferror( stdout ) )
			flexfatal( "error occurred when writing output file" );

		else if ( fclose( stdout ) )
			flexfatal( "error occurred when closing output file" );

		else if ( unlink( outfile_path ) )
			flexfatal( "error occurred when deleting output file" );
		}

	if ( backing_up_report && backing_up_file )
		{
		if ( num_backing_up == 0 )
			fprintf( backing_up_file, "No backing up.\n" );
		else if ( fullspd || fulltbl )
			fprintf( backing_up_file,
				"%d backing up (non-accepting) states.\n",
				num_backing_up );
		else
			fprintf( backing_up_file,
				"Compressed tables always back up.\n" );

		if ( ferror( backing_up_file ) )
			flexfatal( "error occurred when writing backup file" );

		else if ( fclose( backing_up_file ) )
			flexfatal( "error occurred when closing backup file" );
		}

	if ( printstats )
		{
		fprintf( stderr, "%s version %s usage statistics:\n",
			program_name, flex_version );

		fprintf( stderr, "  scanner options: -" );

		if ( C_plus_plus )
			putc( '+', stderr );
		if ( backing_up_report )
			putc( 'b', stderr );
		if ( ddebug )
			putc( 'd', stderr );
		if ( caseins )
			putc( 'i', stderr );
		if ( lex_compat )
			putc( 'l', stderr );
		if ( performance_report > 0 )
			putc( 'p', stderr );
		if ( performance_report > 1 )
			putc( 'p', stderr );
		if ( spprdflt )
			putc( 's', stderr );
		if ( use_stdout )
			putc( 't', stderr );
		if ( printstats )
			putc( 'v', stderr );	/* always true! */
		if ( nowarn )
			putc( 'w', stderr );
		if ( ! interactive )
			putc( 'B', stderr );
		if ( interactive )
			putc( 'I', stderr );
		if ( ! gen_line_dirs )
			putc( 'L', stderr );
		if ( trace )
			putc( 'T', stderr );
		if ( csize == 128 )
			putc( '7', stderr );
		else
			putc( '8', stderr );

		fprintf( stderr, " -C" );

		if ( long_align )
			putc( 'a', stderr );
		if ( fulltbl )
			putc( 'f', stderr );
		if ( fullspd )
			putc( 'F', stderr );
		if ( useecs )
			putc( 'e', stderr );
		if ( usemecs )
			putc( 'm', stderr );
		if ( use_read )
			putc( 'r', stderr );

		if ( skelname )
			fprintf( stderr, " -S%s", skelname );

		if ( strcmp( prefix, "yy" ) )
			fprintf( stderr, " -P%s", prefix );

		putc( '\n', stderr );

		fprintf( stderr, "  %d/%d NFA states\n", lastnfa, current_mns );
		fprintf( stderr, "  %d/%d DFA states (%d words)\n", lastdfa,
			current_max_dfas, totnst );
		fprintf( stderr, "  %d rules\n",
		num_rules + num_eof_rules - 1 /* - 1 for def. rule */ );

		if ( num_backing_up == 0 )
			fprintf( stderr, "  No backing up\n" );
		else if ( fullspd || fulltbl )
			fprintf( stderr,
				"  %d backing-up (non-accepting) states\n",
				num_backing_up );
		else
			fprintf( stderr,
				"  Compressed tables always back-up\n" );

		if ( bol_needed )
			fprintf( stderr,
				"  Beginning-of-line patterns used\n" );

		fprintf( stderr, "  %d/%d start conditions\n", lastsc,
			current_max_scs );
		fprintf( stderr,
			"  %d epsilon states, %d double epsilon states\n",
			numeps, eps2 );

		if ( lastccl == 0 )
			fprintf( stderr, "  no character classes\n" );
		else
			fprintf( stderr,
	"  %d/%d character classes needed %d/%d words of storage, %d reused\n",
				lastccl, current_maxccls,
				cclmap[lastccl] + ccllen[lastccl],
				current_max_ccl_tbl_size, cclreuse );

		fprintf( stderr, "  %d state/nextstate pairs created\n",
			numsnpairs );
		fprintf( stderr, "  %d/%d unique/duplicate transitions\n",
			numuniq, numdup );

		if ( fulltbl )
			{
			tblsiz = lastdfa * numecs;
			fprintf( stderr, "  %d table entries\n", tblsiz );
			}

		else
			{
			tblsiz = 2 * (lastdfa + numtemps) + 2 * tblend;

			fprintf( stderr, "  %d/%d base-def entries created\n",
				lastdfa + numtemps, current_max_dfas );
			fprintf( stderr,
				"  %d/%d (peak %d) nxt-chk entries created\n",
				tblend, current_max_xpairs, peakpairs );
			fprintf( stderr,
			"  %d/%d (peak %d) template nxt-chk entries created\n",
				numtemps * nummecs, current_max_template_xpairs,
				numtemps * numecs );
			fprintf( stderr, "  %d empty table entries\n", nummt );
			fprintf( stderr, "  %d protos created\n", numprots );
			fprintf( stderr, "  %d templates created, %d uses\n",
				numtemps, tmpuses );
			}

		if ( useecs )
			{
			tblsiz = tblsiz + csize;
			fprintf( stderr,
				"  %d/%d equivalence classes created\n",
				numecs, csize );
			}

		if ( usemecs )
			{
			tblsiz = tblsiz + numecs;
			fprintf( stderr,
				"  %d/%d meta-equivalence classes created\n",
				nummecs, csize );
			}

		fprintf( stderr,
			"  %d (%d saved) hash collisions, %d DFAs equal\n",
			hshcol, hshsave, dfaeql );
		fprintf( stderr, "  %d sets of reallocations needed\n",
			num_reallocs );
		fprintf( stderr, "  %d total table entries needed\n", tblsiz );
		}

#ifndef VMS
	exit( exit_status );
#else
	exit( exit_status + 1 );
#endif
	}


/* flexinit - initialize flex */

void flexinit( argc, argv )
int argc;
char **argv;
	{
	int i, sawcmpflag;
	int csize_given, interactive_given;
	char *arg, *mktemp();

	printstats = syntaxerror = trace = spprdflt = caseins = false;
	lex_compat = false;
	C_plus_plus = backing_up_report = ddebug = fulltbl = fullspd = false;
	long_align = nowarn = yymore_used = continued_action = reject = false;
	yytext_is_array = yymore_really_used = reject_really_used = false;
	gen_line_dirs = usemecs = useecs = true;
	performance_report = 0;

	sawcmpflag = false;
	use_read = use_stdout = false;
	csize_given = false;
	interactive_given = false;

	/* Initialize dynamic array for holding the rule actions. */
	action_size = 2048;	/* default size of action array in bytes */
	action_array = allocate_character_array( action_size );
	defs1_offset = prolog_offset = action_offset = action_index = 0;
	action_array[0] = '\0';

	program_name = argv[0];

	if ( program_name[0] != '\0' &&
	     program_name[strlen( program_name ) - 1] == '+' )
		C_plus_plus = true;

	/* read flags */
	for ( --argc, ++argv; argc ; --argc, ++argv )
		{
		if ( argv[0][0] != '-' || argv[0][1] == '\0' )
			break;

		arg = argv[0];

		for ( i = 1; arg[i] != '\0'; ++i )
			switch ( arg[i] )
				{
				case '+':
					C_plus_plus = true;
					break;

				case 'B':
					interactive = false;
					interactive_given = true;
					break;

				case 'b':
					backing_up_report = true;
					break;

				case 'c':
					fprintf( stderr,
	"%s: Assuming use of deprecated -c flag is really intended to be -C\n",
					program_name );

					/* fall through */

				case 'C':
					if ( i != 1 )
						flexerror(
					"-C flag must be given separately" );

					if ( ! sawcmpflag )
						{
						useecs = false;
						usemecs = false;
						fulltbl = false;
						sawcmpflag = true;
						}

					for ( ++i; arg[i] != '\0'; ++i )
						switch ( arg[i] )
							{
							case 'a':
								long_align =
									true;
								break;

							case 'e':
								useecs = true;
								break;

							case 'F':
								fullspd = true;
								break;

							case 'f':
								fulltbl = true;
								break;

							case 'm':
								usemecs = true;
								break;

							case 'r':
								use_read = true;
								break;

							default:
								lerrif(
						"unknown -C option '%c'",
								(int) arg[i] );
								break;
							}

					goto get_next_arg;

				case 'd':
					ddebug = true;
					break;

				case 'f':
					useecs = usemecs = false;
					use_read = fulltbl = true;
					break;

				case 'F':
					useecs = usemecs = false;
					use_read = fullspd = true;
					break;

				case 'h':
					usage();
					exit( 0 );

				case 'I':
					interactive = true;
					interactive_given = true;
					break;

				case 'i':
					caseins = true;
					break;

				case 'l':
					lex_compat = true;
					break;

				case 'L':
					gen_line_dirs = false;
					break;

				case 'n':
					/* Stupid do-nothing deprecated
					 * option.
					 */
					break;

				case 'P':
					if ( i != 1 )
						flexerror(
					"-P flag must be given separately" );

					prefix = arg + i + 1;
					goto get_next_arg;

				case 'p':
					++performance_report;
					break;

				case 'S':
					if ( i != 1 )
						flexerror(
					"-S flag must be given separately" );

					skelname = arg + i + 1;
					goto get_next_arg;

				case 's':
					spprdflt = true;
					break;

				case 't':
					use_stdout = true;
					break;

				case 'T':
					trace = true;
					break;

				case 'v':
					printstats = true;
					break;

				case 'V':
					fprintf( stderr, "%s version %s\n",
						program_name, flex_version );
					exit( 0 );

				case 'w':
					nowarn = true;
					break;

				case '7':
					csize = 128;
					csize_given = true;
					break;

				case '8':
					csize = CSIZE;
					csize_given = true;
					break;

				default:
					fprintf( stderr,
						"%s: unknown flag '%c'\n",
						program_name, (int) arg[i] );
					usage();
					exit( 1 );
				}

		/* Used by -C, -S and -P flags in lieu of a "continue 2"
		 * control.
		 */
		get_next_arg: ;
		}

	if ( ! csize_given )
		{
		if ( (fulltbl || fullspd) && ! useecs )
			csize = DEFAULT_CSIZE;
		else
			csize = CSIZE;
		}

	if ( ! interactive_given )
		{
		if ( fulltbl || fullspd )
			interactive = false;
		else
			interactive = true;
		}

	if ( lex_compat )
		{
		if ( C_plus_plus )
			flexerror( "Can't use -+ with -l option" );

		if ( fulltbl || fullspd )
			flexerror( "Can't use -f or -F with -l option" );

		/* Don't rely on detecting use of yymore() and REJECT,
		 * just assume they'll be used.
		 */
		yymore_really_used = reject_really_used = true;

		yytext_is_array = true;
		use_read = false;
		}

	if ( (fulltbl || fullspd) && usemecs )
		flexerror( "-Cf/-CF and -Cm don't make sense together" );

	if ( (fulltbl || fullspd) && interactive )
		flexerror( "-Cf/-CF and -I are incompatible" );

	if ( fulltbl && fullspd )
		flexerror( "-Cf and -CF are mutually exclusive" );

	if ( C_plus_plus && fullspd )
		flexerror( "Can't use -+ with -CF option" );

	if ( ! use_stdout )
		{
		FILE *prev_stdout;
		char *suffix;

		if ( C_plus_plus )
			suffix = "cc";
		else
			suffix = "c";

		sprintf( outfile_path, outfile_template, prefix, suffix );

		prev_stdout = freopen( outfile_path, "w", stdout );

		if ( prev_stdout == NULL )
			lerrsf( "could not create %s", outfile_path );

		outfile_created = 1;
		}

	num_input_files = argc;
	input_files = argv;
	set_input_file( num_input_files > 0 ? input_files[0] : NULL );

	if ( backing_up_report )
		{
#ifndef SHORT_FILE_NAMES
		backing_up_file = fopen( "lex.backup", "w" );
#else
		backing_up_file = fopen( "lex.bck", "w" );
#endif

		if ( backing_up_file == NULL )
			flexerror( "could not create lex.backup" );
		}

	else
		backing_up_file = NULL;


	lastccl = 0;
	lastsc = 0;

	if ( skelname && (skelfile = fopen( skelname, "r" )) == NULL )
		lerrsf( "can't open skeleton file %s", skelname );

	if ( strcmp( prefix, "yy" ) )
		{
#define GEN_PREFIX(name) printf( "#define yy%s %s%s\n", name, prefix, name );
		GEN_PREFIX( "FlexLexer" );
		GEN_PREFIX( "_create_buffer" );
		GEN_PREFIX( "_delete_buffer" );
		GEN_PREFIX( "_flex_debug" );
		GEN_PREFIX( "_init_buffer" );
		GEN_PREFIX( "_load_buffer_state" );
		GEN_PREFIX( "_switch_to_buffer" );
		GEN_PREFIX( "in" );
		GEN_PREFIX( "leng" );
		GEN_PREFIX( "lex" );
		GEN_PREFIX( "out" );
		GEN_PREFIX( "restart" );
		GEN_PREFIX( "text" );
		GEN_PREFIX( "wrap" );
		printf( "\n" );
		}


	lastdfa = lastnfa = 0;
	num_rules = num_eof_rules = default_rule = 0;
	numas = numsnpairs = tmpuses = 0;
	numecs = numeps = eps2 = num_reallocs = hshcol = dfaeql = totnst = 0;
	numuniq = numdup = hshsave = eofseen = datapos = dataline = 0;
	num_backing_up = onesp = numprots = 0;
	variable_trailing_context_rules = bol_needed = false;

	linenum = sectnum = 1;
	firstprot = NIL;

	/* Used in mkprot() so that the first proto goes in slot 1
	 * of the proto queue.
	 */
	lastprot = 1;

	if ( useecs )
		{
		/* Set up doubly-linked equivalence classes. */

		/* We loop all the way up to csize, since ecgroup[csize] is
		 * the position used for NUL characters.
		 */
		ecgroup[1] = NIL;

		for ( i = 2; i <= csize; ++i )
			{
			ecgroup[i] = i - 1;
			nextecm[i - 1] = i;
			}

		nextecm[csize] = NIL;
		}

	else
		{
		/* Put everything in its own equivalence class. */
		for ( i = 1; i <= csize; ++i )
			{
			ecgroup[i] = i;
			nextecm[i] = BAD_SUBSCRIPT;	/* to catch errors */
			}
		}

	set_up_initial_allocations();
	}


/* readin - read in the rules section of the input file(s) */

void readin()
	{
	skelout();

	line_directive_out( (FILE *) 0 );

	if ( yyparse() )
		{
		pinpoint_message( "fatal parse error" );
		flexend( 1 );
		}

	if ( syntaxerror )
		flexend( 1 );

	if ( yymore_really_used == REALLY_USED )
		yymore_used = true;
	else if ( yymore_really_used == REALLY_NOT_USED )
		yymore_used = false;

	if ( reject_really_used == REALLY_USED )
		reject = true;
	else if ( reject_really_used == REALLY_NOT_USED )
		reject = false;

	if ( performance_report > 0 )
		{
		if ( lex_compat )
			{
			fprintf( stderr,
"-l AT&T lex compatibility option entails a large performance penalty\n" );
			fprintf( stderr,
" and may be the actual source of other reported performance penalties\n" );
			}

		if ( performance_report > 1 )
			{
			if ( interactive )
				fprintf( stderr,
		"-I (interactive) entails a minor performance penalty\n" );

			if ( yymore_used )
				fprintf( stderr,
			"yymore() entails a minor performance penalty\n" );
			}

		if ( reject )
			fprintf( stderr,
			"REJECT entails a large performance penalty\n" );

		if ( variable_trailing_context_rules )
			fprintf( stderr,
"Variable trailing context rules entail a large performance penalty\n" );
		}

	if ( reject )
		real_reject = true;

	if ( variable_trailing_context_rules )
		reject = true;

	if ( (fulltbl || fullspd) && reject )
		{
		if ( real_reject )
			flexerror( "REJECT cannot be used with -f or -F" );
		else
			flexerror(
	"variable trailing context rules cannot be used with -f or -F" );
		}

	if ( csize == 256 )
		puts( "typedef unsigned char YY_CHAR;" );
	else
		puts( "typedef char YY_CHAR;" );

	if ( C_plus_plus )
		{
		puts( "#define yytext_ptr yytext" );

		if ( interactive )
			puts( "#define YY_INTERACTIVE" );
		}

	if ( fullspd )
		printf(
		"typedef const struct yy_trans_info *yy_state_type;\n" );
	else if ( ! C_plus_plus )
		printf( "typedef int yy_state_type;\n" );

	if ( reject )
		printf( "\n#define YY_USES_REJECT\n" );

	if ( ddebug )
		puts( "\n#define FLEX_DEBUG" );

	if ( lex_compat )
		{
		printf( "FILE *yyin = stdin, *yyout = stdout;\n" );
		printf( "extern int yylineno;\n" );
		printf( "int yylineno = 1;\n" );
		}
	else if ( ! C_plus_plus )
		printf( "FILE *yyin = (FILE *) 0, *yyout = (FILE *) 0;\n" );

	if ( C_plus_plus )
		printf( "\n#include <FlexLexer.h>\n" );

	else
		{
		if ( yytext_is_array )
			puts( "extern char yytext[];\n" );

		else
			{
			puts( "extern char *yytext;" );
			puts( "#define yytext_ptr yytext" );
			}
		}

	if ( useecs )
		numecs = cre8ecs( nextecm, ecgroup, csize );
	else
		numecs = csize;

	/* Now map the equivalence class for NUL to its expected place. */
	ecgroup[0] = ecgroup[csize];
	NUL_ec = ABS( ecgroup[0] );

	if ( useecs )
		ccl2ecl();
	}


/* set_up_initial_allocations - allocate memory for internal tables */

void set_up_initial_allocations()
	{
	current_mns = INITIAL_MNS;
	firstst = allocate_integer_array( current_mns );
	lastst = allocate_integer_array( current_mns );
	finalst = allocate_integer_array( current_mns );
	transchar = allocate_integer_array( current_mns );
	trans1 = allocate_integer_array( current_mns );
	trans2 = allocate_integer_array( current_mns );
	accptnum = allocate_integer_array( current_mns );
	assoc_rule = allocate_integer_array( current_mns );
	state_type = allocate_integer_array( current_mns );

	current_max_rules = INITIAL_MAX_RULES;
	rule_type = allocate_integer_array( current_max_rules );
	rule_linenum = allocate_integer_array( current_max_rules );
	rule_useful = allocate_integer_array( current_max_rules );

	current_max_scs = INITIAL_MAX_SCS;
	scset = allocate_integer_array( current_max_scs );
	scbol = allocate_integer_array( current_max_scs );
	scxclu = allocate_integer_array( current_max_scs );
	sceof = allocate_integer_array( current_max_scs );
	scname = allocate_char_ptr_array( current_max_scs );
	actvsc = allocate_integer_array( current_max_scs );

	current_maxccls = INITIAL_MAX_CCLS;
	cclmap = allocate_integer_array( current_maxccls );
	ccllen = allocate_integer_array( current_maxccls );
	cclng = allocate_integer_array( current_maxccls );

	current_max_ccl_tbl_size = INITIAL_MAX_CCL_TBL_SIZE;
	ccltbl = allocate_Character_array( current_max_ccl_tbl_size );

	current_max_dfa_size = INITIAL_MAX_DFA_SIZE;

	current_max_xpairs = INITIAL_MAX_XPAIRS;
	nxt = allocate_integer_array( current_max_xpairs );
	chk = allocate_integer_array( current_max_xpairs );

	current_max_template_xpairs = INITIAL_MAX_TEMPLATE_XPAIRS;
	tnxt = allocate_integer_array( current_max_template_xpairs );

	current_max_dfas = INITIAL_MAX_DFAS;
	base = allocate_integer_array( current_max_dfas );
	def = allocate_integer_array( current_max_dfas );
	dfasiz = allocate_integer_array( current_max_dfas );
	accsiz = allocate_integer_array( current_max_dfas );
	dhash = allocate_integer_array( current_max_dfas );
	dss = allocate_int_ptr_array( current_max_dfas );
	dfaacc = allocate_dfaacc_union( current_max_dfas );

	nultrans = (int *) 0;
	}


void usage()
	{
	fprintf( stderr,
"%s [-bcdfhilnpstvwBFILTV78+ -C[aefFmr] -Pprefix -Sskeleton] [file ...]\n",
		program_name );

	fprintf( stderr,
		"\t-b  generate backing-up information to lex.backup\n" );
	fprintf( stderr, "\t-c  do-nothing POSIX option\n" );
	fprintf( stderr, "\t-d  turn on debug mode in generated scanner\n" );
	fprintf( stderr, "\t-f  generate fast, large scanner\n" );
	fprintf( stderr, "\t-h  produce this help message\n" );
	fprintf( stderr, "\t-i  generate case-insensitive scanner\n" );
	fprintf( stderr, "\t-l  maximal compatibility with original lex\n" );
	fprintf( stderr, "\t-n  do-nothing POSIX option\n" );
	fprintf( stderr, "\t-p  generate performance report to stderr\n" );
	fprintf( stderr,
		"\t-s  suppress default rule to ECHO unmatched text\n" );
	fprintf( stderr,
	"\t-t  write generated scanner on stdout instead of lex.yy.c\n" );
	fprintf( stderr,
		"\t-v  write summary of scanner statistics to stderr\n" );
	fprintf( stderr, "\t-w  do not generate warnings\n" );
	fprintf( stderr, "\t-B  generate batch scanner (opposite of -I)\n" );
	fprintf( stderr,
		"\t-F  use alternative fast scanner representation\n" );
	fprintf( stderr,
		"\t-I  generate interactive scanner (opposite of -B)\n" );
	fprintf( stderr, "\t-L  suppress #line directives in scanner\n" );
	fprintf( stderr, "\t-T  %s should run in trace mode\n", program_name );
	fprintf( stderr, "\t-V  report %s version\n", program_name );
	fprintf( stderr, "\t-7  generate 7-bit scanner\n" );
	fprintf( stderr, "\t-8  generate 8-bit scanner\n" );
	fprintf( stderr, "\t-+  generate C++ scanner class\n" );
	fprintf( stderr,
	"\t-C  specify degree of table compression (default is -Cem):\n" );
	fprintf( stderr,
	"\t\t-Ca  trade off larger tables for better memory alignment\n" );
	fprintf( stderr, "\t\t-Ce  construct equivalence classes\n" );
	fprintf( stderr,
	"\t\t-Cf  do not compress scanner tables; use -f representation\n" );
	fprintf( stderr,
	"\t\t-CF  do not compress scanner tables; use -F representation\n" );
	fprintf( stderr, "\t\t-Cm  construct meta-equivalence classes\n" );
	fprintf( stderr,
		"\t\t-Cr  use read() instead of stdio for scanner input\n" );
	fprintf( stderr, "\t-P  specify scanner prefix other than \"yy\"\n" );
	fprintf( stderr, "\t-S  specify skeleton file\n" );
	}
