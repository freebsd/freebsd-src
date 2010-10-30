#name: --warn-shared-textrel --fatal-warnings
#as: --32
#ld: -shared -melf_i386 --warn-shared-textrel --fatal-warnings
#error: .*warning: creating a DT_TEXTREL in a shared object.
