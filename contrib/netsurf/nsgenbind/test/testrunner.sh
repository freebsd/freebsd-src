#!/bin/sh

outline() {
echo >>${LOGFILE}
echo "-----------------------------------------------------------"  >>${LOGFILE}
echo >>${LOGFILE}
}

BUILDDIR=$1
TESTDIR=$2

# locations
LOGFILE=${BUILDDIR}/testlog

GENJSBIND=${BUILDDIR}/nsgenbind

BINDINGDIR=${TESTDIR}/data/bindings
BINDINGTESTS=$(ls ${BINDINGDIR}/*.bnd)

IDLDIR=${TESTDIR}/data/idl

echo "$*" >${LOGFILE}

for TEST in ${BINDINGTESTS};do

  TESTNAME=$(basename ${TEST} .bnd)

  echo -n "    TEST: ${TESTNAME}......"
  outline

  echo  ${GENJSBIND} -D -v -I ${IDLDIR} -o ${BUILDDIR}/test_${TESTNAME}.c -h ${BUILDDIR}/test_${TESTNAME}.h ${TEST} >>${LOGFILE} 2>&1   

  ${GENJSBIND} -D -v -I ${IDLDIR} -o ${BUILDDIR}/test_${TESTNAME}.c -h ${BUILDDIR}/test_${TESTNAME}.h ${TEST} >>${LOGFILE} 2>&1

  if [ $? -eq 0 ]; then
    echo "PASS"
  else
    echo "FAIL"
  fi
  

done

