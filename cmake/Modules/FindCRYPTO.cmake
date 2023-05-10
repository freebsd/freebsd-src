#
# Try to find libcrypto.
#

# Try to find the header
find_path(CRYPTO_INCLUDE_DIR openssl/crypto.h)

# Try to find the library
find_library(CRYPTO_LIBRARY crypto)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CRYPTO
  DEFAULT_MSG
  CRYPTO_INCLUDE_DIR
  CRYPTO_LIBRARY
)

mark_as_advanced(
  CRYPTO_INCLUDE_DIR
  CRYPTO_LIBRARY
)

set(CRYPTO_INCLUDE_DIRS ${CRYPTO_INCLUDE_DIR})
set(CRYPTO_LIBRARIES ${CRYPTO_LIBRARY})
