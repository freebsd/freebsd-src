/* context.h */

#define M      pos[0].g       /* Index of current token in model. */
#ifdef P
#undef P
#endif
#define P      pos[0].t       /* Index of current group in pos. */
#define G      pos[P].g       /* Index of current group in model. */
#define T      pos[P].t       /* Index of current token in its group. */
#define Tstart pos[P].tstart  /* Index of starting token in its group
				 for AND group testing. */
#define H      pos[P].h       /* Pointer to hit bits for current group. */
#define GHDR   mod[G]         /* Current group header. */
#define TOKEN  mod[M]         /* Current token. */
#define TTYPE (GET(TOKEN.ttype, TTMASK))  /* Token type of current token. */
#define TOCC  (GET(TOKEN.ttype, TOREP))   /* Occurrence for current token. */
#define GTYPE (GET(GHDR.ttype, TTMASK))   /* Token type of current group. */
#define GOCC  (GET(GHDR.ttype, TOREP))    /* Occurrence for current group. */
#define GNUM  GHDR.tu.tnum                /* Number of tokens in current grp. */
