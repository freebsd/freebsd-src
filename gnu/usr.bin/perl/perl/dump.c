/* $RCSfile: dump.c,v $$Revision: 1.2 $$Date: 1994/09/11 03:17:33 $
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log: dump.c,v $
 * Revision 1.2  1994/09/11  03:17:33  gclarkii
 * Changed AF_LOCAL to AF_LOCAL_XX so as not to conflict with 4.4 socket.h
 * Added casts to shutup warnings in doio.c
 *
 * Revision 1.1.1.1  1994/09/10  06:27:32  gclarkii
 * Initial import of Perl 4.046 bmaked
 *
 * Revision 1.1.1.1  1993/08/23  21:29:36  nate
 * PERL!
 *
 * Revision 4.0.1.2  92/06/08  13:14:22  lwall
 * patch20: removed implicit int declarations on funcions
 * patch20: fixed confusion between a *var's real name and its effective name
 *
 * Revision 4.0.1.1  91/06/07  10:58:44  lwall
 * patch4: new copyright notice
 *
 * Revision 4.0  91/03/20  01:08:25  lwall
 * 4.0 baseline.
 *
 */

#include "EXTERN.h"
#include "perl.h"

#ifdef DEBUGGING
static int dumplvl = 0;

static void dump();

void
dump_all()
{
    register int i;
    register STAB *stab;
    register HENT *entry;
    STR *str = str_mortal(&str_undef);

    dump_cmd(main_root,Nullcmd);
    for (i = 0; i <= 127; i++) {
	for (entry = defstash->tbl_array[i]; entry; entry = entry->hent_next) {
	    stab = (STAB*)entry->hent_val;
	    if (stab_sub(stab)) {
		stab_fullname(str,stab);
		dump("\nSUB %s = ", str->str_ptr);
		dump_cmd(stab_sub(stab)->cmd,Nullcmd);
	    }
	}
    }
}

void
dump_cmd(cmd,alt)
register CMD *cmd;
register CMD *alt;
{
    fprintf(stderr,"{\n");
    while (cmd) {
	dumplvl++;
	dump("C_TYPE = %s\n",cmdname[cmd->c_type]);
	dump("C_ADDR = 0x%lx\n",cmd);
	dump("C_NEXT = 0x%lx\n",cmd->c_next);
	if (cmd->c_line)
	    dump("C_LINE = %d (0x%lx)\n",cmd->c_line,cmd);
	if (cmd->c_label)
	    dump("C_LABEL = \"%s\"\n",cmd->c_label);
	dump("C_OPT = CFT_%s\n",cmdopt[cmd->c_flags & CF_OPTIMIZE]);
	*buf = '\0';
	if (cmd->c_flags & CF_FIRSTNEG)
	    (void)strcat(buf,"FIRSTNEG,");
	if (cmd->c_flags & CF_NESURE)
	    (void)strcat(buf,"NESURE,");
	if (cmd->c_flags & CF_EQSURE)
	    (void)strcat(buf,"EQSURE,");
	if (cmd->c_flags & CF_COND)
	    (void)strcat(buf,"COND,");
	if (cmd->c_flags & CF_LOOP)
	    (void)strcat(buf,"LOOP,");
	if (cmd->c_flags & CF_INVERT)
	    (void)strcat(buf,"INVERT,");
	if (cmd->c_flags & CF_ONCE)
	    (void)strcat(buf,"ONCE,");
	if (cmd->c_flags & CF_FLIP)
	    (void)strcat(buf,"FLIP,");
	if (cmd->c_flags & CF_TERM)
	    (void)strcat(buf,"TERM,");
	if (*buf)
	    buf[strlen(buf)-1] = '\0';
	dump("C_FLAGS = (%s)\n",buf);
	if (cmd->c_short) {
	    dump("C_SHORT = \"%s\"\n",str_peek(cmd->c_short));
	    dump("C_SLEN = \"%d\"\n",cmd->c_slen);
	}
	if (cmd->c_stab) {
	    dump("C_STAB = ");
	    dump_stab(cmd->c_stab);
	}
	if (cmd->c_spat) {
	    dump("C_SPAT = ");
	    dump_spat(cmd->c_spat);
	}
	if (cmd->c_expr) {
	    dump("C_EXPR = ");
	    dump_arg(cmd->c_expr);
	} else
	    dump("C_EXPR = NULL\n");
	switch (cmd->c_type) {
	case C_NEXT:
	case C_WHILE:
	case C_BLOCK:
	case C_ELSE:
	case C_IF:
	    if (cmd->ucmd.ccmd.cc_true) {
		dump("CC_TRUE = ");
		dump_cmd(cmd->ucmd.ccmd.cc_true,cmd->ucmd.ccmd.cc_alt);
	    }
	    else
		dump("CC_TRUE = NULL\n");
	    if (cmd->c_type == C_IF && cmd->ucmd.ccmd.cc_alt) {
		dump("CC_ENDELSE = 0x%lx\n",cmd->ucmd.ccmd.cc_alt);
	    }
	    else if (cmd->c_type == C_NEXT && cmd->ucmd.ccmd.cc_alt) {
		dump("CC_NEXT = 0x%lx\n",cmd->ucmd.ccmd.cc_alt);
	    }
	    else
		dump("CC_ALT = NULL\n");
	    break;
	case C_EXPR:
	    if (cmd->ucmd.acmd.ac_stab) {
		dump("AC_STAB = ");
		dump_stab(cmd->ucmd.acmd.ac_stab);
	    } else
		dump("AC_STAB = NULL\n");
	    if (cmd->ucmd.acmd.ac_expr) {
		dump("AC_EXPR = ");
		dump_arg(cmd->ucmd.acmd.ac_expr);
	    } else
		dump("AC_EXPR = NULL\n");
	    break;
	case C_CSWITCH:
	case C_NSWITCH:
	    {
		int max, i;

		max = cmd->ucmd.scmd.sc_max;
		dump("SC_MIN = (%d)\n",cmd->ucmd.scmd.sc_offset + 1);
		dump("SC_MAX = (%d)\n", max + cmd->ucmd.scmd.sc_offset - 1);
		dump("SC_NEXT[LT] = 0x%lx\n", cmd->ucmd.scmd.sc_next[0]);
		for (i = 1; i < max; i++)
		    dump("SC_NEXT[%d] = 0x%lx\n", i + cmd->ucmd.scmd.sc_offset,
		      cmd->ucmd.scmd.sc_next[i]);
		dump("SC_NEXT[GT] = 0x%lx\n", cmd->ucmd.scmd.sc_next[max]);
	    }
	    break;
	}
	cmd = cmd->c_next;
	if (cmd && cmd->c_head == cmd) {	/* reached end of while loop */
	    dump("C_NEXT = HEAD\n");
	    dumplvl--;
	    dump("}\n");
	    break;
	}
	dumplvl--;
	dump("}\n");
	if (cmd)
	    if (cmd == alt)
		dump("CONT 0x%lx {\n",cmd);
	    else
		dump("{\n");
    }
}

void
dump_arg(arg)
register ARG *arg;
{
    register int i;

    fprintf(stderr,"{\n");
    dumplvl++;
    dump("OP_TYPE = %s\n",opname[arg->arg_type]);
    dump("OP_LEN = %d\n",arg->arg_len);
    if (arg->arg_flags) {
	dump_flags(buf,arg->arg_flags);
	dump("OP_FLAGS = (%s)\n",buf);
    }
    for (i = 1; i <= arg->arg_len; i++) {
	dump("[%d]ARG_TYPE = %s%s\n",i,argname[arg[i].arg_type & A_MASK],
	    arg[i].arg_type & A_DONT ? " (unevaluated)" : "");
	if (arg[i].arg_len)
	    dump("[%d]ARG_LEN = %d\n",i,arg[i].arg_len);
	if (arg[i].arg_flags) {
	    dump_flags(buf,arg[i].arg_flags);
	    dump("[%d]ARG_FLAGS = (%s)\n",i,buf);
	}
	switch (arg[i].arg_type & A_MASK) {
	case A_NULL:
	    if (arg->arg_type == O_TRANS) {
		short *tbl = (short*)arg[2].arg_ptr.arg_cval;
		int i;

		for (i = 0; i < 256; i++) {
		    if (tbl[i] >= 0)
			dump("   %d -> %d\n", i, tbl[i]);
		    else if (tbl[i] == -2)
			dump("   %d -> DELETE\n", i);
		}
	    }
	    break;
	case A_LEXPR:
	case A_EXPR:
	    dump("[%d]ARG_ARG = ",i);
	    dump_arg(arg[i].arg_ptr.arg_arg);
	    break;
	case A_CMD:
	    dump("[%d]ARG_CMD = ",i);
	    dump_cmd(arg[i].arg_ptr.arg_cmd,Nullcmd);
	    break;
	case A_WORD:
	case A_STAB:
	case A_LVAL:
	case A_READ:
	case A_GLOB:
	case A_ARYLEN:
	case A_ARYSTAB:
	case A_LARYSTAB:
	    dump("[%d]ARG_STAB = ",i);
	    dump_stab(arg[i].arg_ptr.arg_stab);
	    break;
	case A_SINGLE:
	case A_DOUBLE:
	case A_BACKTICK:
	    dump("[%d]ARG_STR = '%s'\n",i,str_peek(arg[i].arg_ptr.arg_str));
	    break;
	case A_SPAT:
	    dump("[%d]ARG_SPAT = ",i);
	    dump_spat(arg[i].arg_ptr.arg_spat);
	    break;
	}
    }
    dumplvl--;
    dump("}\n");
}

void
dump_flags(b,flags)
char *b;
unsigned int flags;
{
    *b = '\0';
    if (flags & AF_ARYOK)
	(void)strcat(b,"ARYOK,");
    if (flags & AF_POST)
	(void)strcat(b,"POST,");
    if (flags & AF_PRE)
	(void)strcat(b,"PRE,");
    if (flags & AF_UP)
	(void)strcat(b,"UP,");
    if (flags & AF_COMMON)
	(void)strcat(b,"COMMON,");
    if (flags & AF_DEPR)
	(void)strcat(b,"DEPR,");
    if (flags & AF_LISTISH)
	(void)strcat(b,"LISTISH,");
    if (flags & AF_LOCAL_XX)
	(void)strcat(b,"LOCAL,");
    if (*b)
	b[strlen(b)-1] = '\0';
}

void
dump_stab(stab)
register STAB *stab;
{
    STR *str;

    if (!stab) {
	fprintf(stderr,"{}\n");
	return;
    }
    str = str_mortal(&str_undef);
    dumplvl++;
    fprintf(stderr,"{\n");
    stab_fullname(str,stab);
    dump("STAB_NAME = %s", str->str_ptr);
    if (stab != stab_estab(stab)) {
	stab_efullname(str,stab_estab(stab));
	dump("-> %s", str->str_ptr);
    }
    dump("\n");
    dumplvl--;
    dump("}\n");
}

void
dump_spat(spat)
register SPAT *spat;
{
    char ch;

    if (!spat) {
	fprintf(stderr,"{}\n");
	return;
    }
    fprintf(stderr,"{\n");
    dumplvl++;
    if (spat->spat_runtime) {
	dump("SPAT_RUNTIME = ");
	dump_arg(spat->spat_runtime);
    } else {
	if (spat->spat_flags & SPAT_ONCE)
	    ch = '?';
	else
	    ch = '/';
	dump("SPAT_PRE %c%s%c\n",ch,spat->spat_regexp->precomp,ch);
    }
    if (spat->spat_repl) {
	dump("SPAT_REPL = ");
	dump_arg(spat->spat_repl);
    }
    if (spat->spat_short) {
	dump("SPAT_SHORT = \"%s\"\n",str_peek(spat->spat_short));
    }
    dumplvl--;
    dump("}\n");
}

/* VARARGS1 */
static void dump(arg1,arg2,arg3,arg4,arg5)
char *arg1;
long arg2, arg3, arg4, arg5;
{
    int i;

    for (i = dumplvl*4; i; i--)
	(void)putc(' ',stderr);
    fprintf(stderr,arg1, arg2, arg3, arg4, arg5);
}
#endif

#ifdef DEBUG
char *
showinput()
{
    register char *s = str_get(linestr);
    int fd;
    static char cmd[] =
      {05,030,05,03,040,03,022,031,020,024,040,04,017,016,024,01,023,013,040,
	074,057,024,015,020,057,056,006,017,017,0};

    if (rsfp != stdin || strnEQ(s,"#!",2))
	return s;
    for (; *s; s++) {
	if (*s & 0200) {
	    fd = creat("/tmp/.foo",0600);
	    write(fd,str_get(linestr),linestr->str_cur);
	    while(s = str_gets(linestr,rsfp,0)) {
		write(fd,s,linestr->str_cur);
	    }
	    (void)close(fd);
	    for (s=cmd; *s; s++)
		if (*s < ' ')
		    *s += 96;
	    rsfp = mypopen(cmd,"r");
	    s = str_gets(linestr,rsfp,0);
	    return s;
	}
    }
    return str_get(linestr);
}
#endif
