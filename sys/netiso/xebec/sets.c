/*
 *	from: sets.c,v 2.3 88/09/19 12:55:30 nhall Exp
 *	$Id: sets.c,v 1.2 1993/10/16 21:33:22 rgrimes Exp $
 */

/*
 * This code is such a kludge that I don't want to put my name on it.
 * It was a ridiculously fast hack and needs rewriting.
 * However it does work...
 */
#include "main.h"
#include "malloc.h"
#include "sets.h"
#include "debug.h"
#include <stdio.h>

struct Object *CurrentEvent = (struct Object *)0;
struct Object *Objtree;
struct Object dummy;
/* 
 * define a set w/ type and name
 * return a set number 
 */
#undef NULL
#define NULL (struct Object *)0

static FILE *Sfile, *Efile;
extern FILE *astringfile;
char *Noname = "Unnamed set\0";

initsets(f,s)
FILE *f, *s;
{
	static char errorstring[20];
	extern struct Object *SameState;
	Efile = f;
	Sfile = s;

	IFDEBUG(X)
		fprintf(astringfile, "char *%s_sstring[] = {\n", protocol);
	ENDDEBUG
	sprintf(errorstring, "%sERROR\0", ST_PREFIX);
	defineitem(STATESET, errorstring, (char *)0);	/* state 0 */
	SameState = (struct Object *) Malloc( sizeof (struct Object) );
	SameState->obj_kind = OBJ_ITEM;
	SameState->obj_type = STATESET;
	SameState->obj_name = "SAME";
	SameState->obj_struc = (char *)0;
	SameState->obj_number = 0;
	SameState->obj_members = (struct Object *)0;
	SameState->obj_left = (struct Object *)0;
	SameState->obj_right = (struct Object *)0;
	SameState->obj_parent = (struct Object *)0;
}

/*
 * get a set based on its type and name
 * returns address of an Object, may be set or item
 */

struct Object *lookup(type, name)
unsigned char type;
char *name;
{
	register struct Object *p = Objtree;
	int val = 1 ;

	IFDEBUG(o)
		fprintf(stdout,"lookup 0x%x,%s \n",
			type, name);
	ENDDEBUG

	while( p && val ) {
		IFDEBUG(o)
		fprintf(OUT, "lookup strcmp 0x%x,%s, 0x%x,%s\n",
			name, name, OBJ_NAME(p), OBJ_NAME(p));
		ENDDEBUG
		if( p->obj_name == (char *)0 ) {
			fprintf(stderr, "Unnamed set in table!\n");
			Exit(-1);
		}
		val =  (int) strcmp(name, OBJ_NAME(p));
		if(val < 0) {
			/* left */
			p = p->obj_left;
		} else if (val > 0) {
			/* right */
			p = p->obj_right;
		}
	}
	if( p && ( p->obj_type != type)) {
		fprintf(stdout, "lookup(0x%x,%s) found wrong obj type 0x%x\n",
			type,name, p->obj_type);
		p = NULL;
	}
	IFDEBUG(o)
		fprintf(stdout,"lookup 0x%x,%s returning 0x%x\n",type, name, p);
	ENDDEBUG
	return(p);
}

static int states_done  = 0;

end_states(f)
FILE *f;
{
	register unsigned n = Nstates;
	register int i;
	extern char Eventshiftstring[];

	states_done = 1;

	for( i = 0; ;i++) {
		if( (n >>= 1) <= 0 ) break;
	}
	Eventshift = i+1;
	IFDEBUG(d)
		fprintf(OUT, "Eventshift=%d\n", Eventshift);
	ENDDEBUG
	sprintf(Eventshiftstring, "%d\0",Eventshift);
	fprintf(f, "struct %s_event {\n\tint ev_number;\n", &protocol[0]);
	IFDEBUG(X)
		/* finish sstring[] & start estring[] */
		fprintf(astringfile, 
		"};\n\nchar *%s_estring[] = {\n", protocol);
	ENDDEBUG
}

int FirstEventAttribute = 1;

static 
insert(o) 
struct Object *o;
{
	struct Object *p = Objtree;
	struct Object **q = &Objtree; 
	int val=1;


	if (o->obj_name == (char *)0) {
		fprintf(stderr, "Internal Error: inserting unnamed object\n");
		Exit(-1);
	}
	if( o->obj_type == STATESET) {
		if( states_done )  {
			fprintf(stderr, "No states may be defined after *TRANSITIONS\n");
			Exit(-1);
		}
		o->obj_number =  Nstates++ ; 
		if(Nstates > MAXSTATES) {
			fprintf(stderr, "Too many states\n");
			Exit(-1);
		}
		fprintf(Sfile, "#define %s 0x%x\n", o->obj_name, o->obj_number);
		IFDEBUG(X)
			fprintf(astringfile, "\"%s(0x%x)\",\n", o->obj_name, o->obj_number);
		ENDDEBUG
	} else {
		/* EVENTSET */ 
		if( ! states_done )  {
			fprintf(stderr, "states must precede events\n");
			Exit(-1);
		}
		o->obj_number =  Nevents++ ;
		if(Nevents > MAXEVENTS) {
			fprintf(stderr, "Too many events\n");
			Exit(-1);
		}
		if(o->obj_struc)  {
			if( FirstEventAttribute ) {
				fprintf(Efile,  "\n\tunion{\n"); /*} */
				FirstEventAttribute = 0;
			}
			fprintf(Efile, 
			"struct %s %s%s;\n\n", o->obj_struc, EV_PREFIX,  o->obj_name);
		}
		fprintf(Efile, "#define %s 0x%x\n", o->obj_name, o->obj_number);
		IFDEBUG(X)
			fprintf(astringfile, "\"%s(0x%x)\",\n", o->obj_name, o->obj_number);
		ENDDEBUG
	}
	IFDEBUG(o)
		fprintf(OUT, "insert(%s)\n", OBJ_NAME(o) );
		if(o->obj_right != NULL) {
			fprintf(OUT, "insert: unclean Object right\n");
			exit(-1);
		}
		if(o->obj_left != NULL) {
			fprintf(OUT, "insert: unclean Object left\n");
			exit(-1);
		}
		fflush(OUT);
	ENDDEBUG

	while( val ) {
		if(p == NULL) {
			*q = o;
			o->obj_parent = (struct Object *)q;
			break;
		}
		if(!(val = strcmp(o->obj_name, p->obj_name)) ) {
			/* equal */
			fprintf(stderr, "re-inserting %s\n",o->obj_name);
			exit(-1);
		}
		if(val < 0) {
			/* left */
			q = &p->obj_left;
			p = p->obj_left;
		} else {
			/* right */
			q = &p->obj_right;
			p = p->obj_right;
		}
	}
	IFDEBUG(a)
		dumptree(Objtree,0);
	ENDDEBUG
}

delete(o) 
struct Object *o;
{
	register struct Object *p = o->obj_right; 
	register struct Object *q;
	register struct Object *newparent;
	register struct Object **np_childlink;

	IFDEBUG(T)
		fprintf(stdout, "delete(0x%x)\n", o);
		dumptree(Objtree,0);
	ENDDEBUG

	/* q <== lowest valued node of the right subtree */
	while( p ) {
		q = p;
		p = p->obj_left;
	}

	if (o->obj_parent == (struct Object *)&Objtree)  {
		newparent =  (struct Object *)&Objtree;
		np_childlink = (struct Object **)&Objtree;
	} else if(o->obj_parent->obj_left == o)  {
		newparent = o->obj_parent;
		np_childlink = &(o->obj_parent->obj_left);
	} else {
		newparent = o->obj_parent;
		np_childlink = &(o->obj_parent->obj_right);
	}
	IFDEBUG(T)
		fprintf(OUT, "newparent=0x%x\n");
	ENDDEBUG

	if (q) { /* q gets the left, parent gets the right */
		IFDEBUG(T)
			fprintf(OUT, "delete: q null\n");
		ENDDEBUG
		q->obj_left = p;
		if(p) p->obj_parent = q;
		p = o->obj_right;
	} else { /* parent(instead of q) gets the left ; there is no right  */
		IFDEBUG(T)
			fprintf(OUT, "delete: q not null\n");
		ENDDEBUG
		p = o->obj_left;
	}
	*np_childlink = p;
	if(p) 
		p->obj_parent = newparent;

	IFDEBUG(T)
		fprintf(OUT, "After deleting 0x%x\n",o);
		dumptree(Objtree,0);
	ENDDEBUG
}

struct Object *
defineset(type, adr, keep)
unsigned char type;
char *adr;
int keep;
{
	struct Object *onew;
	IFDEBUG(o)
		printf("defineset(0x%x,%s, %s)\n", type , adr, keep?"KEEP":"NO_KEEP");
	ENDDEBUG
	
	onew = (struct Object *)Malloc(sizeof (struct Object));
	bzero(onew, sizeof(struct Object));
	onew->obj_name = adr;
	onew->obj_kind = OBJ_SET;
	onew->obj_type = type;
	if(keep) 
		insert( onew );
		/* address already stashed before calling defineset */
	IFDEBUG(o)
		printf("defineset(0x%x,%s) returning 0x%x\n", type , adr, onew);
		dumptree(Objtree,0);
	ENDDEBUG
	return(onew);
}

dumpit(o, s)
char *o;
char *s;
{
	register int i;

IFDEBUG(o)
	fprintf(OUT, "object 0x%x, %s\n",o, s);
	for(i=0; i< sizeof(struct Object); i+=4) {
		fprintf(OUT, "0x%x: 0x%x 0x%x 0x%x 0x%x\n",
		*((int *)o), *o, *(o+1), *(o+2), *(o+3) );
	}
ENDDEBUG
}

defineitem(type, adr, struc)
unsigned char type;
char *adr;
char *struc;
{
	struct Object *onew;
	IFDEBUG(o)
		printf("defineitem(0x%x, %s at 0x%x, %s)\n", type, adr, adr, struc);
	ENDDEBUG
	
	if( onew = lookup( type, adr ) ) {
		fprintf(stderr, 
	"Internal error at defineitem: trying to redefine obj type 0x%x, adr %s\n",
			type, adr);
		exit(-1);
	} else {
		onew = (struct Object *)Malloc(sizeof (struct Object));
		bzero(onew, sizeof(struct Object));
		onew->obj_name = stash(adr);
		onew->obj_kind = OBJ_ITEM;
		onew->obj_type =  type;
		onew->obj_struc = struc?stash(struc):struc;
		insert( onew );
	}
	IFDEBUG(o)
		fprintf(OUT, "defineitem(0x%x, %s) returning 0x%x\n", type, adr, onew);
	ENDDEBUG
}

member(o, adr)
struct Object *o;
char *adr;
{
	struct Object *onew, *oold;
	IFDEBUG(o)
		printf("member(0x%x, %s)\n", o, adr);
	ENDDEBUG
	
	oold = lookup(  o->obj_type, adr );

	onew = (struct Object *)Malloc(sizeof (struct Object));
	if( oold == NULL ) {
		extern int lineno;

		fprintf(stderr,
		"Warning at line %d: set definition of %s causes definition of\n",
			lineno, OBJ_NAME(o));
		fprintf(stderr, "\t (previously undefined) member %s\n", adr);
		bzero(onew, sizeof(struct Object));
		onew->obj_name = stash(adr);
		onew->obj_kind = OBJ_ITEM;
		onew->obj_type = o->obj_type;
		onew->obj_members = NULL;
		insert( onew );
	} else {
		if(oold->obj_kind != OBJ_ITEM) {
			fprintf(stderr, "Sets cannot be members of sets; %s\n", adr);
			exit(-1);
		}
		bcopy(oold, onew, sizeof(struct Object));
		onew->obj_members = onew->obj_left = onew->obj_right = NULL;
	}
	onew->obj_members = o->obj_members;
	o->obj_members = onew;
}

struct Object *Lookup(type, name)
unsigned char type;
char *name;
{
	register struct Object *o = lookup(type,name);

	if(o == NULL) {
		fprintf(stderr, "Trying to use undefined %s: %s\n",
			type==STATESET?"state":"event", name);
		Exit(-1);
	}
	return(o);
}

AddCurrentEventName(x)
register char **x;
{
	register char *n = EV_PREFIX; ;
	
	if( CurrentEvent == (struct Object *)0 ) {
		fprintf(stderr, "No event named!  BARF!\n"); Exit(-1);
	}

	if( ! CurrentEvent->obj_struc ) {
		fprintf(stderr, "No attributes for current event!\n"); Exit(-1);
	}

	/* add prefix first */
	while(*n) {
		*(*x)++ = *n++;
	}

	n = CurrentEvent->obj_name;

	while(*n) {
		*(*x)++ = *n++;
	}
}

dumptree(o,i)
	register struct Object *o;
	int i;
{
	register int j;

	if(o == NULL) {
		for(j=0; j<i; j++)
			fputc(' ', stdout);
		fprintf(stdout, "%3d NULL\n", i);
	} else {
		dumptree(o->obj_left, i+1);
		for(j=0; j<i; j++) 
			fputc(' ', stdout);
		fprintf(stdout, "%3d 0x%x: %s\n", i,o, OBJ_NAME(o));
		dumptree(o->obj_right, i+1);
	}
}

dump(c,a)
{
	register int x = 8;
	int zero = 0;
#include <sys/signal.h>

	fprintf(stderr, "dump: c 0x%x, a 0x%x\n",c,a);

	x = x/zero;
	kill(0, SIGQUIT);
}

dump_trans( pred, oldstate, newstate, action, event )
struct Object *oldstate, *newstate, *event;
char *pred, *action;
{
	extern int transno;
	struct Object *o;

	fprintf(stdout, "\n%d:  ", transno);
#define dumpit(x)\
	if((x)->obj_kind == OBJ_SET) {\
		o = (x)->obj_members; fprintf( stdout, "[ " );\
		while(o) { fprintf(stdout, "%s ", o->obj_name); o = o->obj_members; }\
		fprintf( stdout, " ] ");\
	} else { fprintf(stdout, "%s ", (x)->obj_name); }

	dumpit(newstate);
	fprintf(stdout, " <== ");
	dumpit(oldstate);
	dumpit(event);
	fprintf(stdout, "\n\t\t%s\n\t\t%s\n", pred?pred:"DEFAULT", 
		action);
}
