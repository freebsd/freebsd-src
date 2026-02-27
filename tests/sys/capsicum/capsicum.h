/*
 * Minimal portability layer for Capsicum-related features.
 */
#ifndef __CAPSICUM_H__
#define __CAPSICUM_H__

#include <stdio.h>

#include "capsicum-freebsd.h"
#include "capsicum-rights.h"

/* New-style Capsicum API extras for debugging */
static inline void cap_rights_describe(const cap_rights_t *rights, char *buffer) {
  int ii;
  for (ii = 0; ii < (CAP_RIGHTS_VERSION+2); ii++) {
    int len = sprintf(buffer, "0x%016llx ", (unsigned long long)rights->cr_rights[ii]);
    buffer += len;
  }
}

#ifdef __cplusplus
#include <iostream>
#include <iomanip>
#include <string>

inline std::ostream& operator<<(std::ostream& os, cap_rights_t rights) {
  for (int ii = 0; ii < (CAP_RIGHTS_VERSION+2); ii++) {
    os << std::hex << std::setw(16) << std::setfill('0') << (unsigned long long)rights.cr_rights[ii] << " ";
  }
  return os;
}
extern std::string capsicum_test_bindir;
#endif

#endif /*__CAPSICUM_H__*/
