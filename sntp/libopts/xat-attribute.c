/* ANSI-C code produced by gperf version 3.0.4 */


#if 0 /* gperf build options: */
// %struct-type
// %language=ANSI-C
// %includes
// %global-table
// %omit-struct-type
// %readonly-tables
// %compare-strncmp
// 
// %define slot-name               xat_name
// %define hash-function-name      xat_attribute_hash
// %define lookup-function-name    find_xat_attribute_name
// %define word-array-name         xat_attribute_table
// %define initializer-suffix      ,XAT_COUNT_KWD
#endif /* gperf build options: */

#include "xat-attribute.h"

typedef struct {
    char const *    xat_name;
    xat_attribute_enum_t   xat_id;
} xat_attribute_map_t;
#include <string.h>

/* maximum key range = 9, duplicates = 0 */

#ifdef __GNUC__
#else
#ifdef __cplusplus
#endif
#endif
inline static unsigned int
xat_attribute_hash (register const char *str, register unsigned int len)
{
  static const unsigned char asso_values[] =
    {
      13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
      13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
      13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
      13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
      13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
      13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
      13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
      13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
      13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
      13, 13, 13, 13, 13, 13, 13, 13, 13,  0,
      13, 13, 13, 13, 13,  5, 13,  5, 13,  0,
      13, 13, 13, 13, 13, 13,  0,  0, 13,  0,
      13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
      13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
      13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
      13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
      13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
      13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
      13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
      13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
      13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
      13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
      13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
      13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
      13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
      13, 13, 13, 13, 13, 13
    };
  return len + asso_values[(unsigned char)str[0]];
}

static const xat_attribute_map_t xat_attribute_table[] =
  {
    {"",XAT_COUNT_KWD}, {"",XAT_COUNT_KWD},
    {"",XAT_COUNT_KWD}, {"",XAT_COUNT_KWD},
    {"type",            XAT_KWD_TYPE},
    {"words",           XAT_KWD_WORDS},
    {"cooked",          XAT_KWD_COOKED},
    {"members",         XAT_KWD_MEMBERS},
    {"uncooked",        XAT_KWD_UNCOOKED},
    {"keep",            XAT_KWD_KEEP},
    {"",XAT_COUNT_KWD}, {"",XAT_COUNT_KWD},
    {"invalid",         XAT_KWD_INVALID}
  };

#ifdef __GNUC__
#if defined __GNUC_STDC_INLINE__ || defined __GNUC_GNU_INLINE__
__attribute__ ((__gnu_inline__))
#endif
#endif
static inline const xat_attribute_map_t *
find_xat_attribute_name (register const char *str, register unsigned int len)
{
  if (len <= 8 && len >= 4)
    {
      register int key = xat_attribute_hash (str, len);

      if (key <= 12 && key >= 0)
        {
          register const char *s = xat_attribute_table[key].xat_name;

          if (*str == *s && !strncmp (str + 1, s + 1, len - 1) && s[len] == '\0')
            return &xat_attribute_table[key];
        }
    }
  return 0;
}


xat_attribute_enum_t
find_xat_attribute_id(char const * str, unsigned int len)
{
    const xat_attribute_map_t * p =
        find_xat_attribute_name(str, len);
    return (p == 0) ? XAT_KWD_INVALID : p->xat_id;
}
