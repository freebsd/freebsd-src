.if !target(__<bsd.suffixes-extra.mk>__)
__<bsd.suffixes-extra.mk>__:	.NOTMAIN

# prefer .s to a .c, add .po, remove stuff not used in the BSD libraries
# .pico used for PIC object files
# .nossppico used for NOSSP PIC object files
# .pieo used for PIE object files
.SUFFIXES: .out .o .bc .ll .po .pico .nossppico .pieo .S .asm .s .c .cc .cpp .cxx .C .f .y .l .ln

.if !defined(PICFLAG)
PICFLAG=-fpic
PIEFLAG=-fpie
.endif

PO_FLAG=-pg

.c.po:
	${CC} ${PO_FLAG} ${STATIC_CFLAGS} ${PO_CFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.c.pico:
	${CC} ${PICFLAG} -DPIC ${SHARED_CFLAGS} ${CFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.c.nossppico:
	${CC} ${PICFLAG} -DPIC ${SHARED_CFLAGS:C/^-fstack-protector.*$//:C/^-fsanitize.*$//} ${CFLAGS:C/^-fstack-protector.*$//:C/^-fsanitize.*$//} -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.c.pieo:
	${CC} ${PIEFLAG} -DPIC ${SHARED_CFLAGS} ${CFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.cc.po .C.po .cpp.po .cxx.po:
	${CXX} ${PO_FLAG} ${STATIC_CXXFLAGS} ${PO_CXXFLAGS} -c ${.IMPSRC} -o ${.TARGET}

.cc.pico .C.pico .cpp.pico .cxx.pico:
	${CXX} ${PICFLAG} -DPIC ${SHARED_CXXFLAGS} ${CXXFLAGS} -c ${.IMPSRC} -o ${.TARGET}

.cc.nossppico .C.nossppico .cpp.nossppico .cxx.nossppico:
	${CXX} ${PICFLAG} -DPIC ${SHARED_CXXFLAGS:C/^-fstack-protector.*$//:C/^-fsanitize.*$//} ${CXXFLAGS:C/^-fstack-protector.*$//:C/^-fsanitize.*$//} -c ${.IMPSRC} -o ${.TARGET}

.cc.pieo .C.pieo .cpp.pieo .cxx.pieo:
	${CXX} ${PIEFLAG} ${SHARED_CXXFLAGS} ${CXXFLAGS} -c ${.IMPSRC} -o ${.TARGET}

.f.po:
	${FC} -pg ${FFLAGS} -o ${.TARGET} -c ${.IMPSRC}
	${CTFCONVERT_CMD}

.f.pico:
	${FC} ${PICFLAG} -DPIC ${FFLAGS} -o ${.TARGET} -c ${.IMPSRC}
	${CTFCONVERT_CMD}

.f.nossppico:
	${FC} ${PICFLAG} -DPIC ${FFLAGS:C/^-fstack-protector.*$//} -o ${.TARGET} -c ${.IMPSRC}
	${CTFCONVERT_CMD}

.s.po .s.pico .s.nossppico .s.pieo:
	${CC:N${CCACHE_BIN}} -x assembler ${ACFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.asm.po:
	${CC:N${CCACHE_BIN}} -x assembler-with-cpp -DPROF ${PO_CFLAGS} \
	    ${ACFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.asm.pico:
	${CC:N${CCACHE_BIN}} -x assembler-with-cpp ${PICFLAG} -DPIC \
	    ${CFLAGS} ${ACFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.asm.nossppico:
	${CC:N${CCACHE_BIN}} -x assembler-with-cpp ${PICFLAG} -DPIC \
	    ${CFLAGS:C/^-fstack-protector.*$//} ${ACFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.asm.pieo:
	${CC:N${CCACHE_BIN}} -x assembler-with-cpp ${PIEFLAG} -DPIC \
	    ${CFLAGS} ${ACFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.S.po:
	${CC:N${CCACHE_BIN}} -DPROF ${PO_CFLAGS} ${ACFLAGS} -c ${.IMPSRC} \
	    -o ${.TARGET}
	${CTFCONVERT_CMD}

.S.pico:
	${CC:N${CCACHE_BIN}} ${PICFLAG} -DPIC ${CFLAGS} ${ACFLAGS} \
	    -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.S.nossppico:
	${CC:N${CCACHE_BIN}} ${PICFLAG} -DPIC ${CFLAGS:C/^-fstack-protector.*$//} ${ACFLAGS} \
	    -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.S.pieo:
	${CC:N${CCACHE_BIN}} ${PIEFLAG} -DPIC ${CFLAGS} ${ACFLAGS} \
	    -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.endif	# !target(__<bsd.suffixes-extra.mk>__)
