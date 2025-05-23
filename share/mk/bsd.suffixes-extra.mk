.if !target(__<bsd.suffixes-extra.mk>__)
__<bsd.suffixes-extra.mk>__:	.NOTMAIN

# prefer .s to a .c, remove stuff not used in the BSD libraries
# .pico used for PIC object files
# .nossppico used for NOSSP PIC object files
# .pieo used for PIE object files
.SUFFIXES: .out .o .bc .ll .pico .nossppico .pieo .S .asm .s .c .cc .cpp .cxx .C .f .y .l .ln

PICFLAG?=-fpic
PIEFLAG?=-fpie

.c.pico:
	${CC} ${PICFLAG} -DPIC \
	    ${SHARED_CFLAGS} ${CFLAGS} \
	    -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.c.nossppico:
	${CC} ${PICFLAG} -DPIC \
	    ${SHARED_CFLAGS:C/^-fstack-protector.*$//:C/^-fstack-clash-protection.*$//:C/^-fsanitize.*$//} \
	    ${CFLAGS:C/^-fstack-protector.*$//:C/^-fstack-clash-protection.*$//:C/^-fsanitize.*$//} \
	    -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.c.pieo:
	${CC} ${PIEFLAG} -DPIC \
	    ${SHARED_CFLAGS} ${CFLAGS} \
	    -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.cc.pico .C.pico .cpp.pico .cxx.pico:
	${CXX} ${PICFLAG} -DPIC \
	    ${SHARED_CXXFLAGS} ${CXXFLAGS} \
	    -c ${.IMPSRC} -o ${.TARGET}

.cc.nossppico .C.nossppico .cpp.nossppico .cxx.nossppico:
	${CXX} ${PICFLAG} -DPIC \
	    ${SHARED_CXXFLAGS:C/^-fstack-protector.*$//:C/^-fstack-clash-protection.*$//:C/^-fsanitize.*$//} \
	    ${CXXFLAGS:C/^-fstack-protector.*$//:C/^-fstack-clash-protection.*$//:C/^-fsanitize.*$//} \
	    -c ${.IMPSRC} -o ${.TARGET}

.cc.pieo .C.pieo .cpp.pieo .cxx.pieo:
	${CXX} ${PIEFLAG} ${SHARED_CXXFLAGS} ${CXXFLAGS} \
	    -c ${.IMPSRC} -o ${.TARGET}

.f.pico:
	${FC} ${PICFLAG} -DPIC ${FFLAGS} -o ${.TARGET} -c ${.IMPSRC}
	${CTFCONVERT_CMD}

.f.nossppico:
	${FC} ${PICFLAG} -DPIC \
	    ${FFLAGS:C/^-fstack-protector.*$//:C/^-fstack-clash-protection.*$//} \
	    -o ${.TARGET} -c ${.IMPSRC}
	${CTFCONVERT_CMD}

.s.pico .s.nossppico .s.pieo:
	${CC:N${CCACHE_BIN}} -x assembler ${ACFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.asm.pico:
	${CC:N${CCACHE_BIN}} -x assembler-with-cpp ${PICFLAG} -DPIC \
	    ${SHARED_CFLAGS} ${CFLAGS} ${ACFLAGS} \
	    -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.asm.nossppico:
	${CC:N${CCACHE_BIN}} -x assembler-with-cpp ${PICFLAG} -DPIC \
	    ${SHARED_CFLAGS:C/^-fstack-protector.*$//:C/^-fstack-clash-protection.*$//} \
	    ${CFLAGS:C/^-fstack-protector.*$//:C/^-fstack-clash-protection.*$//} \
	    ${ACFLAGS} \
	    -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.asm.pieo:
	${CC:N${CCACHE_BIN}} -x assembler-with-cpp ${PIEFLAG} -DPIC \
	    ${SHARED_CFLAGS} ${CFLAGS} ${ACFLAGS} \
	    -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.S.pico:
	${CC:N${CCACHE_BIN}} ${PICFLAG} -DPIC \
	    ${SHARED_CFLAGS} ${CFLAGS} ${ACFLAGS} \
	    -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.S.nossppico:
	${CC:N${CCACHE_BIN}} ${PICFLAG} -DPIC \
	    ${SHARED_CFLAGS:C/^-fstack-protector.*$//:C/^-fstack-clash-protection.*$//} \
	    ${CFLAGS:C/^-fstack-protector.*$//:C/^-fstack-clash-protection.*$//} \
	    ${ACFLAGS} \
	    -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.S.pieo:
	${CC:N${CCACHE_BIN}} ${PIEFLAG} -DPIC \
	    ${SHARED_CFLAGS} ${CFLAGS} ${ACFLAGS} \
	    -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.endif	# !target(__<bsd.suffixes-extra.mk>__)
