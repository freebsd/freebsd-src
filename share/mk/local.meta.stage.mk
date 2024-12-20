.if defined(NO_OBJ) && ${__objdir:U${.CURDIR}} != ${.CURDIR}
# NO_OBJ would cause us to dribble stage* into .CURDIR
# which will prevent the targets being done for multiple MACHINES
.undef NO_OBJ
.OBJDIR: ${__objdir}
.endif
