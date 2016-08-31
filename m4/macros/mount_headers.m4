dnl ######################################################################
dnl an M4 macro to include a list of common headers being used everywhere
define(AMU_MOUNT_HEADERS,
[
#include "${srcdir}/include/mount_headers1.h"
#include AMU_NFS_PROTOCOL_HEADER
#include "${srcdir}/include/mount_headers2.h"

$1
]
)
dnl ======================================================================
