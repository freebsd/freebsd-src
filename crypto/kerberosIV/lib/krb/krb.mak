# Microsoft Developer Studio Generated NMAKE File, Format Version 4.10
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

!IF "$(CFG)" == ""
CFG=krb - Win32 Debug
!MESSAGE No configuration specified.  Defaulting to krb - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "krb - Win32 Release" && "$(CFG)" != "krb - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "krb.mak" CFG="krb - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "krb - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "krb - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 
################################################################################
# Begin Project
# PROP Target_Last_Scanned "krb - Win32 Debug"
RSC=rc.exe
MTL=mktyplib.exe
CPP=cl.exe

!IF  "$(CFG)" == "krb - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
OUTDIR=.\Release
INTDIR=.\Release

ALL : ".\Release\krb.dll"

CLEAN : 
	-@erase ".\Release\cr_err_reply.obj"
	-@erase ".\Release\create_auth_reply.obj"
	-@erase ".\Release\create_ciph.obj"
	-@erase ".\Release\create_ticket.obj"
	-@erase ".\Release\debug_decl.obj"
	-@erase ".\Release\decomp_ticket.obj"
	-@erase ".\Release\dllmain.obj"
	-@erase ".\Release\encrypt_ktext.obj"
	-@erase ".\Release\et_list.obj"
	-@erase ".\Release\get_ad_tkt.obj"
	-@erase ".\Release\get_cred.obj"
	-@erase ".\Release\get_default_principal.obj"
	-@erase ".\Release\get_host.obj"
	-@erase ".\Release\get_in_tkt.obj"
	-@erase ".\Release\get_krbrlm.obj"
	-@erase ".\Release\get_phost.obj"
	-@erase ".\Release\get_svc_in_tkt.obj"
	-@erase ".\Release\get_tf_fullname.obj"
	-@erase ".\Release\get_tf_realm.obj"
	-@erase ".\Release\getaddrs.obj"
	-@erase ".\Release\getrealm.obj"
	-@erase ".\Release\getst.obj"
	-@erase ".\Release\k_flock.obj"
	-@erase ".\Release\k_gethostname.obj"
	-@erase ".\Release\k_getport.obj"
	-@erase ".\Release\k_getsockinst.obj"
	-@erase ".\Release\k_localtime.obj"
	-@erase ".\Release\kdc_reply.obj"
	-@erase ".\Release\kntoln.obj"
	-@erase ".\Release\krb.dll"
	-@erase ".\Release\krb.exp"
	-@erase ".\Release\krb.lib"
	-@erase ".\Release\krb_check_auth.obj"
	-@erase ".\Release\krb_equiv.obj"
	-@erase ".\Release\krb_err_txt.obj"
	-@erase ".\Release\krb_get_in_tkt.obj"
	-@erase ".\Release\lifetime.obj"
	-@erase ".\Release\logging.obj"
	-@erase ".\Release\lsb_addr_comp.obj"
	-@erase ".\Release\mk_auth.obj"
	-@erase ".\Release\mk_err.obj"
	-@erase ".\Release\mk_priv.obj"
	-@erase ".\Release\mk_req.obj"
	-@erase ".\Release\mk_safe.obj"
	-@erase ".\Release\month_sname.obj"
	-@erase ".\Release\name2name.obj"
	-@erase ".\Release\netread.obj"
	-@erase ".\Release\netwrite.obj"
	-@erase ".\Release\one.obj"
	-@erase ".\Release\parse_name.obj"
	-@erase ".\Release\rd_err.obj"
	-@erase ".\Release\rd_priv.obj"
	-@erase ".\Release\rd_req.obj"
	-@erase ".\Release\rd_safe.obj"
	-@erase ".\Release\read_service_key.obj"
	-@erase ".\Release\realm_parse.obj"
	-@erase ".\Release\recvauth.obj"
	-@erase ".\Release\resolve.obj"
	-@erase ".\Release\rw.obj"
	-@erase ".\Release\save_credentials.obj"
	-@erase ".\Release\send_to_kdc.obj"
	-@erase ".\Release\sendauth.obj"
	-@erase ".\Release\stime.obj"
	-@erase ".\Release\str2key.obj"
	-@erase ".\Release\swab.obj"
	-@erase ".\Release\ticket_memory.obj"
	-@erase ".\Release\tkt_string.obj"
	-@erase ".\Release\unparse_name.obj"
	-@erase ".\Release\util.obj"
	-@erase ".\Release\verify_user.obj"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "." /I "..\..\include" /I "..\..\include\win32" /I "..\des" /I "..\roken" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "HAVE_CONFIG_H" /YX /c
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "." /I "..\..\include" /I\
 "..\..\include\win32" /I "..\des" /I "..\roken" /D "NDEBUG" /D "WIN32" /D\
 "_WINDOWS" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)/krb.pch" /YX /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\Release/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
MTL_PROJ=/nologo /D "NDEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/krb.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 ..\roken\Release\roken.lib ..\des\Release\des.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
LINK32_FLAGS=..\roken\Release\roken.lib ..\des\Release\des.lib wsock32.lib\
 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib\
 shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo\
 /subsystem:windows /dll /incremental:no /pdb:"$(OUTDIR)/krb.pdb" /machine:I386\
 /def:".\krb.def" /out:"$(OUTDIR)/krb.dll" /implib:"$(OUTDIR)/krb.lib" 
DEF_FILE= \
	".\krb.def"
LINK32_OBJS= \
	".\Release\cr_err_reply.obj" \
	".\Release\create_auth_reply.obj" \
	".\Release\create_ciph.obj" \
	".\Release\create_ticket.obj" \
	".\Release\debug_decl.obj" \
	".\Release\decomp_ticket.obj" \
	".\Release\dllmain.obj" \
	".\Release\encrypt_ktext.obj" \
	".\Release\et_list.obj" \
	".\Release\get_ad_tkt.obj" \
	".\Release\get_cred.obj" \
	".\Release\get_default_principal.obj" \
	".\Release\get_host.obj" \
	".\Release\get_in_tkt.obj" \
	".\Release\get_krbrlm.obj" \
	".\Release\get_phost.obj" \
	".\Release\get_svc_in_tkt.obj" \
	".\Release\get_tf_fullname.obj" \
	".\Release\get_tf_realm.obj" \
	".\Release\getaddrs.obj" \
	".\Release\getrealm.obj" \
	".\Release\getst.obj" \
	".\Release\k_flock.obj" \
	".\Release\k_gethostname.obj" \
	".\Release\k_getport.obj" \
	".\Release\k_getsockinst.obj" \
	".\Release\k_localtime.obj" \
	".\Release\kdc_reply.obj" \
	".\Release\kntoln.obj" \
	".\Release\krb_check_auth.obj" \
	".\Release\krb_equiv.obj" \
	".\Release\krb_err_txt.obj" \
	".\Release\krb_get_in_tkt.obj" \
	".\Release\lifetime.obj" \
	".\Release\logging.obj" \
	".\Release\lsb_addr_comp.obj" \
	".\Release\mk_auth.obj" \
	".\Release\mk_err.obj" \
	".\Release\mk_priv.obj" \
	".\Release\mk_req.obj" \
	".\Release\mk_safe.obj" \
	".\Release\month_sname.obj" \
	".\Release\name2name.obj" \
	".\Release\netread.obj" \
	".\Release\netwrite.obj" \
	".\Release\one.obj" \
	".\Release\parse_name.obj" \
	".\Release\rd_err.obj" \
	".\Release\rd_priv.obj" \
	".\Release\rd_req.obj" \
	".\Release\rd_safe.obj" \
	".\Release\read_service_key.obj" \
	".\Release\realm_parse.obj" \
	".\Release\recvauth.obj" \
	".\Release\resolve.obj" \
	".\Release\rw.obj" \
	".\Release\save_credentials.obj" \
	".\Release\send_to_kdc.obj" \
	".\Release\sendauth.obj" \
	".\Release\stime.obj" \
	".\Release\str2key.obj" \
	".\Release\swab.obj" \
	".\Release\ticket_memory.obj" \
	".\Release\tkt_string.obj" \
	".\Release\unparse_name.obj" \
	".\Release\util.obj" \
	".\Release\verify_user.obj"

".\Release\krb.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
OUTDIR=.\Debug
INTDIR=.\Debug

ALL : ".\Debug\krb.dll"

CLEAN : 
	-@erase ".\Debug\cr_err_reply.obj"
	-@erase ".\Debug\create_auth_reply.obj"
	-@erase ".\Debug\create_ciph.obj"
	-@erase ".\Debug\create_ticket.obj"
	-@erase ".\Debug\debug_decl.obj"
	-@erase ".\Debug\decomp_ticket.obj"
	-@erase ".\Debug\dllmain.obj"
	-@erase ".\Debug\encrypt_ktext.obj"
	-@erase ".\Debug\et_list.obj"
	-@erase ".\Debug\get_ad_tkt.obj"
	-@erase ".\Debug\get_cred.obj"
	-@erase ".\Debug\get_default_principal.obj"
	-@erase ".\Debug\get_host.obj"
	-@erase ".\Debug\get_in_tkt.obj"
	-@erase ".\Debug\get_krbrlm.obj"
	-@erase ".\Debug\get_phost.obj"
	-@erase ".\Debug\get_svc_in_tkt.obj"
	-@erase ".\Debug\get_tf_fullname.obj"
	-@erase ".\Debug\get_tf_realm.obj"
	-@erase ".\Debug\getaddrs.obj"
	-@erase ".\Debug\getrealm.obj"
	-@erase ".\Debug\getst.obj"
	-@erase ".\Debug\k_flock.obj"
	-@erase ".\Debug\k_gethostname.obj"
	-@erase ".\Debug\k_getport.obj"
	-@erase ".\Debug\k_getsockinst.obj"
	-@erase ".\Debug\k_localtime.obj"
	-@erase ".\Debug\kdc_reply.obj"
	-@erase ".\Debug\kntoln.obj"
	-@erase ".\Debug\krb.dll"
	-@erase ".\Debug\krb.exp"
	-@erase ".\Debug\krb.ilk"
	-@erase ".\Debug\krb.lib"
	-@erase ".\Debug\krb.pdb"
	-@erase ".\Debug\krb_check_auth.obj"
	-@erase ".\Debug\krb_equiv.obj"
	-@erase ".\Debug\krb_err_txt.obj"
	-@erase ".\Debug\krb_get_in_tkt.obj"
	-@erase ".\Debug\lifetime.obj"
	-@erase ".\Debug\logging.obj"
	-@erase ".\Debug\lsb_addr_comp.obj"
	-@erase ".\Debug\mk_auth.obj"
	-@erase ".\Debug\mk_err.obj"
	-@erase ".\Debug\mk_priv.obj"
	-@erase ".\Debug\mk_req.obj"
	-@erase ".\Debug\mk_safe.obj"
	-@erase ".\Debug\month_sname.obj"
	-@erase ".\Debug\name2name.obj"
	-@erase ".\Debug\netread.obj"
	-@erase ".\Debug\netwrite.obj"
	-@erase ".\Debug\one.obj"
	-@erase ".\Debug\parse_name.obj"
	-@erase ".\Debug\rd_err.obj"
	-@erase ".\Debug\rd_priv.obj"
	-@erase ".\Debug\rd_req.obj"
	-@erase ".\Debug\rd_safe.obj"
	-@erase ".\Debug\read_service_key.obj"
	-@erase ".\Debug\realm_parse.obj"
	-@erase ".\Debug\recvauth.obj"
	-@erase ".\Debug\resolve.obj"
	-@erase ".\Debug\rw.obj"
	-@erase ".\Debug\save_credentials.obj"
	-@erase ".\Debug\send_to_kdc.obj"
	-@erase ".\Debug\sendauth.obj"
	-@erase ".\Debug\stime.obj"
	-@erase ".\Debug\str2key.obj"
	-@erase ".\Debug\swab.obj"
	-@erase ".\Debug\ticket_memory.obj"
	-@erase ".\Debug\tkt_string.obj"
	-@erase ".\Debug\unparse_name.obj"
	-@erase ".\Debug\util.obj"
	-@erase ".\Debug\vc40.idb"
	-@erase ".\Debug\vc40.pdb"
	-@erase ".\Debug\verify_user.obj"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "." /I "..\..\include" /I "..\..\include\win32" /I "..\des" /I "..\roken" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "HAVE_CONFIG_H" /YX /c
CPP_PROJ=/nologo /MTd /W3 /Gm /GX /Zi /Od /I "." /I "..\..\include" /I\
 "..\..\include\win32" /I "..\des" /I "..\roken" /D "_DEBUG" /D "WIN32" /D\
 "_WINDOWS" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)/krb.pch" /YX /Fo"$(INTDIR)/"\
 /Fd"$(INTDIR)/" /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
MTL_PROJ=/nologo /D "_DEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/krb.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386
# ADD LINK32 ..\roken\Debug\roken.lib ..\des\Debug\des.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386
LINK32_FLAGS=..\roken\Debug\roken.lib ..\des\Debug\des.lib wsock32.lib\
 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib\
 shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo\
 /subsystem:windows /dll /incremental:yes /pdb:"$(OUTDIR)/krb.pdb" /debug\
 /machine:I386 /def:".\krb.def" /out:"$(OUTDIR)/krb.dll"\
 /implib:"$(OUTDIR)/krb.lib" 
DEF_FILE= \
	".\krb.def"
LINK32_OBJS= \
	".\Debug\cr_err_reply.obj" \
	".\Debug\create_auth_reply.obj" \
	".\Debug\create_ciph.obj" \
	".\Debug\create_ticket.obj" \
	".\Debug\debug_decl.obj" \
	".\Debug\decomp_ticket.obj" \
	".\Debug\dllmain.obj" \
	".\Debug\encrypt_ktext.obj" \
	".\Debug\et_list.obj" \
	".\Debug\get_ad_tkt.obj" \
	".\Debug\get_cred.obj" \
	".\Debug\get_default_principal.obj" \
	".\Debug\get_host.obj" \
	".\Debug\get_in_tkt.obj" \
	".\Debug\get_krbrlm.obj" \
	".\Debug\get_phost.obj" \
	".\Debug\get_svc_in_tkt.obj" \
	".\Debug\get_tf_fullname.obj" \
	".\Debug\get_tf_realm.obj" \
	".\Debug\getaddrs.obj" \
	".\Debug\getrealm.obj" \
	".\Debug\getst.obj" \
	".\Debug\k_flock.obj" \
	".\Debug\k_gethostname.obj" \
	".\Debug\k_getport.obj" \
	".\Debug\k_getsockinst.obj" \
	".\Debug\k_localtime.obj" \
	".\Debug\kdc_reply.obj" \
	".\Debug\kntoln.obj" \
	".\Debug\krb_check_auth.obj" \
	".\Debug\krb_equiv.obj" \
	".\Debug\krb_err_txt.obj" \
	".\Debug\krb_get_in_tkt.obj" \
	".\Debug\lifetime.obj" \
	".\Debug\logging.obj" \
	".\Debug\lsb_addr_comp.obj" \
	".\Debug\mk_auth.obj" \
	".\Debug\mk_err.obj" \
	".\Debug\mk_priv.obj" \
	".\Debug\mk_req.obj" \
	".\Debug\mk_safe.obj" \
	".\Debug\month_sname.obj" \
	".\Debug\name2name.obj" \
	".\Debug\netread.obj" \
	".\Debug\netwrite.obj" \
	".\Debug\one.obj" \
	".\Debug\parse_name.obj" \
	".\Debug\rd_err.obj" \
	".\Debug\rd_priv.obj" \
	".\Debug\rd_req.obj" \
	".\Debug\rd_safe.obj" \
	".\Debug\read_service_key.obj" \
	".\Debug\realm_parse.obj" \
	".\Debug\recvauth.obj" \
	".\Debug\resolve.obj" \
	".\Debug\rw.obj" \
	".\Debug\save_credentials.obj" \
	".\Debug\send_to_kdc.obj" \
	".\Debug\sendauth.obj" \
	".\Debug\stime.obj" \
	".\Debug\str2key.obj" \
	".\Debug\swab.obj" \
	".\Debug\ticket_memory.obj" \
	".\Debug\tkt_string.obj" \
	".\Debug\unparse_name.obj" \
	".\Debug\util.obj" \
	".\Debug\verify_user.obj"

".\Debug\krb.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.c{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

################################################################################
# Begin Target

# Name "krb - Win32 Release"
# Name "krb - Win32 Debug"

!IF  "$(CFG)" == "krb - Win32 Release"

!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=.\krb.def

!IF  "$(CFG)" == "krb - Win32 Release"

!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\get_tf_fullname.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_GET_T=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\get_tf_fullname.obj" : $(SOURCE) $(DEP_CPP_GET_T) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_GET_T=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\get_tf_fullname.obj" : $(SOURCE) $(DEP_CPP_GET_T) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\cr_err_reply.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_CR_ER=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\cr_err_reply.obj" : $(SOURCE) $(DEP_CPP_CR_ER) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_CR_ER=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\cr_err_reply.obj" : $(SOURCE) $(DEP_CPP_CR_ER) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\create_auth_reply.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_CREAT=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\create_auth_reply.obj" : $(SOURCE) $(DEP_CPP_CREAT) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_CREAT=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\create_auth_reply.obj" : $(SOURCE) $(DEP_CPP_CREAT) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\create_ciph.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_CREATE=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\create_ciph.obj" : $(SOURCE) $(DEP_CPP_CREATE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_CREATE=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\create_ciph.obj" : $(SOURCE) $(DEP_CPP_CREATE) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\create_ticket.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_CREATE_=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\create_ticket.obj" : $(SOURCE) $(DEP_CPP_CREATE_) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_CREATE_=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\create_ticket.obj" : $(SOURCE) $(DEP_CPP_CREATE_) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\decomp_ticket.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_DECOM=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\decomp_ticket.obj" : $(SOURCE) $(DEP_CPP_DECOM) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_DECOM=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\decomp_ticket.obj" : $(SOURCE) $(DEP_CPP_DECOM) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\dllmain.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_DLLMA=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	".\ticket_memory.h"\
	

".\Release\dllmain.obj" : $(SOURCE) $(DEP_CPP_DLLMA) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_DLLMA=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	".\ticket_memory.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\dllmain.obj" : $(SOURCE) $(DEP_CPP_DLLMA) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\encrypt_ktext.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_ENCRY=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\encrypt_ktext.obj" : $(SOURCE) $(DEP_CPP_ENCRY) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_ENCRY=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\encrypt_ktext.obj" : $(SOURCE) $(DEP_CPP_ENCRY) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\et_list.c
DEP_CPP_ET_LI=\
	"..\..\include\win32\config.h"\
	

!IF  "$(CFG)" == "krb - Win32 Release"


".\Release\et_list.obj" : $(SOURCE) $(DEP_CPP_ET_LI) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"


".\Debug\et_list.obj" : $(SOURCE) $(DEP_CPP_ET_LI) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\get_ad_tkt.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_GET_A=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\get_ad_tkt.obj" : $(SOURCE) $(DEP_CPP_GET_A) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_GET_A=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\get_ad_tkt.obj" : $(SOURCE) $(DEP_CPP_GET_A) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\get_cred.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_GET_C=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\get_cred.obj" : $(SOURCE) $(DEP_CPP_GET_C) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_GET_C=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\get_cred.obj" : $(SOURCE) $(DEP_CPP_GET_C) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\get_default_principal.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_GET_D=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\get_default_principal.obj" : $(SOURCE) $(DEP_CPP_GET_D) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_GET_D=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\get_default_principal.obj" : $(SOURCE) $(DEP_CPP_GET_D) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\get_host.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_GET_H=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\get_host.obj" : $(SOURCE) $(DEP_CPP_GET_H) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_GET_H=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\get_host.obj" : $(SOURCE) $(DEP_CPP_GET_H) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\get_in_tkt.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_GET_I=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\get_in_tkt.obj" : $(SOURCE) $(DEP_CPP_GET_I) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_GET_I=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\get_in_tkt.obj" : $(SOURCE) $(DEP_CPP_GET_I) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\get_krbrlm.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_GET_K=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\get_krbrlm.obj" : $(SOURCE) $(DEP_CPP_GET_K) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_GET_K=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\get_krbrlm.obj" : $(SOURCE) $(DEP_CPP_GET_K) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\get_phos

!IF  "$(CFG)" == "krb - Win32 Release"

!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\get_svc_in_tkt.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_GET_S=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\get_svc_in_tkt.obj" : $(SOURCE) $(DEP_CPP_GET_S) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_GET_S=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\get_svc_in_tkt.obj" : $(SOURCE) $(DEP_CPP_GET_S) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\get_phost.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_GET_P=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\get_phost.obj" : $(SOURCE) $(DEP_CPP_GET_P) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_GET_P=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\get_phost.obj" : $(SOURCE) $(DEP_CPP_GET_P) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\krb_equiv.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_KRB_E=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\krb_equiv.obj" : $(SOURCE) $(DEP_CPP_KRB_E) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_KRB_E=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\krb_equiv.obj" : $(SOURCE) $(DEP_CPP_KRB_E) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\getaddrs.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_GETAD=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\getaddrs.obj" : $(SOURCE) $(DEP_CPP_GETAD) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_GETAD=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\getaddrs.obj" : $(SOURCE) $(DEP_CPP_GETAD) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\getrealm.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_GETRE=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\getrealm.obj" : $(SOURCE) $(DEP_CPP_GETRE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_GETRE=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\getrealm.obj" : $(SOURCE) $(DEP_CPP_GETRE) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\getst.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_GETST=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\getst.obj" : $(SOURCE) $(DEP_CPP_GETST) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_GETST=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\getst.obj" : $(SOURCE) $(DEP_CPP_GETST) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\k_flock.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_K_FLO=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\k_flock.obj" : $(SOURCE) $(DEP_CPP_K_FLO) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_K_FLO=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\k_flock.obj" : $(SOURCE) $(DEP_CPP_K_FLO) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\k_gethostname.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_K_GET=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\k_gethostname.obj" : $(SOURCE) $(DEP_CPP_K_GET) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_K_GET=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\k_gethostname.obj" : $(SOURCE) $(DEP_CPP_K_GET) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\k_getport.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_K_GETP=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\k_getport.obj" : $(SOURCE) $(DEP_CPP_K_GETP) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_K_GETP=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\k_getport.obj" : $(SOURCE) $(DEP_CPP_K_GETP) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\k_getsockinst.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_K_GETS=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\k_getsockinst.obj" : $(SOURCE) $(DEP_CPP_K_GETS) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_K_GETS=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\k_getsockinst.obj" : $(SOURCE) $(DEP_CPP_K_GETS) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\k_localtime.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_K_LOC=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\k_localtime.obj" : $(SOURCE) $(DEP_CPP_K_LOC) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_K_LOC=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\k_localtime.obj" : $(SOURCE) $(DEP_CPP_K_LOC) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\kdc_reply.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_KDC_R=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\kdc_reply.obj" : $(SOURCE) $(DEP_CPP_KDC_R) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_KDC_R=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\kdc_reply.obj" : $(SOURCE) $(DEP_CPP_KDC_R) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\kntoln.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_KNTOL=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	".\krb.h"\
	".\krb_locl.h"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Release\kntoln.obj" : $(SOURCE) $(DEP_CPP_KNTOL) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_KNTOL=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\kntoln.obj" : $(SOURCE) $(DEP_CPP_KNTOL) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\krb_check_auth.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_KRB_C=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\krb_check_auth.obj" : $(SOURCE) $(DEP_CPP_KRB_C) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_KRB_C=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\krb_check_auth.obj" : $(SOURCE) $(DEP_CPP_KRB_C) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\get_tf_realm.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_GET_TF=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\get_tf_realm.obj" : $(SOURCE) $(DEP_CPP_GET_TF) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_GET_TF=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\get_tf_realm.obj" : $(SOURCE) $(DEP_CPP_GET_TF) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\rd_safe.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_RD_SA=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	".\lsb_addr_comp.h"\
	

".\Release\rd_safe.obj" : $(SOURCE) $(DEP_CPP_RD_SA) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_RD_SA=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\lsb_addr_comp.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\rd_safe.obj" : $(SOURCE) $(DEP_CPP_RD_SA) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\krb_get_in_tkt.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_KRB_G=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\krb_get_in_tkt.obj" : $(SOURCE) $(DEP_CPP_KRB_G) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_KRB_G=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\krb_get_in_tkt.obj" : $(SOURCE) $(DEP_CPP_KRB_G) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lifetime.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_LIFET=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\lifetime.obj" : $(SOURCE) $(DEP_CPP_LIFET) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_LIFET=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\lifetime.obj" : $(SOURCE) $(DEP_CPP_LIFET) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lsb_addr_comp.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_LSB_A=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	".\lsb_addr_comp.h"\
	

".\Release\lsb_addr_comp.obj" : $(SOURCE) $(DEP_CPP_LSB_A) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_LSB_A=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\lsb_addr_comp.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\lsb_addr_comp.obj" : $(SOURCE) $(DEP_CPP_LSB_A) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\mk_auth.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_MK_AU=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\mk_auth.obj" : $(SOURCE) $(DEP_CPP_MK_AU) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_MK_AU=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\mk_auth.obj" : $(SOURCE) $(DEP_CPP_MK_AU) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\mk_err.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_MK_ER=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\mk_err.obj" : $(SOURCE) $(DEP_CPP_MK_ER) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_MK_ER=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\mk_err.obj" : $(SOURCE) $(DEP_CPP_MK_ER) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\mk_priv.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_MK_PR=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	".\lsb_addr_comp.h"\
	

".\Release\mk_priv.obj" : $(SOURCE) $(DEP_CPP_MK_PR) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_MK_PR=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\lsb_addr_comp.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\mk_priv.obj" : $(SOURCE) $(DEP_CPP_MK_PR) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\mk_req.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_MK_RE=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\mk_req.obj" : $(SOURCE) $(DEP_CPP_MK_RE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_MK_RE=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\mk_req.obj" : $(SOURCE) $(DEP_CPP_MK_RE) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\mk_safe.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_MK_SA=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	".\lsb_addr_comp.h"\
	

".\Release\mk_safe.obj" : $(SOURCE) $(DEP_CPP_MK_SA) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_MK_SA=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\lsb_addr_comp.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\mk_safe.obj" : $(SOURCE) $(DEP_CPP_MK_SA) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\month_sname.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_MONTH=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\month_sname.obj" : $(SOURCE) $(DEP_CPP_MONTH) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_MONTH=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\month_sname.obj" : $(SOURCE) $(DEP_CPP_MONTH) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\name2name.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_NAME2=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\name2name.obj" : $(SOURCE) $(DEP_CPP_NAME2) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_NAME2=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\name2name.obj" : $(SOURCE) $(DEP_CPP_NAME2) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\netread.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_NETRE=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\netread.obj" : $(SOURCE) $(DEP_CPP_NETRE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_NETRE=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\netread.obj" : $(SOURCE) $(DEP_CPP_NETRE) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\netwrite.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_NETWR=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\netwrite.obj" : $(SOURCE) $(DEP_CPP_NETWR) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_NETWR=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\netwrite.obj" : $(SOURCE) $(DEP_CPP_NETWR) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\one.c

!IF  "$(CFG)" == "krb - Win32 Release"


".\Release\one.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"


".\Debug\one.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\parse_name.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_PARSE=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\parse_name.obj" : $(SOURCE) $(DEP_CPP_PARSE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_PARSE=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\parse_name.obj" : $(SOURCE) $(DEP_CPP_PARSE) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\rd_err.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_RD_ER=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\rd_err.obj" : $(SOURCE) $(DEP_CPP_RD_ER) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_RD_ER=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\rd_err.obj" : $(SOURCE) $(DEP_CPP_RD_ER) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\rd_priv.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_RD_PR=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	".\lsb_addr_comp.h"\
	

".\Release\rd_priv.obj" : $(SOURCE) $(DEP_CPP_RD_PR) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_RD_PR=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\lsb_addr_comp.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\rd_priv.obj" : $(SOURCE) $(DEP_CPP_RD_PR) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\rd_req.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_RD_RE=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\rd_req.obj" : $(SOURCE) $(DEP_CPP_RD_RE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_RD_RE=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\rd_req.obj" : $(SOURCE) $(DEP_CPP_RD_RE) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\krb_err_txt.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_KRB_ER=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\krb_err_txt.obj" : $(SOURCE) $(DEP_CPP_KRB_ER) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_KRB_ER=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\krb_err_txt.obj" : $(SOURCE) $(DEP_CPP_KRB_ER) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\send_to_kdc.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_SEND_=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\send_to_kdc.obj" : $(SOURCE) $(DEP_CPP_SEND_) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_SEND_=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\send_to_kdc.obj" : $(SOURCE) $(DEP_CPP_SEND_) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\realm_parse.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_REALM=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\realm_parse.obj" : $(SOURCE) $(DEP_CPP_REALM) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_REALM=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\realm_parse.obj" : $(SOURCE) $(DEP_CPP_REALM) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\recvauth.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_RECVA=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\recvauth.obj" : $(SOURCE) $(DEP_CPP_RECVA) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_RECVA=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\recvauth.obj" : $(SOURCE) $(DEP_CPP_RECVA) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\resolve.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_RESOL=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	".\resolve.h"\
	

".\Release\resolve.obj" : $(SOURCE) $(DEP_CPP_RESOL) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_RESOL=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\resolve.obj" : $(SOURCE) $(DEP_CPP_RESOL) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\rw.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_RW_C68=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\des\version.h"\
	".\krb_locl.h"\
	

".\Release\rw.obj" : $(SOURCE) $(DEP_CPP_RW_C68) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_RW_C68=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\des\version.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\rw.obj" : $(SOURCE) $(DEP_CPP_RW_C68) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\save_credentials.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_SAVE_=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\save_credentials.obj" : $(SOURCE) $(DEP_CPP_SAVE_) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_SAVE_=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\save_credentials.obj" : $(SOURCE) $(DEP_CPP_SAVE_) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\read_service_key.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_READ_=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\read_service_key.obj" : $(SOURCE) $(DEP_CPP_READ_) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_READ_=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\read_service_key.obj" : $(SOURCE) $(DEP_CPP_READ_) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\verify_user.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_VERIF=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\verify_user.obj" : $(SOURCE) $(DEP_CPP_VERIF) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_VERIF=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\verify_user.obj" : $(SOURCE) $(DEP_CPP_VERIF) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\stime.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_STIME=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\stime.obj" : $(SOURCE) $(DEP_CPP_STIME) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_STIME=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\stime.obj" : $(SOURCE) $(DEP_CPP_STIME) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\swab.c
DEP_CPP_SWAB_=\
	"..\..\include\win32\config.h"\
	

!IF  "$(CFG)" == "krb - Win32 Release"


".\Release\swab.obj" : $(SOURCE) $(DEP_CPP_SWAB_) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"


".\Debug\swab.obj" : $(SOURCE) $(DEP_CPP_SWAB_) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ticket_memory.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_TICKE=\
	".\krb_locl.h"\
	".\ticket_memory.h"\
	

".\Release\ticket_memory.obj" : $(SOURCE) $(DEP_CPP_TICKE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_TICKE=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	".\ticket_memory.h"\
	

".\Debug\ticket_memory.obj" : $(SOURCE) $(DEP_CPP_TICKE) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\tkt_string.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_TKT_S=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\tkt_string.obj" : $(SOURCE) $(DEP_CPP_TKT_S) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_TKT_S=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\tkt_string.obj" : $(SOURCE) $(DEP_CPP_TKT_S) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\unparse_name.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_UNPAR=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\unparse_name.obj" : $(SOURCE) $(DEP_CPP_UNPAR) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_UNPAR=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\unparse_name.obj" : $(SOURCE) $(DEP_CPP_UNPAR) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\util.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_UTIL_=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\util.obj" : $(SOURCE) $(DEP_CPP_UTIL_) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_UTIL_=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\util.obj" : $(SOURCE) $(DEP_CPP_UTIL_) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\sendauth.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_SENDA=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\sendauth.obj" : $(SOURCE) $(DEP_CPP_SENDA) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_SENDA=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\sendauth.obj" : $(SOURCE) $(DEP_CPP_SENDA) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\logging.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_LOGGI=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\klog.h"\
	".\krb_locl.h"\
	

".\Release\logging.obj" : $(SOURCE) $(DEP_CPP_LOGGI) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_LOGGI=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\klog.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\logging.obj" : $(SOURCE) $(DEP_CPP_LOGGI) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\str2key.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_STR2K=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\str2key.obj" : $(SOURCE) $(DEP_CPP_STR2K) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_STR2K=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\str2key.obj" : $(SOURCE) $(DEP_CPP_STR2K) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\debug_decl.c

!IF  "$(CFG)" == "krb - Win32 Release"

DEP_CPP_DEBUG=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	".\krb_locl.h"\
	

".\Release\debug_decl.obj" : $(SOURCE) $(DEP_CPP_DEBUG) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

DEP_CPP_DEBUG=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\roken\roken.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

".\Debug\debug_decl.obj" : $(SOURCE) $(DEP_CPP_DEBUG) "$(INTDIR)"


!ENDIF 

# End Source File
# End Target
# End Project
################################################################################
