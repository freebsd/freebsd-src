/* $Header: procs.c,v 2.3 88/09/19 12:55:22 nhall Exp $ */
/* $Source: /var/home/tadl/src/argo/xebec/RCS/procs.c,v $ */
/*
 * This code is such a kludge that I don't want to put my name on it.
 * It was a ridiculously fast hack and needs rewriting.
 * However it does work...
 */

#include <stdio.h>
#include <strings.h>
#include "malloc.h"
#include "main.h"
#include "debug.h"
#include "sets.h"
#include "procs.h"

struct Predicate {
	int p_index;
	int p_transno;
	char *p_str;
	struct Predicate *p_next;
};

struct Stateent {
	int s_index;
	int s_newstate;
	int s_action;
	struct Stateent *s_next;
};

struct Object *SameState = (struct Object *)-1;
int Index = 0;
int Nstates = 0;
int Nevents = 0;
struct Predicate **Predlist;
struct Stateent **Statelist;
extern FILE *astringfile;

end_events() {
	int size, part;
	char *addr;

	IFDEBUG(X)
		/* finish estring[], start astring[] */
	if(debug['X'] < 2 )
		fprintf(astringfile, "};\n\nchar *%s_astring[] = {\n\"NULLACTION\",\n",
			protocol);
	ENDDEBUG
	/* NOSTRICT */
	Statelist = 
	  (struct Stateent **) Malloc((Nstates+1) * sizeof(struct Statent *));
	/* NOSTRICT */
	Predlist =  
	  (struct Predicate **) 
	  Malloc ( (((Nevents)<<Eventshift)+Nstates)*sizeof(struct Predicate *) );

	size = (((Nevents)<<Eventshift)+Nstates)*sizeof(struct Predicate *) ;
	addr = (char *)Predlist;
	IFDEBUG(N)
		fprintf(OUT, "Predlist at 0x%x, sbrk 0x%x bzero size %d at addr 0x%x\n",
		Predlist, sbrk(0), size, addr);
	ENDDEBUG
#define BZSIZE 8192
	while(size) {
		part = size>BZSIZE?BZSIZE:size;
	IFDEBUG(N)
		fprintf(OUT, "bzero addr 0x%x part %d size %d\n",addr, part, size);
	ENDDEBUG
		bzero(addr, part);
	IFDEBUG(N)
		fprintf(OUT, "after bzero addr 0x%x part %d size %d\n",addr, part, size);
	ENDDEBUG
		addr += part;
		size -= part;

	}
	IFDEBUG(N)
		fprintf(OUT, "endevents..done \n");
	ENDDEBUG
}

int acttable(f,actstring)
char *actstring;
FILE *f;
{
	static Actindex = 0;
	extern FILE *astringfile;
	extern int pgoption;

	IFDEBUG(a)
		fprintf(OUT,"acttable()\n");
	ENDDEBUG
	fprintf(f, "case 0x%x: \n", ++Actindex);

	if(pgoption) {
		fprintf(f, "asm(\" # dummy statement\");\n");
		fprintf(f, "asm(\"_Xebec_action_%x: \");\n", Actindex );
		fprintf(f, "asm(\".data\");\n");
		fprintf(f, "asm(\".globl _Xebec_action_%x# X profiling\");\n",
			Actindex );
		fprintf(f, "asm(\".long 0 # X profiling\");\n");
		fprintf(f, "asm(\".text # X profiling\");\n");
		fprintf(f, "asm(\"cas r0,r15,r0 # X profiling\");\n");
		fprintf(f, "asm(\"bali r15,mcount   # X profiling\");\n");
	}

	fprintf(f, "\t\t%s\n\t\t break;\n", actstring);
	IFDEBUG(X)
		if(debug['X']<2) {
			register int len = 0;
			fputc('"',astringfile);
			while(*actstring) {
				if( *actstring == '\n' ) {
					fputc('\\', astringfile);
					len++;
					fputc('n', astringfile);
				} else if (*actstring == '\\') {
					fputc('\\', astringfile);
					len ++;
					fputc('\\', astringfile);
				} else if (*actstring == '\"') {
					fputc('\\', astringfile);
					len ++;
					fputc('\"', astringfile);
				} else fputc(*actstring, astringfile);
				actstring++;
				len++;
			}
			fprintf(astringfile,"\",\n");
			if (len > LINELEN) {
				fprintf(stderr, "Action too long: %d\n",len); Exit(-1);
			}
		}
	ENDDEBUG

	return(Actindex);
}

static int Npred=0, Ndefpred=0, Ntrans=0, Ndefevent=0, Nnulla=0;

statetable(string, oldstate, newstate, action, event)
char *string;
int action;
struct Object *oldstate, *newstate, *event; 
{
	register int different;

	IFDEBUG(a)
		fprintf(OUT,"statetable(0x%x, 0x%x,0x%x, 0x%x)\n",
			string, oldstate, newstate, action);
		fprintf(OUT,"statetable(%s, %s,%s, 0x%x)\n",
			string, oldstate->obj_name, newstate->obj_name, action);
	ENDDEBUG

	if( !action) Nnulla++;
	if( newstate->obj_kind == OBJ_SET) {
		fprintf(stderr, "Newstate cannot be a set\n");
		Exit(-1);
	}
	different = (newstate != SameState);

	(void) predtable( oldstate, event, string,
				action, (newstate->obj_number) * different );
	IFDEBUG(a)
		fprintf(OUT,"EXIT statetable\n");
	ENDDEBUG
}

stateentry(index, oldstate, newstate, action)
int index, action;
int oldstate, newstate; 
{
	extern FILE *statevalfile;

	IFDEBUG(a)
		fprintf(OUT,"stateentry(0x%x,0x%x,0x%x,0x%x) Statelist@0x%x, val 0x%x\n",
			index, oldstate, newstate,action, &Statelist, Statelist);
	ENDDEBUG


	fprintf(statevalfile, "{0x%x,0x%x},\n", newstate, action);
}

int predtable(os, oe, str, action, newstate)
struct Object *os, *oe;
char *str;
int action, newstate;
{
	register struct Predicate *p, **q;
	register int event, state;
	register struct Object *e, *s;
	struct Object *firste;

	if (oe == (struct Object *)0 ) {
		Ndefevent ++;
		fprintf(stderr, "DEFAULT EVENTS aren't implemented; trans ignored\n");
		return;
	}
	Ntrans++;
	IFDEBUG(g)
		fprintf(stdout,
		"PREDTAB: s %5s;  e %5s\n", os->obj_kind==OBJ_SET?"SET":"item",
			oe->obj_kind==OBJ_SET?"SET":"item");
	ENDDEBUG
	if (os->obj_kind == OBJ_SET) s = os->obj_members;
	else s = os;
	if (oe->obj_kind == OBJ_SET) firste = oe->obj_members;
	else firste = oe;
	if(newstate) {
		fprintf(statevalfile, "{0x%x,0x%x},\n",newstate, action);
		Index++;
	}
	while (s) {
		if( !newstate ) { /* !newstate --> SAME */
			/* i.e., use old obj_number */
			fprintf(statevalfile, "{0x%x,0x%x},\n",s->obj_number, action);
			Index++;
		}
		e = firste;
		while (e) {
			event = e->obj_number; state = s->obj_number;
			IFDEBUG(g)
				fprintf(stdout,"pred table event=0x%x, state 0x%x\n",
				event, state);
				fflush(stdout);
			ENDDEBUG
			if( !str /* DEFAULT PREDICATE */) {
				Ndefpred++;
				IFDEBUG(g)
					fprintf(stdout,
					"DEFAULT pred state 0x%x, event 0x%x, Index 0x%x\n",
					state, event, Index);
					fflush(stdout);
				ENDDEBUG
			} else 
				Npred++;
			/* put at END of list */
#ifndef LINT
			IFDEBUG(g)
				fprintf(stdout, 
				"predicate for event 0x%x, state 0x%x is 0x%x, %s\n", 
				event, state, Index, str);
				fflush(stdout);
			ENDDEBUG
#endif LINT
			for( ((q = &Predlist[(event<<Eventshift)+state]), 
					 (p = Predlist[(event<<Eventshift)+state]));
							p ; p = p->p_next ) {
				q = &p->p_next;
			}

			p = (struct Predicate *)Malloc(sizeof(struct Predicate));
			p->p_next = (struct Predicate *)0;
			p->p_str = str;
			p->p_index = Index;
			p->p_transno = transno;
			*q = p;

			IFDEBUG(g)
				fprintf(stdout, 
			  	  "predtable index 0x%x, transno %d, E 0x%x, S 0x%x\n",
					 Index, transno, e, s);
			ENDDEBUG

			e = e->obj_members;
		}
		s = s->obj_members;
	}
	return Index ;
}

printprotoerrs()
{
	register int e,s;

	fprintf(stderr, "[ Event, State ] without any transitions :\n");
	for(e = 0; e < Nevents; e++) { 
		fprintf(stderr, "Event 0x%x: states ", e);
		for(s = 0; s < Nstates; s++) {
			if( Predlist[(e<<Eventshift)+s] == 0 )
				fprintf(stderr, "0x%x ", s);
		}
		fprintf(stderr, "\n");
	}
}

#ifndef LINT
dump_predtable(f)
FILE *f;
{
	struct Predicate *p;
	register int e,s, hadapred;
	int defaultindex;
	int defaultItrans;
	extern int bytesmalloced;
	extern int byteswasted;

#ifdef notdef
	fprintf(stdout,
		" Xebec used %8d bytes of storage, wasted %8d bytes\n", 
		bytesmalloced, byteswasted);
#endif notdef
	fprintf(stdout, 
		" %8d states\n %8d events\n %8d transitions\n",
		Nstates, Nevents, Ntrans);
	fprintf(stdout,
		" %8d predicates\n %8d default predicates used\n",
		Npred, Ndefpred);
	fprintf(stdout,
		" %8d null actions\n",
		Nnulla);

	putdriver(f, 5);
	for(e = 0; e < Nevents; e++) { for(s = 0; s < Nstates; s++) {
		p = Predlist[(e<<Eventshift)+s];
		hadapred=0;
		defaultindex=0;
		defaultItrans=0;
		if(p) {
			IFDEBUG(d)
				fflush(f);
			ENDDEBUG
			while(p) {
				if(p->p_str) {
					if(!hadapred)
						fprintf(f, "case 0x%x:\n\t", (e<<Eventshift) + s);
					hadapred = 1;
					fprintf(f, "if %s return 0x%x;\n\t else ", 
					p->p_str, p->p_index);
				} else {
					if(defaultindex) {
						fprintf(stderr, 
"\nConflict between transitions %d and %d: duplicate default \n",
						p->p_transno, defaultItrans);
						Exit(-1);
					}
					defaultindex = p->p_index;
					defaultItrans = p->p_transno;
				}
				p = p->p_next;
			}
			if( hadapred)  {
				fprintf(f, "return 0x%x;\n", defaultindex);
			}
			IFDEBUG(d)
				fflush(f);
			ENDDEBUG
		} 
		IFDEBUG(g)
		fprintf(stdout, 
		"loop: e 0x%x s 0x%x hadapred 0x%x dindex 0x%x for trans 0x%x\n",
			e, s, hadapred, defaultindex, defaultItrans);
		ENDDEBUG
		if ( hadapred ) {
			/* put a -1 in the array  - Predlist is temporary storage */
			Predlist[(e<<Eventshift)+s] = (struct Predicate *)(-1);
		} else {
			/* put defaultindex in the array */
			/* if defaultindex is zero, then the driver will
			 * cause an erroraction (same as if no default
			 * were given and none of the predicates were true;
			 * also same as if no preds or defaults were given
			 * for this combo)
			 */
			Predlist[(e<<Eventshift)+s] = (struct Predicate *)(defaultindex);
		}
	} }
	fprintf(f, "default: return 0;\n} /* end switch */\n");
#ifdef notdef
	fprintf(f, "/*NOTREACHED*/return 0;\n} /* _Xebec_index() */\n");
#else notdef
	fprintf(f, "} /* _Xebec_index() */\n");
#endif notdef
	fprintf(f, "static int inx[%d][%d] = { {", Nevents+1,Nstates);
	for(s = 0; s< Nstates; s++) fprintf(f, "0,"); /* event 0 */
	fprintf(f, "},\n");

	for(e = 0; e < Nevents; e++) { 
		fprintf(f, " {"); 
		for(s = 0; s < Nstates; s++) {
			register struct Predicate *xyz = Predlist[(e<<Eventshift)+s];
			/* this kludge is to avoid a lint msg. concerning
			 * loss of bits 
			 */
			if (xyz == (struct Predicate *)(-1))
				fprintf(f, "-1,");
			else
				fprintf(f, "0x%x,", Predlist[(e<<Eventshift)+s]);
		}
		fprintf(f, " },\n"); 
	}
	fprintf(f, "};");
}
#endif LINT

char *
stash(buf)
char *buf;
{
	register int len;
	register char *c;

	/* grot */
	len = strlen(buf);
	c = Malloc(len+1);
#ifdef LINT
	c =
#endif LINT
	strcpy(c, buf);

	IFDEBUG(z)
		fprintf(stdout,"stash %s at 0x%x\n", c,c);
	ENDDEBUG
	return(c);
}

#ifdef notdef
dump_pentry(event,state)
int event,state;
{
	register struct Predicate *p, **q;

	for( 
	((q = &Predlist[(event<<Eventshift) +state]), 
	 (p = Predlist[(event<<Eventshift) + state]));
		p!= (struct Predicate *)0 ; p = p->p_next ) {
#ifndef LINT
		IFDEBUG(a)
			fprintf(OUT, 
			"dump_pentry for event 0x%x, state 0x%x is 0x%x\n", 
			 event, state, p);
		ENDDEBUG
#endif LINT
		q = &p->p_next;
	}
}
#endif notdef
