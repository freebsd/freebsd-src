/* @(#)hash.h 1.18 92/03/31	 */

/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.3 kit.
 */

/*
 * The number of buckets for the hash table contained in each list.  This
 * should probably be prime.
 */
#define HASHSIZE	151

/*
 * Types of nodes
 */
enum ntype
{
    UNKNOWN, HEADER, ENTRIES, FILES, LIST, RCSNODE,
    RCSVERS, DIRS, UPDATE, LOCK, NDBMNODE
};
typedef enum ntype Ntype;

struct node
{
    Ntype type;
    struct node *next;
    struct node *prev;
    struct node *hashnext;
    struct node *hashprev;
    char *key;
    char *data;
    void (*delproc) ();
};
typedef struct node Node;

struct list
{
    Node *list;
    Node *hasharray[HASHSIZE];
    struct list *next;
};
typedef struct list List;

struct entnode
{
    char *version;
    char *timestamp;
    char *options;
    char *tag;
    char *date;
};
typedef struct entnode Entnode;

#if __STDC__
List *getlist (void);
Node *findnode (List * list, char *key);
Node *getnode (void);
int addnode (List * list, Node * p);
int walklist (List * list, int (*proc) ());
void dellist (List ** listp);
void delnode (Node * p);
void freenode (Node * p);
void sortlist (List * list, int (*comp) ());
#else
List *getlist ();
Node *findnode ();
Node *getnode ();
int addnode ();
int walklist ();
void dellist ();
void delnode ();
void freenode ();
void sortlist ();
#endif				/* __STDC__ */
