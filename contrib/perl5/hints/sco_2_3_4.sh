yacc='/usr/bin/yacc -Sm25000'
ccflags="$ccflags -UM_I86"
usemymalloc='y'
echo "NOTE: you may have problems due to a spurious semicolon on the strerror()" >&4
echo "macro definition in /usr/include/string.h.  If so, delete the semicolon."  >&4
