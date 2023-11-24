#
# Try to find libsmi.
#

# Try to find the header
find_path(SMI_INCLUDE_DIR smi.h)

# Try to find the library
find_library(SMI_LIBRARY smi)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SMI
  DEFAULT_MSG
  SMI_INCLUDE_DIR
  SMI_LIBRARY
)

mark_as_advanced(
  SMI_INCLUDE_DIR
  SMI_LIBRARY
)

set(SMI_INCLUDE_DIRS ${SMI_INCLUDE_DIR})
set(SMI_LIBRARIES ${SMI_LIBRARY})
