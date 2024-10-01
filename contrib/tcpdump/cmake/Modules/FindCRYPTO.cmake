#
# Try to find libcrypto.
#

#
# Were we told where to look for libcrypto?
#
if(NOT CRYPTO_ROOT)
  #
  # No.
  #
  # First, try looking for it with pkg-config, if we have it.
  #
  find_package(PkgConfig)

  #
  # Homebrew's pkg-config does not, by default, look for
  # pkg-config files for packages it has installed.
  # Furthermore, at least for OpenSSL, they appear to be
  # dumped in package-specific directories whose paths are
  # not only package-specific but package-version-specific.
  #
  # So the only way to find openssl is to get the value of
  # PKG_CONFIG_PATH from "brew --env openssl" and add that
  # to PKG_CONFIG_PATH.  (No, we can't just assume it's under
  # /usr/local; Homebrew have conveniently chosen to put it
  # under /opt/homebrew on ARM.)
  #
  # That's the nice thing about Homebrew - it makes things easier!
  # Thanks!
  #
  find_program(BREW brew)
  if(BREW)
    #
    # We have Homebrew.
    # Get the pkg-config directory for openssl.
    #
    execute_process(COMMAND "${BREW}" "--env" "--plain" "openssl"
      RESULT_VARIABLE BREW_RESULT
      OUTPUT_VARIABLE BREW_OUTPUT
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(BREW_RESULT EQUAL 0)
      #
      # brew --env --plain openssl succeeded.
      # Split its output into a list, one entry per line.
      #
      string(REGEX MATCHALL "[^\n\r]+" BREW_OUTPUT_LINES "${BREW_OUTPUT}")

      #
      # Find the line that begins with "PKG_CONFIG_PATH: ", and extract
      # the path following that.
      #
      foreach(LINE IN LISTS BREW_OUTPUT_LINES)
        if(LINE MATCHES "PKG_CONFIG_PATH: \(.*\)")
          string(REGEX REPLACE "PKG_CONFIG_PATH: \(.*\)"
              "\\1" OPENSSL_PKGCONFIG_DIR
              ${LINE})
        endif()
      endforeach()
    endif()
  endif()

  #
  # Save the current value of the PKG_CONFIG_PATH environment
  # variable.
  #
  set(SAVE_PKG_CONFIG_PATH $ENV{PKG_CONFIG_PATH})

  #
  # If we got an additional pkg-config directory from Homebrew, add
  # it to the PKG_CONFIG_PATH environment variable.
  #
  if(OPENSSL_PKGCONFIG_DIR)
    set(ENV{PKG_CONFIG_PATH} "${OPENSSL_PKGCONFIG_DIR}:$ENV{PKG_CONFIG_PATH}")
  endif()

  #
  # Use pkg-config to find libcrypto.
  #
  pkg_check_modules(CRYPTO libcrypto)

  #
  # Revert the change to PKG_CONFIG_PATH.
  #
  set(ENV{PKG_CONFIG_PATH} "${SAVE_PKG_CONFIG_PATH}")

  #
  # Did pkg-config find it?
  #
  if(CRYPTO_FOUND)
    #
    # This "helpfully" supplies CRYPTO_LIBRARIES as a bunch of
    # library names - not paths - and CRYPTO_LIBRARY_DIRS as
    # a bunch of directories.
    #
    # CMake *really* doesn't like the notion of specifying "here are
    # the directories in which to look for libraries" except in
    # find_library() calls; it *really* prefers using full paths to
    # library files, rather than library names.
    #
    # Find the libraries and add their full paths.
    #
    set(CRYPTO_LIBRARY_FULLPATHS)
    foreach(_lib IN LISTS CRYPTO_LIBRARIES)
      #
      # Try to find this library, so we get its full path.
      #
      find_library(_libfullpath ${_lib} HINTS ${CRYPTO_LIBRARY_DIRS})
      list(APPEND CRYPTO_LIBRARY_FULLPATHS ${_libfullpath})
    endforeach()
    set(CRYPTO_LIBRARIES "${CRYPTO_LIBRARY_FULLPATHS}")
  else()
    #
    # No.  If we have Homebrew installed, see if it's in Homebrew.
    #
    if(BREW)
      #
      # The brew man page lies when it speaks of
      # $BREW --prefix --installed <formula>
      # outputting nothing.  In Homebrew 3.3.16,
      # it produces output regardless of whether
      # the formula is installed or not, so we
      # send the standard output and error to
      # the bit bucket.
      #
      # libcrypto isn't a formula, openssl is a formula.
      #
      execute_process(COMMAND "${BREW}" "--prefix" "--installed" "openssl"
        RESULT_VARIABLE BREW_RESULT
        OUTPUT_QUIET
      )
      if(BREW_RESULT EQUAL 0)
        #
        # Yes.  Get the include directory and library
        # directory.  (No, we can't just assume it's
        # under /usr/local; Homebrew have conveniently
        # chosen to put it under /opt/homebrew on ARM.)
        #
        execute_process(COMMAND "${BREW}" "--prefix" "openssl"
          RESULT_VARIABLE BREW_RESULT
          OUTPUT_VARIABLE OPENSSL_PATH
          OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        set(CRYPTO_INCLUDE_DIRS "${OPENSSL_PATH}/include")

        #
        # Search for the libcrypto library under lib.
        #
        find_library(CRYPTO_LIBRARIES crypto
            PATHS "${OPENSSL_PATH}/lib"
            NO_DEFAULT_PATH)
      endif()
    endif()
  endif()
endif()

#
# Have we found it with pkg-config or Homebrew?
#
if(NOT CRYPTO_INCLUDE_DIRS)
  #
  # No.
  # Try to find the openss/evp.h header.
  # We search for that header to make sure that it's installed (if
  # it's just a shared library for the benefit of existing
  # programs, that's not useful).
  #
  find_path(CRYPTO_INCLUDE_DIRS openssl/evp.h)

  #
  # Try to find the library.
  #
  find_library(CRYPTO_LIBRARIES crypto)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CRYPTO
  DEFAULT_MSG
  CRYPTO_INCLUDE_DIRS
  CRYPTO_LIBRARIES
)

mark_as_advanced(
  CRYPTO_INCLUDE_DIRS
  CRYPTO_LIBRARIES
)
