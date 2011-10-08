/* ./combining_table.h */
/* Automatically generated at 2011-09-30T15:24:59.272235 */

#ifndef COMBINING_TABLE_H
#define COMBINING_TABLE_H 1

#include <krb5-types.h>

struct translation {
  uint32_t key;
  unsigned combining_class;	
};

extern const struct translation _wind_combining_table[];

extern const size_t _wind_combining_table_size;
#endif /* COMBINING_TABLE_H */
