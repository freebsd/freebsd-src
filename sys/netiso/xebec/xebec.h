/* $Header: /pub/FreeBSD/FreeBSD-CVS/src/sys/netiso/xebec/Attic/xebec.h,v 1.1.1.1 1994/05/24 10:07:40 rgrimes Exp $ */
/* $Source: /pub/FreeBSD/FreeBSD-CVS/src/sys/netiso/xebec/Attic/xebec.h,v $ */

union llattrib {
	struct {
 char *address; 	} ID;
	int	STRUCT;
	int	SYNONYM;
	struct {
 char *address; 	} PREDICATE;
	struct {
 char *address; 	} ACTION;
	int	PROTOCOL;
	int	LBRACK;
	int	RBRACK;
	int	LANGLE;
	int	EQUAL;
	int	COMMA;
	int	STAR;
	int	EVENTS;
	int	TRANSITIONS;
	int	INCLUDE;
	int	STATES;
	int	SEMI;
	struct {
 char *address; 	} PCB;
	int	DEFAULT;
	int	NULLACTION;
	int	SAME;
	struct {
 char *address; int isevent; 	} pcb;
	struct {
 int type; 	} syn;
	struct {
 struct Object *setnum; 	} setlist;
	struct {
 struct Object *setnum; 	} setlisttail;
	struct {
 unsigned char type; 	} part;
	struct {
 unsigned char type; 	} parttail;
	struct {
 unsigned char type; char *address; 	} partrest;
	struct {
 struct Object *object; 	} setstruct;
	struct {
 unsigned char type,keep; char *address; struct Object *object; 	} setdef;
	int	translist;
	int	transition;
	struct {
 struct Object *object;  	} event;
	struct {
	struct Object *object;		} oldstate;
	struct {
	struct Object *object;		} newstate;
	struct {
	char *string; 	} predicatepart;
	struct {
 	char *string; struct Object *oldstate; struct Object *newstate; 	} actionpart;
};
#define LLTERM	23
#define LLSYM	44
#define LLPROD	38

#define LLINF	10000

#define T_ID                              1
#define T_STRUCT                          2
#define T_SYNONYM                         3
#define T_PREDICATE                       4
#define T_ACTION                          5
#define T_PROTOCOL                        6
#define T_LBRACK                          7
#define T_RBRACK                          8
#define T_LANGLE                          9
#define T_EQUAL                           10
#define T_COMMA                           11
#define T_STAR                            12
#define T_EVENTS                          13
#define T_TRANSITIONS                     14
#define T_INCLUDE                         15
#define T_STATES                          16
#define T_SEMI                            17
#define T_PCB                             18
#define T_DEFAULT                         19
#define T_NULLACTION                      20
#define T_SAME                            21
#define T_ENDMARKER                       22
