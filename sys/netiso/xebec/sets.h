/*
 *	from: sets.h,v 2.1 88/09/19 12:56:33 nhall Exp
 *	$Id: sets.h,v 1.2 1993/10/16 21:33:24 rgrimes Exp $
 */

#define MAXEVENTS 200
#define MAXSTATES 200

#define STATESET 10
#define EVENTSET 5

#define OBJ_ITEM 2
#define OBJ_SET 3

struct Object {
	unsigned char obj_kind;
	unsigned char obj_type; /* state or event */
	char *obj_name;
	char *obj_struc;
	int obj_number; 
	struct Object *obj_members; /* must be null for kind==item */
	/* for the tree */
	struct Object *obj_left;
	struct Object *obj_right;
	struct Object *obj_parent;
} ;

extern char *Noname;

#define OBJ_NAME(o) (((o)->obj_name)?(o)->obj_name:Noname)

extern int Nevents, Nstates;
int Eventshift;
extern struct Object *CurrentEvent;

extern struct Object *Lookup();
extern struct Object *defineset();

