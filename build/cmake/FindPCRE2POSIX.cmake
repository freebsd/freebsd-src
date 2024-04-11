# - Find pcre2posix
# Find the native PCRE2-8 and PCRE2-POSIX include and libraries
#
#  PCRE2_INCLUDE_DIR    - where to find pcre2posix.h, etc.
#  PCRE2POSIX_LIBRARIES - List of libraries when using libpcre2-posix.
#  PCRE2_LIBRARIES      - List of libraries when using libpcre2-8.
#  PCRE2POSIX_FOUND     - True if libpcre2-posix found.
#  PCRE2_FOUND          - True if libpcre2-8 found.

IF (PCRE2_INCLUDE_DIR)
  # Already in cache, be silent
  SET(PCRE2_FIND_QUIETLY TRUE)
ENDIF (PCRE2_INCLUDE_DIR)

FIND_PATH(PCRE2_INCLUDE_DIR pcre2posix.h)
FIND_LIBRARY(PCRE2POSIX_LIBRARY NAMES pcre2-posix libpcre2-posix pcre2-posix-static)
FIND_LIBRARY(PCRE2_LIBRARY NAMES pcre2-8 libpcre2-8 pcre2-8-static)

# handle the QUIETLY and REQUIRED arguments and set PCRE2POSIX_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(PCRE2POSIX DEFAULT_MSG PCRE2POSIX_LIBRARY PCRE2_INCLUDE_DIR)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(PCRE2 DEFAULT_MSG PCRE2_LIBRARY)

IF(PCRE2POSIX_FOUND)
  SET(PCRE2POSIX_LIBRARIES ${PCRE2POSIX_LIBRARY})
  SET(HAVE_LIBPCRE2POSIX 1)
  SET(HAVE_PCRE2POSIX_H 1)
ENDIF(PCRE2POSIX_FOUND)

IF(PCRE2_FOUND)
  SET(PCRE2_LIBRARIES ${PCRE2_LIBRARY})
  SET(HAVE_LIBPCRE2 1)
ENDIF(PCRE2_FOUND)
