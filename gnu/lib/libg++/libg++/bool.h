// Defining TRUE and FALSE is usually a Bad Idea,
// because you will probably be inconsistent with anyone
// else who had the same clever idea.
// Therefore:  DON'T USE THIS FILE.

#ifndef _bool_h
#define _bool_h 1

#undef FALSE
#undef TRUE
enum bool { FALSE = 0, TRUE = 1 };

#endif
