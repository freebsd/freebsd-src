!if "$(PROCESSOR_ARCHITECTURE)" == "x86"
MACHINE = ix86
!else if "$(PROCESSOR_ARCHITECTURE)" == "ALPHA"
MACHINE = alpha
!else  #Assume win9x
MACHINE = ix86
!endif
LIBS= libcmt.lib kernel32.lib

all: tcshfr.dll tcshc.dll tcshde.dll tcshsp.dll tcshja.dll tcsh-it.dll

stubdll.obj:stubdll.c
	cl -W3 -MT -c  stubdll.c

tcshgr.dll: tcshgr.res stubdll.obj
	link -machine:$(MACHINE) -nodefaultlib $(LIBS) tcshgr.res stubdll.obj \
	-out:tcshgr.dll

tcshfr.dll: tcshfr.res stubdll.obj
	link -machine:$(MACHINE) -dll -nodefaultlib $(LIBS) tcshfr.res stubdll.obj \
	-out:tcshfr.dll

tcshde.dll: tcshde.res stubdll.obj
	link -machine:$(MACHINE) -dll -nodefaultlib $(LIBS) tcshde.res stubdll.obj \
	-out:tcshde.dll

tcshsp.dll: tcshsp.res stubdll.obj
	link -machine:$(MACHINE) -dll -nodefaultlib $(LIBS) tcshsp.res stubdll.obj \
	-out:tcshsp.dll

tcsh-it.dll: tcsh-it.res stubdll.obj
	link -machine:$(MACHINE) -dll -nodefaultlib $(LIBS) tcsh-it.res stubdll.obj\
	-out:tcsh-it.dll

tcshja.dll: tcshja.res stubdll.obj
	link -machine:$(MACHINE) -dll -nodefaultlib $(LIBS) tcshja.res stubdll.obj \
	-out:tcshja.dll

tcshc.dll: tcshc.res stubdll.obj
	link -machine:$(MACHINE) -dll -nodefaultlib $(LIBS) tcshc.res stubdll.obj \
	-out:tcshc.dll

test.exe: test.obj
	link test.obj user32.lib -out:test.exe
clean:
	-del *.obj *.res

cleanall: clean
	-del *.dll *.exe
