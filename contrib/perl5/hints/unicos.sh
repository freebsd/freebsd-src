case `uname -r` in
6.1*) shellflags="-m+65536" ;;
esac
case "$optimize" in
'') optimize="-O1" ;;
esac
d_setregid='undef'
d_setreuid='undef'
case "$usemymalloc" in
'') # The perl malloc.c SHOULD work says Ilya.
    # But for the time being (5.004_68), alas, it doesn't.
    # usemymalloc='y'
    # ccflags="$ccflags -DNO_RCHECK"
    usemymalloc='n'
    ;;
esac
