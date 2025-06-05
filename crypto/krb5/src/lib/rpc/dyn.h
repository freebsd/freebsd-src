/*
 * This file is part of libdyn.a, the C Dynamic Object library.  It
 * contains the public header file.
 *
 * There are no restrictions on this code; however, if you make any
 * changes, I request that you document them so that I do not get
 * credit or blame for your modifications.
 *
 * Written by Barr3y Jaspan, Student Information Processing Board (SIPB)
 * and MIT-Project Athena, 1989.
 *
 * 2002-07-17 Moved here from util/dyn; for old changes see dyn.c.
 *            Added macros to rename exposed symbols.  For newer changes
 *            see ChangeLog in the current directory.
 */


/*
 * dyn.h -- header file to be included by programs linking against
 * libdyn.a.
 */

#ifndef _Dyn_h
#define _Dyn_h

typedef char *DynPtr;
typedef struct _DynObject {
     DynPtr	array;
     int	el_size, num_el, size, inc;
     int	debug, paranoid, initzero;
} DynObjectRec, *DynObject;

/* Function macros */
#define DynHigh(obj)	(DynSize(obj) - 1)
#define DynLow(obj)	(0)

/* Return status codes */
#define DYN_OK		-1000
#define DYN_NOMEM	-1001
#define DYN_BADINDEX	-1002
#define DYN_BADVALUE	-1003

#define DynCreate	gssrpcint_DynCreate
#define DynDestroy	gssrpcint_DynDestroy
#define DynRelease	gssrpcint_DynRelease
#define DynAdd		gssrpcint_DynAdd
#define DynPut		gssrpcint_DynPut
#define DynInsert	gssrpcint_DynInsert
#define DynGet		gssrpcint_DynGet
#define DynArray	gssrpcint_DynArray
#define DynSize		gssrpcint_DynSize
#define DynCopy		gssrpcint_DynCopy
#define DynDelete	gssrpcint_DynDelete
#define DynDebug	gssrpcint_DynDebug
#define DynParanoid	gssrpcint_DynParanoid
#define DynInitzero	gssrpcint_DynInitzero
#define DynCapacity	gssrpcint_DynCapacity
#define DynAppend	gssrpcint_DynAppend

/*@null@*//*@only@*/ DynObject DynCreate (int el_size, int inc);
/*@null@*//*@only@*/ DynObject DynCopy (DynObject obj);
int DynDestroy (/*@only@*/DynObject obj), DynRelease (DynObject obj);
int DynAdd (DynObject obj, void *el);
int DynPut (DynObject obj, void *el, int idx);
int DynInsert (DynObject obj, int idx, /*@observer@*/void *els, int num);
int DynDelete (DynObject obj, int idx);
/*@dependent@*//*@null@*/ DynPtr DynGet (DynObject obj, int num);
/*@observer@*/ DynPtr DynArray (DynObject obj);
int DynDebug (DynObject obj, int state);
int DynParanoid (DynObject obj, int state);
int DynInitzero (DynObject obj, int state);
int DynSize (DynObject obj);
int DynCapacity (DynObject obj);
int DynAppend (DynObject obj, DynPtr els, int num);

#undef P

#endif /* _Dyn_h */
/* DO NOT ADD ANYTHING AFTER THIS #endif */
