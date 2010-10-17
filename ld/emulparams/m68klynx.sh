SCRIPT_NAME=m68klynx
OUTPUT_FORMAT="coff-m68k-lynx"
# This is what LynxOS /lib/init1.o wants.
ENTRY=__main
# following are dubious
TEXT_START_ADDR=0
TARGET_PAGE_SIZE=0x1000
ARCH=m68k
