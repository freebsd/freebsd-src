# If you change this file, please also look at files which source this one:
# elf_i386_vxworks.sh elf32ppcvxworks.sh elf32ebmipvxworks.sh

# The Diab tools use a different init/fini convention.  Initialization code
# is place in sections named ".init$NN".  These sections are then concatenated
# into the .init section.  It is important that .init$00 be first and .init$99
# be last. The other sections should be sorted, but the current linker script
# parse does not seem to allow that with the SORT keyword in this context.
INIT_START='_init = .;
            KEEP (*(.init$00));
            KEEP (*(.init$0[1-9]));
            KEEP (*(.init$[1-8][0-9]));
            KEEP (*(.init$9[0-8]));'
INIT_END='KEEP (*(.init$99));'
FINI_START='_fini = .;
            KEEP (*(.fini$00));
            KEEP (*(.fini$0[1-9]));
            KEEP (*(.fini$[1-8][0-9]));
            KEEP (*(.fini$9[0-8]));'
FINI_END='KEEP (*(.fini$99));
          PROVIDE (_etext = .);'

ETEXT_NAME=etext_unrelocated
OTHER_SYMBOLS="PROVIDE (_ehdr = ${TEXT_START_ADDR});"
DATA_END_SYMBOLS=".edata : { PROVIDE (_edata = .); }"
VXWORKS_BASE_EM_FILE=$EXTRA_EM_FILE
EXTRA_EM_FILE=vxworks
unset EMBEDDED
