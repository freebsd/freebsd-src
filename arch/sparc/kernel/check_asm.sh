#!/bin/sh
case $1 in
  -printf)
    sed -n -e '/^#/d;/struct[ 	]*'$2'_struct[ 	]*{/,/};/p' < $3 | sed '/struct[ 	]*'$2'_struct[ 	]*{/d;/:[0-9]*[ 	]*;/d;/^[ 	]*$/d;/};/d;s/^[ 	]*//;s/volatile[ 	]*//;s/\(unsigned\|signed\|struct\)[ 	]*//;s/\(\[\|__attribute__\).*;[ 	]*$//;s/(\*//;s/)(.*)//;s/;[ 	]*$//;s/^[^ 	]*[ 	]*//;s/,/\
/g' | sed 's/^[ 	*]*//;s/[ 	]*$//;s/^.*$/printf ("#define AOFF_'$2'_\0	0x%08x\\n", check_asm_data[i++]); printf("#define ASIZ_'$2'_\0	0x%08x\\n", check_asm_data[i++]);/' >> $4
    echo "printf (\"#define ASIZ_$2\\t0x%08x\\n\", check_asm_data[i++]);" >> $4
  ;;
  -data)
    sed -n -e '/^#/d;/struct[ 	]*'$2'_struct[ 	]*{/,/};/p' < $3 | sed '/struct[ 	]*'$2'_struct[ 	]*{/d;/:[0-9]*[ 	]*;/d;/^[ 	]*$/d;/};/d;s/^[ 	]*//;s/volatile[ 	]*//;s/\(unsigned\|signed\|struct\)[ 	]*//;s/\(\[\|__attribute__\).*;[ 	]*$//;s/(\*//;s/)(.*)//;s/;[ 	]*$//;s/^[^ 	]*[ 	]*//;s/,/\
/g' | sed 's/^[ 	*]*//;s/[ 	]*$//;s/^.*$/	((char *)\&((struct '$2'_struct *)0)->\0) - ((char *)((struct '$2'_struct *)0)),	sizeof(((struct '$2'_struct *)0)->\0),/' >> $4
    echo "	sizeof(struct $2_struct)," >> $4
  ;;
  -ints)
    sed -n -e '/check_asm_data:/,/\.size/p' <$2 | sed -e 's/check_asm_data://' -e 's/\.size.*//' -e 's/\.ident.*//' -e 's/\.global.*//' -e 's/\.section.*//' -e 's/\.long[ 	]\([0-9]*\)/\1,/' >>$3
  ;;
  *)
    exit 1
  ;;
esac
exit 0
