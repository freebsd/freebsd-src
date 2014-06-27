#
# Common make for acpica tools and utilities
#

#
# Note: This makefile is intended to be used from within the native
# ACPICA directory structure, from under top level acpica directory.
# It specifically places all the object files for each tool in separate
# generate/unix subdirectories, not within the various ACPICA source
# code directories. This prevents collisions between different
# compilations of the same source file with different compile options.
#

ifeq ($(OS),efi)
	BUILD_DIRECTORY_PATH = "generate/efi"
	include generate/efi/Makefile.config
	include generate/efi/Makefile.common
else
	BUILD_DIRECTORY_PATH = "generate/unix"
	include generate/unix/Makefile.config
	include generate/unix/Makefile.common
endif
