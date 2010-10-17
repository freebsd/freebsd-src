# This file is sourced by the genscripts.sh script.
# These are shell variables that are used later by either genscripts
# or on of the scripts that it sources.

# The name of the scripttempl script to use.  In this case, genscripts
# uses scripttempl/tic80coff.sc
#
SCRIPT_NAME=tic80coff

# The name of the emultempl script to use.  If set to "template" then
# genscripts.sh will use the script emultempl/template.em.  If not set,
# then the default value is "generic".
#
# TEMPLATE_NAME=

# If this is set to an nonempty string, genscripts.sh will invoke the
# scripttempl script an extra time to create a shared library script.
#
# GENERATE_SHLIB_SCRIPT=

# The BFD output format to use.  The scripttempl script will use it in
# an OUTPUT_FORMAT expression in the linker script.
#
OUTPUT_FORMAT="coff-tic80"

# This is normally set to indicate the architecture to use, such as
# "sparc".  The scripttempl script will normally use it in an OUTPUT_ARCH
# expression in the linker script.
#
ARCH=tic80

# Some scripttempl scripts use this to set the entry address in an ENTRY
# expression in the linker script.
#
# ENTRY=

# The scripttempl script uses this to set the start address of the
# ".text" section.
#
TEXT_START_ADDR=0x2000000

# If this is defined, the genscripts.sh script sets TEXT_START_ADDR to
# its value before running the scripttempl script for the -n and -N
# options.
#
# NONPAGED_TEXT_START_ADDR=

# The genscripts.sh script uses this to set the default value of 
# DATA_ALIGNMENT when running the scripttempl script.
#
# SEGMENT_SIZE=

# If SEGMENT_SIZE is not defined, the genscripts.sh script uses this
# to define it.
#
TARGET_PAGE_SIZE=0x1000
