#ifndef _prototype_h
#define _prototype_h
#ifdef __STDC__
#define P(X) X
#else
#define P(X) ()
#endif

typedef char bool;
#define FALSE 0
#define TRUE 1

#define ODD(X) ((X) & 1)
#define EVEN(X) (!((X) & 1))
#endif /* _prototype_h */
