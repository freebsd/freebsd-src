/*
 *	from: xebec.c,v 2.2 88/09/19 12:55:37 nhall Exp
 *	$Id: xebec.c,v 1.2 1993/10/16 21:33:29 rgrimes Exp $
 */

#include "xebec.h"
#include "llparse.h"
#ifndef	E_TABLE
#define	E_TABLE "xebec.e"
#endif	E_TABLE

#include "main.h"
#include "sets.h"
#include <stdio.h> 

extern FILE *eventfile_h, *actfile; 

llaction(lln,token)
LLtoken *token;
{
	struct llattr *llattr;
	llattr = &llattrdesc[lldescindex-1];
switch(lln) {
case 1:
	llfinprod();
	break;

case 10: {
	
					if(strlen(llattr->llabase[3].ID.address) > 50 ) {
						fprintf(stderr, 
						"Protocol name may not exceed 50 chars in length.\n"); 
						Exit(-1);
					}
					strcpy(protocol, llattr->llabase[3].ID.address); 
					openfiles(protocol); 
				
} break;

case 11: {
 
					llattr->llabase[7].pcb.isevent = 0; 
				
} break;

case 12: {

				  fprintf(actfile, "\ntypedef %s %s%s;\n",
							  llattr->llabase[7].pcb.address,protocol, PCBNAME); 
				  llattr->llabase[8].syn.type = PCB_SYN;
				
} break;

case 13: {
 llattr->llabase[11].part.type = (unsigned char) STATESET; 
} break;

case 14: {
 end_states(eventfile_h); 
} break;

case 15: {
 llattr->llabase[14].pcb.isevent = 1; 
} break;

case 16: {

					fprintf(eventfile_h, "\t"); /* fmq gags on single chars */
					includecode(eventfile_h, llattr->llabase[14].pcb.address);
					fprintf(eventfile_h, "\n"); /* fmq gags on single chars */
					llattr->llabase[15].syn.type = EVENT_SYN;
				
} break;

case 17: {
 
				  	llattr->llabase[16].part.type = (unsigned char)EVENTSET; 
				
} break;

case 18: {
 end_events(); 
} break;

case 19: {
 
					putincludes();
					putdriver(actfile, 9);
				
} break;

case 20: {
	if(llattr->llabase[0].pcb.isevent)  {
					fprintf(stderr, 
					"Event is a list of objects enclosed by \"{}\"\n");
					Exit(-1);
				}
			  fprintf(eventfile_h, "struct "); 
			
} break;

case 21: {
 llattr->llabase[0].pcb.address = llattr->llabase[2].ACTION.address; 
} break;

case 22: {
	if( ! llattr->llabase[0].pcb.isevent)  {
					fprintf(stderr, 
					"Pcb requires a type or structure definition.\"{}\"\n");
					Exit(-1);
				}
			   llattr->llabase[0].pcb.address = llattr->llabase[1].ACTION.address; 
			
} break;

case 23: {
  llattr->llabase[0].pcb.address = llattr->llabase[1].ID.address; 
} break;

case 24: {
 synonyms[llattr->llabase[0].syn.type] = stash( llattr->llabase[2].ID.address ); 
} break;

case 25: {
 includecode(actfile, llattr->llabase[2].ACTION.address);
} break;

case 26: {
 
			llattr->llabase[2].partrest.address = llattr->llabase[1].ID.address;
			llattr->llabase[2].partrest.type = llattr->llabase[0].part.type; 
		
} break;

case 27: {
 llattr->llabase[3].parttail.type = llattr->llabase[0].part.type; 
} break;

case 28: {
 llattr->llabase[1].part.type = llattr->llabase[0].parttail.type; 
} break;

case 29: {
 
			  if(  lookup( llattr->llabase[0].partrest.type, llattr->llabase[0].partrest.address ) ) {
				fprintf(stderr, "bnf:trying to redefine obj type 0x%x, adr %s\n",
					llattr->llabase[0].partrest.type, llattr->llabase[0].partrest.address);
				Exit(-1);
			  } 
			  llattr->llabase[2].setdef.type = llattr->llabase[0].partrest.type;
			  llattr->llabase[2].setdef.address = stash( llattr->llabase[0].partrest.address );
			  llattr->llabase[2].setdef.keep = 1;
			
} break;

case 30: {
 llattr->llabase[3].setstruct.object = llattr->llabase[2].setdef.object; 
} break;

case 31: {
 
		 defineitem(llattr->llabase[0].partrest.type, 
					llattr->llabase[0].partrest.address, llattr->llabase[1].ACTION.address); 
		
} break;

case 32: {
 
			defineitem(llattr->llabase[0].partrest.type, llattr->llabase[0].partrest.address, (char *)0);
		
} break;

case 33: {

				if(llattr->llabase[0].setstruct.object)  {
					/* WHEN COULD THIS BE FALSE?? 
					 * isn't it supposed to be setstruct.object???
					 * (it used to be $ACTION.address)
					 */

					llattr->llabase[0].setstruct.object->obj_struc = llattr->llabase[1].ACTION.address;
					fprintf(eventfile_h, 
						"struct %s %s%s;\n\n", llattr->llabase[1].ACTION.address, 
						EV_PREFIX,  llattr->llabase[0].setstruct.object->obj_name);
				}
			
} break;

case 34: {
 
			llattr->llabase[2].setlist.setnum = 
			defineset(llattr->llabase[0].setdef.type, llattr->llabase[0].setdef.address, llattr->llabase[0].setdef.keep); 
		
} break;

case 35: {
 llattr->llabase[0].setdef.object = llattr->llabase[2].setlist.setnum; 
} break;

case 36: {
 
		member(llattr->llabase[0].setlist.setnum, llattr->llabase[1].ID.address); 
				llattr->llabase[2].setlisttail.setnum = llattr->llabase[0].setlist.setnum; 
	
} break;

case 37: {
 llattr->llabase[2].setlist.setnum = llattr->llabase[0].setlisttail.setnum; 
} break;

case 38: {
 transno ++; 
} break;

case 39: {
 
	 	CurrentEvent /* GAG! */ = llattr->llabase[6].event.object; 
	 
} break;

case 40: {
 
		llattr->llabase[8].actionpart.string = llattr->llabase[7].predicatepart.string; 
		llattr->llabase[8].actionpart.newstate = llattr->llabase[1].newstate.object; 
		llattr->llabase[8].actionpart.oldstate = llattr->llabase[5].oldstate.object;
	
} break;

case 41: {
 
		 llattr->llabase[0].predicatepart.string = stash ( llattr->llabase[1].PREDICATE.address );
	
} break;

case 42: {
 
		llattr->llabase[0].predicatepart.string = (char *)0;
	
} break;

case 43: {

	  statetable( llattr->llabase[0].actionpart.string, llattr->llabase[0].actionpart.oldstate, 
					llattr->llabase[0].actionpart.newstate,
					acttable(actfile, llattr->llabase[1].ACTION.address ), 
					CurrentEvent ); 
	  if( print_trans ) {
	  	dump_trans( llattr->llabase[0].actionpart.string, llattr->llabase[0].actionpart.oldstate, 
					llattr->llabase[0].actionpart.newstate,
					llattr->llabase[1].ACTION.address, CurrentEvent ); 
	  }
	
} break;

case 44: {

	  statetable(llattr->llabase[0].actionpart.string, llattr->llabase[0].actionpart.oldstate, llattr->llabase[0].actionpart.newstate,
				  0, CurrentEvent ); /* KLUDGE - remove this */
	  if( print_trans ) {
	  	dump_trans( llattr->llabase[0].actionpart.string, llattr->llabase[0].actionpart.oldstate, 
					llattr->llabase[0].actionpart.newstate,
					"NULLACTION", CurrentEvent ); 
	  }
	
} break;

case 45: {
	
		llattr->llabase[0].oldstate.object = Lookup(STATESET, llattr->llabase[1].ID.address);
	
} break;

case 46: {

			llattr->llabase[1].setdef.address = (char *)0;
			llattr->llabase[1].setdef.type = (unsigned char)STATESET; 
			llattr->llabase[1].setdef.keep = 0;
		
} break;

case 47: {
 
			llattr->llabase[0].oldstate.object = llattr->llabase[1].setdef.object; 
		
} break;

case 48: {
 
		llattr->llabase[0].newstate.object = Lookup(STATESET, llattr->llabase[1].ID.address); 
	
} break;

case 49: {
 
		extern struct Object *SameState;

		llattr->llabase[0].newstate.object = SameState;
	
} break;

case 50: {

			llattr->llabase[0].event.object = Lookup(EVENTSET, llattr->llabase[1].ID.address); 
		
} break;

case 51: {

			llattr->llabase[1].setdef.address = (char *)0;
			llattr->llabase[1].setdef.type = (unsigned char)EVENTSET; 
			llattr->llabase[1].setdef.keep = 0;
		
} break;

case 52: {
 
			llattr->llabase[0].event.object = llattr->llabase[1].setdef.object; 
		
} break;
}
}
char *llstrings[] = {
	"<null>",
	"ID",
	"STRUCT",
	"SYNONYM",
	"PREDICATE",
	"ACTION",
	"PROTOCOL",
	"LBRACK",
	"RBRACK",
	"LANGLE",
	"EQUAL",
	"COMMA",
	"STAR",
	"EVENTS",
	"TRANSITIONS",
	"INCLUDE",
	"STATES",
	"SEMI",
	"PCB",
	"DEFAULT",
	"NULLACTION",
	"SAME",
	"ENDMARKER",
	"pcb",
	"syn",
	"setlist",
	"setlisttail",
	"part",
	"parttail",
	"partrest",
	"setstruct",
	"setdef",
	"translist",
	"transition",
	"event",
	"oldstate",
	"newstate",
	"predicatepart",
	"actionpart",
	"program",
	"includelist",
	"optsemi",
	"translisttail",
	"$goal$",
	(char *) 0
};
short llnterms = 23;
short llnsyms = 44;
short llnprods = 38;
short llinfinite = 10000;
short llproductions[] = {
41, -21, 5, -20, 2, 
41, -22, 5, 
41, -23, 1, 
-24, 1, 3, 

26, -36, 1, 
25, -37, 11, 

28, -27, 29, -26, 1, 
27, -28, 

30, -30, 31, -29, 10, 
-31, 5, 
-32, 
-33, 5, 

-35, 8, 25, -34, 7, 
42, 33, 
17, 38, -40, 37, -39, 34, 35, 10, 10, 9, -38, 36, 
-50, 1, 
-52, 31, -51, 
-45, 1, 
-47, 31, -46, 
-48, 1, 
-49, 21, 
-41, 4, 
-42, 19, 
-43, 5, 
-44, 20, 
32, -19, 14, -18, 12, 27, -17, 24, -16, 23, -15, 13, -14, 12, 27, -13, 16, 12, 24, -12, 23, -11, 18, 40, 12, -10, 1, 6, 12, 
12, -25, 5, 15, 

17, 

32, 

22, 39, 
0
};
struct llprodindex llprodindex[] = {
{   0,   0,   0 }, {   0,   5,  19 }, {   5,   3,   3 }, {   8,   3,   2 }, 
{  11,   3,   2 }, {  14,   0,   2 }, {  14,   3,   0 }, {  17,   3,   1 }, 
{  20,   0,   0 }, {  20,   5,   3 }, {  25,   2,   0 }, {  27,   0,   3 }, 
{  27,   5,   1 }, {  32,   2,   0 }, {  34,   1,   3 }, {  35,   2,   1 }, 
{  37,   0,   0 }, {  37,   5,   1 }, {  42,   2,   0 }, {  44,  12,   3 }, 
{  56,   2,   2 }, {  58,   3,   2 }, {  61,   2,   0 }, {  63,   3,   2 }, 
{  66,   2,   1 }, {  68,   2,   0 }, {  70,   2,   9 }, {  72,   2,   1 }, 
{  74,   2,   1 }, {  76,   2,   1 }, {  78,  29,   1 }, { 107,   4,   1 }, 
{ 111,   0,   1 }, { 111,   1,   1 }, { 112,   0,   1 }, { 112,   1,   1 }, 
{ 113,   0,   1 }, { 113,   2,   2 }, {   0,   0,   0 }
};
short llepsilon[] = {
 0, 0, 0, 0, 0, 1, 0, 0, 1, 0,
 0, 1, 0, 0, 1, 0, 1, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 1, 0, 1, 0, 1, 0, 0
};
struct llparsetable llparsetable[] = {
{   1,   3 }, {   2,   1 }, {   5,   2 }, {   0,  23 }, {   1,   5 }, 
{   3,   4 }, {  12,   5 }, {   0,  24 }, {   1,   6 }, {   0,  25 }, 
{   8,   8 }, {  11,   7 }, {   0,  26 }, {   1,   9 }, {   0,  27 }, 
{   1,  10 }, {  12,  11 }, {   0,  28 }, {   1,  14 }, {   5,  13 }, 
{  10,  12 }, {  12,  14 }, {   0,  29 }, {   1,  16 }, {   5,  15 }, 
{  12,  16 }, {   0,  30 }, {   7,  17 }, {   0,  31 }, {   1,  18 }, 
{  21,  18 }, {   0,  32 }, {   1,  19 }, {  21,  19 }, {   0,  33 }, 
{   1,  20 }, {   7,  21 }, {   0,  34 }, {   1,  22 }, {   7,  23 }, 
{   0,  35 }, {   1,  24 }, {  21,  25 }, {   0,  36 }, {   4,  26 }, 
{  19,  27 }, {   0,  37 }, {   5,  28 }, {  20,  29 }, {   0,  38 }, 
{  12,  30 }, {   0,  39 }, {  15,  31 }, {  18,  32 }, {   0,  40 }, 
{   1,  34 }, {   3,  34 }, {  12,  34 }, {  17,  33 }, {   0,  41 }, 
{   1,  35 }, {  21,  35 }, {  22,  36 }, {   0,  42 }, {  12,  37 }, 
{   0,  43 }, {   0,   0 }
};
short llparseindex[] = {
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 4, 8, 10, 13, 15, 18,
 23, 27, 29, 32, 35, 38, 41, 44, 47, 50,
 52, 55, 60, 64, 0
};
