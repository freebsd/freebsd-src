:
re=$1; shift

# some Linux systems have deprecated egrep in favor of grep -E
# but not everyone supports that
case "`echo bmake | egrep 'a|b' 2>&1`" in
bmake) ;;
*) egrep() { grep -E "$@"; }
esac

for lib in $*
do
  found=`nm $lib | egrep "$re"`
  case "$found" in
  "") ;;
  *)	echo "$lib: $found";;
  esac
done

    
