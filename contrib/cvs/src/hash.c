/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 * Polk's hash list manager.  So cool.
 */

#include "cvs.h"
#include <assert.h>

/* Global caches.  The idea is that we maintain a linked list of "free"d
   nodes or lists, and get new items from there.  It has been suggested
   to use an obstack instead, but off the top of my head, I'm not sure
   that would gain enough to be worth worrying about.  */
static List *listcache = NULL;
static Node *nodecache = NULL;

static void freenode_mem PROTO((Node * p));

/* hash function */
static int
hashp (key)
    const char *key;
{
    unsigned int h = 0;
    unsigned int g;

    assert(key != NULL);
    
    while (*key != 0)
    {
	unsigned int c = *key++;
	/* The FOLD_FN_CHAR is so that findnode_fn works.  */
	h = (h << 4) + FOLD_FN_CHAR (c);
	if ((g = h & 0xf0000000) != 0)
	    h = (h ^ (g >> 24)) ^ g;
    }

    return (h % HASHSIZE);
}

/*
 * create a new list (or get an old one from the cache)
 */
List *
getlist ()
{
    int i;
    List *list;
    Node *node;

    if (listcache != NULL)
    {
	/* get a list from the cache and clear it */
	list = listcache;
	listcache = listcache->next;
	list->next = (List *) NULL;
	for (i = 0; i < HASHSIZE; i++)
	    list->hasharray[i] = (Node *) NULL;
    }
    else
    {
	/* make a new list from scratch */
	list = (List *) xmalloc (sizeof (List));
	memset ((char *) list, 0, sizeof (List));
	node = getnode ();
	list->list = node;
	node->type = HEADER;
	node->next = node->prev = node;
    }
    return (list);
}

/*
 * free up a list
 */
void
dellist (listp)
    List **listp;
{
    int i;
    Node *p;

    if (*listp == (List *) NULL)
	return;

    p = (*listp)->list;

    /* free each node in the list (except header) */
    while (p->next != p)
	delnode (p->next);

    /* free any list-private data, without freeing the actual header */
    freenode_mem (p);

    /* free up the header nodes for hash lists (if any) */
    for (i = 0; i < HASHSIZE; i++)
    {
	if ((p = (*listp)->hasharray[i]) != (Node *) NULL)
	{
	    /* put the nodes into the cache */
#ifndef NOCACHE
	    p->type = UNKNOWN;
	    p->next = nodecache;
	    nodecache = p;
#else
	    /* If NOCACHE is defined we turn off the cache.  This can make
	       it easier to tools to determine where items were allocated
	       and freed, for tracking down memory leaks and the like.  */
	    free (p);
#endif
	}
    }

    /* put it on the cache */
#ifndef NOCACHE
    (*listp)->next = listcache;
    listcache = *listp;
#else
    free ((*listp)->list);
    free (*listp);
#endif
    *listp = (List *) NULL;
}

/*
 * get a new list node
 */
Node *
getnode ()
{
    Node *p;

    if (nodecache != (Node *) NULL)
    {
	/* get one from the cache */
	p = nodecache;
	nodecache = p->next;
    }
    else
    {
	/* make a new one */
	p = (Node *) xmalloc (sizeof (Node));
    }

    /* always make it clean */
    memset ((char *) p, 0, sizeof (Node));
    p->type = UNKNOWN;

    return (p);
}

/*
 * remove a node from it's list (maybe hash list too) and free it
 */
void
delnode (p)
    Node *p;
{
    if (p == (Node *) NULL)
	return;

    /* take it out of the list */
    p->next->prev = p->prev;
    p->prev->next = p->next;

    /* if it was hashed, remove it from there too */
    if (p->hashnext != (Node *) NULL)
    {
	p->hashnext->hashprev = p->hashprev;
	p->hashprev->hashnext = p->hashnext;
    }

    /* free up the storage */
    freenode (p);
}

/*
 * free up the storage associated with a node
 */
static void
freenode_mem (p)
    Node *p;
{
    if (p->delproc != (void (*) ()) NULL)
	p->delproc (p);			/* call the specified delproc */
    else
    {
	if (p->data != NULL)		/* otherwise free() it if necessary */
	    free (p->data);
    }
    if (p->key != NULL)			/* free the key if necessary */
	free (p->key);

    /* to be safe, re-initialize these */
    p->key = p->data = (char *) NULL;
    p->delproc = (void (*) ()) NULL;
}

/*
 * free up the storage associated with a node and recycle it
 */
void
freenode (p)
    Node *p;
{
    /* first free the memory */
    freenode_mem (p);

    /* then put it in the cache */
#ifndef NOCACHE
    p->type = UNKNOWN;
    p->next = nodecache;
    nodecache = p;
#else
    free (p);
#endif
}

/*
 * Link item P into list LIST before item MARKER.  If P->KEY is non-NULL and
 * that key is already in the hash table, return -1 without modifying any
 * parameter.
 * 
 * return 0 on success
 */
int
insert_before (list, marker, p)
    List *list;
    Node *marker;
    Node *p;
{
    if (p->key != NULL)			/* hash it too? */
    {
	int hashval;
	Node *q;

	hashval = hashp (p->key);
	if (list->hasharray[hashval] == NULL)	/* make a header for list? */
	{
	    q = getnode ();
	    q->type = HEADER;
	    list->hasharray[hashval] = q->hashnext = q->hashprev = q;
	}

	/* put it into the hash list if it's not already there */
	for (q = list->hasharray[hashval]->hashnext;
	     q != list->hasharray[hashval]; q = q->hashnext)
	{
	    if (strcmp (p->key, q->key) == 0)
		return (-1);
	}
	q = list->hasharray[hashval];
	p->hashprev = q->hashprev;
	p->hashnext = q;
	p->hashprev->hashnext = p;
	q->hashprev = p;
    }

    p->next = marker;
    p->prev = marker->prev;
    marker->prev->next = p;
    marker->prev = p;

    return (0);
}

/*
 * insert item p at end of list "list" (maybe hash it too) if hashing and it
 * already exists, return -1 and don't actually put it in the list
 * 
 * return 0 on success
 */
int
addnode (list, p)
    List *list;
    Node *p;
{
  return insert_before (list, list->list, p);
}

/*
 * Like addnode, but insert p at the front of `list'.  This bogosity is
 * necessary to preserve last-to-first output order for some RCS functions.
 */
int
addnode_at_front (list, p)
    List *list;
    Node *p;
{
  return insert_before (list, list->list->next, p);
}

/* Look up an entry in hash list table and return a pointer to the
   node.  Return NULL if not found.  Abort with a fatal error for
   errors.  */
Node *
findnode (list, key)
    List *list;
    const char *key;
{
    Node *head, *p;

    /* This probably should be "assert (list != NULL)" (or if not we
       should document the current behavior), but only if we check all
       the callers to see if any are relying on this behavior.  */
    if ((list == (List *) NULL))
	return ((Node *) NULL);

    assert (key != NULL);

    head = list->hasharray[hashp (key)];
    if (head == (Node *) NULL)
	/* Not found.  */
	return ((Node *) NULL);

    for (p = head->hashnext; p != head; p = p->hashnext)
	if (strcmp (p->key, key) == 0)
	    return (p);
    return ((Node *) NULL);
}

/*
 * Like findnode, but for a filename.
 */
Node *
findnode_fn (list, key)
    List *list;
    const char *key;
{
    Node *head, *p;

    /* This probably should be "assert (list != NULL)" (or if not we
       should document the current behavior), but only if we check all
       the callers to see if any are relying on this behavior.  */
    if (list == (List *) NULL)
	return ((Node *) NULL);

    assert (key != NULL);

    head = list->hasharray[hashp (key)];
    if (head == (Node *) NULL)
	return ((Node *) NULL);

    for (p = head->hashnext; p != head; p = p->hashnext)
	if (fncmp (p->key, key) == 0)
	    return (p);
    return ((Node *) NULL);
}

/*
 * walk a list with a specific proc
 */
int
walklist (list, proc, closure)
    List *list;
    int (*proc) PROTO ((Node *, void *));
    void *closure;
{
    Node *head, *p;
    int err = 0;

    if (list == NULL)
	return (0);

    head = list->list;
    for (p = head->next; p != head; p = p->next)
	err += proc (p, closure);
    return (err);
}

int
list_isempty (list)
    List *list;
{
    return list == NULL || list->list->next == list->list;
}

static int (*client_comp) PROTO ((const Node *, const Node *));
static int qsort_comp PROTO ((const void *, const void *));

static int
qsort_comp (elem1, elem2)
    const void *elem1;
    const void *elem2;
{
    Node **node1 = (Node **) elem1;
    Node **node2 = (Node **) elem2;
    return client_comp (*node1, *node2);
}

/*
 * sort the elements of a list (in place)
 */
void
sortlist (list, comp)
    List *list;
    int (*comp) PROTO ((const Node *, const Node *));
{
    Node *head, *remain, *p, **array;
    int i, n;

    /* save the old first element of the list */
    head = list->list;
    remain = head->next;

    /* count the number of nodes in the list */
    n = 0;
    for (p = remain; p != head; p = p->next)
	n++;

    /* allocate an array of nodes and populate it */
    array = (Node **) xmalloc (sizeof(Node *) * n);
    i = 0;
    for (p = remain; p != head; p = p->next)
	array[i++] = p;

    /* sort the array of nodes */
    client_comp = comp;
    qsort (array, n, sizeof(Node *), qsort_comp);

    /* rebuild the list from beginning to end */
    head->next = head->prev = head;
    for (i = 0; i < n; i++)
    {
	p = array[i];
	p->next = head;
	p->prev = head->prev;
	p->prev->next = p;
	head->prev = p;
    }

    /* release the array of nodes */
    free (array);
}

/*
 * compare two files list node (for sort)
 */
int
fsortcmp (p, q)
    const Node *p;
    const Node *q;
{
    return (strcmp (p->key, q->key));
}

/* Debugging functions.  Quite useful to call from within gdb. */

static char *nodetypestring PROTO ((Ntype));

static char *
nodetypestring (type)
    Ntype type;
{
    switch (type) {
    case UNKNOWN:	return("UNKNOWN");
    case HEADER:	return("HEADER");
    case ENTRIES:	return("ENTRIES");
    case FILES:		return("FILES");
    case LIST:		return("LIST");
    case RCSNODE:	return("RCSNODE");
    case RCSVERS:	return("RCSVERS");
    case DIRS:		return("DIRS");
    case UPDATE:	return("UPDATE");
    case LOCK:		return("LOCK");
    case NDBMNODE:	return("NDBMNODE");
    case FILEATTR:	return("FILEATTR");
    case VARIABLE:	return("VARIABLE");
    case RCSFIELD:	return("RCSFIELD");
    }

    return("<trash>");
}

static int printnode PROTO ((Node *, void *));
static int
printnode (node, closure)
     Node *node;
     void *closure;
{
    if (node == NULL)
    {
	(void) printf("NULL node.\n");
	return(0);
    }

    (void) printf("Node at 0x%p: type = %s, key = 0x%p = \"%s\", data = 0x%p, next = 0x%p, prev = 0x%p\n",
	   node, nodetypestring(node->type), node->key, node->key, node->data, node->next, node->prev);

    return(0);
}

/* This is global, not static, so that its name is unique and to avoid
   compiler warnings about it not being used.  But it is not used by CVS;
   it exists so one can call it from a debugger.  */
void printlist PROTO ((List *));

void
printlist (list)
    List *list;
{
    if (list == NULL)
    {
	(void) printf("NULL list.\n");
	return;
    }

    (void) printf("List at 0x%p: list = 0x%p, HASHSIZE = %d, next = 0x%p\n",
	   list, list->list, HASHSIZE, list->next);
    
    (void) walklist(list, printnode, NULL);

    return;
}
