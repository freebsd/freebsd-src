# $FreeBSD$

.PATH:		${.CURDIR}/../../contrib/dev/acpica/Subsystem/Common \
		${.CURDIR}/../../contrib/dev/acpica/Subsystem/Debugger \
		${.CURDIR}/../../contrib/dev/acpica/Subsystem/Dispatcher \
		${.CURDIR}/../../contrib/dev/acpica/Subsystem/Events \
		${.CURDIR}/../../contrib/dev/acpica/Subsystem/Hardware \
		${.CURDIR}/../../contrib/dev/acpica/Subsystem/Interpreter \
		${.CURDIR}/../../contrib/dev/acpica/Subsystem/Namespace \
		${.CURDIR}/../../contrib/dev/acpica/Subsystem/Parser \
		${.CURDIR}/../../contrib/dev/acpica/Subsystem/Resources \
		${.CURDIR}/../../contrib/dev/acpica/Subsystem/Tables \
		${.CURDIR}/../../pci ${.CURDIR}/../../dev/acpica \
		${.CURDIR}/../../dev/acpica/Osd

KMOD		=  acpica
SRCS		=  cmalloc.c cmclib.c cmcopy.c cmdebug.c cmdelete.c cmeval.c
SRCS		+= cmglobal.c cminit.c cmobject.c cmutils.c cmxface.c dbcmds.c
SRCS		+= dbdisasm.c dbdisply.c dbexec.c dbfileio.c dbhistry.c 
SRCS		+= dbinput.c dbstats.c dbutils.c dbxface.c dsfield.c dsmethod.c
SRCS		+= dsmthdat.c dsobject.c dsopcode.c dsutils.c dswexec.c
SRCS 		+= dswload.c dswscope.c dswstate.c evevent.c evmisc.c evregion.c
SRCS		+= evrgnini.c evsci.c evxface.c evxfevnt.c evxfregn.c hwacpi.c
SRCS		+= hwcpu32.c hwgpe.c hwxface.c hwregs.c amconfig.c amcreate.c amdump.c
SRCS		+= amfield.c amfldio.c ammisc.c ammonad.c amnames.c amprep.c
SRCS		+= amdyadic.c amregion.c amresnte.c amresolv.c amresop.c amstore.c 
SRCS		+= amstoren.c amstorob.c amsystem.c amutils.c amxface.c
SRCS 		+= nsaccess.c nsalloc.c nsdump.c nseval.c nsinit.c nsload.c
SRCS		+= nsnames.c nsobject.c nssearch.c nsutils.c nswalk.c
SRCS		+= nsxfobj.c nsxfname.c  psargs.c psfind.c psopcode.c psparse.c psscope.c
SRCS		+= pstree.c psutils.c pswalk.c psxface.c rsaddr.c rscalc.c
SRCS		+= rscreate.c rsdump.c rsio.c rsirq.c rslist.c rsmemory.c 
SRCS		+= rsmisc.c rsutils.c rsxface.c tbget.c tbinstal.c tbutils.c 
SRCS		+= tbxface.c tbxfroot.c acpi.c acpi_apic.c acpi_button.c
SRCS		+= acpi_ec.c acpi_isa.c acpi_lid.c acpi_pcib.c acpi_processor.c
SRCS		+= acpi_resource.c acpi_thermal.c acpi_timer.c OsdDebug.c 
SRCS		+= OsdHardware.c OsdInterrupt.c OsdMemory.c OsdSchedule.c
SRCS		+= OsdStream.c OsdSynch.c OsdEnvironment.c 
SRCS		+= opt_acpi.h opt_ddb.h
SRCS		+= device_if.h bus_if.h pci_if.h pcib_if.h isa_if.h

CFLAGS +=   -I${.CURDIR}/../../contrib/dev/acpica/Subsystem/Include

#CFLAGS+=	
opt_acpi.h:Makefile
	touch opt_acpi.h
opt_ddb.h:Makefile
	echo '#define DDB 1'  >opt_ddb.h

.include <bsd.kmod.mk>

