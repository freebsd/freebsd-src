/*
 * Target environment for FreeBSD.  It is the same as the generic
 * target, except it arranges to suppress the use of "/" as a comment
 * character.  Some code in the FreeBSD kernel uses "/" to mean
 * division.  (What a concept.)
 */
#define TE_FreeBSD 1
#include "te-generic.h"
