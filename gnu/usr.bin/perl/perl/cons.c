/* $RCSfile: cons.c,v $$Revision: 1.1.1.1 $$Date: 1994/09/10 06:27:32 $
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log: cons.c,v $
 * Revision 1.1.1.1  1994/09/10  06:27:32  gclarkii
 * Initial import of Perl 4.046 bmaked
 *
 * Revision 1.1.1.1  1993/08/23  21:29:35  nate
 * PERL!
 *
 * Revision 4.0.1.4  1993/02/05  19:30:15  lwall
 * patch36: fixed various little coredump bugs
 *
 * Revision 4.0.1.3  92/06/08  12:18:35  lwall
 * patch20: removed implicit int declarations on funcions
 * patch20: deleted some minor memory leaks
 * patch20: fixed double debug break in foreach with implicit array assignment
 * patch20: fixed confusion between a *var's real name and its effective name
 * patch20: Perl now distinguishes overlapped copies from non-overlapped
 * patch20: debugger sometimes displayed wrong source line
 * patch20: various error messages have been clarified
 * patch20: an eval block containing a null block or statement could dump core
 *
 * Revision 4.0.1.2  91/11/05  16:15:13  lwall
 * patch11: debugger got confused over nested subroutine definitions
 * patch11: prepared for ctype implementations that don't define isascii()
 *
 * Revision 4.0.1.1  91/06/07  10:31:15  lwall
 * patch4: new copyright notice
 * patch4: added global modifier for pattern matches
 *
 * Revision 4.0  91/03/20  01:05:51  lwall
 * 4.0 baseline.
 *
 */

#include "EXTERN.h"
#include "perl.h"
#include "perly.h"

extern char *tokename[];
extern int yychar;

static int cmd_tosave();
static int arg_tosave();
static int spat_tosave();
static void make_cswitch();
static void make_nswitch();

static bool saw_return;

SUBR *
make_sub(name,cmd)
char *name;
CMD *cmd;
{
    register SUBR *sub;
    STAB *stab = stabent(name,TRUE);

    if (sub = stab_sub(stab)) {
	if (dowarn) {
	    CMD *oldcurcmd = curcmd;

	    if (cmd)
		curcmd = cmd;
	    warn("Subroutine %s redefined",name);
	    curcmd = oldcurcmd;
	}
	if (!sub->usersub && sub->cmd) {
	    cmd_free(sub->cmd);
	    sub->cmd = Nullcmd;
	    afree(sub->tosave);
	}
	Safefree(sub);
    }
    Newz(101,sub,1,SUBR);
    stab_sub(stab) = sub;
    sub->filestab = curcmd->c_filestab;
    saw_return = FALSE;
    tosave = anew(Nullstab);
    tosave->ary_fill = 0;	/* make 1 based */
    (void)cmd_tosave(cmd,FALSE);	/* this builds the tosave array */
    sub->tosave = tosave;
    if (saw_return) {
	struct compcmd mycompblock;

	mycompblock.comp_true = cmd;
	mycompblock.comp_alt = Nullcmd;
	cmd = add_label(savestr("_SUB_"),make_ccmd(C_BLOCK,0,
	    Nullarg,mycompblock));
	saw_return = FALSE;
	cmd->c_flags |= CF_TERM;
	cmd->c_head = cmd;
    }
    sub->cmd = cmd;
    if (perldb) {
	STR *str;
	STR *tmpstr = str_mortal(&str_undef);

	sprintf(buf,"%s:%ld",stab_val(curcmd->c_filestab)->str_ptr, subline);
	str = str_make(buf,0);
	str_cat(str,"-");
	sprintf(buf,"%ld",(long)curcmd->c_line);
	str_cat(str,buf);
	stab_efullname(tmpstr,stab);
	hstore(stab_xhash(DBsub), tmpstr->str_ptr, tmpstr->str_cur, str, 0);
    }
    Safefree(name);
    return sub;
}

SUBR *
make_usub(name, ix, subaddr, filename)
char *name;
int ix;
int (*subaddr)();
char *filename;
{
    register SUBR *sub;
    STAB *stab = stabent(name,allstabs);

    if (!stab)				/* unused function */
	return Null(SUBR*);
    if (sub = stab_sub(stab)) {
	if (dowarn)
	    warn("Subroutine %s redefined",name);
	if (!sub->usersub && sub->cmd) {
	    cmd_free(sub->cmd);
	    sub->cmd = Nullcmd;
	    afree(sub->tosave);
	}
	Safefree(sub);
    }
    Newz(101,sub,1,SUBR);
    stab_sub(stab) = sub;
    sub->filestab = fstab(filename);
    sub->usersub = subaddr;
    sub->userindex = ix;
    return sub;
}

void
make_form(stab,fcmd)
STAB *stab;
FCMD *fcmd;
{
    if (stab_form(stab)) {
	FCMD *tmpfcmd;
	FCMD *nextfcmd;

	for (tmpfcmd = stab_form(stab); tmpfcmd; tmpfcmd = nextfcmd) {
	    nextfcmd = tmpfcmd->f_next;
	    if (tmpfcmd->f_expr)
		arg_free(tmpfcmd->f_expr);
	    if (tmpfcmd->f_unparsed)
		str_free(tmpfcmd->f_unparsed);
	    if (tmpfcmd->f_pre)
		Safefree(tmpfcmd->f_pre);
	    Safefree(tmpfcmd);
	}
    }
    stab_form(stab) = fcmd;
}

CMD *
block_head(tail)
register CMD *tail;
{
    CMD *head;
    register int opt;
    register int last_opt = 0;
    register STAB *last_stab = Nullstab;
    register int count = 0;
    register CMD *switchbeg = Nullcmd;

    if (tail == Nullcmd) {
	return tail;
    }
    head = tail->c_head;

    for (tail = head; tail; tail = tail->c_next) {

	/* save one measly dereference at runtime */
	if (tail->c_type == C_IF) {
	    if (!(tail->ucmd.ccmd.cc_alt = tail->ucmd.ccmd.cc_alt->c_next))
		tail->c_flags |= CF_TERM;
	}
	else if (tail->c_type == C_EXPR) {
	    ARG *arg;

	    if (tail->ucmd.acmd.ac_expr)
		arg = tail->ucmd.acmd.ac_expr;
	    else
		arg = tail->c_expr;
	    if (arg) {
		if (arg->arg_type == O_RETURN)
		    tail->c_flags |= CF_TERM;
		else if (arg->arg_type == O_ITEM && arg[1].arg_type == A_CMD)
		    tail->c_flags |= CF_TERM;
	    }
	}
	if (!tail->c_next)
	    tail->c_flags |= CF_TERM;

	if (tail->c_expr && (tail->c_flags & CF_OPTIMIZE) == CFT_FALSE)
	    opt_arg(tail,1, tail->c_type == C_EXPR);

	/* now do a little optimization on case-ish structures */
	switch(tail->c_flags & (CF_OPTIMIZE|CF_FIRSTNEG|CF_INVERT)) {
	case CFT_ANCHOR:
	case CFT_STROP:
	    opt = (tail->c_flags & CF_NESURE) ? CFT_STROP : 0;
	    break;
	case CFT_CCLASS:
	    opt = CFT_STROP;
	    break;
	case CFT_NUMOP:
	    opt = (tail->c_slen == O_NE ? 0 : CFT_NUMOP);
	    if ((tail->c_flags&(CF_NESURE|CF_EQSURE)) != (CF_NESURE|CF_EQSURE))
		opt = 0;
	    break;
	default:
	    opt = 0;
	}
	if (opt && opt == last_opt && tail->c_stab == last_stab)
	    count++;
	else {
	    if (count >= 3) {		/* is this the breakeven point? */
		if (last_opt == CFT_NUMOP)
		    make_nswitch(switchbeg,count);
		else
		    make_cswitch(switchbeg,count);
	    }
	    if (opt) {
		count = 1;
		switchbeg = tail;
	    }
	    else
		count = 0;
	}
	last_opt = opt;
	last_stab = tail->c_stab;
    }
    if (count >= 3) {		/* is this the breakeven point? */
	if (last_opt == CFT_NUMOP)
	    make_nswitch(switchbeg,count);
	else
	    make_cswitch(switchbeg,count);
    }
    return head;
}

/* We've spotted a sequence of CMDs that all test the value of the same
 * spat.  Thus we can insert a SWITCH in front and jump directly
 * to the correct one.
 */
static void
make_cswitch(head,count)
register CMD *head;
int count;
{
    register CMD *cur;
    register CMD **loc;
    register int i;
    register int min = 255;
    register int max = 0;

    /* make a new head in the exact same spot */
    New(102,cur, 1, CMD);
    StructCopy(head,cur,CMD);
    Zero(head,1,CMD);
    head->c_head = cur->c_head;
    head->c_type = C_CSWITCH;
    head->c_next = cur;		/* insert new cmd at front of list */
    head->c_stab = cur->c_stab;

    Newz(103,loc,258,CMD*);
    loc++;				/* lie a little */
    while (count--) {
	if ((cur->c_flags & CF_OPTIMIZE) == CFT_CCLASS) {
	    for (i = 0; i <= 255; i++) {
		if (!loc[i] && cur->c_short->str_ptr[i>>3] & (1 << (i&7))) {
		    loc[i] = cur;
		    if (i < min)
			min = i;
		    if (i > max)
			max = i;
		}
	    }
	}
	else {
	    i = *cur->c_short->str_ptr & 255;
	    if (!loc[i]) {
		loc[i] = cur;
		if (i < min)
		    min = i;
		if (i > max)
		    max = i;
	    }
	}
	cur = cur->c_next;
    }
    max++;
    if (min > 0)
	Move(&loc[min],&loc[0], max - min, CMD*);
    loc--;
    min--;
    max -= min;
    for (i = 0; i <= max; i++)
	if (!loc[i])
	    loc[i] = cur;
    Renew(loc,max+1,CMD*);	/* chop it down to size */
    head->ucmd.scmd.sc_offset = min;
    head->ucmd.scmd.sc_max = max;
    head->ucmd.scmd.sc_next = loc;
}

static void
make_nswitch(head,count)
register CMD *head;
int count;
{
    register CMD *cur = head;
    register CMD **loc;
    register int i;
    register int min = 32767;
    register int max = -32768;
    int origcount = count;
    double value;		/* or your money back! */
    short changed;		/* so triple your money back! */

    while (count--) {
	i = (int)str_gnum(cur->c_short);
	value = (double)i;
	if (value != cur->c_short->str_u.str_nval)
	    return;		/* fractional values--just forget it */
	changed = i;
	if (changed != i)
	    return;		/* too big for a short */
	if (cur->c_slen == O_LE)
	    i++;
	else if (cur->c_slen == O_GE)	/* we only do < or > here */
	    i--;
	if (i < min)
	    min = i;
	if (i > max)
	    max = i;
	cur = cur->c_next;
    }
    count = origcount;
    if (max - min > count * 2 + 10)		/* too sparse? */
	return;

    /* now make a new head in the exact same spot */
    New(104,cur, 1, CMD);
    StructCopy(head,cur,CMD);
    Zero(head,1,CMD);
    head->c_head = cur->c_head;
    head->c_type = C_NSWITCH;
    head->c_next = cur;		/* insert new cmd at front of list */
    head->c_stab = cur->c_stab;

    Newz(105,loc, max - min + 3, CMD*);
    loc++;
    max -= min;
    max++;
    while (count--) {
	i = (int)str_gnum(cur->c_short);
	i -= min;
	switch(cur->c_slen) {
	case O_LE:
	    i++;
	case O_LT:
	    for (i--; i >= -1; i--)
		if (!loc[i])
		    loc[i] = cur;
	    break;
	case O_GE:
	    i--;
	case O_GT:
	    for (i++; i <= max; i++)
		if (!loc[i])
		    loc[i] = cur;
	    break;
	case O_EQ:
	    if (!loc[i])
		loc[i] = cur;
	    break;
	}
	cur = cur->c_next;
    }
    loc--;
    min--;
    max++;
    for (i = 0; i <= max; i++)
	if (!loc[i])
	    loc[i] = cur;
    head->ucmd.scmd.sc_offset = min;
    head->ucmd.scmd.sc_max = max;
    head->ucmd.scmd.sc_next = loc;
}

CMD *
append_line(head,tail)
register CMD *head;
register CMD *tail;
{
    if (tail == Nullcmd)
	return head;
    if (!tail->c_head)			/* make sure tail is well formed */
	tail->c_head = tail;
    if (head != Nullcmd) {
	tail = tail->c_head;		/* get to start of tail list */
	if (!head->c_head)
	    head->c_head = head;	/* start a new head list */
	while (head->c_next) {
	    head->c_next->c_head = head->c_head;
	    head = head->c_next;	/* get to end of head list */
	}
	head->c_next = tail;		/* link to end of old list */
	tail->c_head = head->c_head;	/* propagate head pointer */
    }
    while (tail->c_next) {
	tail->c_next->c_head = tail->c_head;
	tail = tail->c_next;
    }
    return tail;
}

CMD *
dodb(cur)
CMD *cur;
{
    register CMD *cmd;
    register CMD *head = cur->c_head;
    STR *str;

    if (!head)
	head = cur;
    if (!head->c_line)
	return cur;
    str = afetch(stab_xarray(curcmd->c_filestab),(int)head->c_line,FALSE);
    if (str == &str_undef || str->str_nok)
	return cur;
    str->str_u.str_nval = (double)head->c_line;
    str->str_nok = 1;
    Newz(106,cmd,1,CMD);
    str_magic(str, curcmd->c_filestab, 0, Nullch, 0);
    str->str_magic->str_u.str_cmd = cmd;
    cmd->c_type = C_EXPR;
    cmd->ucmd.acmd.ac_stab = Nullstab;
    cmd->ucmd.acmd.ac_expr = Nullarg;
    cmd->c_expr = make_op(O_SUBR, 2,
	stab2arg(A_WORD,DBstab),
	Nullarg,
	Nullarg);
    /*SUPPRESS 53*/
    cmd->c_flags |= CF_COND|CF_DBSUB|CFT_D0;
    cmd->c_line = head->c_line;
    cmd->c_label = head->c_label;
    cmd->c_filestab = curcmd->c_filestab;
    cmd->c_stash = curstash;
    return append_line(cmd, cur);
}

CMD *
make_acmd(type,stab,cond,arg)
int type;
STAB *stab;
ARG *cond;
ARG *arg;
{
    register CMD *cmd;

    Newz(107,cmd,1,CMD);
    cmd->c_type = type;
    cmd->ucmd.acmd.ac_stab = stab;
    cmd->ucmd.acmd.ac_expr = arg;
    cmd->c_expr = cond;
    if (cond)
	cmd->c_flags |= CF_COND;
    if (cmdline == NOLINE)
	cmd->c_line = curcmd->c_line;
    else {
	cmd->c_line = cmdline;
	cmdline = NOLINE;
    }
    cmd->c_filestab = curcmd->c_filestab;
    cmd->c_stash = curstash;
    if (perldb)
	cmd = dodb(cmd);
    return cmd;
}

CMD *
make_ccmd(type,debuggable,arg,cblock)
int type;
int debuggable;
ARG *arg;
struct compcmd cblock;
{
    register CMD *cmd;

    Newz(108,cmd, 1, CMD);
    cmd->c_type = type;
    cmd->c_expr = arg;
    cmd->ucmd.ccmd.cc_true = cblock.comp_true;
    cmd->ucmd.ccmd.cc_alt = cblock.comp_alt;
    if (arg)
	cmd->c_flags |= CF_COND;
    if (cmdline == NOLINE)
	cmd->c_line = curcmd->c_line;
    else {
	cmd->c_line = cmdline;
	cmdline = NOLINE;
    }
    cmd->c_filestab = curcmd->c_filestab;
    cmd->c_stash = curstash;
    if (perldb && debuggable)
	cmd = dodb(cmd);
    return cmd;
}

CMD *
make_icmd(type,arg,cblock)
int type;
ARG *arg;
struct compcmd cblock;
{
    register CMD *cmd;
    register CMD *alt;
    register CMD *cur;
    register CMD *head;
    struct compcmd ncblock;

    Newz(109,cmd, 1, CMD);
    head = cmd;
    cmd->c_type = type;
    cmd->c_expr = arg;
    cmd->ucmd.ccmd.cc_true = cblock.comp_true;
    cmd->ucmd.ccmd.cc_alt = cblock.comp_alt;
    if (arg)
	cmd->c_flags |= CF_COND;
    if (cmdline == NOLINE)
	cmd->c_line = curcmd->c_line;
    else {
	cmd->c_line = cmdline;
	cmdline = NOLINE;
    }
    cmd->c_filestab = curcmd->c_filestab;
    cmd->c_stash = curstash;
    cur = cmd;
    alt = cblock.comp_alt;
    while (alt && alt->c_type == C_ELSIF) {
	cur = alt;
	alt = alt->ucmd.ccmd.cc_alt;
    }
    if (alt) {			/* a real life ELSE at the end? */
	ncblock.comp_true = alt;
	ncblock.comp_alt = Nullcmd;
	alt = append_line(cur,make_ccmd(C_ELSE,1,Nullarg,ncblock));
	cur->ucmd.ccmd.cc_alt = alt;
    }
    else
	alt = cur;		/* no ELSE, so cur is proxy ELSE */

    cur = cmd;
    while (cmd) {		/* now point everyone at the ELSE */
	cur = cmd;
	cmd = cur->ucmd.ccmd.cc_alt;
	cur->c_head = head;
	if (cur->c_type == C_ELSIF)
	    cur->c_type = C_IF;
	if (cur->c_type == C_IF)
	    cur->ucmd.ccmd.cc_alt = alt;
	if (cur == alt)
	    break;
	cur->c_next = cmd;
    }
    if (perldb)
	cur = dodb(cur);
    return cur;
}

void
opt_arg(cmd,fliporflop,acmd)
register CMD *cmd;
int fliporflop;
int acmd;
{
    register ARG *arg;
    int opt = CFT_EVAL;
    int sure = 0;
    ARG *arg2;
    int context = 0;	/* 0 = normal, 1 = before &&, 2 = before || */
    int flp = fliporflop;

    if (!cmd)
	return;
    if (!(arg = cmd->c_expr)) {
	cmd->c_flags &= ~CF_COND;
	return;
    }

    /* Can we turn && and || into if and unless? */

    if (acmd && !cmd->ucmd.acmd.ac_expr && !(cmd->c_flags & CF_TERM) &&
      (arg->arg_type == O_AND || arg->arg_type == O_OR) ) {
	dehoist(arg,1);
	arg[2].arg_type &= A_MASK;	/* don't suppress eval */
	dehoist(arg,2);
	cmd->ucmd.acmd.ac_expr = arg[2].arg_ptr.arg_arg;
	cmd->c_expr = arg[1].arg_ptr.arg_arg;
	if (arg->arg_type == O_OR)
	    cmd->c_flags ^= CF_INVERT;		/* || is like unless */
	arg->arg_len = 0;
	free_arg(arg);
	arg = cmd->c_expr;
    }

    /* Turn "if (!expr)" into "unless (expr)" */

    if (!(cmd->c_flags & CF_TERM)) {		/* unless return value wanted */
	while (arg->arg_type == O_NOT) {
	    dehoist(arg,1);
	    cmd->c_flags ^= CF_INVERT;		/* flip sense of cmd */
	    cmd->c_expr = arg[1].arg_ptr.arg_arg; /* hoist the rest of expr */
	    free_arg(arg);
	    arg = cmd->c_expr;			/* here we go again */
	}
    }

    if (!arg->arg_len) {		/* sanity check */
	cmd->c_flags |= opt;
	return;
    }

    /* for "cond .. cond" we set up for the initial check */

    if (arg->arg_type == O_FLIP)
	context |= 4;

    /* for "cond && expr" and "cond || expr" we can ignore expr, sort of */

  morecontext:
    if (arg->arg_type == O_AND)
	context |= 1;
    else if (arg->arg_type == O_OR)
	context |= 2;
    if (context && (arg[flp].arg_type & A_MASK) == A_EXPR) {
	arg = arg[flp].arg_ptr.arg_arg;
	flp = 1;
	if (arg->arg_type == O_AND || arg->arg_type == O_OR)
	    goto morecontext;
    }
    if ((context & 3) == 3)
	return;

    if (arg[flp].arg_flags & (AF_PRE|AF_POST)) {
	cmd->c_flags |= opt;
	if (acmd && !cmd->ucmd.acmd.ac_expr && !(cmd->c_flags & CF_TERM)
	  && cmd->c_expr->arg_type == O_ITEM) {
	    arg[flp].arg_flags &= ~AF_POST;	/* prefer ++$foo to $foo++ */
	    arg[flp].arg_flags |= AF_PRE;	/*  if value not wanted */
	}
	return;				/* side effect, can't optimize */
    }

    if (arg->arg_type == O_ITEM || arg->arg_type == O_FLIP ||
      arg->arg_type == O_AND || arg->arg_type == O_OR) {
	if ((arg[flp].arg_type & A_MASK) == A_SINGLE) {
	    opt = (str_true(arg[flp].arg_ptr.arg_str) ? CFT_TRUE : CFT_FALSE);
	    cmd->c_short = str_smake(arg[flp].arg_ptr.arg_str);
	    goto literal;
	}
	else if ((arg[flp].arg_type & A_MASK) == A_STAB ||
	  (arg[flp].arg_type & A_MASK) == A_LVAL) {
	    cmd->c_stab  = arg[flp].arg_ptr.arg_stab;
	    if (!context)
		arg[flp].arg_ptr.arg_stab = Nullstab;
	    opt = CFT_REG;
	  literal:
	    if (!context) {	/* no && or ||? */
		arg_free(arg);
		cmd->c_expr = Nullarg;
	    }
	    if (!(context & 1))
		cmd->c_flags |= CF_EQSURE;
	    if (!(context & 2))
		cmd->c_flags |= CF_NESURE;
	}
    }
    else if (arg->arg_type == O_MATCH || arg->arg_type == O_SUBST ||
	     arg->arg_type == O_NMATCH || arg->arg_type == O_NSUBST) {
	if ((arg[1].arg_type == A_STAB || arg[1].arg_type == A_LVAL) &&
		(arg[2].arg_type & A_MASK) == A_SPAT &&
		arg[2].arg_ptr.arg_spat->spat_short &&
		(arg->arg_type == O_SUBST || arg->arg_type == O_NSUBST ||
		 (arg[2].arg_ptr.arg_spat->spat_flags & SPAT_GLOBAL) == 0 )) {
	    cmd->c_stab  = arg[1].arg_ptr.arg_stab;
	    cmd->c_short = str_smake(arg[2].arg_ptr.arg_spat->spat_short);
	    cmd->c_slen  = arg[2].arg_ptr.arg_spat->spat_slen;
	    if (arg[2].arg_ptr.arg_spat->spat_flags & SPAT_ALL &&
		!(arg[2].arg_ptr.arg_spat->spat_flags & SPAT_ONCE) &&
		(arg->arg_type == O_MATCH || arg->arg_type == O_NMATCH) )
		sure |= CF_EQSURE;		/* (SUBST must be forced even */
						/* if we know it will work.) */
	    if (arg->arg_type != O_SUBST) {
		str_free(arg[2].arg_ptr.arg_spat->spat_short);
		arg[2].arg_ptr.arg_spat->spat_short = Nullstr;
		arg[2].arg_ptr.arg_spat->spat_slen = 0; /* only one chk */
	    }
	    sure |= CF_NESURE;		/* normally only sure if it fails */
	    if (arg->arg_type == O_NMATCH || arg->arg_type == O_NSUBST)
		cmd->c_flags |= CF_FIRSTNEG;
	    if (context & 1) {		/* only sure if thing is false */
		if (cmd->c_flags & CF_FIRSTNEG)
		    sure &= ~CF_NESURE;
		else
		    sure &= ~CF_EQSURE;
	    }
	    else if (context & 2) {	/* only sure if thing is true */
		if (cmd->c_flags & CF_FIRSTNEG)
		    sure &= ~CF_EQSURE;
		else
		    sure &= ~CF_NESURE;
	    }
	    if (sure & (CF_EQSURE|CF_NESURE)) {	/* if we know anything*/
		if (arg[2].arg_ptr.arg_spat->spat_flags & SPAT_SCANFIRST)
		    opt = CFT_SCAN;
		else
		    opt = CFT_ANCHOR;
		if (sure == (CF_EQSURE|CF_NESURE)	/* really sure? */
		    && arg->arg_type == O_MATCH
		    && context & 4
		    && fliporflop == 1) {
		    spat_free(arg[2].arg_ptr.arg_spat);
		    arg[2].arg_ptr.arg_spat = Nullspat;	/* don't do twice */
		}
		else
		    cmd->c_spat = arg[2].arg_ptr.arg_spat;
		cmd->c_flags |= sure;
	    }
	}
    }
    else if (arg->arg_type == O_SEQ || arg->arg_type == O_SNE ||
	     arg->arg_type == O_SLT || arg->arg_type == O_SGT) {
	if (arg[1].arg_type == A_STAB || arg[1].arg_type == A_LVAL) {
	    if (arg[2].arg_type == A_SINGLE) {
		/*SUPPRESS 594*/
		char *junk = str_get(arg[2].arg_ptr.arg_str);

		cmd->c_stab  = arg[1].arg_ptr.arg_stab;
		cmd->c_short = str_smake(arg[2].arg_ptr.arg_str);
		cmd->c_slen  = cmd->c_short->str_cur+1;
		switch (arg->arg_type) {
		case O_SLT: case O_SGT:
		    sure |= CF_EQSURE;
		    cmd->c_flags |= CF_FIRSTNEG;
		    break;
		case O_SNE:
		    cmd->c_flags |= CF_FIRSTNEG;
		    /* FALL THROUGH */
		case O_SEQ:
		    sure |= CF_NESURE|CF_EQSURE;
		    break;
		}
		if (context & 1) {	/* only sure if thing is false */
		    if (cmd->c_flags & CF_FIRSTNEG)
			sure &= ~CF_NESURE;
		    else
			sure &= ~CF_EQSURE;
		}
		else if (context & 2) { /* only sure if thing is true */
		    if (cmd->c_flags & CF_FIRSTNEG)
			sure &= ~CF_EQSURE;
		    else
			sure &= ~CF_NESURE;
		}
		if (sure & (CF_EQSURE|CF_NESURE)) {
		    opt = CFT_STROP;
		    cmd->c_flags |= sure;
		}
	    }
	}
    }
    else if (arg->arg_type == O_EQ || arg->arg_type == O_NE ||
	     arg->arg_type == O_LE || arg->arg_type == O_GE ||
	     arg->arg_type == O_LT || arg->arg_type == O_GT) {
	if (arg[1].arg_type == A_STAB || arg[1].arg_type == A_LVAL) {
	    if (arg[2].arg_type == A_SINGLE) {
		cmd->c_stab  = arg[1].arg_ptr.arg_stab;
		if (dowarn) {
		    STR *str = arg[2].arg_ptr.arg_str;

		    if ((!str->str_nok && !looks_like_number(str)))
			warn("Possible use of == on string value");
		}
		cmd->c_short = str_nmake(str_gnum(arg[2].arg_ptr.arg_str));
		cmd->c_slen = arg->arg_type;
		sure |= CF_NESURE|CF_EQSURE;
		if (context & 1) {	/* only sure if thing is false */
		    sure &= ~CF_EQSURE;
		}
		else if (context & 2) { /* only sure if thing is true */
		    sure &= ~CF_NESURE;
		}
		if (sure & (CF_EQSURE|CF_NESURE)) {
		    opt = CFT_NUMOP;
		    cmd->c_flags |= sure;
		}
	    }
	}
    }
    else if (arg->arg_type == O_ASSIGN &&
	     (arg[1].arg_type == A_STAB || arg[1].arg_type == A_LVAL) &&
	     arg[1].arg_ptr.arg_stab == defstab &&
	     arg[2].arg_type == A_EXPR ) {
	arg2 = arg[2].arg_ptr.arg_arg;
	if (arg2->arg_type == O_ITEM && arg2[1].arg_type == A_READ) {
	    opt = CFT_GETS;
	    cmd->c_stab = arg2[1].arg_ptr.arg_stab;
	    if (!(stab_io(arg2[1].arg_ptr.arg_stab)->flags & IOF_ARGV)) {
		free_arg(arg2);
		arg[2].arg_ptr.arg_arg = Nullarg;
		free_arg(arg);
		cmd->c_expr = Nullarg;
	    }
	}
    }
    else if (arg->arg_type == O_CHOP &&
	     (arg[1].arg_type == A_STAB || arg[1].arg_type == A_LVAL) ) {
	opt = CFT_CHOP;
	cmd->c_stab = arg[1].arg_ptr.arg_stab;
	free_arg(arg);
	cmd->c_expr = Nullarg;
    }
    if (context & 4)
	opt |= CF_FLIP;
    cmd->c_flags |= opt;

    if (cmd->c_flags & CF_FLIP) {
	if (fliporflop == 1) {
	    arg = cmd->c_expr;	/* get back to O_FLIP arg */
	    New(110,arg[3].arg_ptr.arg_cmd, 1, CMD);
	    Copy(cmd, arg[3].arg_ptr.arg_cmd, 1, CMD);
	    New(111,arg[4].arg_ptr.arg_cmd,1,CMD);
	    Copy(cmd, arg[4].arg_ptr.arg_cmd, 1, CMD);
	    opt_arg(arg[4].arg_ptr.arg_cmd,2,acmd);
	    arg->arg_len = 2;		/* this is a lie */
	}
	else {
	    if ((opt & CF_OPTIMIZE) == CFT_EVAL)
		cmd->c_flags = (cmd->c_flags & ~CF_OPTIMIZE) | CFT_UNFLIP;
	}
    }
}

CMD *
add_label(lbl,cmd)
char *lbl;
register CMD *cmd;
{
    if (cmd)
	cmd->c_label = lbl;
    return cmd;
}

CMD *
addcond(cmd, arg)
register CMD *cmd;
register ARG *arg;
{
    cmd->c_expr = arg;
    cmd->c_flags |= CF_COND;
    return cmd;
}

CMD *
addloop(cmd, arg)
register CMD *cmd;
register ARG *arg;
{
    void while_io();

    cmd->c_expr = arg;
    cmd->c_flags |= CF_COND|CF_LOOP;

    if (!(cmd->c_flags & CF_INVERT))
	while_io(cmd);		/* add $_ =, if necessary */

    if (cmd->c_type == C_BLOCK)
	cmd->c_flags &= ~CF_COND;
    else {
	arg = cmd->ucmd.acmd.ac_expr;
	if (arg && arg->arg_type == O_ITEM && arg[1].arg_type == A_CMD)
	    cmd->c_flags &= ~CF_COND;  /* "do {} while" happens at least once */
	if (arg && (arg->arg_flags & AF_DEPR) &&
	  (arg->arg_type == O_SUBR || arg->arg_type == O_DBSUBR) )
	    cmd->c_flags &= ~CF_COND;  /* likewise for "do subr() while" */
    }
    return cmd;
}

CMD *
invert(cmd)
CMD *cmd;
{
    register CMD *targ = cmd;
    if (targ->c_head)
	targ = targ->c_head;
    if (targ->c_flags & CF_DBSUB)
	targ = targ->c_next;
    targ->c_flags ^= CF_INVERT;
    return cmd;
}

void
cpy7bit(d,s,l)
register char *d;
register char *s;
register int l;
{
    while (l--)
	*d++ = *s++ & 127;
    *d = '\0';
}

int
yyerror(s)
char *s;
{
    char tmpbuf[258];
    char tmp2buf[258];
    char *tname = tmpbuf;

    if (bufptr > oldoldbufptr && bufptr - oldoldbufptr < 200 &&
      oldoldbufptr != oldbufptr && oldbufptr != bufptr) {
	while (isSPACE(*oldoldbufptr))
	    oldoldbufptr++;
	cpy7bit(tmp2buf, oldoldbufptr, bufptr - oldoldbufptr);
	sprintf(tname,"next 2 tokens \"%s\"",tmp2buf);
    }
    else if (bufptr > oldbufptr && bufptr - oldbufptr < 200 &&
      oldbufptr != bufptr) {
	while (isSPACE(*oldbufptr))
	    oldbufptr++;
	cpy7bit(tmp2buf, oldbufptr, bufptr - oldbufptr);
	sprintf(tname,"next token \"%s\"",tmp2buf);
    }
    else if (yychar > 256)
	tname = "next token ???";
    else if (!yychar)
	(void)strcpy(tname,"at EOF");
    else if (yychar < 32)
	(void)sprintf(tname,"next char ^%c",yychar+64);
    else if (yychar == 127)
	(void)strcpy(tname,"at EOF");
    else
	(void)sprintf(tname,"next char %c",yychar);
    (void)sprintf(buf, "%s in file %s at line %d, %s\n",
      s,stab_val(curcmd->c_filestab)->str_ptr,curcmd->c_line,tname);
    if (curcmd->c_line == multi_end && multi_start < multi_end)
	sprintf(buf+strlen(buf),
	  "  (Might be a runaway multi-line %c%c string starting on line %d)\n",
	  multi_open,multi_close,multi_start);
    if (in_eval)
	str_cat(stab_val(stabent("@",TRUE)),buf);
    else
	fputs(buf,stderr);
    if (++error_count >= 10)
	fatal("%s has too many errors.\n",
	stab_val(curcmd->c_filestab)->str_ptr);
}

void
while_io(cmd)
register CMD *cmd;
{
    register ARG *arg = cmd->c_expr;
    STAB *asgnstab;

    /* hoist "while (<channel>)" up into command block */

    if (arg && arg->arg_type == O_ITEM && arg[1].arg_type == A_READ) {
	cmd->c_flags &= ~CF_OPTIMIZE;	/* clear optimization type */
	cmd->c_flags |= CFT_GETS;	/* and set it to do the input */
	cmd->c_stab = arg[1].arg_ptr.arg_stab;
	if (stab_io(arg[1].arg_ptr.arg_stab)->flags & IOF_ARGV) {
	    cmd->c_expr = l(make_op(O_ASSIGN, 2,	/* fake up "$_ =" */
	       stab2arg(A_LVAL,defstab), arg, Nullarg));
	}
	else {
	    free_arg(arg);
	    cmd->c_expr = Nullarg;
	}
    }
    else if (arg && arg->arg_type == O_ITEM && arg[1].arg_type == A_INDREAD) {
	cmd->c_flags &= ~CF_OPTIMIZE;	/* clear optimization type */
	cmd->c_flags |= CFT_INDGETS;	/* and set it to do the input */
	cmd->c_stab = arg[1].arg_ptr.arg_stab;
	free_arg(arg);
	cmd->c_expr = Nullarg;
    }
    else if (arg && arg->arg_type == O_ITEM && arg[1].arg_type == A_GLOB) {
	if ((cmd->c_flags & CF_OPTIMIZE) == CFT_ARRAY)
	    asgnstab = cmd->c_stab;
	else
	    asgnstab = defstab;
	cmd->c_expr = l(make_op(O_ASSIGN, 2,	/* fake up "$foo =" */
	   stab2arg(A_LVAL,asgnstab), arg, Nullarg));
	cmd->c_flags &= ~CF_OPTIMIZE;	/* clear optimization type */
    }
}

CMD *
wopt(cmd)
register CMD *cmd;
{
    register CMD *tail;
    CMD *newtail;
    register int i;

    if (cmd->c_expr && (cmd->c_flags & CF_OPTIMIZE) == CFT_FALSE)
	opt_arg(cmd,1, cmd->c_type == C_EXPR);

    while_io(cmd);		/* add $_ =, if necessary */

    /* First find the end of the true list */

    tail = cmd->ucmd.ccmd.cc_true;
    if (tail == Nullcmd)
	return cmd;
    New(112,newtail, 1, CMD);	/* guaranteed continue */
    for (;;) {
	/* optimize "next" to point directly to continue block */
	if (tail->c_type == C_EXPR &&
	    tail->ucmd.acmd.ac_expr &&
	    tail->ucmd.acmd.ac_expr->arg_type == O_NEXT &&
	    (tail->ucmd.acmd.ac_expr->arg_len == 0 ||
	     (cmd->c_label &&
	      strEQ(cmd->c_label,
		    tail->ucmd.acmd.ac_expr[1].arg_ptr.arg_str->str_ptr) )))
	{
	    arg_free(tail->ucmd.acmd.ac_expr);
	    tail->ucmd.acmd.ac_expr = Nullarg;
	    tail->c_type = C_NEXT;
	    if (cmd->ucmd.ccmd.cc_alt != Nullcmd)
		tail->ucmd.ccmd.cc_alt = cmd->ucmd.ccmd.cc_alt;
	    else
		tail->ucmd.ccmd.cc_alt = newtail;
	    tail->ucmd.ccmd.cc_true = Nullcmd;
	}
	else if (tail->c_type == C_IF && !tail->ucmd.ccmd.cc_alt) {
	    if (cmd->ucmd.ccmd.cc_alt != Nullcmd)
		tail->ucmd.ccmd.cc_alt = cmd->ucmd.ccmd.cc_alt;
	    else
		tail->ucmd.ccmd.cc_alt = newtail;
	}
	else if (tail->c_type == C_CSWITCH || tail->c_type == C_NSWITCH) {
	    if (cmd->ucmd.ccmd.cc_alt != Nullcmd) {
		for (i = tail->ucmd.scmd.sc_max; i >= 0; i--)
		    if (!tail->ucmd.scmd.sc_next[i])
			tail->ucmd.scmd.sc_next[i] = cmd->ucmd.ccmd.cc_alt;
	    }
	    else {
		for (i = tail->ucmd.scmd.sc_max; i >= 0; i--)
		    if (!tail->ucmd.scmd.sc_next[i])
			tail->ucmd.scmd.sc_next[i] = newtail;
	    }
	}

	if (!tail->c_next)
	    break;
	tail = tail->c_next;
    }

    /* if there's a continue block, link it to true block and find end */

    if (cmd->ucmd.ccmd.cc_alt != Nullcmd) {
	tail->c_next = cmd->ucmd.ccmd.cc_alt;
	tail = tail->c_next;
	for (;;) {
	    /* optimize "next" to point directly to continue block */
	    if (tail->c_type == C_EXPR &&
		tail->ucmd.acmd.ac_expr &&
		tail->ucmd.acmd.ac_expr->arg_type == O_NEXT &&
		(tail->ucmd.acmd.ac_expr->arg_len == 0 ||
		 (cmd->c_label &&
		  strEQ(cmd->c_label,
			tail->ucmd.acmd.ac_expr[1].arg_ptr.arg_str->str_ptr) )))
	    {
		arg_free(tail->ucmd.acmd.ac_expr);
		tail->ucmd.acmd.ac_expr = Nullarg;
		tail->c_type = C_NEXT;
		tail->ucmd.ccmd.cc_alt = newtail;
		tail->ucmd.ccmd.cc_true = Nullcmd;
	    }
	    else if (tail->c_type == C_IF && !tail->ucmd.ccmd.cc_alt) {
		tail->ucmd.ccmd.cc_alt = newtail;
	    }
	    else if (tail->c_type == C_CSWITCH || tail->c_type == C_NSWITCH) {
		for (i = tail->ucmd.scmd.sc_max; i >= 0; i--)
		    if (!tail->ucmd.scmd.sc_next[i])
			tail->ucmd.scmd.sc_next[i] = newtail;
	    }

	    if (!tail->c_next)
		break;
	    tail = tail->c_next;
	}
	/*SUPPRESS 530*/
	for ( ; tail->c_next; tail = tail->c_next) ;
    }

    /* Here's the real trick: link the end of the list back to the beginning,
     * inserting a "last" block to break out of the loop.  This saves one or
     * two procedure calls every time through the loop, because of how cmd_exec
     * does tail recursion.
     */

    tail->c_next = newtail;
    tail = newtail;
    if (!cmd->ucmd.ccmd.cc_alt)
	cmd->ucmd.ccmd.cc_alt = tail;	/* every loop has a continue now */

#ifndef lint
    Copy((char *)cmd, (char *)tail, 1, CMD);
#endif
    tail->c_type = C_EXPR;
    tail->c_flags ^= CF_INVERT;		/* turn into "last unless" */
    tail->c_next = tail->ucmd.ccmd.cc_true;	/* loop directly back to top */
    tail->ucmd.acmd.ac_expr = make_op(O_LAST,0,Nullarg,Nullarg,Nullarg);
    tail->ucmd.acmd.ac_stab = Nullstab;
    return cmd;
}

CMD *
over(eachstab,cmd)
STAB *eachstab;
register CMD *cmd;
{
    /* hoist "for $foo (@bar)" up into command block */

    cmd->c_flags &= ~CF_OPTIMIZE;	/* clear optimization type */
    cmd->c_flags |= CFT_ARRAY;		/* and set it to do the iteration */
    cmd->c_stab = eachstab;
    cmd->c_short = Str_new(23,0);	/* just to save a field in struct cmd */
    cmd->c_short->str_u.str_useful = -1;

    return cmd;
}

void
cmd_free(cmd)
register CMD *cmd;
{
    register CMD *tofree;
    register CMD *head = cmd;

    if (!cmd)
	return;
    if (cmd->c_head != cmd)
	warn("Malformed cmd links\n");
    while (cmd) {
	if (cmd->c_type != C_WHILE) {	/* WHILE block is duplicated */
	    if (cmd->c_label) {
		Safefree(cmd->c_label);
		cmd->c_label = Nullch;
	    }
	    if (cmd->c_short) {
		str_free(cmd->c_short);
		cmd->c_short = Nullstr;
	    }
	    if (cmd->c_expr) {
		arg_free(cmd->c_expr);
		cmd->c_expr = Nullarg;
	    }
	}
	switch (cmd->c_type) {
	case C_WHILE:
	case C_BLOCK:
	case C_ELSE:
	case C_IF:
	    if (cmd->ucmd.ccmd.cc_true) {
		cmd_free(cmd->ucmd.ccmd.cc_true);
		cmd->ucmd.ccmd.cc_true = Nullcmd;
	    }
	    break;
	case C_EXPR:
	    if (cmd->ucmd.acmd.ac_expr) {
		arg_free(cmd->ucmd.acmd.ac_expr);
		cmd->ucmd.acmd.ac_expr = Nullarg;
	    }
	    break;
	}
	tofree = cmd;
	cmd = cmd->c_next;
	if (tofree != head)		/* to get Saber to shut up */
	    Safefree(tofree);
	if (cmd && cmd == head)		/* reached end of while loop */
	    break;
    }
    Safefree(head);
}

void
arg_free(arg)
register ARG *arg;
{
    register int i;

    if (!arg)
	return;
    for (i = 1; i <= arg->arg_len; i++) {
	switch (arg[i].arg_type & A_MASK) {
	case A_NULL:
	    if (arg->arg_type == O_TRANS) {
		Safefree(arg[i].arg_ptr.arg_cval);
		arg[i].arg_ptr.arg_cval = Nullch;
	    }
	    break;
	case A_LEXPR:
	    if (arg->arg_type == O_AASSIGN &&
	      arg[i].arg_ptr.arg_arg->arg_type == O_LARRAY) {
		char *name =
		  stab_name(arg[i].arg_ptr.arg_arg[1].arg_ptr.arg_stab);

		if (strnEQ("_GEN_",name, 5))	/* array for foreach */
		    hdelete(defstash,name,strlen(name));
	    }
	    /* FALL THROUGH */
	case A_EXPR:
	    arg_free(arg[i].arg_ptr.arg_arg);
	    arg[i].arg_ptr.arg_arg = Nullarg;
	    break;
	case A_CMD:
	    cmd_free(arg[i].arg_ptr.arg_cmd);
	    arg[i].arg_ptr.arg_cmd = Nullcmd;
	    break;
	case A_WORD:
	case A_STAB:
	case A_LVAL:
	case A_READ:
	case A_GLOB:
	case A_ARYLEN:
	case A_LARYLEN:
	case A_ARYSTAB:
	case A_LARYSTAB:
	    break;
	case A_SINGLE:
	case A_DOUBLE:
	case A_BACKTICK:
	    str_free(arg[i].arg_ptr.arg_str);
	    arg[i].arg_ptr.arg_str = Nullstr;
	    break;
	case A_SPAT:
	    spat_free(arg[i].arg_ptr.arg_spat);
	    arg[i].arg_ptr.arg_spat = Nullspat;
	    break;
	}
    }
    free_arg(arg);
}

void
spat_free(spat)
register SPAT *spat;
{
    register SPAT *sp;
    HENT *entry;

    if (!spat)
	return;
    if (spat->spat_runtime) {
	arg_free(spat->spat_runtime);
	spat->spat_runtime = Nullarg;
    }
    if (spat->spat_repl) {
	arg_free(spat->spat_repl);
	spat->spat_repl = Nullarg;
    }
    if (spat->spat_short) {
	str_free(spat->spat_short);
	spat->spat_short = Nullstr;
    }
    if (spat->spat_regexp) {
	regfree(spat->spat_regexp);
	spat->spat_regexp = Null(REGEXP*);
    }

    /* now unlink from spat list */

    for (entry = defstash->tbl_array['_']; entry; entry = entry->hent_next) {
	register HASH *stash;
	STAB *stab = (STAB*)entry->hent_val;

	if (!stab)
	    continue;
	stash = stab_hash(stab);
	if (!stash || stash->tbl_spatroot == Null(SPAT*))
	    continue;
	if (stash->tbl_spatroot == spat)
	    stash->tbl_spatroot = spat->spat_next;
	else {
	    for (sp = stash->tbl_spatroot;
	      sp && sp->spat_next != spat;
	      sp = sp->spat_next)
		/*SUPPRESS 530*/
		;
	    if (sp)
		sp->spat_next = spat->spat_next;
	}
    }
    Safefree(spat);
}

/* Recursively descend a command sequence and push the address of any string
 * that needs saving on recursion onto the tosave array.
 */

static int
cmd_tosave(cmd,willsave)
register CMD *cmd;
int willsave;				/* willsave passes down the tree */
{
    register CMD *head = cmd;
    int shouldsave = FALSE;		/* shouldsave passes up the tree */
    int tmpsave;
    register CMD *lastcmd = Nullcmd;

    while (cmd) {
	if (cmd->c_expr)
	    shouldsave |= arg_tosave(cmd->c_expr,willsave);
	switch (cmd->c_type) {
	case C_WHILE:
	    if (cmd->ucmd.ccmd.cc_true) {
		tmpsave = cmd_tosave(cmd->ucmd.ccmd.cc_true,willsave);

		/* Here we check to see if the temporary array generated for
		 * a foreach needs to be localized because of recursion.
		 */
		if (tmpsave && (cmd->c_flags & CF_OPTIMIZE) == CFT_ARRAY) {
		    if (lastcmd &&
		      lastcmd->c_type == C_EXPR &&
		      lastcmd->c_expr) {
			ARG *arg = lastcmd->c_expr;

			if (arg->arg_type == O_ASSIGN &&
			    arg[1].arg_type == A_LEXPR &&
			    arg[1].arg_ptr.arg_arg->arg_type == O_LARRAY &&
			    strnEQ("_GEN_",
			      stab_name(
				arg[1].arg_ptr.arg_arg[1].arg_ptr.arg_stab),
			      5)) {	/* array generated for foreach */
			    (void)localize(arg);
			}
		    }

		    /* in any event, save the iterator */

		    if (cmd->c_short)  /* Better safe than sorry */
			(void)apush(tosave,cmd->c_short);
		}
		shouldsave |= tmpsave;
	    }
	    break;
	case C_BLOCK:
	case C_ELSE:
	case C_IF:
	    if (cmd->ucmd.ccmd.cc_true)
		shouldsave |= cmd_tosave(cmd->ucmd.ccmd.cc_true,willsave);
	    break;
	case C_EXPR:
	    if (cmd->ucmd.acmd.ac_expr)
		shouldsave |= arg_tosave(cmd->ucmd.acmd.ac_expr,willsave);
	    break;
	}
	lastcmd = cmd;
	cmd = cmd->c_next;
	if (cmd && cmd == head)		/* reached end of while loop */
	    break;
    }
    return shouldsave;
}

static int
arg_tosave(arg,willsave)
register ARG *arg;
int willsave;
{
    register int i;
    int shouldsave = FALSE;

    for (i = arg->arg_len; i >= 1; i--) {
	switch (arg[i].arg_type & A_MASK) {
	case A_NULL:
	    break;
	case A_LEXPR:
	case A_EXPR:
	    shouldsave |= arg_tosave(arg[i].arg_ptr.arg_arg,shouldsave);
	    break;
	case A_CMD:
	    shouldsave |= cmd_tosave(arg[i].arg_ptr.arg_cmd,shouldsave);
	    break;
	case A_WORD:
	case A_STAB:
	case A_LVAL:
	case A_READ:
	case A_GLOB:
	case A_ARYLEN:
	case A_SINGLE:
	case A_DOUBLE:
	case A_BACKTICK:
	    break;
	case A_SPAT:
	    shouldsave |= spat_tosave(arg[i].arg_ptr.arg_spat);
	    break;
	}
    }
    switch (arg->arg_type) {
    case O_RETURN:
	saw_return = TRUE;
	break;
    case O_EVAL:
    case O_SUBR:
	shouldsave = TRUE;
	break;
    }
    if (willsave && arg->arg_ptr.arg_str)
	(void)apush(tosave,arg->arg_ptr.arg_str);
    return shouldsave;
}

static int
spat_tosave(spat)
register SPAT *spat;
{
    int shouldsave = FALSE;

    if (spat->spat_runtime)
	shouldsave |= arg_tosave(spat->spat_runtime,FALSE);
    if (spat->spat_repl) {
	shouldsave |= arg_tosave(spat->spat_repl,FALSE);
    }

    return shouldsave;
}

