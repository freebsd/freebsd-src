SCRIPT_NAME=aout
OUTPUT_FORMAT="a.out-hp300hpux"
TEXT_START_ADDR=0
TARGET_PAGE_SIZE=4096
ARCH=m68k
STACKZERO="___stack_zero = 0x2000; __DYNAMIC = 0;"
# This is needed for HPUX 9.0; it is unnecessary but harmless for 8.0.
SHLIB_PATH="___dld_shlib_path = 0;"
