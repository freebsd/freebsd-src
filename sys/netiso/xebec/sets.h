/* $Header: /pub/FreeBSD/FreeBSD-CVS/src/sys/netiso/xebec/Attic/sets.h,v 1.2 1995/05/30 08:12:14 rgrimes Exp $ */
/* $Source: /pub/FreeBSD/FreeBSD-CVS/src/sys/netiso/xebec/Attic/sets.h,v $ */

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

