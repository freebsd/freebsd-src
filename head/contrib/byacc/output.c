/* $Id: output.c,v 1.47 2014/01/01 17:22:38 tom Exp $ */

#include "defs.h"

#define StaticOrR	(rflag ? "" : "static ")
#define CountLine(fp)   (!rflag || ((fp) == code_file))

static int nvectors;
static int nentries;
static Value_t **froms;
static Value_t **tos;
static Value_t *tally;
static Value_t *width;
static Value_t *state_count;
static Value_t *order;
static Value_t *base;
static Value_t *pos;
static int maxtable;
static Value_t *table;
static Value_t *check;
static int lowzero;
static int high;

static void
putc_code(FILE * fp, int c)
{
    if ((c == '\n') && (fp == code_file))
	++outline;
    putc(c, fp);
}

static void
putl_code(FILE * fp, const char *s)
{
    if (fp == code_file)
	++outline;
    fputs(s, fp);
}

static void
puts_code(FILE * fp, const char *s)
{
    fputs(s, fp);
}

static void
write_code_lineno(FILE * fp)
{
    if (!lflag && (fp == code_file))
    {
	++outline;
	fprintf(fp, line_format, outline, code_file_name);
    }
}

static void
write_input_lineno(void)
{
    if (!lflag)
    {
	++outline;
	fprintf(code_file, line_format, lineno, input_file_name);
    }
}

static void
define_prefixed(FILE * fp, const char *name)
{
    int bump_line = CountLine(fp);
    if (bump_line)
	++outline;
    fprintf(fp, "\n");

    if (bump_line)
	++outline;
    fprintf(fp, "#ifndef %s\n", name);

    if (bump_line)
	++outline;
    fprintf(fp, "#define %-10s %s%s\n", name, symbol_prefix, name + 2);

    if (bump_line)
	++outline;
    fprintf(fp, "#endif /* %s */\n", name);
}

static void
output_prefix(FILE * fp)
{
    if (symbol_prefix == NULL)
    {
	symbol_prefix = "yy";
    }
    else
    {
	define_prefixed(fp, "yyparse");
	define_prefixed(fp, "yylex");
	define_prefixed(fp, "yyerror");
	define_prefixed(fp, "yychar");
	define_prefixed(fp, "yyval");
	define_prefixed(fp, "yylval");
	define_prefixed(fp, "yydebug");
	define_prefixed(fp, "yynerrs");
	define_prefixed(fp, "yyerrflag");
	define_prefixed(fp, "yylhs");
	define_prefixed(fp, "yylen");
	define_prefixed(fp, "yydefred");
	define_prefixed(fp, "yydgoto");
	define_prefixed(fp, "yysindex");
	define_prefixed(fp, "yyrindex");
	define_prefixed(fp, "yygindex");
	define_prefixed(fp, "yytable");
	define_prefixed(fp, "yycheck");
	define_prefixed(fp, "yyname");
	define_prefixed(fp, "yyrule");
    }
    if (CountLine(fp))
	++outline;
    fprintf(fp, "#define YYPREFIX \"%s\"\n", symbol_prefix);
}

static void
output_newline(void)
{
    if (!rflag)
	++outline;
    putc('\n', output_file);
}

static void
output_line(const char *value)
{
    fputs(value, output_file);
    output_newline();
}

static void
output_int(int value)
{
    fprintf(output_file, "%5d,", value);
}

static void
start_int_table(const char *name, int value)
{
    int need = 34 - (int)(strlen(symbol_prefix) + strlen(name));

    if (need < 6)
	need = 6;
    fprintf(output_file,
	    "%sconst short %s%s[] = {%*d,",
	    StaticOrR, symbol_prefix, name, need, value);
}

static void
start_str_table(const char *name)
{
    fprintf(output_file,
	    "%sconst char *%s%s[] = {",
	    StaticOrR, "yy", name);
    output_newline();
}

static void
end_table(void)
{
    output_newline();
    output_line("};");
}

static void
output_rule_data(void)
{
    int i;
    int j;

    start_int_table("lhs", symbol_value[start_symbol]);

    j = 10;
    for (i = 3; i < nrules; i++)
    {
	if (j >= 10)
	{
	    output_newline();
	    j = 1;
	}
	else
	    ++j;

	output_int(symbol_value[rlhs[i]]);
    }
    end_table();

    start_int_table("len", 2);

    j = 10;
    for (i = 3; i < nrules; i++)
    {
	if (j >= 10)
	{
	    output_newline();
	    j = 1;
	}
	else
	    j++;

	output_int(rrhs[i + 1] - rrhs[i] - 1);
    }
    end_table();
}

static void
output_yydefred(void)
{
    int i, j;

    start_int_table("defred", (defred[0] ? defred[0] - 2 : 0));

    j = 10;
    for (i = 1; i < nstates; i++)
    {
	if (j < 10)
	    ++j;
	else
	{
	    output_newline();
	    j = 1;
	}

	output_int((defred[i] ? defred[i] - 2 : 0));
    }

    end_table();
}

static void
token_actions(void)
{
    int i, j;
    Value_t shiftcount, reducecount;
    int max, min;
    Value_t *actionrow, *r, *s;
    action *p;

    actionrow = NEW2(2 * ntokens, Value_t);
    for (i = 0; i < nstates; ++i)
    {
	if (parser[i])
	{
	    for (j = 0; j < 2 * ntokens; ++j)
		actionrow[j] = 0;

	    shiftcount = 0;
	    reducecount = 0;
	    for (p = parser[i]; p; p = p->next)
	    {
		if (p->suppressed == 0)
		{
		    if (p->action_code == SHIFT)
		    {
			++shiftcount;
			actionrow[p->symbol] = p->number;
		    }
		    else if (p->action_code == REDUCE && p->number != defred[i])
		    {
			++reducecount;
			actionrow[p->symbol + ntokens] = p->number;
		    }
		}
	    }

	    tally[i] = shiftcount;
	    tally[nstates + i] = reducecount;
	    width[i] = 0;
	    width[nstates + i] = 0;
	    if (shiftcount > 0)
	    {
		froms[i] = r = NEW2(shiftcount, Value_t);
		tos[i] = s = NEW2(shiftcount, Value_t);
		min = MAXSHORT;
		max = 0;
		for (j = 0; j < ntokens; ++j)
		{
		    if (actionrow[j])
		    {
			if (min > symbol_value[j])
			    min = symbol_value[j];
			if (max < symbol_value[j])
			    max = symbol_value[j];
			*r++ = symbol_value[j];
			*s++ = actionrow[j];
		    }
		}
		width[i] = (Value_t) (max - min + 1);
	    }
	    if (reducecount > 0)
	    {
		froms[nstates + i] = r = NEW2(reducecount, Value_t);
		tos[nstates + i] = s = NEW2(reducecount, Value_t);
		min = MAXSHORT;
		max = 0;
		for (j = 0; j < ntokens; ++j)
		{
		    if (actionrow[ntokens + j])
		    {
			if (min > symbol_value[j])
			    min = symbol_value[j];
			if (max < symbol_value[j])
			    max = symbol_value[j];
			*r++ = symbol_value[j];
			*s++ = (Value_t) (actionrow[ntokens + j] - 2);
		    }
		}
		width[nstates + i] = (Value_t) (max - min + 1);
	    }
	}
    }
    FREE(actionrow);
}

static int
default_goto(int symbol)
{
    int i;
    int m;
    int n;
    int default_state;
    int max;

    m = goto_map[symbol];
    n = goto_map[symbol + 1];

    if (m == n)
	return (0);

    for (i = 0; i < nstates; i++)
	state_count[i] = 0;

    for (i = m; i < n; i++)
	state_count[to_state[i]]++;

    max = 0;
    default_state = 0;
    for (i = 0; i < nstates; i++)
    {
	if (state_count[i] > max)
	{
	    max = state_count[i];
	    default_state = i;
	}
    }

    return (default_state);
}

static void
save_column(int symbol, int default_state)
{
    int i;
    int m;
    int n;
    Value_t *sp;
    Value_t *sp1;
    Value_t *sp2;
    Value_t count;
    int symno;

    m = goto_map[symbol];
    n = goto_map[symbol + 1];

    count = 0;
    for (i = m; i < n; i++)
    {
	if (to_state[i] != default_state)
	    ++count;
    }
    if (count == 0)
	return;

    symno = symbol_value[symbol] + 2 * nstates;

    froms[symno] = sp1 = sp = NEW2(count, Value_t);
    tos[symno] = sp2 = NEW2(count, Value_t);

    for (i = m; i < n; i++)
    {
	if (to_state[i] != default_state)
	{
	    *sp1++ = from_state[i];
	    *sp2++ = to_state[i];
	}
    }

    tally[symno] = count;
    width[symno] = (Value_t) (sp1[-1] - sp[0] + 1);
}

static void
goto_actions(void)
{
    int i, j, k;

    state_count = NEW2(nstates, Value_t);

    k = default_goto(start_symbol + 1);
    start_int_table("dgoto", k);
    save_column(start_symbol + 1, k);

    j = 10;
    for (i = start_symbol + 2; i < nsyms; i++)
    {
	if (j >= 10)
	{
	    output_newline();
	    j = 1;
	}
	else
	    ++j;

	k = default_goto(i);
	output_int(k);
	save_column(i, k);
    }

    end_table();
    FREE(state_count);
}

static void
sort_actions(void)
{
    Value_t i;
    int j;
    int k;
    int t;
    int w;

    order = NEW2(nvectors, Value_t);
    nentries = 0;

    for (i = 0; i < nvectors; i++)
    {
	if (tally[i] > 0)
	{
	    t = tally[i];
	    w = width[i];
	    j = nentries - 1;

	    while (j >= 0 && (width[order[j]] < w))
		j--;

	    while (j >= 0 && (width[order[j]] == w) && (tally[order[j]] < t))
		j--;

	    for (k = nentries - 1; k > j; k--)
		order[k + 1] = order[k];

	    order[j + 1] = i;
	    nentries++;
	}
    }
}

/*  The function matching_vector determines if the vector specified by	*/
/*  the input parameter matches a previously considered	vector.  The	*/
/*  test at the start of the function checks if the vector represents	*/
/*  a row of shifts over terminal symbols or a row of reductions, or a	*/
/*  column of shifts over a nonterminal symbol.  Berkeley Yacc does not	*/
/*  check if a column of shifts over a nonterminal symbols matches a	*/
/*  previously considered vector.  Because of the nature of LR parsing	*/
/*  tables, no two columns can match.  Therefore, the only possible	*/
/*  match would be between a row and a column.  Such matches are	*/
/*  unlikely.  Therefore, to save time, no attempt is made to see if a	*/
/*  column matches a previously considered vector.			*/
/*									*/
/*  Matching_vector is poorly designed.  The test could easily be made	*/
/*  faster.  Also, it depends on the vectors being in a specific	*/
/*  order.								*/

static int
matching_vector(int vector)
{
    int i;
    int j;
    int k;
    int t;
    int w;
    int match;
    int prev;

    i = order[vector];
    if (i >= 2 * nstates)
	return (-1);

    t = tally[i];
    w = width[i];

    for (prev = vector - 1; prev >= 0; prev--)
    {
	j = order[prev];
	if (width[j] != w || tally[j] != t)
	    return (-1);

	match = 1;
	for (k = 0; match && k < t; k++)
	{
	    if (tos[j][k] != tos[i][k] || froms[j][k] != froms[i][k])
		match = 0;
	}

	if (match)
	    return (j);
    }

    return (-1);
}

static int
pack_vector(int vector)
{
    int i, j, k, l;
    int t;
    int loc;
    int ok;
    Value_t *from;
    Value_t *to;
    int newmax;

    i = order[vector];
    t = tally[i];
    assert(t);

    from = froms[i];
    to = tos[i];

    j = lowzero - from[0];
    for (k = 1; k < t; ++k)
	if (lowzero - from[k] > j)
	    j = lowzero - from[k];
    for (;; ++j)
    {
	if (j == 0)
	    continue;
	ok = 1;
	for (k = 0; ok && k < t; k++)
	{
	    loc = j + from[k];
	    if (loc >= maxtable - 1)
	    {
		if (loc >= MAXTABLE - 1)
		    fatal("maximum table size exceeded");

		newmax = maxtable;
		do
		{
		    newmax += 200;
		}
		while (newmax <= loc);

		table = TREALLOC(Value_t, table, newmax);
		NO_SPACE(table);

		check = TREALLOC(Value_t, check, newmax);
		NO_SPACE(check);

		for (l = maxtable; l < newmax; ++l)
		{
		    table[l] = 0;
		    check[l] = -1;
		}
		maxtable = newmax;
	    }

	    if (check[loc] != -1)
		ok = 0;
	}
	for (k = 0; ok && k < vector; k++)
	{
	    if (pos[k] == j)
		ok = 0;
	}
	if (ok)
	{
	    for (k = 0; k < t; k++)
	    {
		loc = j + from[k];
		table[loc] = to[k];
		check[loc] = from[k];
		if (loc > high)
		    high = loc;
	    }

	    while (check[lowzero] != -1)
		++lowzero;

	    return (j);
	}
    }
}

static void
pack_table(void)
{
    int i;
    Value_t place;
    int state;

    base = NEW2(nvectors, Value_t);
    pos = NEW2(nentries, Value_t);

    maxtable = 1000;
    table = NEW2(maxtable, Value_t);
    check = NEW2(maxtable, Value_t);

    lowzero = 0;
    high = 0;

    for (i = 0; i < maxtable; i++)
	check[i] = -1;

    for (i = 0; i < nentries; i++)
    {
	state = matching_vector(i);

	if (state < 0)
	    place = (Value_t) pack_vector(i);
	else
	    place = base[state];

	pos[i] = place;
	base[order[i]] = place;
    }

    for (i = 0; i < nvectors; i++)
    {
	if (froms[i])
	    FREE(froms[i]);
	if (tos[i])
	    FREE(tos[i]);
    }

    FREE(froms);
    FREE(tos);
    FREE(pos);
}

static void
output_base(void)
{
    int i, j;

    start_int_table("sindex", base[0]);

    j = 10;
    for (i = 1; i < nstates; i++)
    {
	if (j >= 10)
	{
	    output_newline();
	    j = 1;
	}
	else
	    ++j;

	output_int(base[i]);
    }

    end_table();

    start_int_table("rindex", base[nstates]);

    j = 10;
    for (i = nstates + 1; i < 2 * nstates; i++)
    {
	if (j >= 10)
	{
	    output_newline();
	    j = 1;
	}
	else
	    ++j;

	output_int(base[i]);
    }

    end_table();

    start_int_table("gindex", base[2 * nstates]);

    j = 10;
    for (i = 2 * nstates + 1; i < nvectors - 1; i++)
    {
	if (j >= 10)
	{
	    output_newline();
	    j = 1;
	}
	else
	    ++j;

	output_int(base[i]);
    }

    end_table();
    FREE(base);
}

static void
output_table(void)
{
    int i;
    int j;

    ++outline;
    fprintf(code_file, "#define YYTABLESIZE %d\n", high);
    start_int_table("table", table[0]);

    j = 10;
    for (i = 1; i <= high; i++)
    {
	if (j >= 10)
	{
	    output_newline();
	    j = 1;
	}
	else
	    ++j;

	output_int(table[i]);
    }

    end_table();
    FREE(table);
}

static void
output_check(void)
{
    int i;
    int j;

    start_int_table("check", check[0]);

    j = 10;
    for (i = 1; i <= high; i++)
    {
	if (j >= 10)
	{
	    output_newline();
	    j = 1;
	}
	else
	    ++j;

	output_int(check[i]);
    }

    end_table();
    FREE(check);
}

static void
output_actions(void)
{
    nvectors = 2 * nstates + nvars;

    froms = NEW2(nvectors, Value_t *);
    tos = NEW2(nvectors, Value_t *);
    tally = NEW2(nvectors, Value_t);
    width = NEW2(nvectors, Value_t);

    token_actions();
    FREE(lookaheads);
    FREE(LA);
    FREE(LAruleno);
    FREE(accessing_symbol);

    goto_actions();
    FREE(goto_map + ntokens);
    FREE(from_state);
    FREE(to_state);

    sort_actions();
    pack_table();
    output_base();
    output_table();
    output_check();
}

static int
is_C_identifier(char *name)
{
    char *s;
    int c;

    s = name;
    c = *s;
    if (c == '"')
    {
	c = *++s;
	if (!isalpha(c) && c != '_' && c != '$')
	    return (0);
	while ((c = *++s) != '"')
	{
	    if (!isalnum(c) && c != '_' && c != '$')
		return (0);
	}
	return (1);
    }

    if (!isalpha(c) && c != '_' && c != '$')
	return (0);
    while ((c = *++s) != 0)
    {
	if (!isalnum(c) && c != '_' && c != '$')
	    return (0);
    }
    return (1);
}

static void
output_defines(FILE * fp)
{
    int c, i;
    char *s;

    for (i = 2; i < ntokens; ++i)
    {
	s = symbol_name[i];
	if (is_C_identifier(s) && (!sflag || *s != '"'))
	{
	    fprintf(fp, "#define ");
	    c = *s;
	    if (c == '"')
	    {
		while ((c = *++s) != '"')
		{
		    putc(c, fp);
		}
	    }
	    else
	    {
		do
		{
		    putc(c, fp);
		}
		while ((c = *++s) != 0);
	    }
	    if (fp == code_file)
		++outline;
	    fprintf(fp, " %d\n", symbol_value[i]);
	}
    }

    if (fp == code_file)
	++outline;
    if (fp != defines_file || iflag)
	fprintf(fp, "#define YYERRCODE %d\n", symbol_value[1]);

    if (fp == defines_file || (iflag && !dflag))
    {
	if (unionized)
	{
	    if (union_file != 0)
	    {
		rewind(union_file);
		while ((c = getc(union_file)) != EOF)
		    putc(c, fp);
	    }
	    fprintf(fp, "extern YYSTYPE %slval;\n", symbol_prefix);
	}
    }
}

static void
output_stored_text(FILE * fp)
{
    int c;
    FILE *in;

    rewind(text_file);
    if (text_file == NULL)
	open_error("text_file");
    in = text_file;
    if ((c = getc(in)) == EOF)
	return;
    putc_code(fp, c);
    while ((c = getc(in)) != EOF)
    {
	putc_code(fp, c);
    }
    write_code_lineno(fp);
}

static void
output_debug(void)
{
    int i, j, k, max;
    const char **symnam;
    const char *s;

    ++outline;
    fprintf(code_file, "#define YYFINAL %d\n", final_state);

    putl_code(code_file, "#ifndef YYDEBUG\n");
    ++outline;
    fprintf(code_file, "#define YYDEBUG %d\n", tflag);
    putl_code(code_file, "#endif\n");

    if (rflag)
    {
	fprintf(output_file, "#ifndef YYDEBUG\n");
	fprintf(output_file, "#define YYDEBUG %d\n", tflag);
	fprintf(output_file, "#endif\n");
    }

    max = 0;
    for (i = 2; i < ntokens; ++i)
	if (symbol_value[i] > max)
	    max = symbol_value[i];

    ++outline;
    fprintf(code_file, "#define YYMAXTOKEN %d\n", max);
    fprintf(code_file, "#define YYTRANSLATE(a) ((a) > YYMAXTOKEN ? "
	    "(YYMAXTOKEN + 1) : (a))\n");

    symnam = TMALLOC(const char *, max + 2);
    NO_SPACE(symnam);

    /* Note that it is  not necessary to initialize the element         */
    /* symnam[max].                                                     */
    for (i = 0; i <= max; ++i)
	symnam[i] = 0;
    for (i = ntokens - 1; i >= 2; --i)
	symnam[symbol_value[i]] = symbol_name[i];
    symnam[0] = "end-of-file";
    symnam[max + 1] = "illegal-symbol";

    /*
     * bison's yytname[] array is roughly the same as byacc's yyname[] array.
     * The difference is that byacc does not predefine "$end", "$error" or
     * "$undefined". 
     *
     * If the grammar declares "%token-table", define symbol "yytname" so
     * an application such as ntpd can build.
     */
    if (token_table)
    {
	output_line("#undef yytname");
	output_line("#define yytname yyname");
    }
    else
    {
	output_line("#if YYDEBUG");
    }

    start_str_table("name");
    j = 80;
    for (i = 0; i <= max + 1; ++i)
    {
	if ((s = symnam[i]) != 0)
	{
	    if (s[0] == '"')
	    {
		k = 7;
		while (*++s != '"')
		{
		    ++k;
		    if (*s == '\\')
		    {
			k += 2;
			if (*++s == '\\')
			    ++k;
		    }
		}
		j += k;
		if (j > 80)
		{
		    output_newline();
		    j = k;
		}
		fprintf(output_file, "\"\\\"");
		s = symnam[i];
		while (*++s != '"')
		{
		    if (*s == '\\')
		    {
			fprintf(output_file, "\\\\");
			if (*++s == '\\')
			    fprintf(output_file, "\\\\");
			else
			    putc(*s, output_file);
		    }
		    else
			putc(*s, output_file);
		}
		fprintf(output_file, "\\\"\",");
	    }
	    else if (s[0] == '\'')
	    {
		if (s[1] == '"')
		{
		    j += 7;
		    if (j > 80)
		    {
			output_newline();
			j = 7;
		    }
		    fprintf(output_file, "\"'\\\"'\",");
		}
		else
		{
		    k = 5;
		    while (*++s != '\'')
		    {
			++k;
			if (*s == '\\')
			{
			    k += 2;
			    if (*++s == '\\')
				++k;
			}
		    }
		    j += k;
		    if (j > 80)
		    {
			output_newline();
			j = k;
		    }
		    fprintf(output_file, "\"'");
		    s = symnam[i];
		    while (*++s != '\'')
		    {
			if (*s == '\\')
			{
			    fprintf(output_file, "\\\\");
			    if (*++s == '\\')
				fprintf(output_file, "\\\\");
			    else
				putc(*s, output_file);
			}
			else
			    putc(*s, output_file);
		    }
		    fprintf(output_file, "'\",");
		}
	    }
	    else
	    {
		k = (int)strlen(s) + 3;
		j += k;
		if (j > 80)
		{
		    output_newline();
		    j = k;
		}
		putc('"', output_file);
		do
		{
		    putc(*s, output_file);
		}
		while (*++s);
		fprintf(output_file, "\",");
	    }
	}
	else
	{
	    j += 2;
	    if (j > 80)
	    {
		output_newline();
		j = 2;
	    }
	    fprintf(output_file, "0,");
	}
    }
    end_table();
    FREE(symnam);

    if (token_table)
	output_line("#if YYDEBUG");
    start_str_table("rule");
    for (i = 2; i < nrules; ++i)
    {
	fprintf(output_file, "\"%s :", symbol_name[rlhs[i]]);
	for (j = rrhs[i]; ritem[j] > 0; ++j)
	{
	    s = symbol_name[ritem[j]];
	    if (s[0] == '"')
	    {
		fprintf(output_file, " \\\"");
		while (*++s != '"')
		{
		    if (*s == '\\')
		    {
			if (s[1] == '\\')
			    fprintf(output_file, "\\\\\\\\");
			else
			    fprintf(output_file, "\\\\%c", s[1]);
			++s;
		    }
		    else
			putc(*s, output_file);
		}
		fprintf(output_file, "\\\"");
	    }
	    else if (s[0] == '\'')
	    {
		if (s[1] == '"')
		    fprintf(output_file, " '\\\"'");
		else if (s[1] == '\\')
		{
		    if (s[2] == '\\')
			fprintf(output_file, " '\\\\\\\\");
		    else
			fprintf(output_file, " '\\\\%c", s[2]);
		    s += 2;
		    while (*++s != '\'')
			putc(*s, output_file);
		    putc('\'', output_file);
		}
		else
		    fprintf(output_file, " '%c'", s[1]);
	    }
	    else
		fprintf(output_file, " %s", s);
	}
	fprintf(output_file, "\",");
	output_newline();
    }

    end_table();
    output_line("#endif");
}

static void
output_pure_parser(FILE * fp)
{
    putc_code(fp, '\n');

    if (fp == code_file)
	outline += 1;
    fprintf(fp, "#define YYPURE %d\n", pure_parser);
    putc_code(fp, '\n');
}

static void
output_stype(FILE * fp)
{
    if (!unionized && ntags == 0)
    {
	putc_code(fp, '\n');
	putl_code(fp, "#ifndef YYSTYPE\n");
	putl_code(fp, "typedef int YYSTYPE;\n");
	putl_code(fp, "#endif\n");
    }
}

static void
output_trailing_text(void)
{
    int c, last;
    FILE *in;

    if (line == 0)
	return;

    in = input_file;
    c = *cptr;
    if (c == '\n')
    {
	++lineno;
	if ((c = getc(in)) == EOF)
	    return;
	write_input_lineno();
	putc_code(code_file, c);
	last = c;
    }
    else
    {
	write_input_lineno();
	do
	{
	    putc_code(code_file, c);
	}
	while ((c = *++cptr) != '\n');
	putc_code(code_file, c);
	last = '\n';
    }

    while ((c = getc(in)) != EOF)
    {
	putc_code(code_file, c);
	last = c;
    }

    if (last != '\n')
    {
	putc_code(code_file, '\n');
    }
    write_code_lineno(code_file);
}

static void
output_semantic_actions(void)
{
    int c, last;

    rewind(action_file);
    if ((c = getc(action_file)) == EOF)
	return;

    last = c;
    putc_code(code_file, c);
    while ((c = getc(action_file)) != EOF)
    {
	putc_code(code_file, c);
	last = c;
    }

    if (last != '\n')
    {
	putc_code(code_file, '\n');
    }

    write_code_lineno(code_file);
}

static void
output_parse_decl(FILE * fp)
{
    putl_code(fp, "\n");
    putl_code(fp, "/* compatibility with bison */\n");
    putl_code(fp, "#ifdef YYPARSE_PARAM\n");
    putl_code(fp, "/* compatibility with FreeBSD */\n");
    putl_code(fp, "# ifdef YYPARSE_PARAM_TYPE\n");
    putl_code(fp,
	      "#  define YYPARSE_DECL() yyparse(YYPARSE_PARAM_TYPE YYPARSE_PARAM)\n");
    putl_code(fp, "# else\n");
    putl_code(fp, "#  define YYPARSE_DECL() yyparse(void *YYPARSE_PARAM)\n");
    putl_code(fp, "# endif\n");
    putl_code(fp, "#else\n");

    puts_code(fp, "# define YYPARSE_DECL() yyparse(");
    if (!parse_param)
	puts_code(fp, "void");
    else
    {
	param *p;
	for (p = parse_param; p; p = p->next)
	    fprintf(fp, "%s %s%s%s", p->type, p->name, p->type2,
		    p->next ? ", " : "");
    }
    putl_code(fp, ")\n");

    putl_code(fp, "#endif\n");
}

static void
output_lex_decl(FILE * fp)
{
    putl_code(fp, "\n");
    putl_code(fp, "/* Parameters sent to lex. */\n");
    putl_code(fp, "#ifdef YYLEX_PARAM\n");
    if (pure_parser)
    {
	putl_code(fp, "# ifdef YYLEX_PARAM_TYPE\n");
	putl_code(fp, "#  define YYLEX_DECL() yylex(YYSTYPE *yylval,"
		  " YYLEX_PARAM_TYPE YYLEX_PARAM)\n");
	putl_code(fp, "# else\n");
	putl_code(fp, "#  define YYLEX_DECL() yylex(YYSTYPE *yylval,"
		  " void * YYLEX_PARAM)\n");
	putl_code(fp, "# endif\n");
	putl_code(fp, "# define YYLEX yylex(&yylval, YYLEX_PARAM)\n");
    }
    else
    {
	putl_code(fp, "# define YYLEX_DECL() yylex(void *YYLEX_PARAM)\n");
	putl_code(fp, "# define YYLEX yylex(YYLEX_PARAM)\n");
    }
    putl_code(fp, "#else\n");
    if (pure_parser && lex_param)
    {
	param *p;
	puts_code(fp, "# define YYLEX_DECL() yylex(YYSTYPE *yylval, ");
	for (p = lex_param; p; p = p->next)
	    fprintf(fp, "%s %s%s%s", p->type, p->name, p->type2,
		    p->next ? ", " : "");
	putl_code(fp, ")\n");

	puts_code(fp, "# define YYLEX yylex(&yylval, ");
	for (p = lex_param; p; p = p->next)
	    fprintf(fp, "%s%s", p->name, p->next ? ", " : "");
	putl_code(fp, ")\n");
    }
    else if (pure_parser)
    {
	putl_code(fp, "# define YYLEX_DECL() yylex(YYSTYPE *yylval)\n");
	putl_code(fp, "# define YYLEX yylex(&yylval)\n");
    }
    else if (lex_param)
    {
	param *p;
	puts_code(fp, "# define YYLEX_DECL() yylex(");
	for (p = lex_param; p; p = p->next)
	    fprintf(fp, "%s %s%s%s", p->type, p->name, p->type2,
		    p->next ? ", " : "");
	putl_code(fp, ")\n");

	puts_code(fp, "# define YYLEX yylex(");
	for (p = lex_param; p; p = p->next)
	    fprintf(fp, "%s%s", p->name, p->next ? ", " : "");
	putl_code(fp, ")\n");
    }
    else
    {
	putl_code(fp, "# define YYLEX_DECL() yylex(void)\n");
	putl_code(fp, "# define YYLEX yylex()\n");
    }
    putl_code(fp, "#endif\n");
}

static void
output_error_decl(FILE * fp)
{
    putl_code(fp, "\n");
    putl_code(fp, "/* Parameters sent to yyerror. */\n");
    if (parse_param)
    {
	param *p;

	putl_code(fp, "#ifndef YYERROR_DECL\n");
	fprintf(fp, "#define YYERROR_DECL() yyerror(");
	for (p = parse_param; p; p = p->next)
	    fprintf(fp, "%s %s%s, ", p->type, p->name, p->type2);
	putl_code(fp, "const char *s)\n");
	putl_code(fp, "#endif\n");

	putl_code(fp, "#ifndef YYERROR_CALL\n");
	puts_code(fp, "#define YYERROR_CALL(msg) yyerror(");

	for (p = parse_param; p; p = p->next)
	    fprintf(fp, "%s, ", p->name);

	putl_code(fp, "msg)\n");
	putl_code(fp, "#endif\n");
    }
    else
    {
	putl_code(fp, "#ifndef YYERROR_DECL\n");
	putl_code(fp, "#define YYERROR_DECL() yyerror(const char *s)\n");
	putl_code(fp, "#endif\n");
	putl_code(fp, "#ifndef YYERROR_CALL\n");
	putl_code(fp, "#define YYERROR_CALL(msg) yyerror(msg)\n");
	putl_code(fp, "#endif\n");
    }
}

static void
free_itemsets(void)
{
    core *cp, *next;

    FREE(state_table);
    for (cp = first_state; cp; cp = next)
    {
	next = cp->next;
	FREE(cp);
    }
}

static void
free_shifts(void)
{
    shifts *sp, *next;

    FREE(shift_table);
    for (sp = first_shift; sp; sp = next)
    {
	next = sp->next;
	FREE(sp);
    }
}

static void
free_reductions(void)
{
    reductions *rp, *next;

    FREE(reduction_table);
    for (rp = first_reduction; rp; rp = next)
    {
	next = rp->next;
	FREE(rp);
    }
}

static void
output_yyerror_call(const char *msg)
{
    FILE *fp = code_file;

    puts_code(fp, "    yyerror(");
    if (parse_param)
    {
	param *p;
	for (p = parse_param; p; p = p->next)
	    fprintf(fp, "%s, ", p->name);
    }
    puts_code(fp, "\"");
    puts_code(fp, msg);
    putl_code(fp, "\");\n");
}

static void
output_externs(FILE * fp, const char *const section[])
{
    int c;
    int i;
    const char *s;

    for (i = 0; (s = section[i]) != 0; ++i)
    {
	if (*s && *s != '#')
	    fputs("extern\t", fp);
	while ((c = *s) != 0)
	{
	    putc(c, fp);
	    ++s;
	}
	if (fp == code_file)
	    ++outline;
	putc('\n', fp);
    }
}

void
output(void)
{
    FILE *fp;

    free_itemsets();
    free_shifts();
    free_reductions();

    if (iflag)
    {
	++outline;
	fprintf(code_file, "#include \"%s\"\n", externs_file_name);
	fp = externs_file;
    }
    else
	fp = code_file;

    output_prefix(iflag ? externs_file : output_file);
    output_pure_parser(fp);
    output_stored_text(fp);
    output_stype(fp);
    output_parse_decl(fp);
    output_lex_decl(fp);
    output_error_decl(fp);
    write_section(fp, xdecls);

    if (iflag)
    {
	output_externs(externs_file, global_vars);
	if (!pure_parser)
	    output_externs(externs_file, impure_vars);
    }

    if (iflag)
    {
	if (dflag)
	{
	    ++outline;
	    fprintf(code_file, "#include \"%s\"\n", defines_file_name);
	}
	else
	    output_defines(externs_file);
    }
    else
    {
	putc_code(code_file, '\n');
	output_defines(code_file);
    }

    if (dflag)
	output_defines(defines_file);

    output_rule_data();
    output_yydefred();
    output_actions();
    free_parser();
    output_debug();
    if (rflag)
    {
	output_prefix(code_file);
	write_section(code_file, xdecls);
	write_section(code_file, tables);
    }
    write_section(code_file, global_vars);
    if (!pure_parser)
    {
	write_section(code_file, impure_vars);
    }
    write_section(code_file, hdr_defs);
    if (!pure_parser)
    {
	write_section(code_file, hdr_vars);
    }
    output_trailing_text();
    write_section(code_file, body_1);
    if (pure_parser)
    {
	write_section(code_file, body_vars);
    }
    write_section(code_file, body_2);
    output_yyerror_call("syntax error");
    write_section(code_file, body_3);
    output_semantic_actions();
    write_section(code_file, trailer);
    output_yyerror_call("yacc stack overflow");
    write_section(code_file, trailer_2);
}

#ifdef NO_LEAKS
void
output_leaks(void)
{
    DO_FREE(tally);
    DO_FREE(width);
    DO_FREE(order);
}
#endif
