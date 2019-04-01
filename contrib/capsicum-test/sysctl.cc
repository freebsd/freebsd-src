#include "capsicum.h"
#include "capsicum-test.h"

#ifdef HAVE_SYSCTL
#include <sys/sysctl.h>

// Certain sysctls are permitted in capability mode, but most are not.  Test
// for the ones that should be, and try one or two that shouldn't.
TEST(Sysctl, Capability) {
  int oid[2] = {CTL_KERN, KERN_OSRELDATE};
  int ii;
  size_t len = sizeof(ii);
  EXPECT_OK(sysctl(oid, 2, &ii, &len, NULL, 0));
}
#endif
