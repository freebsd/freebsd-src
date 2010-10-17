#/bin/sh
# Generate test result data for xstormy16 GAS testing.
# It is intended to be run in the testsuite source directory.
#
# Syntax: build.sh /path/to/build/gas

if [ $# = 0 ] ; then
  if [ ! -x ../gas/as-new ] ; then
    echo "Usage: $0 [/path/to/gas/build]"
  else
    BUILD=`pwd`/../gas
  fi
else
  BUILD=$1
fi

if [ ! -x $BUILD/as-new ] ; then
  echo "$BUILD is not a gas build directory"
  exit 1
fi

# Put results here, so we preserve the existing set for comparison.
rm -rf tmpdir
mkdir tmpdir
cd tmpdir

function gentest {
    rm -f a.out
    $BUILD/as-new ${1}.s -o a.out
    echo "#as:" >${1}.d
    echo "#objdump: -dr" >>${1}.d
    echo "#name: $1" >>${1}.d
    $BUILD/../binutils/objdump -dr a.out | 	sed -e 's/(/\\(/g'             -e 's/)/\\)/g'             -e 's/\$/\\$/g'             -e 's/\[/\\\[/g'             -e 's/\]/\\\]/g'             -e 's/[+]/\\+/g'             -e 's/[.]/\\./g'             -e 's/[*]/\\*/g' | 	sed -e 's/^.*file format.*$/.*: +file format .*/' 	>>${1}.d
    rm -f a.out
}

# Now come all the testcases.
cat > gcc.s <<EOF
	mov.w r0,#-1
	mov.w r0,#0xFFFF
	add r0,#some_external_symbol
EOF

# Finally, generate the .d file.
gentest gcc
