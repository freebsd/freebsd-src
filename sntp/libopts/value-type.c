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
// %define slot-name               vtp_name
// %define hash-function-name      value_type_hash
// %define lookup-function-name    find_value_type_name
// %define word-array-name         value_type_table
// %define initializer-suffix      ,VTP_COUNT_KWD
#endif /* gperf build options: */

#include "value-type.h"

typedef struct {
    char const *    vtp_name;
    value_type_enum_t   vtp_id;
} value_type_map_t;
#include <string.h>

/* maximum key range = 20, duplicates = 0 */

#ifdef __GNUC__
#else
#ifdef __cplusplus
#endif
#endif
inline static unsigned int
value_type_hash (register const char *str, register unsigned int len)
{
  static const unsigned char asso_values[] =
    {
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 10, 23, 23, 23, 23, 23, 23, 23, 23,
      23,  5, 23, 23,  5,  0,  0, 23, 15, 23,
      23, 10, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23
    };
  return len + asso_values[(unsigned char)str[2]];
}

static const value_type_map_t value_type_table[] =
  {
    {"",VTP_COUNT_KWD}, {"",VTP_COUNT_KWD},
    {"",VTP_COUNT_KWD},
    {"set",             VTP_KWD_SET},
    {"",VTP_COUNT_KWD}, {"",VTP_COUNT_KWD},
    {"nested",          VTP_KWD_NESTED},
    {"integer",         VTP_KWD_INTEGER},
    {"",VTP_COUNT_KWD},
    {"bool",            VTP_KWD_BOOL},
    {"",VTP_COUNT_KWD},
    {"string",          VTP_KWD_STRING},
    {"boolean",         VTP_KWD_BOOLEAN},
    {"",VTP_COUNT_KWD},
    {"set-membership",  VTP_KWD_SET_MEMBERSHIP},
    {"",VTP_COUNT_KWD}, {"",VTP_COUNT_KWD},
    {"keyword",         VTP_KWD_KEYWORD},
    {"",VTP_COUNT_KWD},
    {"hierarchy",       VTP_KWD_HIERARCHY},
    {"",VTP_COUNT_KWD}, {"",VTP_COUNT_KWD},
    {"invalid",         VTP_KWD_INVALID}
  };

#ifdef __GNUC__
#if defined __GNUC_STDC_INLINE__ || defined __GNUC_GNU_INLINE__
__attribute__ ((__gnu_inline__))
#endif
#endif
static inline const value_type_map_t *
find_value_type_name (register const char *str, register unsigned int len)
{
  if (len <= 14 && len >= 3)
    {
      register int key = value_type_hash (str, len);

      if (key <= 22 && key >= 0)
        {
          register const char *s = value_type_table[key].vtp_name;

          if (*str == *s && !strncmp (str + 1, s + 1, len - 1) && s[len] == '\0')
            return &value_type_table[key];
        }
    }
  return 0;
}


value_type_enum_t
find_value_type_id(char const * str, unsigned int len)
{
    const value_type_map_t * p =
        find_value_type_name(str, len);
    return (p == 0) ? VTP_KWD_INVALID : p->vtp_id;
}
