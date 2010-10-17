#ifndef cg_dfn_h
#define cg_dfn_h

/*
 * Flags which mark a symbol as topologically ``busy'' or as
 * topologically ``not_numbered'':
 */
#define	DFN_BUSY	-1
#define	DFN_NAN		0

/*
 * Depth-first numbering of a call-graph.
 */

extern void cg_dfn PARAMS ((Sym * root));

#endif /* cg_dfn_h */
