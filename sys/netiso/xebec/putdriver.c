/* $Header: putdriver.c,v 2.2 88/09/19 12:55:27 nhall Exp $ */
/* $Source: /var/home/tadl/src/argo/xebec/RCS/putdriver.c,v $ */

/*
 * This code is such a kludge that I don't want to put my name on it.
 * It was a ridiculously fast hack and needs rewriting.
 * However it does work...
 */

/* The original idea was to put all the driver code
 * in one place so it would be easy to modify
 * but as hacks got thrown in it got worse and worse...
 * It's to the point where a user would be better off
 * writing his own driver and xebec should JUST produce
 * the tables.
 */

#include <stdio.h>
#include "main.h"
#include "debug.h"

extern char protocol[];
char Eventshiftstring[10];
static char statename[] = {'_', 's', 't', 'a', 't', 'e', 0 };

static char *strings[] = {

#define PART1 { 0,3 }

	"\n#include \"",
	kerneldirname,
	protocol,
	"_states.h\"",
	(char *)0,
	(char *)0,
	(char *)0,
	(char *)0,
	(char *)0,
	(char *)0,

#define PART12 { 10,12 }
	"\n\nstatic struct act_ent {\n",
	"\tint a_newstate;\n\tint a_action;\n",
	"} statetable[] = { {0,0},\n",
	(char *)0,
	(char *)0,
	(char *)0,
	(char *)0,
	(char *)0,
	(char *)0,
	(char *)0,

#define PART2 { 20,20 }
	"};\n",
	(char *)0,
	(char *)0,
	(char *)0,
	(char *)0,
	(char *)0,
	(char *)0,
	(char *)0,
	(char *)0,
	(char *)0,

#define PART3 { 30,41 }
	"\n",
	protocol,
	"_driver(p, e)\nregister ",
	protocol,
	PCBNAME,
	" *p;\nregister struct ",
	protocol,
	"_event *e;\n",
	"{\n",
		"\tregister int index, error=0;\n",
		"\tstruct act_ent *a;\n",
		"\tstatic struct act_ent erroraction = {0,-1};\n",
	(char *)0,
	(char *)0,
	(char *)0,
	(char *)0,
	(char *)0,
	(char *)0,
	(char *)0,
	(char *)0,

#define PART4 { 50,54 }

	"\textern int ",
	protocol,
	"_debug;\n\textern FILE *",
	protocol,
	"_astringfile;\n", 
	(char *)0,
	(char *)0,
	(char *)0,
	(char *)0,
	(char *)0,

#define PART6 { 60, 65 }
	"\n\tindex = inx[1 + e->ev_number][p->",
		protocol,
		statename,
		"];\n\tif(index<0) index=_Xebec_index(e, p);\n",
		"\tif (index==0) {\n\t\ta = &erroraction;\n",
		"\t} else\n\t\ta = &statetable[index];\n\n",
	(char *)0,
	(char *)0,
	(char *)0,
	(char *)0,

#define PART7 {70, 77 }
	"\tif(",
	protocol,
	"_debug) fprintf(",
	protocol,
	"_astringfile, \"%15s <-- %15s [%15s] \\n\\t%s\\n\",\n",
	"\t\tsstring[a->a_newstate], sstring[p->",
	protocol,
	"_state], estring[e->ev_number], astring[a->a_action]);\n\n",
	(char *)0,
	(char *)0,

#define PART8 { 80, 84 }
		"\tif(a->a_action)\n",
		"\t\terror = _Xebec_action( a->a_action, e, p );\n",
		"\tif(error==0)\n\tp->",
		protocol,
		"_state = a->a_newstate;\n\treturn error;\n}\n",
	(char *)0,
	(char *)0,
	(char *)0,
	(char *)0,
	(char *)0,

#define PART9 { 90, 99 }
	"\n_XEBEC_PG int _Xebec_action(a,e,p)\nint a;\nstruct ",
	protocol,
	"_event *e;\n",
	protocol, 
	PCBNAME,
	" *p;\n{\n",
	"switch(a) {\n",
	"case -1:  return ",
	protocol,
	"_protocol_error(e,p);\n",
	(char *)0,

#define PART10 { 101, 105 }
	"\tif(",
	protocol,
	"_debug) fprintf(",
	protocol,
	"_astringfile, \"index 0x%5x\\n\", index);\n",
	(char *)0,
	(char *)0,
	(char *)0,
	(char *)0,

#define PART5 { 110, 121 }
	"\n_XEBEC_PG int\n_Xebec_index( e,p )\n",
	"\tstruct ",
	protocol,
	"_event *e;\n\t", 
	protocol, 
	PCBNAME,
	" *p;\n{\nswitch( (e->ev_number<<",
	Eventshiftstring,
	")+(p->",
	protocol, 
	statename,
	") ) {\n", 
	(char *)0,
	(char *)0,
	(char *)0,
	(char *)0,
	(char *)0,
	(char *)0,
	(char *)0,
	(char *)0,

#define PART11 {130, 137 }
	"\tIFTRACE(D_DRIVER)\n",
	"\t",
	protocol,
	"trace(DRIVERTRACE,",
	"\t\ta->a_newstate, p->",
	protocol,
	"_state, e->ev_number, a->a_action, 0);\n\n",
	"\tENDTRACE\n",
	(char *)0,
	(char *)0,

#define PART13 {140, 147 }
	"\tif(",
	protocol,
	"_debug) fprintf(",
	protocol,
	"_astringfile, \"%15s <-- %15s [%15s] \\n\",\n",
	"\t\tsstring[a->a_newstate], sstring[p->",
	protocol,
	"_state], estring[e->ev_number]);\n\n",
	(char *)0,
	(char *)0,

#define PART14 { 150,150 }
	"#define _XEBEC_PG static\n",

#define PART15 { 151,151 }
	"#define _XEBEC_PG  \n",

};

static struct { int start; int finish; } parts[] = {
	{ 0,0 },
	PART1,
	PART2,
	PART3,
	PART4,
	PART5,
	PART6,
	PART7,
	PART8,
	PART9,
	PART10,
	PART11,
	PART12,
	PART13,
	PART14,
	PART15,
};

putdriver(f, x) 
FILE *f;
int x;
{
	register int i; 

	for( i = parts[x].start; i<= parts[x].finish; i++)
		fprintf(f, "%s", strings[i]);
	IFDEBUG(d)
		fflush(f);
	ENDDEBUG
}
