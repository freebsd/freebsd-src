# Microsoft Developer Studio Generated NMAKE File, Based on krb.dsp
!IF "$(CFG)" == ""
CFG=krb - Win32 Release
!MESSAGE No configuration specified. Defaulting to krb - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "krb - Win32 Release" && "$(CFG)" != "krb - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "krb.mak" CFG="krb - Win32 Release"
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

!IF  "$(CFG)" == "krb - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\krb.dll"

!ELSE 

ALL : "des - Win32 Release" "$(OUTDIR)\krb.dll"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"des - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN : 
!ENDIF 
	-@erase "$(INTDIR)\cr_err_reply.obj"
	-@erase "$(INTDIR)\create_auth_reply.obj"
	-@erase "$(INTDIR)\create_ciph.obj"
	-@erase "$(INTDIR)\create_ticket.obj"
	-@erase "$(INTDIR)\debug_decl.obj"
	-@erase "$(INTDIR)\decomp_ticket.obj"
	-@erase "$(INTDIR)\dllmain.obj"
	-@erase "$(INTDIR)\encrypt_ktext.obj"
	-@erase "$(INTDIR)\get_ad_tkt.obj"
	-@erase "$(INTDIR)\get_cred.obj"
	-@erase "$(INTDIR)\get_default_principal.obj"
	-@erase "$(INTDIR)\get_host.obj"
	-@erase "$(INTDIR)\get_in_tkt.obj"
	-@erase "$(INTDIR)\get_krbrlm.obj"
	-@erase "$(INTDIR)\get_svc_in_tkt.obj"
	-@erase "$(INTDIR)\get_tf_fullname.obj"
	-@erase "$(INTDIR)\get_tf_realm.obj"
	-@erase "$(INTDIR)\getaddrs.obj"
	-@erase "$(INTDIR)\getfile.obj"
	-@erase "$(INTDIR)\getrealm.obj"
	-@erase "$(INTDIR)\getst.obj"
	-@erase "$(INTDIR)\k_flock.obj"
	-@erase "$(INTDIR)\k_gethostname.obj"
	-@erase "$(INTDIR)\k_getport.obj"
	-@erase "$(INTDIR)\k_getsockinst.obj"
	-@erase "$(INTDIR)\k_localtime.obj"
	-@erase "$(INTDIR)\kdc_reply.obj"
	-@erase "$(INTDIR)\kntoln.obj"
	-@erase "$(INTDIR)\krb.res"
	-@erase "$(INTDIR)\krb_check_auth.obj"
	-@erase "$(INTDIR)\krb_equiv.obj"
	-@erase "$(INTDIR)\krb_err_txt.obj"
	-@erase "$(INTDIR)\krb_get_in_tkt.obj"
	-@erase "$(INTDIR)\lifetime.obj"
	-@erase "$(INTDIR)\logging.obj"
	-@erase "$(INTDIR)\lsb_addr_comp.obj"
	-@erase "$(INTDIR)\mk_auth.obj"
	-@erase "$(INTDIR)\mk_err.obj"
	-@erase "$(INTDIR)\mk_priv.obj"
	-@erase "$(INTDIR)\mk_req.obj"
	-@erase "$(INTDIR)\mk_safe.obj"
	-@erase "$(INTDIR)\month_sname.obj"
	-@erase "$(INTDIR)\name2name.obj"
	-@erase "$(INTDIR)\netread.obj"
	-@erase "$(INTDIR)\netwrite.obj"
	-@erase "$(INTDIR)\one.obj"
	-@erase "$(INTDIR)\parse_name.obj"
	-@erase "$(INTDIR)\rd_err.obj"
	-@erase "$(INTDIR)\rd_priv.obj"
	-@erase "$(INTDIR)\rd_req.obj"
	-@erase "$(INTDIR)\rd_safe.obj"
	-@erase "$(INTDIR)\read_service_key.obj"
	-@erase "$(INTDIR)\realm_parse.obj"
	-@erase "$(INTDIR)\recvauth.obj"
	-@erase "$(INTDIR)\rw.obj"
	-@erase "$(INTDIR)\save_credentials.obj"
	-@erase "$(INTDIR)\send_to_kdc.obj"
	-@erase "$(INTDIR)\sendauth.obj"
	-@erase "$(INTDIR)\stime.obj"
	-@erase "$(INTDIR)\str2key.obj"
	-@erase "$(INTDIR)\ticket_memory.obj"
	-@erase "$(INTDIR)\time.obj"
	-@erase "$(INTDIR)\tkt_string.obj"
	-@erase "$(INTDIR)\unparse_name.obj"
	-@erase "$(INTDIR)\util.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\verify_user.obj"
	-@erase "$(OUTDIR)\krb.dll"
	-@erase "$(OUTDIR)\krb.exp"
	-@erase "$(OUTDIR)\krb.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "." /I "..\..\include" /I\
 "..\..\include\win32" /I "..\des" /I "..\roken" /D "NDEBUG" /D "WIN32" /D\
 "_WINDOWS" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\krb.pch" /YX /Fo"$(INTDIR)\\"\
 /Fd"$(INTDIR)\\" /FD /c 
CPP_OBJS=.\Release/
CPP_SBRS=.

.c{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC=rc.exe
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\krb.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\krb.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=..\roken\Release\roken.lib ..\des\Release\des.lib wsock32.lib\
 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib\
 shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows /dll\
 /incremental:no /pdb:"$(OUTDIR)\krb.pdb" /machine:I386 /def:".\krb.def"\
 /out:"$(OUTDIR)\krb.dll" /implib:"$(OUTDIR)\krb.lib" 
DEF_FILE= \
	".\krb.def"
LINK32_OBJS= \
	"$(INTDIR)\cr_err_reply.obj" \
	"$(INTDIR)\create_auth_reply.obj" \
	"$(INTDIR)\create_ciph.obj" \
	"$(INTDIR)\create_ticket.obj" \
	"$(INTDIR)\debug_decl.obj" \
	"$(INTDIR)\decomp_ticket.obj" \
	"$(INTDIR)\dllmain.obj" \
	"$(INTDIR)\encrypt_ktext.obj" \
	"$(INTDIR)\get_ad_tkt.obj" \
	"$(INTDIR)\get_cred.obj" \
	"$(INTDIR)\get_default_principal.obj" \
	"$(INTDIR)\get_host.obj" \
	"$(INTDIR)\get_in_tkt.obj" \
	"$(INTDIR)\get_krbrlm.obj" \
	"$(INTDIR)\get_svc_in_tkt.obj" \
	"$(INTDIR)\get_tf_fullname.obj" \
	"$(INTDIR)\get_tf_realm.obj" \
	"$(INTDIR)\getaddrs.obj" \
	"$(INTDIR)\getfile.obj" \
	"$(INTDIR)\getrealm.obj" \
	"$(INTDIR)\getst.obj" \
	"$(INTDIR)\k_flock.obj" \
	"$(INTDIR)\k_gethostname.obj" \
	"$(INTDIR)\k_getport.obj" \
	"$(INTDIR)\k_getsockinst.obj" \
	"$(INTDIR)\k_localtime.obj" \
	"$(INTDIR)\kdc_reply.obj" \
	"$(INTDIR)\kntoln.obj" \
	"$(INTDIR)\krb.res" \
	"$(INTDIR)\krb_check_auth.obj" \
	"$(INTDIR)\krb_equiv.obj" \
	"$(INTDIR)\krb_err_txt.obj" \
	"$(INTDIR)\krb_get_in_tkt.obj" \
	"$(INTDIR)\lifetime.obj" \
	"$(INTDIR)\logging.obj" \
	"$(INTDIR)\lsb_addr_comp.obj" \
	"$(INTDIR)\mk_auth.obj" \
	"$(INTDIR)\mk_err.obj" \
	"$(INTDIR)\mk_priv.obj" \
	"$(INTDIR)\mk_req.obj" \
	"$(INTDIR)\mk_safe.obj" \
	"$(INTDIR)\month_sname.obj" \
	"$(INTDIR)\name2name.obj" \
	"$(INTDIR)\netread.obj" \
	"$(INTDIR)\netwrite.obj" \
	"$(INTDIR)\one.obj" \
	"$(INTDIR)\parse_name.obj" \
	"$(INTDIR)\rd_err.obj" \
	"$(INTDIR)\rd_priv.obj" \
	"$(INTDIR)\rd_req.obj" \
	"$(INTDIR)\rd_safe.obj" \
	"$(INTDIR)\read_service_key.obj" \
	"$(INTDIR)\realm_parse.obj" \
	"$(INTDIR)\recvauth.obj" \
	"$(INTDIR)\rw.obj" \
	"$(INTDIR)\save_credentials.obj" \
	"$(INTDIR)\send_to_kdc.obj" \
	"$(INTDIR)\sendauth.obj" \
	"$(INTDIR)\stime.obj" \
	"$(INTDIR)\str2key.obj" \
	"$(INTDIR)\ticket_memory.obj" \
	"$(INTDIR)\time.obj" \
	"$(INTDIR)\tkt_string.obj" \
	"$(INTDIR)\unparse_name.obj" \
	"$(INTDIR)\util.obj" \
	"$(INTDIR)\verify_user.obj" \
	"..\des\Release\des.lib"

"$(OUTDIR)\krb.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\krb.dll"

!ELSE 

ALL : "des - Win32 Debug" "$(OUTDIR)\krb.dll"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"des - Win32 DebugCLEAN" 
!ELSE 
CLEAN : 
!ENDIF 
	-@erase "$(INTDIR)\cr_err_reply.obj"
	-@erase "$(INTDIR)\create_auth_reply.obj"
	-@erase "$(INTDIR)\create_ciph.obj"
	-@erase "$(INTDIR)\create_ticket.obj"
	-@erase "$(INTDIR)\debug_decl.obj"
	-@erase "$(INTDIR)\decomp_ticket.obj"
	-@erase "$(INTDIR)\dllmain.obj"
	-@erase "$(INTDIR)\encrypt_ktext.obj"
	-@erase "$(INTDIR)\get_ad_tkt.obj"
	-@erase "$(INTDIR)\get_cred.obj"
	-@erase "$(INTDIR)\get_default_principal.obj"
	-@erase "$(INTDIR)\get_host.obj"
	-@erase "$(INTDIR)\get_in_tkt.obj"
	-@erase "$(INTDIR)\get_krbrlm.obj"
	-@erase "$(INTDIR)\get_svc_in_tkt.obj"
	-@erase "$(INTDIR)\get_tf_fullname.obj"
	-@erase "$(INTDIR)\get_tf_realm.obj"
	-@erase "$(INTDIR)\getaddrs.obj"
	-@erase "$(INTDIR)\getfile.obj"
	-@erase "$(INTDIR)\getrealm.obj"
	-@erase "$(INTDIR)\getst.obj"
	-@erase "$(INTDIR)\k_flock.obj"
	-@erase "$(INTDIR)\k_gethostname.obj"
	-@erase "$(INTDIR)\k_getport.obj"
	-@erase "$(INTDIR)\k_getsockinst.obj"
	-@erase "$(INTDIR)\k_localtime.obj"
	-@erase "$(INTDIR)\kdc_reply.obj"
	-@erase "$(INTDIR)\kntoln.obj"
	-@erase "$(INTDIR)\krb.res"
	-@erase "$(INTDIR)\krb_check_auth.obj"
	-@erase "$(INTDIR)\krb_equiv.obj"
	-@erase "$(INTDIR)\krb_err_txt.obj"
	-@erase "$(INTDIR)\krb_get_in_tkt.obj"
	-@erase "$(INTDIR)\lifetime.obj"
	-@erase "$(INTDIR)\logging.obj"
	-@erase "$(INTDIR)\lsb_addr_comp.obj"
	-@erase "$(INTDIR)\mk_auth.obj"
	-@erase "$(INTDIR)\mk_err.obj"
	-@erase "$(INTDIR)\mk_priv.obj"
	-@erase "$(INTDIR)\mk_req.obj"
	-@erase "$(INTDIR)\mk_safe.obj"
	-@erase "$(INTDIR)\month_sname.obj"
	-@erase "$(INTDIR)\name2name.obj"
	-@erase "$(INTDIR)\netread.obj"
	-@erase "$(INTDIR)\netwrite.obj"
	-@erase "$(INTDIR)\one.obj"
	-@erase "$(INTDIR)\parse_name.obj"
	-@erase "$(INTDIR)\rd_err.obj"
	-@erase "$(INTDIR)\rd_priv.obj"
	-@erase "$(INTDIR)\rd_req.obj"
	-@erase "$(INTDIR)\rd_safe.obj"
	-@erase "$(INTDIR)\read_service_key.obj"
	-@erase "$(INTDIR)\realm_parse.obj"
	-@erase "$(INTDIR)\recvauth.obj"
	-@erase "$(INTDIR)\rw.obj"
	-@erase "$(INTDIR)\save_credentials.obj"
	-@erase "$(INTDIR)\send_to_kdc.obj"
	-@erase "$(INTDIR)\sendauth.obj"
	-@erase "$(INTDIR)\stime.obj"
	-@erase "$(INTDIR)\str2key.obj"
	-@erase "$(INTDIR)\ticket_memory.obj"
	-@erase "$(INTDIR)\time.obj"
	-@erase "$(INTDIR)\tkt_string.obj"
	-@erase "$(INTDIR)\unparse_name.obj"
	-@erase "$(INTDIR)\util.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\vc50.pdb"
	-@erase "$(INTDIR)\verify_user.obj"
	-@erase "$(OUTDIR)\krb.dll"
	-@erase "$(OUTDIR)\krb.exp"
	-@erase "$(OUTDIR)\krb.ilk"
	-@erase "$(OUTDIR)\krb.lib"
	-@erase "$(OUTDIR)\krb.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MDd /W3 /Gm /GX /Zi /Od /I "." /I "..\..\include" /I\
 "..\..\include\win32" /I "..\des" /I "..\roken" /D "_DEBUG" /D "WIN32" /D\
 "_WINDOWS" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\krb.pch" /YX /Fo"$(INTDIR)\\"\
 /Fd"$(INTDIR)\\" /FD /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.

.c{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC=rc.exe
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\krb.res" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\krb.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=..\roken\Debug\roken.lib ..\des\Debug\des.lib wsock32.lib\
 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib\
 shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows /dll\
 /incremental:yes /pdb:"$(OUTDIR)\krb.pdb" /debug /machine:I386 /def:".\krb.def"\
 /out:"$(OUTDIR)\krb.dll" /implib:"$(OUTDIR)\krb.lib" 
DEF_FILE= \
	".\krb.def"
LINK32_OBJS= \
	"$(INTDIR)\cr_err_reply.obj" \
	"$(INTDIR)\create_auth_reply.obj" \
	"$(INTDIR)\create_ciph.obj" \
	"$(INTDIR)\create_ticket.obj" \
	"$(INTDIR)\debug_decl.obj" \
	"$(INTDIR)\decomp_ticket.obj" \
	"$(INTDIR)\dllmain.obj" \
	"$(INTDIR)\encrypt_ktext.obj" \
	"$(INTDIR)\get_ad_tkt.obj" \
	"$(INTDIR)\get_cred.obj" \
	"$(INTDIR)\get_default_principal.obj" \
	"$(INTDIR)\get_host.obj" \
	"$(INTDIR)\get_in_tkt.obj" \
	"$(INTDIR)\get_krbrlm.obj" \
	"$(INTDIR)\get_svc_in_tkt.obj" \
	"$(INTDIR)\get_tf_fullname.obj" \
	"$(INTDIR)\get_tf_realm.obj" \
	"$(INTDIR)\getaddrs.obj" \
	"$(INTDIR)\getfile.obj" \
	"$(INTDIR)\getrealm.obj" \
	"$(INTDIR)\getst.obj" \
	"$(INTDIR)\k_flock.obj" \
	"$(INTDIR)\k_gethostname.obj" \
	"$(INTDIR)\k_getport.obj" \
	"$(INTDIR)\k_getsockinst.obj" \
	"$(INTDIR)\k_localtime.obj" \
	"$(INTDIR)\kdc_reply.obj" \
	"$(INTDIR)\kntoln.obj" \
	"$(INTDIR)\krb.res" \
	"$(INTDIR)\krb_check_auth.obj" \
	"$(INTDIR)\krb_equiv.obj" \
	"$(INTDIR)\krb_err_txt.obj" \
	"$(INTDIR)\krb_get_in_tkt.obj" \
	"$(INTDIR)\lifetime.obj" \
	"$(INTDIR)\logging.obj" \
	"$(INTDIR)\lsb_addr_comp.obj" \
	"$(INTDIR)\mk_auth.obj" \
	"$(INTDIR)\mk_err.obj" \
	"$(INTDIR)\mk_priv.obj" \
	"$(INTDIR)\mk_req.obj" \
	"$(INTDIR)\mk_safe.obj" \
	"$(INTDIR)\month_sname.obj" \
	"$(INTDIR)\name2name.obj" \
	"$(INTDIR)\netread.obj" \
	"$(INTDIR)\netwrite.obj" \
	"$(INTDIR)\one.obj" \
	"$(INTDIR)\parse_name.obj" \
	"$(INTDIR)\rd_err.obj" \
	"$(INTDIR)\rd_priv.obj" \
	"$(INTDIR)\rd_req.obj" \
	"$(INTDIR)\rd_safe.obj" \
	"$(INTDIR)\read_service_key.obj" \
	"$(INTDIR)\realm_parse.obj" \
	"$(INTDIR)\recvauth.obj" \
	"$(INTDIR)\rw.obj" \
	"$(INTDIR)\save_credentials.obj" \
	"$(INTDIR)\send_to_kdc.obj" \
	"$(INTDIR)\sendauth.obj" \
	"$(INTDIR)\stime.obj" \
	"$(INTDIR)\str2key.obj" \
	"$(INTDIR)\ticket_memory.obj" \
	"$(INTDIR)\time.obj" \
	"$(INTDIR)\tkt_string.obj" \
	"$(INTDIR)\unparse_name.obj" \
	"$(INTDIR)\util.obj" \
	"$(INTDIR)\verify_user.obj" \
	"..\des\Debug\des.lib"

"$(OUTDIR)\krb.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(CFG)" == "krb - Win32 Release" || "$(CFG)" == "krb - Win32 Debug"
SOURCE=.\cr_err_reply.c
DEP_CPP_CR_ER=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\cr_err_reply.obj" : $(SOURCE) $(DEP_CPP_CR_ER) "$(INTDIR)"


SOURCE=.\create_auth_reply.c
DEP_CPP_CREAT=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\create_auth_reply.obj" : $(SOURCE) $(DEP_CPP_CREAT) "$(INTDIR)"


SOURCE=.\create_ciph.c
DEP_CPP_CREATE=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\create_ciph.obj" : $(SOURCE) $(DEP_CPP_CREATE) "$(INTDIR)"


SOURCE=.\create_ticket.c
DEP_CPP_CREATE_=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\create_ticket.obj" : $(SOURCE) $(DEP_CPP_CREATE_) "$(INTDIR)"


SOURCE=.\debug_decl.c
DEP_CPP_DEBUG=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\debug_decl.obj" : $(SOURCE) $(DEP_CPP_DEBUG) "$(INTDIR)"


SOURCE=.\decomp_ticket.c
DEP_CPP_DECOM=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\decomp_ticket.obj" : $(SOURCE) $(DEP_CPP_DECOM) "$(INTDIR)"


SOURCE=.\dllmain.c
DEP_CPP_DLLMA=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	".\ticket_memory.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\dllmain.obj" : $(SOURCE) $(DEP_CPP_DLLMA) "$(INTDIR)"


SOURCE=.\encrypt_ktext.c
DEP_CPP_ENCRY=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\encrypt_ktext.obj" : $(SOURCE) $(DEP_CPP_ENCRY) "$(INTDIR)"


SOURCE=.\get_ad_tkt.c
DEP_CPP_GET_A=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\get_ad_tkt.obj" : $(SOURCE) $(DEP_CPP_GET_A) "$(INTDIR)"


SOURCE=.\get_cred.c
DEP_CPP_GET_C=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\get_cred.obj" : $(SOURCE) $(DEP_CPP_GET_C) "$(INTDIR)"


SOURCE=.\get_default_principal.c
DEP_CPP_GET_D=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\get_default_principal.obj" : $(SOURCE) $(DEP_CPP_GET_D) "$(INTDIR)"


SOURCE=.\get_host.c
DEP_CPP_GET_H=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\get_host.obj" : $(SOURCE) $(DEP_CPP_GET_H) "$(INTDIR)"


SOURCE=.\get_in_tkt.c
DEP_CPP_GET_I=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\get_in_tkt.obj" : $(SOURCE) $(DEP_CPP_GET_I) "$(INTDIR)"


SOURCE=.\get_krbrlm.c
DEP_CPP_GET_K=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\get_krbrlm.obj" : $(SOURCE) $(DEP_CPP_GET_K) "$(INTDIR)"


SOURCE=.\get_svc_in_tkt.c
DEP_CPP_GET_S=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\get_svc_in_tkt.obj" : $(SOURCE) $(DEP_CPP_GET_S) "$(INTDIR)"


SOURCE=.\get_tf_fullname.c
DEP_CPP_GET_T=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\get_tf_fullname.obj" : $(SOURCE) $(DEP_CPP_GET_T) "$(INTDIR)"


SOURCE=.\get_tf_realm.c
DEP_CPP_GET_TF=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\get_tf_realm.obj" : $(SOURCE) $(DEP_CPP_GET_TF) "$(INTDIR)"


SOURCE=.\getaddrs.c
DEP_CPP_GETAD=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\getaddrs.obj" : $(SOURCE) $(DEP_CPP_GETAD) "$(INTDIR)"


SOURCE=.\getfile.c
DEP_CPP_GETFI=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\getfile.obj" : $(SOURCE) $(DEP_CPP_GETFI) "$(INTDIR)"


SOURCE=.\getrealm.c
DEP_CPP_GETRE=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	".\resolve.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\getrealm.obj" : $(SOURCE) $(DEP_CPP_GETRE) "$(INTDIR)"


SOURCE=.\getst.c
DEP_CPP_GETST=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\getst.obj" : $(SOURCE) $(DEP_CPP_GETST) "$(INTDIR)"


SOURCE=.\k_flock.c
DEP_CPP_K_FLO=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\k_flock.obj" : $(SOURCE) $(DEP_CPP_K_FLO) "$(INTDIR)"


SOURCE=.\k_gethostname.c
DEP_CPP_K_GET=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\k_gethostname.obj" : $(SOURCE) $(DEP_CPP_K_GET) "$(INTDIR)"


SOURCE=.\k_getport.c
DEP_CPP_K_GETP=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\k_getport.obj" : $(SOURCE) $(DEP_CPP_K_GETP) "$(INTDIR)"


SOURCE=.\k_getsockinst.c
DEP_CPP_K_GETS=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\k_getsockinst.obj" : $(SOURCE) $(DEP_CPP_K_GETS) "$(INTDIR)"


SOURCE=.\k_localtime.c
DEP_CPP_K_LOC=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\k_localtime.obj" : $(SOURCE) $(DEP_CPP_K_LOC) "$(INTDIR)"


SOURCE=.\kdc_reply.c
DEP_CPP_KDC_R=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\kdc_reply.obj" : $(SOURCE) $(DEP_CPP_KDC_R) "$(INTDIR)"


SOURCE=.\kntoln.c
DEP_CPP_KNTOL=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\kntoln.obj" : $(SOURCE) $(DEP_CPP_KNTOL) "$(INTDIR)"


SOURCE=.\krb_check_auth.c
DEP_CPP_KRB_C=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\krb_check_auth.obj" : $(SOURCE) $(DEP_CPP_KRB_C) "$(INTDIR)"


SOURCE=.\krb_equiv.c
DEP_CPP_KRB_E=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\


"$(INTDIR)\krb_equiv.obj" : $(SOURCE) $(DEP_CPP_KRB_E) "$(INTDIR)"


SOURCE=.\krb_err_txt.c
DEP_CPP_KRB_ER=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\krb_err_txt.obj" : $(SOURCE) $(DEP_CPP_KRB_ER) "$(INTDIR)"


SOURCE=.\krb_get_in_tkt.c
DEP_CPP_KRB_G=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\krb_get_in_tkt.obj" : $(SOURCE) $(DEP_CPP_KRB_G) "$(INTDIR)"


SOURCE=.\lifetime.c
DEP_CPP_LIFET=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\lifetime.obj" : $(SOURCE) $(DEP_CPP_LIFET) "$(INTDIR)"


SOURCE=.\logging.c
DEP_CPP_LOGGI=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\klog.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\logging.obj" : $(SOURCE) $(DEP_CPP_LOGGI) "$(INTDIR)"


SOURCE=.\lsb_addr_comp.c
DEP_CPP_LSB_A=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-archaeology.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\lsb_addr_comp.obj" : $(SOURCE) $(DEP_CPP_LSB_A) "$(INTDIR)"


SOURCE=.\mk_auth.c
DEP_CPP_MK_AU=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\mk_auth.obj" : $(SOURCE) $(DEP_CPP_MK_AU) "$(INTDIR)"


SOURCE=.\mk_err.c
DEP_CPP_MK_ER=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\mk_err.obj" : $(SOURCE) $(DEP_CPP_MK_ER) "$(INTDIR)"


SOURCE=.\mk_priv.c
DEP_CPP_MK_PR=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-archaeology.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\mk_priv.obj" : $(SOURCE) $(DEP_CPP_MK_PR) "$(INTDIR)"


SOURCE=.\mk_req.c
DEP_CPP_MK_RE=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\mk_req.obj" : $(SOURCE) $(DEP_CPP_MK_RE) "$(INTDIR)"


SOURCE=.\mk_safe.c
DEP_CPP_MK_SA=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-archaeology.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\mk_safe.obj" : $(SOURCE) $(DEP_CPP_MK_SA) "$(INTDIR)"


SOURCE=.\month_sname.c
DEP_CPP_MONTH=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\month_sname.obj" : $(SOURCE) $(DEP_CPP_MONTH) "$(INTDIR)"


SOURCE=.\name2name.c
DEP_CPP_NAME2=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\name2name.obj" : $(SOURCE) $(DEP_CPP_NAME2) "$(INTDIR)"


SOURCE=.\netread.c
DEP_CPP_NETRE=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\netread.obj" : $(SOURCE) $(DEP_CPP_NETRE) "$(INTDIR)"


SOURCE=.\netwrite.c
DEP_CPP_NETWR=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\netwrite.obj" : $(SOURCE) $(DEP_CPP_NETWR) "$(INTDIR)"


SOURCE=.\one.c

"$(INTDIR)\one.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\parse_name.c
DEP_CPP_PARSE=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\parse_name.obj" : $(SOURCE) $(DEP_CPP_PARSE) "$(INTDIR)"


SOURCE=.\rd_err.c
DEP_CPP_RD_ER=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\rd_err.obj" : $(SOURCE) $(DEP_CPP_RD_ER) "$(INTDIR)"


SOURCE=.\rd_priv.c
DEP_CPP_RD_PR=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-archaeology.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\rd_priv.obj" : $(SOURCE) $(DEP_CPP_RD_PR) "$(INTDIR)"


SOURCE=.\rd_req.c
DEP_CPP_RD_RE=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\rd_req.obj" : $(SOURCE) $(DEP_CPP_RD_RE) "$(INTDIR)"


SOURCE=.\rd_safe.c
DEP_CPP_RD_SA=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-archaeology.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\


"$(INTDIR)\rd_safe.obj" : $(SOURCE) $(DEP_CPP_RD_SA) "$(INTDIR)"


SOURCE=.\read_service_key.c
DEP_CPP_READ_=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\read_service_key.obj" : $(SOURCE) $(DEP_CPP_READ_) "$(INTDIR)"


SOURCE=.\realm_parse.c
DEP_CPP_REALM=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\realm_parse.obj" : $(SOURCE) $(DEP_CPP_REALM) "$(INTDIR)"


SOURCE=.\recvauth.c
DEP_CPP_RECVA=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\recvauth.obj" : $(SOURCE) $(DEP_CPP_RECVA) "$(INTDIR)"


SOURCE=.\resolve.c
DEP_CPP_RESOL=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\resolve.obj" : $(SOURCE) $(DEP_CPP_RESOL) "$(INTDIR)"


SOURCE=.\rw.c
DEP_CPP_RW_C6a=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\..\include\win32\version.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\


"$(INTDIR)\rw.obj" : $(SOURCE) $(DEP_CPP_RW_C6a) "$(INTDIR)"


SOURCE=.\save_credentials.c
DEP_CPP_SAVE_=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\save_credentials.obj" : $(SOURCE) $(DEP_CPP_SAVE_) "$(INTDIR)"


SOURCE=.\send_to_kdc.c
DEP_CPP_SEND_=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\base64.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\send_to_kdc.obj" : $(SOURCE) $(DEP_CPP_SEND_) "$(INTDIR)"


SOURCE=.\sendauth.c
DEP_CPP_SENDA=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\sendauth.obj" : $(SOURCE) $(DEP_CPP_SENDA) "$(INTDIR)"


SOURCE=.\stime.c
DEP_CPP_STIME=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\stime.obj" : $(SOURCE) $(DEP_CPP_STIME) "$(INTDIR)"


SOURCE=.\str2key.c
DEP_CPP_STR2K=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\


"$(INTDIR)\str2key.obj" : $(SOURCE) $(DEP_CPP_STR2K) "$(INTDIR)"


SOURCE=.\ticket_memory.c
DEP_CPP_TICKE=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	".\ticket_memory.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\ticket_memory.obj" : $(SOURCE) $(DEP_CPP_TICKE) "$(INTDIR)"


SOURCE=.\time.c
DEP_CPP_TIME_=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\


"$(INTDIR)\time.obj" : $(SOURCE) $(DEP_CPP_TIME_) "$(INTDIR)"


SOURCE=.\tkt_string.c
DEP_CPP_TKT_S=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\tkt_string.obj" : $(SOURCE) $(DEP_CPP_TKT_S) "$(INTDIR)"


SOURCE=.\unparse_name.c
DEP_CPP_UNPAR=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\unparse_name.obj" : $(SOURCE) $(DEP_CPP_UNPAR) "$(INTDIR)"


SOURCE=.\util.c
DEP_CPP_UTIL_=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\util.obj" : $(SOURCE) $(DEP_CPP_UTIL_) "$(INTDIR)"


SOURCE=.\verify_user.c
DEP_CPP_VERIF=\
	"..\..\include\protos.h"\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\..\include\win32\roken.h"\
	"..\des\des.h"\
	"..\roken\err.h"\
	"..\roken\roken-common.h"\
	".\krb-protos.h"\
	".\krb.h"\
	".\krb_locl.h"\
	".\krb_log.h"\
	".\prot.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\verify_user.obj" : $(SOURCE) $(DEP_CPP_VERIF) "$(INTDIR)"


SOURCE=.\krb.rc

"$(INTDIR)\krb.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) $(RSC_PROJ) $(SOURCE)
	

!IF  "$(CFG)" == "krb - Win32 Release"

"des - Win32 Release" : 
   cd "\tmp\wirus-krb\krb4-pre-0.9.9\lib\des"
   $(MAKE) /$(MAKEFLAGS) /F ".\des.mak" CFG="des - Win32 Release" 
   cd "..\krb"

"des - Win32 ReleaseCLEAN" : 
   cd "\tmp\wirus-krb\krb4-pre-0.9.9\lib\des"
   $(MAKE) /$(MAKEFLAGS) CLEAN /F ".\des.mak" CFG="des - Win32 Release"\
 RECURSE=1 
   cd "..\krb"

!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

"des - Win32 Debug" : 
   cd "\tmp\wirus-krb\krb4-pre-0.9.9\lib\des"
   $(MAKE) /$(MAKEFLAGS) /F ".\des.mak" CFG="des - Win32 Debug" 
   cd "..\krb"

"des - Win32 DebugCLEAN" : 
   cd "\tmp\wirus-krb\krb4-pre-0.9.9\lib\des"
   $(MAKE) /$(MAKEFLAGS) CLEAN /F ".\des.mak" CFG="des - Win32 Debug" RECURSE=1\
	
   cd "..\krb"

!ENDIF 


!ENDIF 

