
.if ${.MAKE.DEPENDFILE:M*.${MACHINE}} == ""
# by default only MACHINE0 does updates
UPDATE_DEPENDFILE_MACHINE?= ${MACHINE0:U${MACHINE}}
.if ${MACHINE} != ${UPDATE_DEPENDFILE_MACHINE}
UPDATE_DEPENDFILE= no
.endif
.endif

NOSSPPICO?= .nossppico
PIEO?= .pieo
OBJ_EXTENSIONS+= ${NOSSPPICO} ${PIEO}

CLEANFILES+= .depend

# handy for debugging
.SUFFIXES:  .S .c .cc .cpp .cpp-out


.S.cpp-out .c.cpp-out: .NOMETA
	@${CC} -E ${CFLAGS} ${.IMPSRC} | grep -v '^[[:space:]]*$$'

.cc.cpp-out: .NOMETA
	@${CXX} -E ${CXXFLAGS} ${.IMPSRC} | grep -v '^[[:space:]]*$$'

.-include <site.autodep.mk>

.ifdef _RECURSING_CRUNCH
# crunchgen does not want to see our stats
_reldir_finish: .NOTMAIN
.endif
