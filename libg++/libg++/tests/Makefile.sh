cat <<'EOF'
# test files
TSRCS =  tObstack.cc tString.cc tInteger.cc tRational.cc \
 tBitSet.cc tBitString.cc tRandom.cc tList.cc tPlex.cc \
 tLList.cc tVec.cc tStack.cc tQueue.cc tDeque.cc tPQ.cc tSet.cc  tBag.cc \
 tMap.cc tFix.cc tFix16.cc tFix24.cc \
 tGetOpt.cc \
 tiLList.cc
EOF

TESTS0="tObstack tString tInteger tRational tBitSet"\
" tBitString tFix tFix16 tFix24 tRandom"
TESTS1="tStack tQueue tDeque tPQ tSet tBag tMap tList tPlex tLList tVec"

cat <<EOF
# executables
TOUTS =  test_h ${TESTS0} ${TESTS1} tiLList tGetOpt

EOF

cat <<'EOF'
# files for archived prototype classes
LOBJS = \
 iList.o iSLList.o iDLList.o iVec.o iAVec.o \
 iPlex.o  iFPlex.o  iXPlex.o iRPlex.o iMPlex.o \
 iSet.o iBag.o iMap.o iPQ.o \
 iXPSet.o  iOXPSet.o  iSLSet.o  iOSLSet.o  iBSTSet.o iCHNode.o \
 iAVLSet.o  iSplayNode.o iSplaySet.o  iVHSet.o  iVOHSet.o  iCHSet.o \
 iXPBag.o  iOXPBag.o  iSLBag.o  iOSLBag.o  iSplayBag.o iVHBag.o  iCHBag.o \
 iVHMap.o  iCHMap.o  iSplayMap.o  iAVLMap.o iRAVLMap.o \
 iSplayPQ.o  iPHPQ.o  iXPPQ.o \
 iVStack.o iVQueue.o iStack.o iQueue.o iDeque.o \
 iXPStack.o iSLStack.o iXPQueue.o  iSLQueue.o iXPDeque.o iDLDeque.o

LSRCS = \
 iList.cc iSLList.cc iDLList.cc iVec.cc iAVec.cc \
 iPlex.cc  iFPlex.cc  iXPlex.cc iRPlex.cc iMPlex.cc \
 iSet.cc iBag.cc iMap.cc iPQ.cc \
 iXPSet.cc  iOXPSet.cc  iSLSet.cc  iOSLSet.cc  iBSTSet.cc iCHNode.cc \
 iAVLSet.cc  iSplayNode.cc iSplaySet.cc  iVHSet.cc  iVOHSet.cc  iCHSet.cc \
 iXPBag.cc iOXPBag.cc iSLBag.cc iOSLBag.cc  iSplayBag.cc iVHBag.cc iCHBag.cc \
 iVHMap.cc  iCHMap.cc  iSplayMap.cc  iAVLMap.cc iRAVLMap.cc \
 iSplayPQ.cc  iPHPQ.cc  iXPPQ.cc \
 iVStack.cc iVQueue.cc iStack.cc iQueue.cc iDeque.cc \
 iXPStack.cc iSLStack.cc iXPQueue.cc  iSLQueue.cc iXPDeque.cc iDLDeque.cc

DEPEND_SOURCES = $(srcdir)/*.cc $(LSRCS)

LHDRS =  idefs.h 

.PHONY: all
all:

.PHONY: info
info:
.PHONY: install-info
install-info:
.PHONY: clean-info
clean-info:

.PHONY: check
check: tests

.PHONY: check-tGetOpt
EOF

for TEST in ${TESTS0} ${TESTS1} tiLList ; do
  echo ".PHONY: check-${TEST}"
  if [ -f ${srcdir}/${TEST}.inp ] ; then
    echo "check-${TEST}: ${TEST}" '$(srcdir)'"/${TEST}.inp"
    echo "	./${TEST} < "'$(srcdir)'"/${TEST}.inp > ${TEST}.out 2>&1"
  else
    echo "check-${TEST}: ${TEST}"
    echo "	./${TEST} > ${TEST}.out 2>&1"
  fi
  echo '	diff -b $(srcdir)/'"${TEST}.exp ${TEST}.out"
done

cat <<'EOF'

check-tGetOpt: tGetOpt $(srcdir)/tGetOpt.inp
	./tGetOpt -abc -de10 -2000 -h3i \
	  <$(srcdir)/tGetOpt.inp >tGetOpt.out 2>&1
	diff -b $(srcdir)/tGetOpt.exp tGetOpt.out

$(TOUTS): $(LIBGXX)

LIBTEST=libtest.a

# We don't do check-tRandom, because it is not portable.

# Comment this out if your compiler doesn't handle templates:
CHECK_TEMPLATES=check-templates

tests checktests: clean_tests test_h \
  check-tObstack check-tString check-tInteger \
  check-tRational check-tBitSet check-tBitString \
  check-tFix check-tFix16 check-tFix24 check-tGetOpt \
  check-tList check-tPlex check-tLList check-tVec \
  check-tStack check-tQueue check-tDeque check-tPQ \
  check-tSet check-tBag check-tMap $(CHECK_TEMPLATES)
	./test_h

check-templates: check-tiLList

# Build all the tests, but don't run them. (Useful when cross-compiling.)

EOF

cat <<'EOF'
make-tests: $(TOUTS)

test_h: test_h.o
	$(CXX) $(LDFLAGS) test_h.o -o $@ $(LIBS) -lm

$(LIBTEST): $(LHDRS) $(LOBJS)
	rm -f $(LIBTEST)
	$(AR) r $(LIBTEST) $(LOBJS)
	$(RANLIB) $(LIBTEST)

#
# other tests
#
EOF

LIB_FOR_tRational=-lm
LIB_FOR_tInteger=-lm
LIB_FOR_tRandom=-lm
LIB_FOR_tFix=-lm
LIB_FOR_tFix16=-lm
LIB_FOR_tFix24=-lm

for TEST in $TESTS0 tiLList tGetOpt; do
  echo "${TEST}: ${TEST}.o"
  echo '	$(CXX) $(LDFLAGS)' "${TEST}.o" '-o $@ $(LIBS)' \
	`eval echo '$LIB_FOR_'$TEST`
  echo ""
done
for TEST in twrapper tgwrapper $TESTS1; do
  echo "${TEST}: " '$(LIBTEST)' " ${TEST}.o"
  echo '	$(CXX) $(LDFLAGS)' "${TEST}.o" '-o $@ $(LIBTEST) $(LIBS)'
  echo ""
done

cat <<'EOF'
idefs.h:
	PROTODIR=$(PROTODIR); export PROTODIR; $(GENCLASS) int val defs i
EOF

for TEST in Set XPSet OXPSet SLSet OSLSet BSTSet AVLSet SplayNode SplaySet VHSet VOHSet CHSet CHNode Bag XPBag OXPBag SLBag OSLBag SplayBag VHBag CHBag PQ PHPQ SplayPQ XPPQ Stack Queue Deque SLStack SLQueue DLDeque List Plex FPlex XPlex MPlex RPlex FPStack XPStack FPQueue XPQueue XPDeque SLList DLList Vec AVec; do
  echo "i$TEST.h i$TEST.cc:"
  echo '	PROTODIR=$(PROTODIR); export PROTODIR; $(GENCLASS) int val' $TEST i
done

for TEST in Map VHMap CHMap SplayMap AVLMap RAVLMap; do
  echo "i${TEST}.h i$TEST.cc:"
  echo '	PROTODIR=$(PROTODIR); export PROTODIR; $(GENCLASS) -2 int val int val' $TEST i
done

cat <<'EOF'
iVStack.h iVStack.cc: iStack.h
	PROTODIR=$(PROTODIR); export PROTODIR; $(GENCLASS) int val VStack i
iVQueue.h iVQueue.cc: iQueue.h
	PROTODIR=$(PROTODIR); export PROTODIR; $(GENCLASS) int val VQueue i

relink: force
	rm -f $(TOUTS)

.PHONY: clean_tests
clean_tests: force
	rm -f *.out

force:
EOF
