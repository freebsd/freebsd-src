#define SICP
#include "scm-tags.h"
#undef SCM_NCELLP
#define SCM_NCELLP(x) 	((SCM_SIZE-1) & (int)(x))
#define SCM_ITAG8_DATA(X)	((X)>>8)
#define SCM_ICHR(x)	((unsigned char)SCM_ITAG8_DATA(x))
#define SCM_ICHRP(x)    (SCM_ITAG8(x) == scm_tc8_char)
#define scm_tc8_char 0xf4
#define SCM_IFLAGP(n)            ((0x87 & (int)(n))==4)
#define SCM_ISYMNUM(n)           ((int)((n)>>9))
#define SCM_ISYMCHARS(n)         (scm_isymnames[SCM_ISYMNUM(n)])
#define SCM_ILOCP(n)             ((0xff & (int)(n))==0xfc)
#define SCM_ITAG8(X)             ((int)(X) & 0xff)
#define SCM_TYP7(x)              (0x7f & (int)SCM_CAR(x))
#define SCM_LENGTH(x) (((unsigned long)SCM_CAR(x))>>8)
#define SCM_NCONSP(x) (1 & (int)SCM_CAR(x))
#define SCM_NECONSP(x) (SCM_NCONSP(x) && (1 != SCM_TYP3(x)))
#define SCM_CAR(x) scm_get_field (x, 0)
#define SCM_CDR(x) scm_get_field (x, 1)
#define SCM_VELTS(x) ((SCM *)SCM_CDR(x))
#define SCM_CLOSCAR(x) (SCM_CAR(x)-scm_tc3_closure)
#define SCM_CODE(x) SCM_CAR(SCM_CLOSCAR (x))
#define SCM_MAKINUM(x) (((x)<<2)+2L)

#ifdef __STDC__		/* Forward decls for prototypes */
struct value;
#endif

extern int scm_value_print PARAMS ((struct value *, GDB_FILE*,
				    int, enum val_prettyprint));

extern int scm_val_print PARAMS ((struct type*, char*, CORE_ADDR, GDB_FILE*,
				 int, int, int, enum val_prettyprint));

extern LONGEST scm_get_field PARAMS ((LONGEST, int));

extern int scm_scmval_print PARAMS ((LONGEST, GDB_FILE *,
				     int, int, int, enum val_prettyprint));

extern int is_scmvalue_type PARAMS ((struct type*));

extern void scm_printchar PARAMS ((int, GDB_FILE*));

extern struct value * scm_evaluate_string PARAMS ((char*, int));

extern struct type *builtin_type_scm;

extern int scm_parse ();

extern LONGEST scm_unpack PARAMS ((struct type *, char *, enum type_code));
