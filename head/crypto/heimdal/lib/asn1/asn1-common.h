/* $Id: asn1-common.h 22429 2008-01-13 10:25:50Z lha $ */

#include <stddef.h>
#include <time.h>

#ifndef __asn1_common_definitions__
#define __asn1_common_definitions__

typedef struct heim_integer {
    size_t length;
    void *data;
    int negative;
} heim_integer;

typedef struct heim_octet_string {
    size_t length;
    void *data;
} heim_octet_string;

typedef char *heim_general_string;
typedef char *heim_utf8_string;
typedef char *heim_printable_string;
typedef char *heim_ia5_string;

typedef struct heim_bmp_string {
    size_t length;
    uint16_t *data;
} heim_bmp_string;

typedef struct heim_universal_string {
    size_t length;
    uint32_t *data;
} heim_universal_string;

typedef char *heim_visible_string;

typedef struct heim_oid {
    size_t length;
    unsigned *components;
} heim_oid;

typedef struct heim_bit_string {
    size_t length;
    void *data;
} heim_bit_string;

typedef struct heim_octet_string heim_any;
typedef struct heim_octet_string heim_any_set;

#define ASN1_MALLOC_ENCODE(T, B, BL, S, L, R)                  \
  do {                                                         \
    (BL) = length_##T((S));                                    \
    (B) = malloc((BL));                                        \
    if((B) == NULL) {                                          \
      (R) = ENOMEM;                                            \
    } else {                                                   \
      (R) = encode_##T(((unsigned char*)(B)) + (BL) - 1, (BL), \
                       (S), (L));                              \
      if((R) != 0) {                                           \
        free((B));                                             \
        (B) = NULL;                                            \
      }                                                        \
    }                                                          \
  } while (0)

#endif
