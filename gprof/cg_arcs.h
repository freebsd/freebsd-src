#ifndef cg_arcs_h
#define cg_arcs_h

/*
 * Arc structure for call-graph.
 *
 * With pointers to the symbols of the parent and the child, a count
 * of how many times this arc was traversed, and pointers to the next
 * parent of this child and the next child of this parent.
 */
typedef struct arc
  {
    Sym *parent;		/* source vertice of arc */
    Sym *child;			/* dest vertice of arc */
    unsigned long count;	/* # of calls from parent to child */
    double time;		/* time inherited along arc */
    double child_time;		/* child-time inherited along arc */
    struct arc *next_parent;	/* next parent of CHILD */
    struct arc *next_child;	/* next child of PARENT */
    int has_been_placed;	/* have this arc's functions been placed? */
  }
Arc;

extern unsigned int num_cycles;	/* number of cycles discovered */
extern Sym *cycle_header;	/* cycle headers */

extern void arc_add PARAMS ((Sym * parent, Sym * child, unsigned long count));
extern Arc *arc_lookup PARAMS ((Sym * parent, Sym * child));
extern Sym **cg_assemble PARAMS ((void));
extern Arc **arcs;
extern unsigned int numarcs;

#endif /* cg_arcs_h */
