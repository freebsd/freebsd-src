case "$optimize" in
'') optimize="-O1" ;;
esac
d_setregid='undef'
d_setreuid='undef'
case "$usemymalloc" in
'') usemymalloc='y'
    ccflags="$ccflags -DNO_RCHECK"
    ;;
esac
