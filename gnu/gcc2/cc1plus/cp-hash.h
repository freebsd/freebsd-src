/* C code produced by gperf version 2.5 (GNU C++ version) */
/* Command-line: gperf -p -j1 -g -o -t -N is_reserved_word -k1,4,$,7 gplus.gperf  */
/* Command-line: gperf -p -j1 -g -o -t -N is_reserved_word -k1,4,$,7 gplus.gperf  */
struct resword { char *name; short token; enum rid rid;};

#define TOTAL_KEYWORDS 82
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 13
#define MIN_HASH_VALUE 4
#define MAX_HASH_VALUE 140
/* maximum key range = 137, duplicates = 0 */

#ifdef __GNUC__
inline
#endif
static unsigned int
hash (str, len)
     register char *str;
     register int unsigned len;
{
  static unsigned char asso_values[] =
    {
     141, 141, 141, 141, 141, 141, 141, 141, 141, 141,
     141, 141, 141, 141, 141, 141, 141, 141, 141, 141,
     141, 141, 141, 141, 141, 141, 141, 141, 141, 141,
     141, 141, 141, 141, 141, 141, 141, 141, 141, 141,
     141, 141, 141, 141, 141, 141, 141, 141, 141, 141,
     141, 141, 141, 141, 141, 141, 141, 141, 141, 141,
     141, 141, 141, 141, 141, 141, 141, 141, 141, 141,
     141, 141, 141, 141, 141, 141, 141, 141, 141, 141,
     141, 141, 141, 141, 141, 141, 141, 141, 141, 141,
     141, 141, 141, 141, 141,   0, 141,  49,   3,  28,
      28,   0,   5,  11,  32,  37, 141,   2,  24,  35,
      51,   0,  19, 141,  23,   0,   8,  48,   0,  36,
       0,  11, 141, 141, 141, 141, 141, 141,
    };
  register int hval = len;

  switch (hval)
    {
      default:
      case 7:
        hval += asso_values[str[6]];
      case 6:
      case 5:
      case 4:
        hval += asso_values[str[3]];
      case 3:
      case 2:
      case 1:
        hval += asso_values[str[0]];
    }
  return hval + asso_values[str[len - 1]];
}

#ifdef __GNUC__
inline
#endif
struct resword *
is_reserved_word (str, len)
     register char *str;
     register unsigned int len;
{
  static struct resword wordlist[] =
    {
      {"",}, {"",}, {"",}, {"",}, 
      {"else",  ELSE, NORID,},
      {"",}, {"",}, 
      {"__asm__",  GCC_ASM_KEYWORD, NORID},
      {"",}, {"",}, 
      {"__headof__",  HEADOF, NORID},
      {"sizeof",  SIZEOF, NORID,},
      {"this",  THIS, NORID,},
      {"__headof",  HEADOF, NORID},
      {"except",  EXCEPT, NORID		/* Extension */,},
      {"goto",  GOTO, NORID,},
      {"",}, 
      {"__const__",  TYPE_QUAL, RID_CONST},
      {"__volatile",  TYPE_QUAL, RID_VOLATILE},
      {"typeof",  TYPEOF, NORID,},
      {"__volatile__",  TYPE_QUAL, RID_VOLATILE},
      {"__typeof__",  TYPEOF, NORID},
      {"try",  TRY, NORID			/* Extension */,},
      {"__const",  TYPE_QUAL, RID_CONST},
      {"__typeof",  TYPEOF, NORID},
      {"typedef",  SCSPEC, RID_TYPEDEF,},
      {"private",  VISSPEC, RID_PRIVATE,},
      {"",}, 
      {"raise",  RAISE, NORID		/* Extension */,},
      {"raises",  RAISES, NORID		/* Extension */,},
      {"do",  DO, NORID,},
      {"for",  FOR, NORID,},
      {"case",  CASE, NORID,},
      {"class",  AGGR, RID_CLASS,},
      {"delete",  DELETE, NORID,},
      {"__classof__",  CLASSOF, NORID},
      {"short",  TYPESPEC, RID_SHORT,},
      {"double",  TYPESPEC, RID_DOUBLE,},
      {"__classof",  CLASSOF, NORID},
      {"friend",  SCSPEC, RID_FRIEND,},
      {"__asm",  GCC_ASM_KEYWORD, NORID},
      {"const",  TYPE_QUAL, RID_CONST,},
      {"static",  SCSPEC, RID_STATIC,},
      {"template",  TEMPLATE, NORID,},
      {"if",  IF, NORID,},
      {"classof",  CLASSOF, NORID,},
      {"switch",  SWITCH, NORID,},
      {"__signed__",  TYPESPEC, RID_SIGNED},
      {"int",  TYPESPEC, RID_INT,},
      {"throw",  THROW, NORID		/* Extension */,},
      {"long",  TYPESPEC, RID_LONG,},
      {"",}, {"",}, 
      {"auto",  SCSPEC, RID_AUTO,},
      {"operator",  OPERATOR, NORID,},
      {"",}, 
      {"__attribute",  ATTRIBUTE, NORID},
      {"extern",  SCSPEC, RID_EXTERN,},
      {"__attribute__",  ATTRIBUTE, NORID},
      {"break",  BREAK, NORID,},
      {"void",  TYPESPEC, RID_VOID,},
      {"",}, 
      {"struct",  AGGR, RID_RECORD,},
      {"virtual",  SCSPEC, RID_VIRTUAL,},
      {"__extension__",  EXTENSION, NORID},
      {"while",  WHILE, NORID,},
      {"",}, 
      {"float",  TYPESPEC, RID_FLOAT,},
      {"__wchar_t",  TYPESPEC, RID_WCHAR  /* Unique to ANSI C++ */,},
      {"",}, {"",}, 
      {"headof",  HEADOF, NORID,},
      {"protected",  VISSPEC, RID_PROTECTED,},
      {"__signed",  TYPESPEC, RID_SIGNED},
      {"enum",  ENUM, NORID,},
      {"",}, 
      {"all",  ALL, NORID			/* Extension */,},
      {"public",  VISSPEC, RID_PUBLIC,},
      {"char",  TYPESPEC, RID_CHAR,},
      {"reraise",  RERAISE, NORID		/* Extension */,},
      {"inline",  SCSPEC, RID_INLINE,},
      {"volatile",  TYPE_QUAL, RID_VOLATILE,},
      {"__label__",  LABEL, NORID},
      {"",}, {"",}, 
      {"signed",  TYPESPEC, RID_SIGNED,},
      {"__alignof__",  ALIGNOF, NORID},
      {"asm",  ASM_KEYWORD, NORID,},
      {"",}, 
      {"__alignof",  ALIGNOF, NORID},
      {"new",  NEW, NORID,},
      {"register",  SCSPEC, RID_REGISTER,},
      {"continue",  CONTINUE, NORID,},
      {"catch",  CATCH, NORID,},
      {"",}, {"",}, {"",}, 
      {"exception",  AGGR, RID_EXCEPTION	/* Extension */,},
      {"",}, {"",}, 
      {"default",  DEFAULT, NORID,},
      {"",}, {"",}, {"",}, 
      {"union",  AGGR, RID_UNION,},
      {"",}, {"",}, {"",}, 
      {"overload",  OVERLOAD, NORID,},
      {"",}, 
      {"__inline",  SCSPEC, RID_INLINE},
      {"",}, 
      {"__inline__",  SCSPEC, RID_INLINE},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"unsigned",  TYPESPEC, RID_UNSIGNED,},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"return",  RETURN, NORID,},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, 
      {"dynamic",  DYNAMIC, NORID,},
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register char *s = wordlist[key].name;

          if (*s == *str && !strcmp (str + 1, s + 1))
            return &wordlist[key];
        }
    }
  return 0;
}
