/* Machine specific defines for the SCO Unix V.3.2 ODT */
#define scounix

/* Return true if s (a non null string pointer), points to a local variable name. */
#define LOCAL_LABEL(n)  ((n)[0] == '.' && (n)[1] == 'L')

/* end of te-sco386.h */
