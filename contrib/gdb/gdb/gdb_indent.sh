#!/bin/sh

# Try to find a GNU indent.  There could be a BSD indent in front of a
# GNU gindent so when indent is found, keep looking.

gindent=
indent=
paths=`echo $PATH | sed \
	-e 's/::/:.:/g' \
	-e 's/^:/.:/' \
	-e 's/:$/:./' \
	-e 's/:/ /g'`
for path in $paths
do
    if test ! -n "${gindent}" -a -x ${path}/gindent
    then
	gindent=${path}/gindent
	break
    elif test ! -n "${indent}" -a -x ${path}/indent
    then
	indent=${path}/indent
    fi
done

if test -n "${gindent}"
then
    indent=${gindent}
elif test -n "${indent}"
then
    :
else
    echo "Indent not found" 1>&2
fi


# Check that the indent found is both GNU and a reasonable version.
# Different indent versions give different indentation.

case `${indent} --version 2>/dev/null < /dev/null` in
    GNU*2.2.6 ) ;;
    *GNU* ) echo "Incorrect version of GNU indent" 1>&2 ;;
    * ) echo "Indent is not GNU" 1>&2 ;;
esac


# Check that we're in the GDB source directory

case `pwd` in
    */gdb ) ;;
    * ) echo "Not in GDB directory" 1>&2 ; exit 1 ;;
esac


# Run indent per GDB specs

types="-T FILE `cat *.h | sed -n \
    -e 's/^.*[^a-z0-9_]\([a-z0-9_]*_ftype\).*$/-T \1/p' \
    -e 's/^.*[^a-z0-9_]\([a-z0-9_]*_func\).*$/-T \1/p' \
    -e 's/^typedef.*[^a-zA-Z0-9_]\([a-zA-Z0-9_]*[a-zA-Z0-9_]\);$/-T \1/p' \
    | sort -u`"

${indent} ${types} "$@"
