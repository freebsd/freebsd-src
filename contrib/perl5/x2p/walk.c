/* $RCSfile: walk.c,v $$Revision: 4.1 $$Date: 92/08/07 18:29:31 $
 *
 *    Copyright (c) 1991-2001, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log:	walk.c,v $
 */

#include "EXTERN.h"
#include "a2p.h"
#include "util.h"

bool exitval = FALSE;
bool realexit = FALSE;
bool saw_getline = FALSE;
bool subretnum = FALSE;
bool saw_FNR = FALSE;
bool saw_argv0 = FALSE;
bool saw_fh = FALSE;
int maxtmp = 0;
char *lparen;
char *rparen;
char *limit;
STR *subs;
STR *curargs = Nullstr;

static void addsemi ( STR *str );
static void emit_split ( STR *str, int level );
static void fixtab ( STR *str, int lvl );
static void numericize ( int node );
static void tab ( STR *str, int lvl );

int prewalk ( int numit, int level, int node, int *numericptr );
STR * walk ( int useval, int level, int node, int *numericptr, int minprec );


STR *
walk(int useval, int level, register int node, int *numericptr, int minprec)
           
          
                  
                
            			/* minimum precedence without parens */
{
    register int len;
    register STR *str;
    register int type;
    register int i;
    register STR *tmpstr;
    STR *tmp2str;
    STR *tmp3str;
    char *t;
    char *d, *s;
    int numarg;
    int numeric = FALSE;
    STR *fstr;
    int prec = P_MAX;		/* assume no parens needed */

    if (!node) {
	*numericptr = 0;
	return str_make("");
    }
    type = ops[node].ival;
    len = type >> 8;
    type &= 255;
    switch (type) {
    case OPROG:
	arymax = 0;
	if (namelist) {
	    while (isalpha(*namelist)) {
		for (d = tokenbuf,s=namelist;
		  isalpha(*s) || isdigit(*s) || *s == '_';
		  *d++ = *s++) ;
		*d = '\0';
		while (*s && !isalpha(*s)) s++;
		namelist = s;
		nameary[++arymax] = savestr(tokenbuf);
	    }
	}
	if (maxfld < arymax)
	    maxfld = arymax;
	opens = str_new(0);
	subs = str_new(0);
	str = walk(0,level,ops[node+1].ival,&numarg,P_MIN);
	if (do_split && need_entire && !absmaxfld)
	    split_to_array = TRUE;
	if (do_split && split_to_array)
	    set_array_base = TRUE;
	if (set_array_base) {
	    str_cat(str,"$[ = 1;\t\t\t# set array base to 1\n");
	}
	if (fswitch && !const_FS)
	    const_FS = fswitch;
	if (saw_FS > 1 || saw_RS)
	    const_FS = 0;
	if (saw_ORS && need_entire)
	    do_chop = TRUE;
	if (fswitch) {
	    str_cat(str,"$FS = '");
	    if (strchr("*+?.[]()|^$\\",fswitch))
		str_cat(str,"\\");
	    sprintf(tokenbuf,"%c",fswitch);
	    str_cat(str,tokenbuf);
	    str_cat(str,"';\t\t# field separator from -F switch\n");
	}
	else if (saw_FS && !const_FS) {
	    str_cat(str,"$FS = ' ';\t\t# set field separator\n");
	}
	if (saw_OFS) {
	    str_cat(str,"$, = ' ';\t\t# set output field separator\n");
	}
	if (saw_ORS) {
	    str_cat(str,"$\\ = \"\\n\";\t\t# set output record separator\n");
	}
	if (saw_argv0) {
	    str_cat(str,"$ARGV0 = $0;\t\t# remember what we ran as\n");
	}
	if (str->str_cur > 20)
	    str_cat(str,"\n");
	if (ops[node+2].ival) {
	    str_scat(str,fstr=walk(0,level,ops[node+2].ival,&numarg,P_MIN));
	    str_free(fstr);
	    str_cat(str,"\n\n");
	}
	fstr = walk(0,level+1,ops[node+3].ival,&numarg,P_MIN);
	if (*fstr->str_ptr) {
	    if (saw_line_op)
		str_cat(str,"line: ");
	    str_cat(str,"while (<>) {\n");
	    tab(str,++level);
	    if (saw_FS && !const_FS)
		do_chop = TRUE;
	    if (do_chop) {
		str_cat(str,"chomp;\t# strip record separator\n");
		tab(str,level);
	    }
	    if (do_split)
		emit_split(str,level);
	    str_scat(str,fstr);
	    str_free(fstr);
	    fixtab(str,--level);
	    str_cat(str,"}\n");
	    if (saw_FNR)
		str_cat(str,"continue {\n    $FNRbase = $. if eof;\n}\n");
	}
	else if (old_awk)
	    str_cat(str,"while (<>) { }		# (no line actions)\n");
	if (ops[node+4].ival) {
	    realexit = TRUE;
	    str_cat(str,"\n");
	    tab(str,level);
	    str_scat(str,fstr=walk(0,level,ops[node+4].ival,&numarg,P_MIN));
	    str_free(fstr);
	    str_cat(str,"\n");
	}
	if (exitval)
	    str_cat(str,"exit $ExitValue;\n");
	if (subs->str_ptr) {
	    str_cat(str,"\n");
	    str_scat(str,subs);
	}
	if (saw_getline) {
	    for (len = 0; len < 4; len++) {
		if (saw_getline & (1 << len)) {
		    sprintf(tokenbuf,"\nsub Getline%d {\n",len);
		    str_cat(str, tokenbuf);
		    if (len & 2) {
			if (do_fancy_opens)
			    str_cat(str,"    &Pick('',@_);\n");
			else
			    str_cat(str,"    ($fh) = @_;\n");
		    }
		    else {
			if (saw_FNR)
			    str_cat(str,"    $FNRbase = $. if eof;\n");
		    }
		    if (len & 1)
			str_cat(str,"    local($_);\n");
		    if (len & 2)
			str_cat(str,
			  "    if ($getline_ok = (($_ = <$fh>) ne ''))");
		    else
			str_cat(str,
			  "    if ($getline_ok = (($_ = <>) ne ''))");
		    str_cat(str, " {\n");
		    level += 2;
		    tab(str,level);
		    i = 0;
		    if (do_chop) {
			i++;
			str_cat(str,"chomp;\t# strip record separator\n");
			tab(str,level);
		    }
		    if (do_split && !(len & 1)) {
			i++;
			emit_split(str,level);
		    }
		    if (!i)
			str_cat(str,";\n");
		    fixtab(str,--level);
		    str_cat(str,"}\n    $_;\n}\n");
		    --level;
		}
	    }
	}
	if (do_fancy_opens) {
	    str_cat(str,"\n\
sub Pick {\n\
    local($mode,$name,$pipe) = @_;\n\
    $fh = $name;\n\
    open($name,$mode.$name.$pipe) unless $opened{$name}++;\n\
}\n\
");
	}
	break;
    case OHUNKS:
	str = walk(0,level,ops[node+1].ival,&numarg,P_MIN);
	str_scat(str,fstr=walk(0,level,ops[node+2].ival,&numarg,P_MIN));
	str_free(fstr);
	if (len == 3) {
	    str_scat(str,fstr=walk(0,level,ops[node+3].ival,&numarg,P_MIN));
	    str_free(fstr);
	}
	else {
	}
	break;
    case ORANGE:
	prec = P_DOTDOT;
	str = walk(1,level,ops[node+1].ival,&numarg,prec+1);
	str_cat(str," .. ");
	str_scat(str,fstr=walk(1,level,ops[node+2].ival,&numarg,prec+1));
	str_free(fstr);
	break;
    case OPAT:
	goto def;
    case OREGEX:
	str = str_new(0);
	str_set(str,"/");
	tmpstr=walk(0,level,ops[node+1].ival,&numarg,P_MIN);
	/* translate \nnn to [\nnn] */
	for (s = tmpstr->str_ptr, d = tokenbuf; *s; s++, d++) {
	    if (*s == '\\' && isdigit(s[1]) && isdigit(s[2]) && isdigit(s[3])){
		*d++ = '[';
		*d++ = *s++;
		*d++ = *s++;
		*d++ = *s++;
		*d++ = *s;
		*d = ']';
	    }
	    else
		*d = *s;
	}
	*d = '\0';
	for (d=tokenbuf; *d; d++)
	    *d += 128;
	str_cat(str,tokenbuf);
	str_free(tmpstr);
	str_cat(str,"/");
	break;
    case OHUNK:
	if (len == 1) {
	    str = str_new(0);
	    str = walk(0,level,oper1(OPRINT,0),&numarg,P_MIN);
	    str_cat(str," if ");
	    str_scat(str,fstr=walk(0,level,ops[node+1].ival,&numarg,P_MIN));
	    str_free(fstr);
	    str_cat(str,";");
	}
	else {
	    tmpstr = walk(0,level,ops[node+1].ival,&numarg,P_MIN);
	    if (*tmpstr->str_ptr) {
		str = str_new(0);
		str_set(str,"if (");
		str_scat(str,tmpstr);
		str_cat(str,") {\n");
		tab(str,++level);
		str_scat(str,fstr=walk(0,level,ops[node+2].ival,&numarg,P_MIN));
		str_free(fstr);
		fixtab(str,--level);
		str_cat(str,"}\n");
		tab(str,level);
	    }
	    else {
		str = walk(0,level,ops[node+2].ival,&numarg,P_MIN);
	    }
	}
	break;
    case OPPAREN:
	str = str_new(0);
	str_set(str,"(");
	str_scat(str,fstr=walk(useval != 0,level,ops[node+1].ival,&numarg,P_MIN));
	str_free(fstr);
	str_cat(str,")");
	break;
    case OPANDAND:
	prec = P_ANDAND;
	str = walk(1,level,ops[node+1].ival,&numarg,prec);
	str_cat(str," && ");
	str_scat(str,fstr=walk(1,level,ops[node+2].ival,&numarg,prec+1));
	str_free(fstr);
	str_scat(str,fstr=walk(1,level,ops[node+3].ival,&numarg,prec+1));
	str_free(fstr);
	break;
    case OPOROR:
	prec = P_OROR;
	str = walk(1,level,ops[node+1].ival,&numarg,prec);
	str_cat(str," || ");
	str_scat(str,fstr=walk(1,level,ops[node+2].ival,&numarg,prec+1));
	str_free(fstr);
	str_scat(str,fstr=walk(1,level,ops[node+3].ival,&numarg,prec+1));
	str_free(fstr);
	break;
    case OPNOT:
	prec = P_UNARY;
	str = str_new(0);
	str_set(str,"!");
	str_scat(str,fstr=walk(1,level,ops[node+1].ival,&numarg,prec));
	str_free(fstr);
	break;
    case OCOND:
	prec = P_COND;
	str = walk(1,level,ops[node+1].ival,&numarg,prec);
	str_cat(str," ? ");
	str_scat(str,fstr=walk(1,level,ops[node+2].ival,&numarg,prec+1));
	str_free(fstr);
	str_cat(str," : ");
	str_scat(str,fstr=walk(1,level,ops[node+3].ival,&numarg,prec+1));
	str_free(fstr);
	break;
    case OCPAREN:
	str = str_new(0);
	str_set(str,"(");
	str_scat(str,fstr=walk(useval != 0,level,ops[node+1].ival,&numarg,P_MIN));
	str_free(fstr);
	numeric |= numarg;
	str_cat(str,")");
	break;
    case OCANDAND:
	prec = P_ANDAND;
	str = walk(1,level,ops[node+1].ival,&numarg,prec);
	numeric = 1;
	str_cat(str," && ");
	str_scat(str,fstr=walk(1,level,ops[node+2].ival,&numarg,prec+1));
	str_free(fstr);
	str_scat(str,fstr=walk(1,level,ops[node+3].ival,&numarg,prec+1));
	str_free(fstr);
	break;
    case OCOROR:
	prec = P_OROR;
	str = walk(1,level,ops[node+1].ival,&numarg,prec);
	numeric = 1;
	str_cat(str," || ");
	str_scat(str,fstr=walk(1,level,ops[node+2].ival,&numarg,prec+1));
	str_free(fstr);
	str_scat(str,fstr=walk(1,level,ops[node+3].ival,&numarg,prec+1));
	str_free(fstr);
	break;
    case OCNOT:
	prec = P_UNARY;
	str = str_new(0);
	str_set(str,"!");
	str_scat(str,fstr=walk(1,level,ops[node+1].ival,&numarg,prec));
	str_free(fstr);
	numeric = 1;
	break;
    case ORELOP:
	prec = P_REL;
	str = walk(1,level,ops[node+2].ival,&numarg,prec+1);
	numeric |= numarg;
	tmpstr = walk(0,level,ops[node+1].ival,&numarg,P_MIN);
	tmp2str = walk(1,level,ops[node+3].ival,&numarg,prec+1);
	numeric |= numarg;
	if (!numeric ||
	 (!numarg && (*tmp2str->str_ptr == '"' || *tmp2str->str_ptr == '\''))) {
	    t = tmpstr->str_ptr;
	    if (strEQ(t,"=="))
		str_set(tmpstr,"eq");
	    else if (strEQ(t,"!="))
		str_set(tmpstr,"ne");
	    else if (strEQ(t,"<"))
		str_set(tmpstr,"lt");
	    else if (strEQ(t,"<="))
		str_set(tmpstr,"le");
	    else if (strEQ(t,">"))
		str_set(tmpstr,"gt");
	    else if (strEQ(t,">="))
		str_set(tmpstr,"ge");
	    if (!strchr(tmpstr->str_ptr,'\'') && !strchr(tmpstr->str_ptr,'"') &&
	      !strchr(tmp2str->str_ptr,'\'') && !strchr(tmp2str->str_ptr,'"') )
		numeric |= 2;
	}
	if (numeric & 2) {
	    if (numeric & 1)		/* numeric is very good guess */
		str_cat(str," ");
	    else
		str_cat(str,"\377");
	    numeric = 1;
	}
	else
	    str_cat(str," ");
	str_scat(str,tmpstr);
	str_free(tmpstr);
	str_cat(str," ");
	str_scat(str,tmp2str);
	str_free(tmp2str);
	numeric = 1;
	break;
    case ORPAREN:
	str = str_new(0);
	str_set(str,"(");
	str_scat(str,fstr=walk(useval != 0,level,ops[node+1].ival,&numarg,P_MIN));
	str_free(fstr);
	numeric |= numarg;
	str_cat(str,")");
	break;
    case OMATCHOP:
	prec = P_MATCH;
	str = walk(1,level,ops[node+2].ival,&numarg,prec+1);
	str_cat(str," ");
	tmpstr = walk(0,level,ops[node+1].ival,&numarg,P_MIN);
	if (strEQ(tmpstr->str_ptr,"~"))
	    str_cat(str,"=~");
	else {
	    str_scat(str,tmpstr);
	    str_free(tmpstr);
	}
	str_cat(str," ");
	str_scat(str,fstr=walk(1,level,ops[node+3].ival,&numarg,prec+1));
	str_free(fstr);
	numeric = 1;
	break;
    case OMPAREN:
	str = str_new(0);
	str_set(str,"(");
	str_scat(str,
	  fstr=walk(useval != 0,level,ops[node+1].ival,&numarg,P_MIN));
	str_free(fstr);
	numeric |= numarg;
	str_cat(str,")");
	break;
    case OCONCAT:
	prec = P_ADD;
	type = ops[ops[node+1].ival].ival & 255;
	str = walk(1,level,ops[node+1].ival,&numarg,prec+(type != OCONCAT));
	str_cat(str," . ");
	type = ops[ops[node+2].ival].ival & 255;
	str_scat(str,
	  fstr=walk(1,level,ops[node+2].ival,&numarg,prec+(type != OCONCAT)));
	str_free(fstr);
	break;
    case OASSIGN:
	prec = P_ASSIGN;
	str = walk(0,level,ops[node+2].ival,&numarg,prec+1);
	str_cat(str," ");
	tmpstr = walk(0,level,ops[node+1].ival,&numarg,P_MIN);
	str_scat(str,tmpstr);
	if (str_len(tmpstr) > 1)
	    numeric = 1;
	str_free(tmpstr);
	str_cat(str," ");
	str_scat(str,fstr=walk(1,level,ops[node+3].ival,&numarg,prec));
	str_free(fstr);
	numeric |= numarg;
	if (strEQ(str->str_ptr,"$/ = ''"))
	    str_set(str, "$/ = \"\\n\\n\"");
	break;
    case OADD:
	prec = P_ADD;
	str = walk(1,level,ops[node+1].ival,&numarg,prec);
	str_cat(str," + ");
	str_scat(str,fstr=walk(1,level,ops[node+2].ival,&numarg,prec+1));
	str_free(fstr);
	numeric = 1;
	break;
    case OSUBTRACT:
	prec = P_ADD;
	str = walk(1,level,ops[node+1].ival,&numarg,prec);
	str_cat(str," - ");
	str_scat(str,fstr=walk(1,level,ops[node+2].ival,&numarg,prec+1));
	str_free(fstr);
	numeric = 1;
	break;
    case OMULT:
	prec = P_MUL;
	str = walk(1,level,ops[node+1].ival,&numarg,prec);
	str_cat(str," * ");
	str_scat(str,fstr=walk(1,level,ops[node+2].ival,&numarg,prec+1));
	str_free(fstr);
	numeric = 1;
	break;
    case ODIV:
	prec = P_MUL;
	str = walk(1,level,ops[node+1].ival,&numarg,prec);
	str_cat(str," / ");
	str_scat(str,fstr=walk(1,level,ops[node+2].ival,&numarg,prec+1));
	str_free(fstr);
	numeric = 1;
	break;
    case OPOW:
	prec = P_POW;
	str = walk(1,level,ops[node+1].ival,&numarg,prec+1);
	str_cat(str," ** ");
	str_scat(str,fstr=walk(1,level,ops[node+2].ival,&numarg,prec));
	str_free(fstr);
	numeric = 1;
	break;
    case OMOD:
	prec = P_MUL;
	str = walk(1,level,ops[node+1].ival,&numarg,prec);
	str_cat(str," % ");
	str_scat(str,fstr=walk(1,level,ops[node+2].ival,&numarg,prec+1));
	str_free(fstr);
	numeric = 1;
	break;
    case OPOSTINCR:
	prec = P_AUTO;
	str = walk(1,level,ops[node+1].ival,&numarg,prec+1);
	str_cat(str,"++");
	numeric = 1;
	break;
    case OPOSTDECR:
	prec = P_AUTO;
	str = walk(1,level,ops[node+1].ival,&numarg,prec+1);
	str_cat(str,"--");
	numeric = 1;
	break;
    case OPREINCR:
	prec = P_AUTO;
	str = str_new(0);
	str_set(str,"++");
	str_scat(str,fstr=walk(1,level,ops[node+1].ival,&numarg,prec+1));
	str_free(fstr);
	numeric = 1;
	break;
    case OPREDECR:
	prec = P_AUTO;
	str = str_new(0);
	str_set(str,"--");
	str_scat(str,fstr=walk(1,level,ops[node+1].ival,&numarg,prec+1));
	str_free(fstr);
	numeric = 1;
	break;
    case OUMINUS:
	prec = P_UNARY;
	str = str_new(0);
	str_set(str,"-");
	str_scat(str,fstr=walk(1,level,ops[node+1].ival,&numarg,prec));
	str_free(fstr);
	numeric = 1;
	break;
    case OUPLUS:
	numeric = 1;
	goto def;
    case OPAREN:
	str = str_new(0);
	str_set(str,"(");
	str_scat(str,
	  fstr=walk(useval != 0,level,ops[node+1].ival,&numarg,P_MIN));
	str_free(fstr);
	str_cat(str,")");
	numeric |= numarg;
	break;
    case OGETLINE:
	str = str_new(0);
	if (useval)
	    str_cat(str,"(");
	if (len > 0) {
	    str_scat(str,fstr=walk(1,level,ops[node+1].ival,&numarg,P_MIN));
	    if (!*fstr->str_ptr) {
		str_cat(str,"$_");
		len = 2;		/* a legal fiction */
	    }
	    str_free(fstr);
	}
	else
	    str_cat(str,"$_");
	if (len > 1) {
	    tmpstr=walk(1,level,ops[node+3].ival,&numarg,P_MIN);
	    fstr=walk(1,level,ops[node+2].ival,&numarg,P_MIN);
	    if (!do_fancy_opens) {
		t = tmpstr->str_ptr;
		if (*t == '"' || *t == '\'')
		    t = cpytill(tokenbuf,t+1,*t);
		else
		    fatal("Internal error: OGETLINE %s", t);
		d = savestr(t);
		s = savestr(tokenbuf);
		for (t = tokenbuf; *t; t++) {
		    *t &= 127;
		    if (islower(*t))
			*t = toupper(*t);
		    if (!isalpha(*t) && !isdigit(*t))
			*t = '_';
		}
		if (!strchr(tokenbuf,'_'))
		    strcpy(t,"_FH");
		tmp3str = hfetch(symtab,tokenbuf);
		if (!tmp3str) {
		    do_opens = TRUE;
		    str_cat(opens,"open(");
		    str_cat(opens,tokenbuf);
		    str_cat(opens,", ");
		    d[1] = '\0';
		    str_cat(opens,d);
		    str_cat(opens,tmpstr->str_ptr+1);
		    opens->str_cur--;
		    if (*fstr->str_ptr == '|')
			str_cat(opens,"|");
		    str_cat(opens,d);
		    if (*fstr->str_ptr == '|')
			str_cat(opens,") || die 'Cannot pipe from \"");
		    else
			str_cat(opens,") || die 'Cannot open file \"");
		    if (*d == '"')
			str_cat(opens,"'.\"");
		    str_cat(opens,s);
		    if (*d == '"')
			str_cat(opens,"\".'");
		    str_cat(opens,"\".';\n");
		    hstore(symtab,tokenbuf,str_make("x"));
		}
		safefree(s);
		safefree(d);
		str_set(tmpstr,"'");
		str_cat(tmpstr,tokenbuf);
		str_cat(tmpstr,"'");
	    }
	    if (*fstr->str_ptr == '|')
		str_cat(tmpstr,", '|'");
	    str_free(fstr);
	}
	else
	    tmpstr = str_make("");
	sprintf(tokenbuf," = &Getline%d(%s)",len,tmpstr->str_ptr);
	str_cat(str,tokenbuf); 
	str_free(tmpstr);
	if (useval)
	    str_cat(str,",$getline_ok)");
	saw_getline |= 1 << len;
	break;
    case OSPRINTF:
	str = str_new(0);
	str_set(str,"sprintf(");
	str_scat(str,fstr=walk(1,level,ops[node+1].ival,&numarg,P_MIN));
	str_free(fstr);
	str_cat(str,")");
	break;
    case OSUBSTR:
	str = str_new(0);
	str_set(str,"substr(");
	str_scat(str,fstr=walk(1,level,ops[node+1].ival,&numarg,P_COMMA+1));
	str_free(fstr);
	str_cat(str,", ");
	str_scat(str,fstr=walk(1,level,ops[node+2].ival,&numarg,P_COMMA+1));
	str_free(fstr);
	str_cat(str,", ");
	if (len == 3) {
	    str_scat(str,fstr=walk(1,level,ops[node+3].ival,&numarg,P_COMMA+1));
	    str_free(fstr);
	}
	else
	    str_cat(str,"999999");
	str_cat(str,")");
	break;
    case OSTRING:
	str = str_new(0);
	str_set(str,ops[node+1].cval);
	break;
    case OSPLIT:
	str = str_new(0);
	limit = ", 9999)";
	numeric = 1;
	tmpstr = walk(1,level,ops[node+2].ival,&numarg,P_MIN);
	if (useval)
	    str_set(str,"(@");
	else
	    str_set(str,"@");
	str_scat(str,tmpstr);
	str_cat(str," = split(");
	if (len == 3) {
	    fstr = walk(1,level,ops[node+3].ival,&numarg,P_COMMA+1);
	    if (str_len(fstr) == 3 && *fstr->str_ptr == '\'') {
		i = fstr->str_ptr[1] & 127;
		if (strchr("*+?.[]()|^$\\",i))
		    sprintf(tokenbuf,"/\\%c/",i);
		else if (i == ' ')
		    sprintf(tokenbuf,"' '");
		else
		    sprintf(tokenbuf,"/%c/",i);
		str_cat(str,tokenbuf);
	    }
	    else
		str_scat(str,fstr);
	    str_free(fstr);
	}
	else if (const_FS) {
	    sprintf(tokenbuf,"/[%c\\n]/",const_FS);
	    str_cat(str,tokenbuf);
	}
	else if (saw_FS)
	    str_cat(str,"$FS");
	else {
	    str_cat(str,"' '");
	    limit = ")";
	}
	str_cat(str,", ");
	str_scat(str,fstr=walk(1,level,ops[node+1].ival,&numarg,P_COMMA+1));
	str_free(fstr);
	str_cat(str,limit);
	if (useval) {
	    str_cat(str,")");
	}
	str_free(tmpstr);
	break;
    case OINDEX:
	str = str_new(0);
	str_set(str,"index(");
	str_scat(str,fstr=walk(1,level,ops[node+1].ival,&numarg,P_COMMA+1));
	str_free(fstr);
	str_cat(str,", ");
	str_scat(str,fstr=walk(1,level,ops[node+2].ival,&numarg,P_COMMA+1));
	str_free(fstr);
	str_cat(str,")");
	numeric = 1;
	break;
    case OMATCH:
	str = str_new(0);
	prec = P_ANDAND;
	str_scat(str,fstr=walk(1,level,ops[node+1].ival,&numarg,P_MATCH+1));
	str_free(fstr);
	str_cat(str," =~ ");
	str_scat(str,fstr=walk(1,level,ops[node+2].ival,&numarg,P_MATCH+1));
	str_free(fstr);
	str_cat(str," && ($RLENGTH = length($&), $RSTART = length($`)+1)");
	numeric = 1;
	break;
    case OUSERDEF:
	str = str_new(0);
	subretnum = FALSE;
	fstr=walk(1,level-1,ops[node+2].ival,&numarg,P_MIN);
	curargs = str_new(0);
	str_sset(curargs,fstr);
	str_cat(curargs,",");
	tmp2str=walk(1,level,ops[node+5].ival,&numarg,P_MIN);
	str_free(curargs);
	curargs = Nullstr;
	level--;
	subretnum |= numarg;
	s = Nullch;
	t = tmp2str->str_ptr;
	while (t = instr(t,"return "))
	    s = t++;
	if (s) {
	    i = 0;
	    for (t = s+7; *t; t++) {
		if (*t == ';' || *t == '}')
		    i++;
	    }
	    if (i == 1) {
		strcpy(s,s+7);
		tmp2str->str_cur -= 7;
	    }
	}
	str_set(str,"\n");
	tab(str,level);
	str_cat(str,"sub ");
	str_scat(str,tmpstr=walk(1,level,ops[node+1].ival,&numarg,P_MIN));
	str_cat(str," {\n");
	tab(str,++level);
	if (fstr->str_cur) {
	    str_cat(str,"local(");
	    str_scat(str,fstr);
	    str_cat(str,") = @_;");
	}
	str_free(fstr);
	str_scat(str,fstr=walk(1,level,ops[node+3].ival,&numarg,P_MIN));
	str_free(fstr);
	fixtab(str,level);
	str_scat(str,fstr=walk(1,level,ops[node+4].ival,&numarg,P_MIN));
	str_free(fstr);
	fixtab(str,level);
	str_scat(str,tmp2str);
	str_free(tmp2str);
	fixtab(str,--level);
	str_cat(str,"}\n");
	tab(str,level);
	str_scat(subs,str);
	str_set(str,"");
	str_cat(tmpstr,"(");
	tmp2str = str_new(0);
	if (subretnum)
	    str_set(tmp2str,"1");
	hstore(symtab,tmpstr->str_ptr,tmp2str);
	str_free(tmpstr);
	level++;
	break;
    case ORETURN:
	str = str_new(0);
	if (len > 0) {
	    str_cat(str,"return ");
	    str_scat(str,fstr=walk(1,level,ops[node+1].ival,&numarg,P_UNI+1));
	    str_free(fstr);
	    if (numarg)
		subretnum = TRUE;
	}
	else
	    str_cat(str,"return");
	break;
    case OUSERFUN:
	str = str_new(0);
	str_set(str,"&");
	str_scat(str,fstr=walk(1,level,ops[node+1].ival,&numarg,P_MIN));
	str_free(fstr);
	str_cat(str,"(");
	tmpstr = hfetch(symtab,str->str_ptr+3);
	if (tmpstr && tmpstr->str_ptr)
	    numeric |= atoi(tmpstr->str_ptr);
	str_scat(str,fstr=walk(1,level,ops[node+2].ival,&numarg,P_MIN));
	str_free(fstr);
	str_cat(str,")");
	break;
    case OGSUB:
    case OSUB:
	if (type == OGSUB)
	    s = "g";
	else
	    s = "";
	str = str_new(0);
	tmpstr = str_new(0);
	i = 0;
	if (len == 3) {
	    tmpstr = walk(1,level,ops[node+3].ival,&numarg,P_MATCH+1);
	    if (strNE(tmpstr->str_ptr,"$_")) {
		str_cat(tmpstr, " =~ s");
		i++;
	    }
	    else
		str_set(tmpstr, "s");
	}
	else
	    str_set(tmpstr, "s");
	type = ops[ops[node+2].ival].ival;
	len = type >> 8;
	type &= 255;
	tmp3str = str_new(0);
	if (type == OSTR) {
	    tmp2str=walk(1,level,ops[ops[node+2].ival+1].ival,&numarg,P_MIN);
	    for (t = tmp2str->str_ptr, d=tokenbuf; *t; d++,t++) {
		if (*t == '&')
		    *d++ = '$' + 128;
		else if (*t == '$')
		    *d++ = '\\' + 128;
		*d = *t + 128;
	    }
	    *d = '\0';
	    str_set(tmp2str,tokenbuf);
	}
	else {
	    tmp2str=walk(1,level,ops[node+2].ival,&numarg,P_MIN);
	    str_set(tmp3str,"($s_ = '\"'.(");
	    str_scat(tmp3str,tmp2str);
	    str_cat(tmp3str,").'\"') =~ s/&/\\$&/g, ");
	    str_set(tmp2str,"eval $s_");
	    s = (char*)(*s == 'g' ? "ge" : "e");
	    i++;
	}
	type = ops[ops[node+1].ival].ival;
	len = type >> 8;
	type &= 255;
	fstr=walk(1,level,ops[node+1].ival,&numarg,P_MIN);
	if (type == OREGEX) {
	    if (useval && i)
		str_cat(str,"(");
	    str_scat(str,tmp3str);
	    str_scat(str,tmpstr);
	    str_scat(str,fstr);
	    str_scat(str,tmp2str);
	    str_cat(str,"/");
	    str_cat(str,s);
	}
	else if ((type == OFLD && !split_to_array) || (type == OVAR && len == 1)) {
	    if (useval && i)
		str_cat(str,"(");
	    str_scat(str,tmp3str);
	    str_scat(str,tmpstr);
	    str_cat(str,"/");
	    str_scat(str,fstr);
	    str_cat(str,"/");
	    str_scat(str,tmp2str);
	    str_cat(str,"/");
	    str_cat(str,s);
	}
	else {
	    i++;
	    if (useval)
		str_cat(str,"(");
	    str_cat(str,"$s = ");
	    str_scat(str,fstr);
	    str_cat(str,", ");
	    str_scat(str,tmp3str);
	    str_scat(str,tmpstr);
	    str_cat(str,"/$s/");
	    str_scat(str,tmp2str);
	    str_cat(str,"/");
	    str_cat(str,s);
	}
	if (useval && i)
	    str_cat(str,")");
	str_free(fstr);
	str_free(tmpstr);
	str_free(tmp2str);
	str_free(tmp3str);
	numeric = 1;
	break;
    case ONUM:
	str = walk(1,level,ops[node+1].ival,&numarg,P_MIN);
	numeric = 1;
	break;
    case OSTR:
	tmpstr = walk(1,level,ops[node+1].ival,&numarg,P_MIN);
	s = "'";
	for (t = tmpstr->str_ptr, d=tokenbuf; *t; d++,t++) {
	    if (*t == '\'')
		s = "\"";
	    else if (*t == '\\') {
		s = "\"";
		*d++ = *t++ + 128;
		switch (*t) {
		case '\\': case '"': case 'n': case 't': case '$':
		    break;
		default:	/* hide this from perl */
		    *d++ = '\\' + 128;
		}
	    }
	    *d = *t + 128;
	}
	*d = '\0';
	str = str_new(0);
	str_set(str,s);
	str_cat(str,tokenbuf);
	str_free(tmpstr);
	str_cat(str,s);
	break;
    case ODEFINED:
	prec = P_UNI;
	str = str_new(0);
	str_set(str,"defined $");
	goto addvar;
    case ODELETE:
	str = str_new(0);
	str_set(str,"delete $");
	goto addvar;
    case OSTAR:
	str = str_new(0);
	str_set(str,"*");
	goto addvar;
    case OVAR:
	str = str_new(0);
	str_set(str,"$");
      addvar:
	str_scat(str,tmpstr=walk(1,level,ops[node+1].ival,&numarg,P_MIN));
	if (len == 1) {
	    tmp2str = hfetch(symtab,tmpstr->str_ptr);
	    if (tmp2str && atoi(tmp2str->str_ptr))
		numeric = 2;
	    if (strEQ(str->str_ptr,"$FNR")) {
		numeric = 1;
		saw_FNR++;
		str_set(str,"($.-$FNRbase)");
	    }
	    else if (strEQ(str->str_ptr,"$NR")) {
		numeric = 1;
		str_set(str,"$.");
	    }
	    else if (strEQ(str->str_ptr,"$NF")) {
		numeric = 1;
		str_set(str,"$#Fld");
	    }
	    else if (strEQ(str->str_ptr,"$0"))
		str_set(str,"$_");
	    else if (strEQ(str->str_ptr,"$ARGC"))
		str_set(str,"($#ARGV+1)");
	}
	else {
#ifdef NOTDEF
	    if (curargs) {
		sprintf(tokenbuf,"$%s,",tmpstr->str_ptr);
	???	if (instr(curargs->str_ptr,tokenbuf))
		    str_cat(str,"\377");	/* can't translate yet */
	    }
#endif
	    str_cat(tmpstr,"[]");
	    tmp2str = hfetch(symtab,tmpstr->str_ptr);
	    if (tmp2str && atoi(tmp2str->str_ptr))
		str_cat(str,"[");
	    else
		str_cat(str,"{");
	    str_scat(str,fstr=walk(1,level,ops[node+2].ival,&numarg,P_MIN));
	    str_free(fstr);
	    if (strEQ(str->str_ptr,"$ARGV[0")) {
		str_set(str,"$ARGV0");
		saw_argv0++;
	    }
	    else {
		if (tmp2str && atoi(tmp2str->str_ptr))
		    strcpy(tokenbuf,"]");
		else
		    strcpy(tokenbuf,"}");
		*tokenbuf += 128;
		str_cat(str,tokenbuf);
	    }
	}
	str_free(tmpstr);
	break;
    case OFLD:
	str = str_new(0);
	if (split_to_array) {
	    str_set(str,"$Fld");
	    str_cat(str,"[");
	    str_scat(str,fstr=walk(1,level,ops[node+1].ival,&numarg,P_MIN));
	    str_free(fstr);
	    str_cat(str,"]");
	}
	else {
	    i = atoi(walk(1,level,ops[node+1].ival,&numarg,P_MIN)->str_ptr);
	    if (i <= arymax)
		sprintf(tokenbuf,"$%s",nameary[i]);
	    else
		sprintf(tokenbuf,"$Fld%d",i);
	    str_set(str,tokenbuf);
	}
	break;
    case OVFLD:
	str = str_new(0);
	str_set(str,"$Fld[");
	i = ops[node+1].ival;
	if ((ops[i].ival & 255) == OPAREN)
	    i = ops[i+1].ival;
	tmpstr=walk(1,level,i,&numarg,P_MIN);
	str_scat(str,tmpstr);
	str_free(tmpstr);
	str_cat(str,"]");
	break;
    case OJUNK:
	goto def;
    case OSNEWLINE:
	str = str_new(2);
	str_set(str,";\n");
	tab(str,level);
	break;
    case ONEWLINE:
	str = str_new(1);
	str_set(str,"\n");
	tab(str,level);
	break;
    case OSCOMMENT:
	str = str_new(0);
	str_set(str,";");
	tmpstr = walk(0,level,ops[node+1].ival,&numarg,P_MIN);
	for (s = tmpstr->str_ptr; *s && *s != '\n'; s++)
	    *s += 128;
	str_scat(str,tmpstr);
	str_free(tmpstr);
	tab(str,level);
	break;
    case OCOMMENT:
	str = str_new(0);
	tmpstr = walk(0,level,ops[node+1].ival,&numarg,P_MIN);
	for (s = tmpstr->str_ptr; *s && *s != '\n'; s++)
	    *s += 128;
	str_scat(str,tmpstr);
	str_free(tmpstr);
	tab(str,level);
	break;
    case OCOMMA:
	prec = P_COMMA;
	str = walk(1,level,ops[node+1].ival,&numarg,prec);
	str_cat(str,", ");
	str_scat(str,fstr=walk(1,level,ops[node+2].ival,&numarg,P_MIN));
	str_free(fstr);
	str_scat(str,fstr=walk(1,level,ops[node+3].ival,&numarg,prec+1));
	str_free(fstr);
	break;
    case OSEMICOLON:
	str = str_new(1);
	str_set(str,";\n");
	tab(str,level);
	break;
    case OSTATES:
	str = walk(0,level,ops[node+1].ival,&numarg,P_MIN);
	str_scat(str,fstr=walk(0,level,ops[node+2].ival,&numarg,P_MIN));
	str_free(fstr);
	break;
    case OSTATE:
	str = str_new(0);
	if (len >= 1) {
	    str_scat(str,fstr=walk(0,level,ops[node+1].ival,&numarg,P_MIN));
	    str_free(fstr);
	    if (len >= 2) {
		tmpstr = walk(0,level,ops[node+2].ival,&numarg,P_MIN);
		if (*tmpstr->str_ptr == ';') {
		    addsemi(str);
		    str_cat(str,tmpstr->str_ptr+1);
		}
		str_free(tmpstr);
	    }
	}
	break;
    case OCLOSE:
	str = str_make("close(");
	tmpstr = walk(1,level,ops[node+1].ival,&numarg,P_MIN);
	if (!do_fancy_opens) {
	    t = tmpstr->str_ptr;
	    if (*t == '"' || *t == '\'')
		t = cpytill(tokenbuf,t+1,*t);
	    else
		fatal("Internal error: OCLOSE %s",t);
	    s = savestr(tokenbuf);
	    for (t = tokenbuf; *t; t++) {
		*t &= 127;
		if (islower(*t))
		    *t = toupper(*t);
		if (!isalpha(*t) && !isdigit(*t))
		    *t = '_';
	    }
	    if (!strchr(tokenbuf,'_'))
		strcpy(t,"_FH");
	    str_free(tmpstr);
	    safefree(s);
	    str_set(str,"close ");
	    str_cat(str,tokenbuf);
	}
	else {
	    sprintf(tokenbuf,"delete $opened{%s} && close(%s)",
	       tmpstr->str_ptr, tmpstr->str_ptr);
	    str_free(tmpstr);
	    str_set(str,tokenbuf);
	}
	break;
    case OPRINTF:
    case OPRINT:
	lparen = "";	/* set to parens if necessary */
	rparen = "";
	str = str_new(0);
	if (len == 3) {		/* output redirection */
	    tmpstr = walk(1,level,ops[node+3].ival,&numarg,P_MIN);
	    tmp2str = walk(1,level,ops[node+2].ival,&numarg,P_MIN);
	    if (!do_fancy_opens) {
		t = tmpstr->str_ptr;
		if (*t == '"' || *t == '\'')
		    t = cpytill(tokenbuf,t+1,*t);
		else
		    fatal("Internal error: OPRINT");
		d = savestr(t);
		s = savestr(tokenbuf);
		for (t = tokenbuf; *t; t++) {
		    *t &= 127;
		    if (islower(*t))
			*t = toupper(*t);
		    if (!isalpha(*t) && !isdigit(*t))
			*t = '_';
		}
		if (!strchr(tokenbuf,'_'))
		    strcpy(t,"_FH");
		tmp3str = hfetch(symtab,tokenbuf);
		if (!tmp3str) {
		    str_cat(opens,"open(");
		    str_cat(opens,tokenbuf);
		    str_cat(opens,", ");
		    d[1] = '\0';
		    str_cat(opens,d);
		    str_scat(opens,tmp2str);
		    str_cat(opens,tmpstr->str_ptr+1);
		    if (*tmp2str->str_ptr == '|')
			str_cat(opens,") || die 'Cannot pipe to \"");
		    else
			str_cat(opens,") || die 'Cannot create file \"");
		    if (*d == '"')
			str_cat(opens,"'.\"");
		    str_cat(opens,s);
		    if (*d == '"')
			str_cat(opens,"\".'");
		    str_cat(opens,"\".';\n");
		    hstore(symtab,tokenbuf,str_make("x"));
		}
		str_free(tmpstr);
		str_free(tmp2str);
		safefree(s);
		safefree(d);
	    }
	    else {
		sprintf(tokenbuf,"&Pick('%s', %s) &&\n",
		   tmp2str->str_ptr, tmpstr->str_ptr);
		str_cat(str,tokenbuf);
		tab(str,level+1);
		strcpy(tokenbuf,"$fh");
		str_free(tmpstr);
		str_free(tmp2str);
		lparen = "(";
		rparen = ")";
	    }
	}
	else
	    strcpy(tokenbuf,"");
	str_cat(str,lparen);	/* may be null */
	if (type == OPRINTF)
	    str_cat(str,"printf");
	else
	    str_cat(str,"print");
	saw_fh = 0;
	if (len == 3 || do_fancy_opens) {
	    if (*tokenbuf) {
		str_cat(str," ");
		saw_fh = 1;
	    }
	    str_cat(str,tokenbuf);
	}
	tmpstr = walk(1+(type==OPRINT),level,ops[node+1].ival,&numarg,P_MIN);
	if (!*tmpstr->str_ptr && lval_field) {
	    t = (char*)(saw_OFS ? "$," : "' '");
	    if (split_to_array) {
		sprintf(tokenbuf,"join(%s,@Fld)",t);
		str_cat(tmpstr,tokenbuf);
	    }
	    else {
		for (i = 1; i < maxfld; i++) {
		    if (i <= arymax)
			sprintf(tokenbuf,"$%s, ",nameary[i]);
		    else
			sprintf(tokenbuf,"$Fld%d, ",i);
		    str_cat(tmpstr,tokenbuf);
		}
		if (maxfld <= arymax)
		    sprintf(tokenbuf,"$%s",nameary[maxfld]);
		else
		    sprintf(tokenbuf,"$Fld%d",maxfld);
		str_cat(tmpstr,tokenbuf);
	    }
	}
	if (*tmpstr->str_ptr) {
	    str_cat(str," ");
	    if (!saw_fh && *tmpstr->str_ptr == '(') {
		str_cat(str,"(");
		str_scat(str,tmpstr);
		str_cat(str,")");
	    }
	    else
		str_scat(str,tmpstr);
	}
	else {
	    str_cat(str," $_");
	}
	str_cat(str,rparen);	/* may be null */
	str_free(tmpstr);
	break;
    case ORAND:
	str = str_make("rand(1)");
	break;
    case OSRAND:
	str = str_make("srand(");
	goto maybe0;
    case OATAN2:
	str = str_make("atan2(");
	goto maybe0;
    case OSIN:
	str = str_make("sin(");
	goto maybe0;
    case OCOS:
	str = str_make("cos(");
	goto maybe0;
    case OSYSTEM:
	str = str_make("system(");
	goto maybe0;
    case OLENGTH:
	str = str_make("length(");
	goto maybe0;
    case OLOG:
	str = str_make("log(");
	goto maybe0;
    case OEXP:
	str = str_make("exp(");
	goto maybe0;
    case OSQRT:
	str = str_make("sqrt(");
	goto maybe0;
    case OINT:
	str = str_make("int(");
      maybe0:
	numeric = 1;
	if (len > 0)
	    tmpstr = walk(1,level,ops[node+1].ival,&numarg,P_MIN);
	else
	    tmpstr = str_new(0);
	if (!tmpstr->str_ptr || !*tmpstr->str_ptr) {
	    if (lval_field) {
		t = (char*)(saw_OFS ? "$," : "' '");
		if (split_to_array) {
		    sprintf(tokenbuf,"join(%s,@Fld)",t);
		    str_cat(tmpstr,tokenbuf);
		}
		else {
		    sprintf(tokenbuf,"join(%s, ",t);
		    str_cat(tmpstr,tokenbuf);
		    for (i = 1; i < maxfld; i++) {
			if (i <= arymax)
			    sprintf(tokenbuf,"$%s,",nameary[i]);
			else
			    sprintf(tokenbuf,"$Fld%d,",i);
			str_cat(tmpstr,tokenbuf);
		    }
		    if (maxfld <= arymax)
			sprintf(tokenbuf,"$%s)",nameary[maxfld]);
		    else
			sprintf(tokenbuf,"$Fld%d)",maxfld);
		    str_cat(tmpstr,tokenbuf);
		}
	    }
	    else
		str_cat(tmpstr,"$_");
	}
	if (strEQ(tmpstr->str_ptr,"$_")) {
	    if (type == OLENGTH && !do_chop) {
		str = str_make("(length(");
		str_cat(tmpstr,") - 1");
	    }
	}
	str_scat(str,tmpstr);
	str_free(tmpstr);
	str_cat(str,")");
	break;
    case OBREAK:
	str = str_new(0);
	str_set(str,"last");
	break;
    case ONEXT:
	str = str_new(0);
	str_set(str,"next line");
	break;
    case OEXIT:
	str = str_new(0);
	if (realexit) {
	    prec = P_UNI;
	    str_set(str,"exit");
	    if (len == 1) {
		str_cat(str," ");
		exitval = TRUE;
		str_scat(str,
		  fstr=walk(1,level,ops[node+1].ival,&numarg,prec+1));
		str_free(fstr);
	    }
	}
	else {
	    if (len == 1) {
		str_set(str,"$ExitValue = ");
		exitval = TRUE;
		str_scat(str,
		  fstr=walk(1,level,ops[node+1].ival,&numarg,P_ASSIGN));
		str_free(fstr);
		str_cat(str,"; ");
	    }
	    str_cat(str,"last line");
	}
	break;
    case OCONTINUE:
	str = str_new(0);
	str_set(str,"next");
	break;
    case OREDIR:
	goto def;
    case OIF:
	str = str_new(0);
	str_set(str,"if (");
	str_scat(str,fstr=walk(1,level,ops[node+1].ival,&numarg,P_MIN));
	str_free(fstr);
	str_cat(str,") ");
	str_scat(str,fstr=walk(0,level,ops[node+2].ival,&numarg,P_MIN));
	str_free(fstr);
	if (len == 3) {
	    i = ops[node+3].ival;
	    if (i) {
		if ((ops[i].ival & 255) == OBLOCK) {
		    i = ops[i+1].ival;
		    if (i) {
			if ((ops[i].ival & 255) != OIF)
			    i = 0;
		    }
		}
		else
		    i = 0;
	    }
	    if (i) {
		str_cat(str,"els");
		str_scat(str,fstr=walk(0,level,i,&numarg,P_MIN));
		str_free(fstr);
	    }
	    else {
		str_cat(str,"else ");
		str_scat(str,fstr=walk(0,level,ops[node+3].ival,&numarg,P_MIN));
		str_free(fstr);
	    }
	}
	break;
    case OWHILE:
	str = str_new(0);
	str_set(str,"while (");
	str_scat(str,fstr=walk(1,level,ops[node+1].ival,&numarg,P_MIN));
	str_free(fstr);
	str_cat(str,") ");
	str_scat(str,fstr=walk(0,level,ops[node+2].ival,&numarg,P_MIN));
	str_free(fstr);
	break;
    case ODO:
	str = str_new(0);
	str_set(str,"do ");
	str_scat(str,fstr=walk(1,level,ops[node+1].ival,&numarg,P_MIN));
	str_free(fstr);
	if (str->str_ptr[str->str_cur - 1] == '\n')
	    --str->str_cur;
	str_cat(str," while (");
	str_scat(str,fstr=walk(0,level,ops[node+2].ival,&numarg,P_MIN));
	str_free(fstr);
	str_cat(str,");");
	break;
    case OFOR:
	str = str_new(0);
	str_set(str,"for (");
	str_scat(str,tmpstr=walk(1,level,ops[node+1].ival,&numarg,P_MIN));
	i = numarg;
	if (i) {
	    t = s = tmpstr->str_ptr;
	    while (isalpha(*t) || isdigit(*t) || *t == '$' || *t == '_')
		t++;
	    i = t - s;
	    if (i < 2)
		i = 0;
	}
	str_cat(str,"; ");
	fstr=walk(1,level,ops[node+2].ival,&numarg,P_MIN);
	if (i && (t = strchr(fstr->str_ptr,0377))) {
	    if (strnEQ(fstr->str_ptr,s,i))
		*t = ' ';
	}
	str_scat(str,fstr);
	str_free(fstr);
	str_free(tmpstr);
	str_cat(str,"; ");
	str_scat(str,fstr=walk(1,level,ops[node+3].ival,&numarg,P_MIN));
	str_free(fstr);
	str_cat(str,") ");
	str_scat(str,fstr=walk(0,level,ops[node+4].ival,&numarg,P_MIN));
	str_free(fstr);
	break;
    case OFORIN:
	tmpstr = walk(0,level,ops[node+1].ival,&numarg,P_MIN);
	d = strchr(tmpstr->str_ptr,'$');
	if (!d)
	    fatal("Illegal for loop: %s",tmpstr->str_ptr);
	s = strchr(d,'{');
	if (!s)
	    s = strchr(d,'[');
	if (!s)
	    fatal("Illegal for loop: %s",d);
	*s++ = '\0';
	for (t = s; i = *t; t++) {
	    i &= 127;
	    if (i == '}' || i == ']')
		break;
	}
	if (*t)
	    *t = '\0';
	str = str_new(0);
	str_set(str,d+1);
	str_cat(str,"[]");
	tmp2str = hfetch(symtab,str->str_ptr);
	if (tmp2str && atoi(tmp2str->str_ptr)) {
	    sprintf(tokenbuf,
	      "foreach %s ($[ .. $#%s) ",
	      s,
	      d+1);
	}
	else {
	    sprintf(tokenbuf,
	      "foreach %s (keys %%%s) ",
	      s,
	      d+1);
	}
	str_set(str,tokenbuf);
	str_scat(str,fstr=walk(0,level,ops[node+2].ival,&numarg,P_MIN));
	str_free(fstr);
	str_free(tmpstr);
	break;
    case OBLOCK:
	str = str_new(0);
	str_set(str,"{");
	if (len >= 2 && ops[node+2].ival) {
	    str_scat(str,fstr=walk(0,level,ops[node+2].ival,&numarg,P_MIN));
	    str_free(fstr);
	}
	fixtab(str,++level);
	str_scat(str,fstr=walk(0,level,ops[node+1].ival,&numarg,P_MIN));
	str_free(fstr);
	addsemi(str);
	fixtab(str,--level);
	str_cat(str,"}\n");
	tab(str,level);
	if (len >= 3) {
	    str_scat(str,fstr=walk(0,level,ops[node+3].ival,&numarg,P_MIN));
	    str_free(fstr);
	}
	break;
    default:
      def:
	if (len) {
	    if (len > 5)
		fatal("Garbage length in walk");
	    str = walk(0,level,ops[node+1].ival,&numarg,P_MIN);
	    for (i = 2; i<= len; i++) {
		str_scat(str,fstr=walk(0,level,ops[node+i].ival,&numarg,P_MIN));
		str_free(fstr);
	    }
	}
	else {
	    str = Nullstr;
	}
	break;
    }
    if (!str)
	str = str_new(0);

    if (useval && prec < minprec) {		/* need parens? */
	fstr = str_new(str->str_cur+2);
	str_nset(fstr,"(",1);
	str_scat(fstr,str);
	str_ncat(fstr,")",1);
	str_free(str);
	str = fstr;
    }

    *numericptr = numeric;
#ifdef DEBUGGING
    if (debug & 4) {
	printf("%3d %5d %15s %d %4d ",level,node,opname[type],len,str->str_cur);
	for (t = str->str_ptr; *t && t - str->str_ptr < 40; t++)
	    if (*t == '\n')
		printf("\\n");
	    else if (*t == '\t')
		printf("\\t");
	    else
		putchar(*t);
	putchar('\n');
    }
#endif
    return str;
}

static void
tab(register STR *str, register int lvl)
{
    while (lvl > 1) {
	str_cat(str,"\t");
	lvl -= 2;
    }
    if (lvl)
	str_cat(str,"    ");
}

static void
fixtab(register STR *str, register int lvl)
{
    register char *s;

    /* strip trailing white space */

    s = str->str_ptr+str->str_cur - 1;
    while (s >= str->str_ptr && (*s == ' ' || *s == '\t' || *s == '\n'))
	s--;
    s[1] = '\0';
    str->str_cur = s + 1 - str->str_ptr;
    if (s >= str->str_ptr && *s != '\n')
	str_cat(str,"\n");

    tab(str,lvl);
}

static void
addsemi(register STR *str)
{
    register char *s;

    s = str->str_ptr+str->str_cur - 1;
    while (s >= str->str_ptr && (*s == ' ' || *s == '\t' || *s == '\n'))
	s--;
    if (s >= str->str_ptr && *s != ';' && *s != '}')
	str_cat(str,";");
}

static void
emit_split(register STR *str, int level)
{
    register int i;

    if (split_to_array)
	str_cat(str,"@Fld");
    else {
	str_cat(str,"(");
	for (i = 1; i < maxfld; i++) {
	    if (i <= arymax)
		sprintf(tokenbuf,"$%s,",nameary[i]);
	    else
		sprintf(tokenbuf,"$Fld%d,",i);
	    str_cat(str,tokenbuf);
	}
	if (maxfld <= arymax)
	    sprintf(tokenbuf,"$%s)",nameary[maxfld]);
	else
	    sprintf(tokenbuf,"$Fld%d)",maxfld);
	str_cat(str,tokenbuf);
    }
    if (const_FS) {
	sprintf(tokenbuf," = split(/[%c\\n]/, $_, 9999);\n",const_FS);
	str_cat(str,tokenbuf);
    }
    else if (saw_FS)
	str_cat(str," = split($FS, $_, 9999);\n");
    else
	str_cat(str," = split(' ', $_, 9999);\n");
    tab(str,level);
}

int
prewalk(int numit, int level, register int node, int *numericptr)
{
    register int len;
    register int type;
    register int i;
    int numarg;
    int numeric = FALSE;
    STR *tmpstr;
    STR *tmp2str;

    if (!node) {
	*numericptr = 0;
	return 0;
    }
    type = ops[node].ival;
    len = type >> 8;
    type &= 255;
    switch (type) {
    case OPROG:
	prewalk(0,level,ops[node+1].ival,&numarg);
	if (ops[node+2].ival) {
	    prewalk(0,level,ops[node+2].ival,&numarg);
	}
	++level;
	prewalk(0,level,ops[node+3].ival,&numarg);
	--level;
	if (ops[node+3].ival) {
	    prewalk(0,level,ops[node+4].ival,&numarg);
	}
	break;
    case OHUNKS:
	prewalk(0,level,ops[node+1].ival,&numarg);
	prewalk(0,level,ops[node+2].ival,&numarg);
	if (len == 3) {
	    prewalk(0,level,ops[node+3].ival,&numarg);
	}
	break;
    case ORANGE:
	prewalk(1,level,ops[node+1].ival,&numarg);
	prewalk(1,level,ops[node+2].ival,&numarg);
	break;
    case OPAT:
	goto def;
    case OREGEX:
	prewalk(0,level,ops[node+1].ival,&numarg);
	break;
    case OHUNK:
	if (len == 1) {
	    prewalk(0,level,ops[node+1].ival,&numarg);
	}
	else {
	    i = prewalk(0,level,ops[node+1].ival,&numarg);
	    if (i) {
		++level;
		prewalk(0,level,ops[node+2].ival,&numarg);
		--level;
	    }
	    else {
		prewalk(0,level,ops[node+2].ival,&numarg);
	    }
	}
	break;
    case OPPAREN:
	prewalk(0,level,ops[node+1].ival,&numarg);
	break;
    case OPANDAND:
	prewalk(0,level,ops[node+1].ival,&numarg);
	prewalk(0,level,ops[node+2].ival,&numarg);
	break;
    case OPOROR:
	prewalk(0,level,ops[node+1].ival,&numarg);
	prewalk(0,level,ops[node+2].ival,&numarg);
	break;
    case OPNOT:
	prewalk(0,level,ops[node+1].ival,&numarg);
	break;
    case OCPAREN:
	prewalk(0,level,ops[node+1].ival,&numarg);
	numeric |= numarg;
	break;
    case OCANDAND:
	prewalk(0,level,ops[node+1].ival,&numarg);
	numeric = 1;
	prewalk(0,level,ops[node+2].ival,&numarg);
	break;
    case OCOROR:
	prewalk(0,level,ops[node+1].ival,&numarg);
	numeric = 1;
	prewalk(0,level,ops[node+2].ival,&numarg);
	break;
    case OCNOT:
	prewalk(0,level,ops[node+1].ival,&numarg);
	numeric = 1;
	break;
    case ORELOP:
	prewalk(0,level,ops[node+2].ival,&numarg);
	numeric |= numarg;
	prewalk(0,level,ops[node+1].ival,&numarg);
	prewalk(0,level,ops[node+3].ival,&numarg);
	numeric |= numarg;
	numeric = 1;
	break;
    case ORPAREN:
	prewalk(0,level,ops[node+1].ival,&numarg);
	numeric |= numarg;
	break;
    case OMATCHOP:
	prewalk(0,level,ops[node+2].ival,&numarg);
	prewalk(0,level,ops[node+1].ival,&numarg);
	prewalk(0,level,ops[node+3].ival,&numarg);
	numeric = 1;
	break;
    case OMPAREN:
	prewalk(0,level,ops[node+1].ival,&numarg);
	numeric |= numarg;
	break;
    case OCONCAT:
	prewalk(0,level,ops[node+1].ival,&numarg);
	prewalk(0,level,ops[node+2].ival,&numarg);
	break;
    case OASSIGN:
	prewalk(0,level,ops[node+2].ival,&numarg);
	prewalk(0,level,ops[node+1].ival,&numarg);
	prewalk(0,level,ops[node+3].ival,&numarg);
	if (numarg || strlen(ops[ops[node+1].ival+1].cval) > (Size_t)1) {
	    numericize(ops[node+2].ival);
	    if (!numarg)
		numericize(ops[node+3].ival);
	}
	numeric |= numarg;
	break;
    case OADD:
	prewalk(1,level,ops[node+1].ival,&numarg);
	prewalk(1,level,ops[node+2].ival,&numarg);
	numeric = 1;
	break;
    case OSUBTRACT:
	prewalk(1,level,ops[node+1].ival,&numarg);
	prewalk(1,level,ops[node+2].ival,&numarg);
	numeric = 1;
	break;
    case OMULT:
	prewalk(1,level,ops[node+1].ival,&numarg);
	prewalk(1,level,ops[node+2].ival,&numarg);
	numeric = 1;
	break;
    case ODIV:
	prewalk(1,level,ops[node+1].ival,&numarg);
	prewalk(1,level,ops[node+2].ival,&numarg);
	numeric = 1;
	break;
    case OPOW:
	prewalk(1,level,ops[node+1].ival,&numarg);
	prewalk(1,level,ops[node+2].ival,&numarg);
	numeric = 1;
	break;
    case OMOD:
	prewalk(1,level,ops[node+1].ival,&numarg);
	prewalk(1,level,ops[node+2].ival,&numarg);
	numeric = 1;
	break;
    case OPOSTINCR:
	prewalk(1,level,ops[node+1].ival,&numarg);
	numeric = 1;
	break;
    case OPOSTDECR:
	prewalk(1,level,ops[node+1].ival,&numarg);
	numeric = 1;
	break;
    case OPREINCR:
	prewalk(1,level,ops[node+1].ival,&numarg);
	numeric = 1;
	break;
    case OPREDECR:
	prewalk(1,level,ops[node+1].ival,&numarg);
	numeric = 1;
	break;
    case OUMINUS:
	prewalk(1,level,ops[node+1].ival,&numarg);
	numeric = 1;
	break;
    case OUPLUS:
	prewalk(1,level,ops[node+1].ival,&numarg);
	numeric = 1;
	break;
    case OPAREN:
	prewalk(0,level,ops[node+1].ival,&numarg);
	numeric |= numarg;
	break;
    case OGETLINE:
	break;
    case OSPRINTF:
	prewalk(0,level,ops[node+1].ival,&numarg);
	break;
    case OSUBSTR:
	prewalk(0,level,ops[node+1].ival,&numarg);
	prewalk(1,level,ops[node+2].ival,&numarg);
	if (len == 3) {
	    prewalk(1,level,ops[node+3].ival,&numarg);
	}
	break;
    case OSTRING:
	break;
    case OSPLIT:
	numeric = 1;
	prewalk(0,level,ops[node+2].ival,&numarg);
	if (len == 3)
	    prewalk(0,level,ops[node+3].ival,&numarg);
	prewalk(0,level,ops[node+1].ival,&numarg);
	break;
    case OINDEX:
	prewalk(0,level,ops[node+1].ival,&numarg);
	prewalk(0,level,ops[node+2].ival,&numarg);
	numeric = 1;
	break;
    case OMATCH:
	prewalk(0,level,ops[node+1].ival,&numarg);
	prewalk(0,level,ops[node+2].ival,&numarg);
	numeric = 1;
	break;
    case OUSERDEF:
	subretnum = FALSE;
	--level;
	tmpstr = walk(0,level,ops[node+1].ival,&numarg,P_MIN);
	++level;
	prewalk(0,level,ops[node+2].ival,&numarg);
	prewalk(0,level,ops[node+4].ival,&numarg);
	prewalk(0,level,ops[node+5].ival,&numarg);
	--level;
	str_cat(tmpstr,"(");
	tmp2str = str_new(0);
	if (subretnum || numarg)
	    str_set(tmp2str,"1");
	hstore(symtab,tmpstr->str_ptr,tmp2str);
	str_free(tmpstr);
	level++;
	break;
    case ORETURN:
	if (len > 0) {
	    prewalk(0,level,ops[node+1].ival,&numarg);
	    if (numarg)
		subretnum = TRUE;
	}
	break;
    case OUSERFUN:
	tmp2str = str_new(0);
	str_scat(tmp2str,tmpstr=walk(1,level,ops[node+1].ival,&numarg,P_MIN));
	fixrargs(tmpstr->str_ptr,ops[node+2].ival,0);
	str_free(tmpstr);
	str_cat(tmp2str,"(");
	tmpstr = hfetch(symtab,tmp2str->str_ptr);
	if (tmpstr && tmpstr->str_ptr)
	    numeric |= atoi(tmpstr->str_ptr);
	prewalk(0,level,ops[node+2].ival,&numarg);
	str_free(tmp2str);
	break;
    case OGSUB:
    case OSUB:
	if (len >= 3)
	    prewalk(0,level,ops[node+3].ival,&numarg);
	prewalk(0,level,ops[ops[node+2].ival+1].ival,&numarg);
	prewalk(0,level,ops[node+1].ival,&numarg);
	numeric = 1;
	break;
    case ONUM:
	prewalk(0,level,ops[node+1].ival,&numarg);
	numeric = 1;
	break;
    case OSTR:
	prewalk(0,level,ops[node+1].ival,&numarg);
	break;
    case ODEFINED:
    case ODELETE:
    case OSTAR:
    case OVAR:
	prewalk(0,level,ops[node+1].ival,&numarg);
	if (len == 1) {
	    if (numit)
		numericize(node);
	}
	else {
	    prewalk(0,level,ops[node+2].ival,&numarg);
	}
	break;
    case OFLD:
	prewalk(0,level,ops[node+1].ival,&numarg);
	break;
    case OVFLD:
	i = ops[node+1].ival;
	prewalk(0,level,i,&numarg);
	break;
    case OJUNK:
	goto def;
    case OSNEWLINE:
	break;
    case ONEWLINE:
	break;
    case OSCOMMENT:
	break;
    case OCOMMENT:
	break;
    case OCOMMA:
	prewalk(0,level,ops[node+1].ival,&numarg);
	prewalk(0,level,ops[node+2].ival,&numarg);
	prewalk(0,level,ops[node+3].ival,&numarg);
	break;
    case OSEMICOLON:
	break;
    case OSTATES:
	prewalk(0,level,ops[node+1].ival,&numarg);
	prewalk(0,level,ops[node+2].ival,&numarg);
	break;
    case OSTATE:
	if (len >= 1) {
	    prewalk(0,level,ops[node+1].ival,&numarg);
	    if (len >= 2) {
		prewalk(0,level,ops[node+2].ival,&numarg);
	    }
	}
	break;
    case OCLOSE:
	prewalk(0,level,ops[node+1].ival,&numarg);
	break;
    case OPRINTF:
    case OPRINT:
	if (len == 3) {		/* output redirection */
	    prewalk(0,level,ops[node+3].ival,&numarg);
	    prewalk(0,level,ops[node+2].ival,&numarg);
	}
	prewalk(0+(type==OPRINT),level,ops[node+1].ival,&numarg);
	break;
    case ORAND:
	break;
    case OSRAND:
	goto maybe0;
    case OATAN2:
	goto maybe0;
    case OSIN:
	goto maybe0;
    case OCOS:
	goto maybe0;
    case OSYSTEM:
	goto maybe0;
    case OLENGTH:
	goto maybe0;
    case OLOG:
	goto maybe0;
    case OEXP:
	goto maybe0;
    case OSQRT:
	goto maybe0;
    case OINT:
      maybe0:
	numeric = 1;
	if (len > 0)
	    prewalk(type != OLENGTH && type != OSYSTEM,
	      level,ops[node+1].ival,&numarg);
	break;
    case OBREAK:
	break;
    case ONEXT:
	break;
    case OEXIT:
	if (len == 1) {
	    prewalk(1,level,ops[node+1].ival,&numarg);
	}
	break;
    case OCONTINUE:
	break;
    case OREDIR:
	goto def;
    case OIF:
	prewalk(0,level,ops[node+1].ival,&numarg);
	prewalk(0,level,ops[node+2].ival,&numarg);
	if (len == 3) {
	    prewalk(0,level,ops[node+3].ival,&numarg);
	}
	break;
    case OWHILE:
	prewalk(0,level,ops[node+1].ival,&numarg);
	prewalk(0,level,ops[node+2].ival,&numarg);
	break;
    case OFOR:
	prewalk(0,level,ops[node+1].ival,&numarg);
	prewalk(0,level,ops[node+2].ival,&numarg);
	prewalk(0,level,ops[node+3].ival,&numarg);
	prewalk(0,level,ops[node+4].ival,&numarg);
	break;
    case OFORIN:
	prewalk(0,level,ops[node+2].ival,&numarg);
	prewalk(0,level,ops[node+1].ival,&numarg);
	break;
    case OBLOCK:
	if (len == 2) {
	    prewalk(0,level,ops[node+2].ival,&numarg);
	}
	++level;
	prewalk(0,level,ops[node+1].ival,&numarg);
	--level;
	break;
    default:
      def:
	if (len) {
	    if (len > 5)
		fatal("Garbage length in prewalk");
	    prewalk(0,level,ops[node+1].ival,&numarg);
	    for (i = 2; i<= len; i++) {
		prewalk(0,level,ops[node+i].ival,&numarg);
	    }
	}
	break;
    }
    *numericptr = numeric;
    return 1;
}

static void
numericize(register int node)
{
    register int len;
    register int type;
    STR *tmpstr;
    STR *tmp2str;
    int numarg;

    type = ops[node].ival;
    len = type >> 8;
    type &= 255;
    if (type == OVAR && len == 1) {
	tmpstr=walk(0,0,ops[node+1].ival,&numarg,P_MIN);
	tmp2str = str_make("1");
	hstore(symtab,tmpstr->str_ptr,tmp2str);
    }
}
