CFLAGS = /decc $(CC_OPTIONS)/Diagnostics /Define=(NDEBUG) /Object=$@ /Include=([])

LINKFLAGS	= /map=$(MMS$TARGET_NAME)/cross_reference/exec=$(MMS$TARGET_NAME).exe

LINKER	      = cc

OBJS	      = closure.obj, \
		error.obj, \
		lalr.obj, \
		lr0.obj, \
		main.obj, \
		mkpar.obj, \
		output.obj, \
		reader.obj, \
		skeleton.obj, \
		symtab.obj, \
		verbose.obj, \
		warshall.obj

PROGRAM	      = yacc.exe

all :		$(PROGRAM)

$(PROGRAM) :     $(OBJS)
	@ write sys$output "Loading $(PROGRAM) ... "
	@ $(LINK) $(LINKFLAGS) $(OBJS)
	@ write sys$output "done"

clean :
	@- if f$search("*.obj") .nes. "" then delete *.obj;*
	@- if f$search("*.lis") .nes. "" then delete *.lis;*
	@- if f$search("*.log") .nes. "" then delete *.log;*

clobber :	clean
	@- if f$search("*.exe") .nes. "" then delete *.exe;*

$(OBJS) : defs.h
