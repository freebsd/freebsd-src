# If you change this file, please also look at files which source this one:
# h8300helf.sh h8300self.sh

SCRIPT_NAME=elf
OUTPUT_FORMAT="elf32-h8300"
TEXT_START_ADDR=0x100
MAXPAGESIZE=2
TARGET_PAGE_SIZE=128
ARCH=h8300
TEMPLATE_NAME=elf32
EMBEDDED=yes
STACK_ADDR=0xfefc
