SCRIPT_NAME=sparclynx
OUTPUT_FORMAT="coff-sparc-lynx"
# This is what LynxOS /lib/init1.o wants.
ENTRY=__main
# following are dubious
TARGET_PAGE_SIZE=0x1000
TEXT_START_ADDR=0
NONPAGED_TEXT_START_ADDR=0x1000
ARCH=sparc
