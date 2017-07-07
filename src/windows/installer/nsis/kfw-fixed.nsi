;-----------------------------------------------------------------
; KfW defines and functionality
; Copyright (c) 2004,2005,2006,2007 Massachusetts Institute of Technology
; Copyright (c) 2006,2007 Secure Endpoints Inc.

!define KFW_VERSION "${KFW_MAJORVERSION}.${KFW_MINORVERSION}.${KFW_PATCHLEVEL}"

!define PROGRAM_NAME "Kerberos for Windows"
!ifdef RELEASE
!ifndef DEBUG        ; !DEBUG on v2.0b4
Name "MIT ${PROGRAM_NAME} ${KFW_VERSION}"
!else                ; DEBUG on v2.0b4
Name "MIT ${PROGRAM_NAME} ${KFW_VERSION} Checked/Debug"
!endif               ; End DEBUG/!DEBUG
!else
!ifdef BETA
!ifndef DEBUG        ; !DEBUG on v2.0b4
Name "MIT ${PROGRAM_NAME} ${KFW_VERSION} Beta ${BETA}"
!else                ; DEBUG on v2.0b4
Name "MIT ${PROGRAM_NAME} ${KFW_VERSION} Beta ${BETA} Checked/Debug"
!endif               ; End DEBUG/!DEBUG
!else
!ifndef DEBUG        ; !DEBUG on v2.0b4
Name "MIT ${PROGRAM_NAME} ${KFW_VERSION} ${__DATE__} ${__TIME__}"
!else                ; DEBUG on v2.0b4
Name "MIT ${PROGRAM_NAME} ${KFW_VERSION} ${__DATE__} ${__TIME__} Checked/Debug"
!endif               ; End DEBUG/!DEBUG
!endif
!endif
VIProductVersion "${KFW_MAJORVERSION}.${KFW_MINORVERSION}.${KFW_PATCHLEVEL}.00"
VIAddVersionKey "ProductName" "${PROGRAM_NAME}"
VIAddVersionKey "CompanyName" "Massachusetts Institute of Technology"
VIAddVersionKey "FileVersion"  ${VIProductVersion}
VIAddVersionKey "ProductVersion"  "${KFW_MAJORVERSION}.${KFW_MINORVERSION}.${KFW_PATCHLEVEL}.0"
VIAddVersionKey "FileDescription" "MIT Kerberos for Windows Installer"
VIAddVersionKey "LegalCopyright" "(C)2004,2005,2006,2007"
!ifdef DEBUG
VIAddVersionKey "PrivateBuild" "Checked/Debug"
!endif               ; End DEBUG


;--------------------------------
;Configuration

  ;General
  SetCompressor lzma
!ifndef DEBUG
  OutFile "MITKerberosForWindows.exe"
!else
  OutFile "MITKerberosForWindows-DEBUG.exe"
!endif
  SilentInstall normal
  ShowInstDetails show
  XPStyle on
  !define MUI_ICON "kfw.ico"
  !define MUI_UNICON "kfw.ico"
  !define KFW_COMPANY_NAME "Massachusetts Institute of Technology"
  !define KFW_PRODUCT_NAME "${PROGRAM_NAME}"
  !define KFW_REGKEY_ROOT  "Software\MIT\Kerberos\"
  !define NIM_REGKEY_ROOT  "Software\MIT\NetIDMgr\"
  CRCCheck force
  !define REPLACEDLL_NOREGISTER

  ;Folder selection page
  InstallDir "$PROGRAMFILES\MIT\Kerberos"      ; Install to shorter path
  
  ;Remember install folder
  InstallDirRegKey HKLM "${KFW_REGKEY_ROOT}" ""
  
  ;Remember the installer language
  !define MUI_LANGDLL_REGISTRY_ROOT "HKLM" 
  !define MUI_LANGDLL_REGISTRY_KEY "${KFW_REGKEY_ROOT}" 
  !define MUI_LANGDLL_REGISTRY_VALUENAME "Installer Language"
  
  ;Where are the files?
  !define KFW_BIN_DIR "${KFW_TARGETDIR}\bin\i386"
  !define KFW_DOC_DIR "${KFW_TARGETDIR}\doc"
  !define KFW_INC_DIR "${KFW_TARGETDIR}\inc"
  !define KFW_LIB_DIR "${KFW_TARGETDIR}\lib\i386"
  !define KFW_SAMPLE_DIR "${KFW_TARGETDIR}\sample"
  !define KFW_INSTALL_DIR "${KFW_TARGETDIR}\install"
  !define SYSTEMDIR   "$%SystemRoot%\System32" 
 

;--------------------------------
;Modern UI Configuration

  !define MUI_LICENSEPAGE
  !define MUI_CUSTOMPAGECOMMANDS
  !define MUI_WELCOMEPAGE
  !define MUI_COMPONENTSPAGE
  !define MUI_COMPONENTSPAGE_SMALLDESC
  !define MUI_DIRECTORYPAGE

  !define MUI_ABORTWARNING
  !define MUI_FINISHPAGE
  
  !define MUI_UNINSTALLER
  !define MUI_UNCONFIRMPAGE
  
  
  !insertmacro MUI_PAGE_WELCOME
  !insertmacro MUI_PAGE_LICENSE "Licenses.rtf"
  !insertmacro MUI_PAGE_COMPONENTS
  !insertmacro MUI_PAGE_DIRECTORY
  Page custom KFWPageGetConfigFiles
  Page custom KFWPageGetStartupConfig
  !insertmacro MUI_PAGE_INSTFILES
  !insertmacro MUI_PAGE_FINISH
  
;--------------------------------
;Languages

  !insertmacro MUI_LANGUAGE "English"
  
;--------------------------------
;Language Strings
    
  ;Descriptions
  LangString DESC_SecCopyUI ${LANG_ENGLISH} "${PROGRAM_NAME}: English"

  LangString DESC_secClient ${LANG_ENGLISH} "Client: Allows you to utilize MIT Kerberos from your Windows PC."
  
  LangString DESC_secDebug ${LANG_ENGLISH} "Debug Symbols: Used for debugging problems with MIT Kerberos for Windows"
  
  LangString DESC_secSDK ${LANG_ENGLISH} "SDK: Allows you to build MIT Kerberos aware applications."
  
  LangString DESC_secDocs ${LANG_ENGLISH} "Documentation: Release Notes and User Manuals."
  
; Popup error messages
  LangString RealmNameError ${LANG_ENGLISH} "You must specify a realm name for your client to use."

  LangString ConfigFileError ${LANG_ENGLISH} "You must specify a valid configuration file location from which files can be copied during the install"
 
  LangString URLError ${LANG_ENGLISH} "You must specify a URL if you choose the option to download the config files."
  
; Upgrade/re-install strings
   LangString UPGRADE_CLIENT ${LANG_ENGLISH} "Upgrade Kerberos Client"
   LangString REINSTALL_CLIENT ${LANG_ENGLISH} "Re-install Kerberos Client"
   LangString DOWNGRADE_CLIENT ${LANG_ENGLISH} "Downgrade Kerberos Client"
  
   LangString UPGRADE_SDK ${LANG_ENGLISH} "Upgrade Kerberos SDK"
   LangString REINSTALL_SDK ${LANG_ENGLISH} "Re-install Kerberos SDK"
   LangString DOWNGRADE_SDK ${LANG_ENGLISH} "Downgrade Kerberos SDK"
  
   LangString UPGRADE_DOCS ${LANG_ENGLISH} "Upgrade Kerberos Documentation"
   LangString REINSTALL_DOCS ${LANG_ENGLISH} "Re-install Kerberos Documentation"
   LangString DOWNGRADE_DOCS ${LANG_ENGLISH} "Downgrade Kerberos Documentation"
  
  ReserveFile "${KFW_CONFIG_DIR}\sample\krb.con"
  ReserveFile "${KFW_CONFIG_DIR}\sample\krbrealm.con"
  ReserveFile "${KFW_CONFIG_DIR}\sample\krb5.ini"
  !insertmacro MUI_RESERVEFILE_INSTALLOPTIONS ;InstallOptions plug-in
  !insertmacro MUI_RESERVEFILE_LANGDLL ;Language selection dialog

;--------------------------------
;Reserve Files
  
  ;Things that need to be extracted on first (keep these lines before any File command!)
  ;Only useful for BZIP2 compression
  !insertmacro MUI_RESERVEFILE_LANGDLL
  
;--------------------------------
; Load Macros
!include "utils.nsi"

;--------------------------------
;Installer Sections

;----------------------
; Kerberos for Windows CLIENT
Section "KfW Client" secClient

  SetShellVarContext all
  ; Stop any running services or we can't replace the files
  ; Stop the running processes
  GetTempFileName $R0
  File /oname=$R0 "Killer.exe"
  nsExec::Exec '$R0 netidmgr.exe'
  nsExec::Exec '$R0 leash32.exe'
  nsExec::Exec '$R0 krbcc32s.exe'
  nsExec::Exec '$R0 k95.exe'
  nsExec::Exec '$R0 k95g.exe'
  nsExec::Exec '$R0 krb5.exe'
  nsExec::Exec '$R0 gss.exe'
  nsExec::Exec '$R0 afscreds.exe'

  RMDir /r "$INSTDIR\bin"

   ; Do client components
  SetOutPath "$INSTDIR\bin"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\comerr32.dll"        "$INSTDIR\bin\comerr32.dll"      "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\gss.exe"             "$INSTDIR\bin\gss.exe"           "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\gss-client.exe"      "$INSTDIR\bin\gss-client.exe"    "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\gss-server.exe"      "$INSTDIR\bin\gss-server.exe"    "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\gssapi32.dll"        "$INSTDIR\bin\gssapi32.dll"      "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\k524init.exe"        "$INSTDIR\bin\k524init.exe"      "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\kclnt32.dll"         "$INSTDIR\bin\kclnt32.dll"       "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\kdestroy.exe"        "$INSTDIR\bin\kdestroy.exe"      "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\kinit.exe"           "$INSTDIR\bin\kinit.exe"         "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\klist.exe"           "$INSTDIR\bin\klist.exe"         "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\kpasswd.exe"         "$INSTDIR\bin\kpasswd.exe"       "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\kvno.exe"            "$INSTDIR\bin\kvno.exe"          "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\krb5_32.dll"         "$INSTDIR\bin\krb5_32.dll"       "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\k5sprt32.dll"        "$INSTDIR\bin\k5sprt32.dll"      "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\krb524.dll"          "$INSTDIR\bin\krb524.dll"        "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\krbcc32.dll"         "$INSTDIR\bin\krbcc32.dll"       "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\krbcc32s.exe"        "$INSTDIR\bin\krbcc32s.exe"      "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\krbv4w32.dll"        "$INSTDIR\bin\krbv4w32.dll"      "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\netidmgr.chm"         "$INSTDIR\bin\netidmgr.chm"       "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\krb4cred.dll"         "$INSTDIR\bin\krb4cred.dll"       "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\krb5cred.dll"         "$INSTDIR\bin\krb5cred.dll"       "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\krb4cred_en_us.dll"   "$INSTDIR\bin\krb4cred_en_us.dll"       "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\krb5cred_en_us.dll"   "$INSTDIR\bin\krb5cred_en_us.dll"       "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\leashw32.dll"        "$INSTDIR\bin\leashw32.dll"      "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\ms2mit.exe"          "$INSTDIR\bin\ms2mit.exe"        "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\mit2ms.exe"          "$INSTDIR\bin\mit2ms.exe"        "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\kcpytkt.exe"          "$INSTDIR\bin\kcpytkt.exe"        "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\kdeltkt.exe"          "$INSTDIR\bin\kdeltkt.exe"        "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\wshelp32.dll"        "$INSTDIR\bin\wshelp32.dll"      "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\xpprof32.dll"        "$INSTDIR\bin\xpprof32.dll"      "$INSTDIR"

  Call GetWindowsVersion
  Pop $R0
  StrCmp $R0 "2000" nid_inst2000
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\netidmgr.exe"         "$INSTDIR\bin\netidmgr.exe"       "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\nidmgr32.dll"         "$INSTDIR\bin\nidmgr32.dll"       "$INSTDIR"
  goto nid_done
nid_inst2000:  
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\W2K\netidmgr.exe"     "$INSTDIR\bin\netidmgr.exe"       "$INSTDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\W2K\nidmgr32.dll"     "$INSTDIR\bin\nidmgr32.dll"       "$INSTDIR"
nid_done:

!ifdef DEBUG
!IFDEF CL_1400
  !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcr80d.dll"    "$INSTDIR\bin\msvcr80d.dll"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcp80d.dll"    "$INSTDIR\bin\msvcp80d.dll"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\mfc80d.dll"      "$INSTDIR\bin\mfc80d.dll"    "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80CHS.DLL"    "$INSTDIR\bin\MFC80CHS.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80CHT.DLL"    "$INSTDIR\bin\MFC80CHT.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80DEU.DLL"    "$INSTDIR\bin\MFC80DEU.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80ENU.DLL"    "$INSTDIR\bin\MFC80ENU.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80ESP.DLL"    "$INSTDIR\bin\MFC80ESP.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80FRA.DLL"    "$INSTDIR\bin\MFC80FRA.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80ITA.DLL"    "$INSTDIR\bin\MFC80ITA.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80JPN.DLL"    "$INSTDIR\bin\MFC80JPN.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80KOR.DLL"    "$INSTDIR\bin\MFC80KOR.DLL"  "$INSTDIR"
!ELSE                                                                   
!IFDEF CL_1310
  !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcr71d.dll"    "$INSTDIR\bin\msvcr71d.dll"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcp71d.dll"    "$INSTDIR\bin\msvcp71d.dll"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\mfc71d.dll"      "$INSTDIR\bin\mfc71d.dll"    "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71CHS.DLL"    "$INSTDIR\bin\MFC71CHS.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71CHT.DLL"    "$INSTDIR\bin\MFC71CHT.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71DEU.DLL"    "$INSTDIR\bin\MFC71DEU.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71ENU.DLL"    "$INSTDIR\bin\MFC71ENU.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71ESP.DLL"    "$INSTDIR\bin\MFC71ESP.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71FRA.DLL"    "$INSTDIR\bin\MFC71FRA.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71ITA.DLL"    "$INSTDIR\bin\MFC71ITA.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71JPN.DLL"    "$INSTDIR\bin\MFC71JPN.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71KOR.DLL"    "$INSTDIR\bin\MFC71KOR.DLL"  "$INSTDIR"
!ELSE                                                                   
!IFDEF CL_1300                                                          
  !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcr70d.dll"    "$INSTDIR\bin\msvcr70d.dll"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcp70d.dll"    "$INSTDIR\bin\msvcp70d.dll"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\mfc70d.dll"      "$INSTDIR\bin\mfc70d.dll"    "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70CHS.DLL"    "$INSTDIR\bin\MFC70CHS.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70CHT.DLL"    "$INSTDIR\bin\MFC70CHT.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70DEU.DLL"    "$INSTDIR\bin\MFC70DEU.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70ENU.DLL"    "$INSTDIR\bin\MFC70ENU.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70ESP.DLL"    "$INSTDIR\bin\MFC70ESP.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70FRA.DLL"    "$INSTDIR\bin\MFC70FRA.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70ITA.DLL"    "$INSTDIR\bin\MFC70ITA.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70JPN.DLL"    "$INSTDIR\bin\MFC70JPN.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70KOR.DLL"    "$INSTDIR\bin\MFC70KOR.DLL"  "$INSTDIR"
!ELSE                                                                   
  !insertmacro ReplaceDLL "${SYSTEMDIR}\mfc42d.dll"      "$INSTDIR\bin\mfc42d.dll"    "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcp60d.dll"    "$INSTDIR\bin\msvcp60d.dll"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcrtd.dll"     "$INSTDIR\bin\msvcrtd.dll"   "$INSTDIR"
!ENDIF                                                                  
!ENDIF                                                                  
!ENDIF
!ELSE                                                                   
!IFDEF CL_1400
  !insertmacro ReplaceDLL "${SYSTEMDIR}\mfc80.dll"       "$INSTDIR\bin\mfc80.dll"     "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcr80.dll"     "$INSTDIR\bin\msvcr80.dll"   "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcp80.dll"     "$INSTDIR\bin\msvcp80.dll"   "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80CHS.DLL"    "$INSTDIR\bin\MFC80CHS.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80CHT.DLL"    "$INSTDIR\bin\MFC80CHT.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80DEU.DLL"    "$INSTDIR\bin\MFC80DEU.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80ENU.DLL"    "$INSTDIR\bin\MFC80ENU.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80ESP.DLL"    "$INSTDIR\bin\MFC80ESP.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80FRA.DLL"    "$INSTDIR\bin\MFC80FRA.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80ITA.DLL"    "$INSTDIR\bin\MFC80ITA.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80JPN.DLL"    "$INSTDIR\bin\MFC80JPN.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80KOR.DLL"    "$INSTDIR\bin\MFC80KOR.DLL"  "$INSTDIR"
!ELSE                                                                   
!IFDEF CL_1310                                                          
  !insertmacro ReplaceDLL "${SYSTEMDIR}\mfc71.dll"       "$INSTDIR\bin\mfc71.dll"     "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcr71.dll"     "$INSTDIR\bin\msvcr71.dll"   "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcp71.dll"     "$INSTDIR\bin\msvcp71.dll"   "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71CHS.DLL"    "$INSTDIR\bin\MFC71CHS.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71CHT.DLL"    "$INSTDIR\bin\MFC71CHT.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71DEU.DLL"    "$INSTDIR\bin\MFC71DEU.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71ENU.DLL"    "$INSTDIR\bin\MFC71ENU.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71ESP.DLL"    "$INSTDIR\bin\MFC71ESP.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71FRA.DLL"    "$INSTDIR\bin\MFC71FRA.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71ITA.DLL"    "$INSTDIR\bin\MFC71ITA.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71JPN.DLL"    "$INSTDIR\bin\MFC71JPN.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71KOR.DLL"    "$INSTDIR\bin\MFC71KOR.DLL"  "$INSTDIR"
!ELSE                                                                   
!IFDEF CL_1300                                                          
  !insertmacro ReplaceDLL "${SYSTEMDIR}\mfc70.dll"       "$INSTDIR\bin\mfc70.dll"     "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcr70.dll"     "$INSTDIR\bin\msvcr70.dll"   "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcp70.dll"     "$INSTDIR\bin\msvcp70.dll"   "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70CHS.DLL"    "$INSTDIR\bin\MFC70CHS.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70CHT.DLL"    "$INSTDIR\bin\MFC70CHT.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70DEU.DLL"    "$INSTDIR\bin\MFC70DEU.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70ENU.DLL"    "$INSTDIR\bin\MFC70ENU.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70ESP.DLL"    "$INSTDIR\bin\MFC70ESP.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70FRA.DLL"    "$INSTDIR\bin\MFC70FRA.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70ITA.DLL"    "$INSTDIR\bin\MFC70ITA.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70JPN.DLL"    "$INSTDIR\bin\MFC70JPN.DLL"  "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70KOR.DLL"    "$INSTDIR\bin\MFC70KOR.DLL"  "$INSTDIR"
!ELSE                                                                   
  !insertmacro ReplaceDLL "${SYSTEMDIR}\mfc42.dll"       "$INSTDIR\bin\mfc42.dll"     "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcp60.dll"     "$INSTDIR\bin\msvcp60.dll"   "$INSTDIR"
  !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcrt.dll"      "$INSTDIR\bin\msvcrt.dll"    "$INSTDIR"
!ENDIF                                                                  
!ENDIF                                                                  
!ENDIF
!ENDIF                                                                  
  !insertmacro ReplaceDLL "${SYSTEMDIR}\psapi.dll"       "$INSTDIR\bin\psapi.dll"     "$INSTDIR"
   
  ; Do WINDOWSDIR components
  ;SetOutPath "$WINDOWSDIR"
!ifdef DEBUG
!endif
  
  ; Do Windows SYSDIR (Control panel)
  SetOutPath "$SYSDIR"
  !insertmacro ReplaceDLL "${KFW_BIN_DIR}\kfwlogon.dll" "$SYSDIR\kfwlogon.dll" "$INSTDIR"
  File "${KFW_BIN_DIR}\kfwcpcc.exe"  

  ; Get Kerberos config files
  Call kfw.GetConfigFiles

  Call KFWCommon.Install
  
  ; KfW Reg entries
  DeleteRegKey HKLM "${KFW_REGKEY_ROOT}\Client\CurrentVersion"
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\Client\CurrentVersion" "VersionString" ${KFW_VERSION}
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\Client\CurrentVersion" "Title" "KfW"
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\Client\CurrentVersion" "Description" "${PROGRAM_NAME}"
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\Client\CurrentVersion" "PathName" "$INSTDIR"
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\Client\CurrentVersion" "Software Type" "Authentication"
  WriteRegDWORD HKLM "${KFW_REGKEY_ROOT}\Client\CurrentVersion" "MajorVersion" ${KFW_MAJORVERSION}
  WriteRegDWORD HKLM "${KFW_REGKEY_ROOT}\Client\CurrentVersion" "MinorVersion" ${KFW_MINORVERSION}
  WriteRegDWORD HKLM "${KFW_REGKEY_ROOT}\Client\CurrentVersion" "PatchLevel" ${KFW_PATCHLEVEL}

  DeleteRegKey HKLM "${KFW_REGKEY_ROOT}\Client\${KFW_VERSION}"
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\Client\${KFW_VERSION}" "VersionString" ${KFW_VERSION}
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\Client\${KFW_VERSION}" "Title" "KfW"
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\Client\${KFW_VERSION}" "Description" "${PROGRAM_NAME}"
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\Client\${KFW_VERSION}" "PathName" "$INSTDIR"
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\Client\${KFW_VERSION}" "Software Type" "Authentication"
  WriteRegDWORD HKLM "${KFW_REGKEY_ROOT}\Client\${KFW_VERSION}" "MajorVersion" ${KFW_MAJORVERSION}
  WriteRegDWORD HKLM "${KFW_REGKEY_ROOT}\Client\${KFW_VERSION}" "MinorVersion" ${KFW_MINORVERSION}
  WriteRegDWORD HKLM "${KFW_REGKEY_ROOT}\Client\${KFW_VERSION}" "PatchLevel" ${KFW_PATCHLEVEL}

  ; Daemon entries
  WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\MIT Kerberos" "" ""
  WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\MIT Kerberos\NetworkProvider" "ProviderPath" "$SYSDIR\kfwlogon.dll"
  WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\MIT Kerberos\NetworkProvider" "AuthentProviderPath" "$SYSDIR\kfwlogon.dll"
  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Services\MIT Kerberos\NetworkProvider" "Class" 2
  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Services\MIT Kerberos\NetworkProvider" "VerboseLogging" 10

  ; Must also add HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\NetworkProvider\HwOrder
  ; and HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\NetworkProvider\Order
  ; to also include the service name.
  Call AddProvider
  ReadINIStr $R0 $1 "Field 7" "State"
  WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\MIT Kerberos\NetworkProvider" "Name" "MIT Kerberos"
  
  ; WinLogon Event Notification
  WriteRegDWORD HKLM "Software\Microsoft\Windows NT\CurrentVersion\WinLogon\Notify\MIT_KFW" "Asynchronous" 0
  WriteRegDWORD HKLM "Software\Microsoft\Windows NT\CurrentVersion\WinLogon\Notify\MIT_KFW" "Impersonate"  0
  WriteRegStr HKLM "Software\Microsoft\Windows NT\CurrentVersion\WinLogon\Notify\MIT_KFW" "DLLName" "kfwlogon.dll"
  WriteRegStr HKLM "Software\Microsoft\Windows NT\CurrentVersion\WinLogon\Notify\MIT_KFW" "Logon" "KFW_Logon_Event"

  ; NetIdMgr Reg entries
  WriteRegStr HKLM "Software\MIT\NetIDMgr\PluginManager\Modules\MITKrb5" "ImagePath" "$INSTDIR\bin\krb5cred.dll"
  WriteRegStr HKLM "Software\MIT\NetIDMgr\PluginManager\Modules\MITKrb5" "PluginList" "Krb5Cred,Krb5Ident"

  WriteRegStr HKLM "Software\MIT\NetIDMgr\PluginManager\Plugins\Krb5Cred" "Module" "MITKrb5"
  WriteRegStr HKLM "Software\MIT\NetIDMgr\PluginManager\Plugins\Krb5Cred" "Description" "Kerberos v5 Credentials Provider"
  WriteRegDWORD HKLM "Software\MIT\NetIDMgr\PluginManager\Plugins\Krb5Cred" "Type"  1
  WriteRegDWORD HKLM "Software\MIT\NetIDMgr\PluginManager\Plugins\Krb5Cred" "Flags" 0
  
  WriteRegStr HKLM "Software\MIT\NetIDMgr\PluginManager\Plugins\Krb5Ident" "Module" "MITKrb5"
  WriteRegStr HKLM "Software\MIT\NetIDMgr\PluginManager\Plugins\Krb5Ident" "Description" "Kerberos v5 Identity Provider"
  WriteRegStr HKLM "Software\MIT\NetIDMgr\PluginManager\Plugins\Krb5Ident" "Dependencies" "Krb5Cred"
  WriteRegDWORD HKLM "Software\MIT\NetIDMgr\PluginManager\Plugins\Krb5Ident" "Type"  2
  WriteRegDWORD HKLM "Software\MIT\NetIDMgr\PluginManager\Plugins\Krb5Ident" "Flags" 0

  WriteRegStr HKLM "Software\MIT\NetIDMgr\PluginManager\Modules\MITKrb4" "ImagePath" "$INSTDIR\bin\krb4cred.dll"
  WriteRegStr HKLM "Software\MIT\NetIDMgr\PluginManager\Modules\MITKrb4" "PluginList" "Krb4Cred"

  WriteRegStr HKLM "Software\MIT\NetIDMgr\PluginManager\Plugins\Krb4Cred" "Module" "MITKrb4"
  WriteRegStr HKLM "Software\MIT\NetIDMgr\PluginManager\Plugins\Krb4Cred" "Description" "Kerberos v4 Credentials Provider"
  WriteRegStr HKLM "Software\MIT\NetIDMgr\PluginManager\Plugins\Krb4Cred" "Dependencies" "Krb5Cred"
  WriteRegDWORD HKLM "Software\MIT\NetIDMgr\PluginManager\Plugins\Krb4Cred" "Type"  1
  WriteRegDWORD HKLM "Software\MIT\NetIDMgr\PluginManager\Plugins\Krb4Cred" "Flags" 0
  
  ;Write start menu entries
  CreateDirectory "$SMPROGRAMS\${PROGRAM_NAME}"
  SetOutPath "$INSTDIR\bin"
  CreateShortCut  "$SMPROGRAMS\${PROGRAM_NAME}\Uninstall ${PROGRAM_NAME}.lnk" "$INSTDIR\Uninstall.exe"

  ReadINIStr $R0 $1 "Field 2" "State"  ; startup

  CreateShortCut  "$SMPROGRAMS\${PROGRAM_NAME}\Network Identity Manager.lnk" "$INSTDIR\bin\netidmgr.exe" "" "$INSTDIR\bin\netidmgr.exe" 

startshort:
  StrCmp $R0 "0" nostart
  CreateShortCut  "$SMSTARTUP\Network Identity Manager.lnk" "$INSTDIR\bin\netidmgr.exe" "" "$INSTDIR\bin\netidmgr.exe" 0 SW_SHOWMINIMIZED
  goto checkconflicts

nostart:
  Delete  "$SMSTARTUP\Network Identity Manager.lnk"

checkconflicts:
  Call GetSystemPath
  Push "krb5_32.dll"
  Call SearchPath
  Pop  $R0
  StrCmp $R0 "" addpath

  Push $R0
  Call GetParent
  Pop $R0
  StrCmp $R0 "$INSTDIR\bin" addpath
  MessageBox MB_OK|MB_ICONINFORMATION|MB_TOPMOST "A previous installation of MIT Kerberos for Windows binaries has been found in folder $R0.  This may interfere with the use of the current installation."

addpath:
  ; Add kfw bin to path
  Push "$INSTDIR\bin"
  Call AddToSystemPath

  Call GetWindowsVersion
  Pop $R0
  StrCmp $R0 "2003" addAllowTgtKey
  StrCmp $R0 "2000" addAllowTgtKey
  StrCmp $R0 "XP"   addAllowTgtKey
  goto skipAllowTgtKey

addAllowTgtKey:
  ReadRegDWORD $R0 HKLM "SYSTEM\CurrentControlSet\Control\Lsa\Kerberos\Parameters" "AllowTGTSessionKey" 
  WriteRegDWORD HKLM "${KFW_REGKEY_ROOT}\Client\${KFW_VERSION}" "AllowTGTSessionKeyBackup" $R0
  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Control\Lsa\Kerberos\Parameters" "AllowTGTSessionKey" "1"
  ReadRegDWORD $R0 HKLM "SYSTEM\CurrentControlSet\Control\Lsa\Kerberos" "AllowTGTSessionKey" 
  WriteRegDWORD HKLM "${KFW_REGKEY_ROOT}\Client\${KFW_VERSION}" "AllowTGTSessionKeyBackup2" $R0
  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Control\Lsa\Kerberos" "AllowTGTSessionKey" "1"
skipAllowTgtKey:  

  ; The following are keys added for Terminal Server compatibility
  ; http://support.microsoft.com/default.aspx?scid=kb;EN-US;186499
  WriteRegDWORD HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\netidmgr" "Flags" 0x408
  WriteRegDWORD HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\kinit" "Flags" 0x408
  WriteRegDWORD HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\klist" "Flags" 0x408
  WriteRegDWORD HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\kdestroy" "Flags" 0x408
  WriteRegDWORD HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\gss" "Flags" 0x408
  WriteRegDWORD HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\gss-client" "Flags" 0x408
  WriteRegDWORD HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\gss-server" "Flags" 0x408
  WriteRegDWORD HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\k524init" "Flags" 0x408
  WriteRegDWORD HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\kpasswd" "Flags" 0x408
  WriteRegDWORD HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\kvno" "Flags" 0x408
  WriteRegDWORD HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\ms2mit" "Flags" 0x408
  WriteRegDWORD HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\mit2ms" "Flags" 0x408
  WriteRegDWORD HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\mit2ms" "Flags" 0x408
  WriteRegDWORD HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\kcpytkt" "Flags" 0x408
  WriteRegDWORD HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\kdeltkt" "Flags" 0x408
  WriteRegDWORD HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\k95" "Flags" 0x408
  WriteRegDWORD HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\k95g" "Flags" 0x408

SectionEnd

Section "Debug Symbols" secDebug

  SetOutPath "$INSTDIR\bin"
  File "${KFW_BIN_DIR}\comerr32.pdb"
  File "${KFW_BIN_DIR}\gss.pdb"
  File "${KFW_BIN_DIR}\gss-client.pdb"
  File "${KFW_BIN_DIR}\gss-server.pdb"
  File "${KFW_BIN_DIR}\gssapi32.pdb"
  File "${KFW_BIN_DIR}\k524init.pdb"
  File "${KFW_BIN_DIR}\kclnt32.pdb"
  File "${KFW_BIN_DIR}\kdestroy.pdb"
  File "${KFW_BIN_DIR}\kinit.pdb"
  File "${KFW_BIN_DIR}\klist.pdb"
  File "${KFW_BIN_DIR}\kpasswd.pdb"
  File "${KFW_BIN_DIR}\kvno.pdb"
  File "${KFW_BIN_DIR}\krb5_32.pdb"
  File "${KFW_BIN_DIR}\k5sprt32.pdb"
  File "${KFW_BIN_DIR}\krb524.pdb"
  File "${KFW_BIN_DIR}\krbcc32.pdb"
  File "${KFW_BIN_DIR}\krbcc32s.pdb"
  File "${KFW_BIN_DIR}\krbv4w32.pdb"
  File "${KFW_BIN_DIR}\leashw32.pdb"
  File "${KFW_BIN_DIR}\krb4cred.pdb"
  File "${KFW_BIN_DIR}\krb5cred.pdb"
  File "${KFW_BIN_DIR}\ms2mit.pdb"
  File "${KFW_BIN_DIR}\mit2ms.pdb"
  File "${KFW_BIN_DIR}\kcpytkt.pdb"
  File "${KFW_BIN_DIR}\kdeltkt.pdb"
  File "${KFW_BIN_DIR}\wshelp32.pdb"
  File "${KFW_BIN_DIR}\xpprof32.pdb"

  Call GetWindowsVersion
  Pop $R0
  StrCmp $R0 "2000" nidpdb_inst2000
  File "${KFW_BIN_DIR}\netidmgr.pdb"
  File "${KFW_BIN_DIR}\nidmgr32.pdb"
  goto nidpdb_done
nidpdb_inst2000:
  File "${KFW_BIN_DIR}\W2K\netidmgr.pdb"
  File "${KFW_BIN_DIR}\W2K\nidmgr32.pdb"
nidpdb_done:

!IFDEF DEBUG
!IFDEF CL_1400
  File "${SYSTEMDIR}\msvcr80d.pdb"                                           
  File "${SYSTEMDIR}\msvcp80d.pdb"                                           
  File "${SYSTEMDIR}\mfc80d.pdb"                                             
!ELSE                                                                   
!IFDEF CL_1310
  File "${SYSTEMDIR}\msvcr71d.pdb"                                           
  File "${SYSTEMDIR}\msvcp71d.pdb"                                           
  File "${SYSTEMDIR}\mfc71d.pdb"                                             
!ELSE                                                                   
!IFDEF CL_1300                                                          
  File "${SYSTEMDIR}\msvcr70d.pdb"                                           
  File "${SYSTEMDIR}\msvcp70d.pdb"                                           
  File "${SYSTEMDIR}\mfc70d.pdb"                                             
!ELSE                                                                   
  File "${SYSTEMDIR}\mfc42d.pdb"                                             
  File "${SYSTEMDIR}\msvcp60d.pdb"                                           
  File "${SYSTEMDIR}\msvcrtd.pdb"                                            
!ENDIF                                                                  
!ENDIF                                                                  
!ENDIF
!ENDIF

  SetOutPath "$SYSDIR"
  File "${KFW_BIN_DIR}\kfwlogon.pdb"
  File "${KFW_BIN_DIR}\kfwcpcc.pdb"

SectionEnd

;----------------------
; Kerberos for Windows SDK
Section "KfW SDK" secSDK

  RMDir /r "$INSTDIR\inc"
  RMDir /r "$INSTDIR\lib"
  RMDir /r "$INSTDIR\install"
  RMDir /r "$INSTDIR\sample"

  SetOutPath "$INSTDIR\doc"
  File /r "${KFW_DOC_DIR}\netiddev.chm"

  SetOutPath "$INSTDIR\inc\kclient"
  File /r "${KFW_INC_DIR}\kclient\*"  

  SetOutPath "$INSTDIR\inc\krb4"
  File /r "${KFW_INC_DIR}\krb4\*"  

  SetOutPath "$INSTDIR\inc\krb5"
  File /r "${KFW_INC_DIR}\krb5\*"  

  SetOutPath "$INSTDIR\inc\krbcc"
  File /r "${KFW_INC_DIR}\krbcc\*"  

  SetOutPath "$INSTDIR\inc\leash"
  File /r "${KFW_INC_DIR}\leash\*"  

  SetOutPath "$INSTDIR\inc\loadfuncs"
  File /r "${KFW_INC_DIR}\loadfuncs\*"  

  SetOutPath "$INSTDIR\inc\netidmgr"
  File /r "${KFW_INC_DIR}\netidmgr\*"  

  SetOutPath "$INSTDIR\inc\wshelper"
  File /r "${KFW_INC_DIR}\wshelper\*"  

  SetOutPath "$INSTDIR\lib\i386"
  File /r "${KFW_LIB_DIR}\*"

  SetOutPath "$INSTDIR\install"
  File /r "${KFW_INSTALL_DIR}\*"

  SetOutPath "$INSTDIR\sample"
  File /r "${KFW_SAMPLE_DIR}\*"

  CreateShortCut  "$SMPROGRAMS\${PROGRAM_NAME}\Network Identity Developer Documentation.lnk" "$INSTDIR\bin\netiddev.chm" 

  Call KFWCommon.Install
  
  ; KfW Reg entries
  DeleteRegKey HKLM "${KFW_REGKEY_ROOT}\SDK\CurrentVersion"
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\SDK\CurrentVersion" "VersionString" ${KFW_VERSION}
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\SDK\CurrentVersion" "Title" "KfW"
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\SDK\CurrentVersion" "Description" "${PROGRAM_NAME}"
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\SDK\CurrentVersion" "PathName" "$INSTDIR"
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\SDK\CurrentVersion" "Software Type" "Authentication"
  WriteRegDWORD HKLM "${KFW_REGKEY_ROOT}\SDK\CurrentVersion" "MajorVersion" ${KFW_MAJORVERSION}
  WriteRegDWORD HKLM "${KFW_REGKEY_ROOT}\SDK\CurrentVersion" "MinorVersion" ${KFW_MINORVERSION}
  WriteRegDWORD HKLM "${KFW_REGKEY_ROOT}\SDK\CurrentVersion" "PatchLevel" ${KFW_PATCHLEVEL}

  DeleteRegKey HKLM "${KFW_REGKEY_ROOT}\SDK\${KFW_VERSION}"
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\SDK\${KFW_VERSION}" "VersionString" ${KFW_VERSION}
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\SDK\${KFW_VERSION}" "Title" "KfW"
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\SDK\${KFW_VERSION}" "Description" "${PROGRAM_NAME}"
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\SDK\${KFW_VERSION}" "PathName" "$INSTDIR"
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\SDK\${KFW_VERSION}" "Software Type" "Authentication"
  WriteRegDWORD HKLM "${KFW_REGKEY_ROOT}\SDK\${KFW_VERSION}" "MajorVersion" ${KFW_MAJORVERSION}
  WriteRegDWORD HKLM "${KFW_REGKEY_ROOT}\SDK\${KFW_VERSION}" "MinorVersion" ${KFW_MINORVERSION}
  WriteRegDWORD HKLM "${KFW_REGKEY_ROOT}\SDK\${KFW_VERSION}" "PatchLevel" ${KFW_PATCHLEVEL}
  WriteRegDWORD HKLM "${KFW_REGKEY_ROOT}\SDK\${KFW_VERSION}" "PatchLevel" ${KFW_PATCHLEVEL}

SectionEnd

;----------------------
; Kerberos for Windows Documentation
Section "KfW Documentation" secDocs

  RMDir /r "$INSTDIR\doc"

  SetOutPath "$INSTDIR\doc"
  File "${KFW_DOC_DIR}\relnotes.html"
  File "${KFW_DOC_DIR}\netidmgr_userdoc.pdf"
   
  Call KFWCommon.Install
  
  ; KfW Reg entries
  DeleteRegKey HKLM "${KFW_REGKEY_ROOT}\Documentation\CurrentVersion"
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\Documentation\CurrentVersion" "VersionString" ${KFW_VERSION}
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\Documentation\CurrentVersion" "Title" "KfW"
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\Documentation\CurrentVersion" "Description" "${PROGRAM_NAME}"
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\Documentation\CurrentVersion" "PathName" "$INSTDIR"
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\Documentation\CurrentVersion" "Software Type" "Authentication"
  WriteRegDWORD HKLM "${KFW_REGKEY_ROOT}\Documentation\CurrentVersion" "MajorVersion" ${KFW_MAJORVERSION}
  WriteRegDWORD HKLM "${KFW_REGKEY_ROOT}\Documentation\CurrentVersion" "MinorVersion" ${KFW_MINORVERSION}
  WriteRegDWORD HKLM "${KFW_REGKEY_ROOT}\Documentation\CurrentVersion" "PatchLevel" ${KFW_PATCHLEVEL}

  DeleteRegKey HKLM "${KFW_REGKEY_ROOT}\Documentation\${KFW_VERSION}"
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\Documentation\${KFW_VERSION}" "VersionString" ${KFW_VERSION}
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\Documentation\${KFW_VERSION}" "Title" "KfW"
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\Documentation\${KFW_VERSION}" "Description" "${PROGRAM_NAME}"
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\Documentation\${KFW_VERSION}" "PathName" "$INSTDIR"
  WriteRegStr HKLM "${KFW_REGKEY_ROOT}\Documentation\${KFW_VERSION}" "Software Type" "Authentication"
  WriteRegDWORD HKLM "${KFW_REGKEY_ROOT}\Documentation\${KFW_VERSION}" "MajorVersion" ${KFW_MAJORVERSION}
  WriteRegDWORD HKLM "${KFW_REGKEY_ROOT}\Documentation\${KFW_VERSION}" "MinorVersion" ${KFW_MINORVERSION}
  WriteRegDWORD HKLM "${KFW_REGKEY_ROOT}\Documentation\${KFW_VERSION}" "PatchLevel" ${KFW_PATCHLEVEL}
  WriteRegDWORD HKLM "${KFW_REGKEY_ROOT}\Documentation\${KFW_VERSION}" "PatchLevel" ${KFW_PATCHLEVEL}
  
  ;Write start menu entries
  CreateDirectory "$SMPROGRAMS\${PROGRAM_NAME}"
  SetOutPath "$INSTDIR\doc"
  CreateShortCut  "$SMPROGRAMS\${PROGRAM_NAME}\Release Notes.lnk" "$INSTDIR\doc\relnotes.html" 
  CreateShortCut  "$SMPROGRAMS\${PROGRAM_NAME}\Network Identity Manager User Documentation.lnk" "$INSTDIR\doc\netidmgr_userdoc.pdf" 
  CreateShortCut  "$SMPROGRAMS\${PROGRAM_NAME}\Network Identity Manager Documentation.lnk" "$INSTDIR\bin\netidmgr.chm" 
SectionEnd

;Display the Finish header
;Insert this macro after the sections if you are not using a finish page
;!insertmacro MUI_SECTIONS_FINISHHEADER

;--------------------------------
;Installer Functions

Function .onInit
  !insertmacro MUI_LANGDLL_DISPLAY
  
  ; Set the default install options
  Push $0

   Call IsUserAdmin
   Pop $R0
   StrCmp $R0 "true" checkVer

   MessageBox MB_OK|MB_ICONSTOP|MB_TOPMOST "You must be an administrator of this machine to install this software."
   Abort
   
checkVer:
  ; Check Version of Windows.   Do not install onto Windows 95
   Call GetWindowsVersion
   Pop $R0
   StrCmp $R0 "95" wrongVersion
   StrCmp $R0 "98" wrongVersion
   StrCmp $R0 "ME" wrongVersion
   StrCmp $R0 "NT 4.0" wrongVersion
   goto checkIPHLPAPI

wrongVersion:
   MessageBox MB_OK|MB_ICONSTOP|MB_TOPMOST "MIT ${PROGRAM_NAME} requires Microsoft Windows 2000 or higher."
   Abort

checkIPHLPAPI:
   ClearErrors
   ReadEnvStr $R0 "WinDir"
   GetDLLVersion "$R0\System32\iphlpapi.dll" $R1 $R2
   IfErrors +1 +3 
   GetDLLVersion "$R0\System\iphlpapi.dll" $R1 $R2
   IfErrors iphlperror
   IntOp $R3 $R2 / 0x00010000
   IntCmpU $R3 1952 iphlpwarning checkprevious checkprevious

iphlperror:
   MessageBox MB_OK|MB_ICONSTOP|MB_TOPMOST "MIT ${PROGRAM_NAME} requires Internet Explorer version 5.01 or higher. IPHLPAPI.DLL is missing."
   Abort

iphlpwarning:
   MessageBox MB_OK|MB_ICONINFORMATION|MB_TOPMOST "IPHLPAPI.DLL must be upgraded.  Please install Internet Explorer 5.01 or higher."

checkprevious:
  ClearErrors
  ReadRegStr $R0 HKLM \
  "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PROGRAM_NAME}" \
  "DisplayVersion"
  IfErrors testWIX
  StrCmp $R0 "${KFW_VERSION}" contInstall

  MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION \
  "${PROGRAM_NAME} is already installed. $\n$\nClick `OK` to remove the \
  previous version or `Cancel` to cancel this upgrade or downgrade." \
  IDOK uninstNSIS
  Abort
  
;Run the uninstaller
uninstNSIS:
  ReadRegStr $R0 HKLM \
  "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PROGRAM_NAME}" \
  "UninstallString"
  ClearErrors
  ExecWait '$R0 _?=$INSTDIR' ;Do not copy the uninstaller to a temp file

  IfErrors no_remove_uninstaller
    ;You can either use Delete /REBOOTOK in the uninstaller or add some code
    ;here to remove the uninstaller. Use a registry key to check
    ;whether the user has chosen to uninstall. If you are using an uninstaller
    ;components page, make sure all sections are uninstalled.

  Push $R1
  Call RestartRequired
  Pop $R1
  StrCmp $R1 "1" Restart DoNotRestart

testWIX:
  ClearErrors
  ReadRegStr $R0 HKLM \
  "Software\Microsoft\Windows\CurrentVersion\Uninstall\{FD5B1F41-81BB-4BBC-9F7E-4B971660AE1A}" \
  "DisplayVersion"
  IfErrors testSWRT

  MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION \
  "${PROGRAM_NAME} is already installed. $\n$\nClick `OK` to remove the \
  previous version or `Cancel` to cancel this installation." \
  IDOK uninstMSI1
  Abort
  
;Run the uninstaller
uninstMSI1:
  Call GetWindowsVersion
  Pop $R0
  StrCmp $R0 "2000" uninstMSI1_2000

  ClearErrors
  ExecWait 'MSIEXEC /x{FD5B1F41-81BB-4BBC-9F7E-4B971660AE1A} /passive /promptrestart'

  IfErrors no_remove_uninstaller
    ;You can either use Delete /REBOOTOK in the uninstaller or add some code
    ;here to remove the uninstaller. Use a registry key to check
    ;whether the user has chosen to uninstall. If you are using an uninstaller
    ;components page, make sure all sections are uninstalled.

  Push $R1
  Call RestartRequired
  Pop $R1
  StrCmp $R1 "1" Restart DoNotRestart
  
uninstMSI1_2000:
  ClearErrors
  ExecWait 'MSIEXEC /x{FD5B1F41-81BB-4BBC-9F7E-4B971660AE1A}'

  IfErrors no_remove_uninstaller
    ;You can either use Delete /REBOOTOK in the uninstaller or add some code
    ;here to remove the uninstaller. Use a registry key to check
    ;whether the user has chosen to uninstall. If you are using an uninstaller
    ;components page, make sure all sections are uninstalled.

  Push $R1
  Call RestartRequired
  Pop $R1
  StrCmp $R1 "1" Restart DoNotRestart
  
testSWRT:
  ClearErrors
  ReadRegStr $R0 HKLM \
  "Software\Microsoft\Windows\CurrentVersion\Uninstall\{61211594-AAA1-4A98-A299-757326763CC7}" \
  "DisplayVersion"
  IfErrors testPismere

  MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION \
  "${PROGRAM_NAME} is already installed. $\n$\nClick `OK` to remove the \
  previous version or `Cancel` to cancel this installation." \
  IDOK uninstMSI2
  Abort
  
;Run the uninstaller
uninstMSI2:
  Call GetWindowsVersion
  Pop $R0
  StrCmp $R0 "2000" uninstMSI2_2000

  ClearErrors
  ExecWait 'MSIEXEC /x{61211594-AAA1-4A98-A299-757326763CC7} /passive /promptrestart'

  IfErrors no_remove_uninstaller
    ;You can either use Delete /REBOOTOK in the uninstaller or add some code
    ;here to remove the uninstaller. Use a registry key to check
    ;whether the user has chosen to uninstall. If you are using an uninstaller
    ;components page, make sure all sections are uninstalled.

  Push $R1
  Call RestartRequired
  Pop $R1
  StrCmp $R1 "1" Restart DoNotRestart

uninstMSI2_2000:
  ClearErrors
  ExecWait 'MSIEXEC /x{61211594-AAA1-4A98-A299-757326763CC7}'

  IfErrors no_remove_uninstaller
    ;You can either use Delete /REBOOTOK in the uninstaller or add some code
    ;here to remove the uninstaller. Use a registry key to check
    ;whether the user has chosen to uninstall. If you are using an uninstaller
    ;components page, make sure all sections are uninstalled.

  Push $R1
  Call RestartRequired
  Pop $R1
  StrCmp $R1 "1" Restart DoNotRestart

testPismere:
  ClearErrors
  ReadRegStr $R0 HKLM \
  "Software\Microsoft\Windows\CurrentVersion\Uninstall\{83977767-388D-4DF8-BB08-3BF2401635BD}" \
  "DisplayVersion"
  IfErrors contInstall

  MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION \
  "${PROGRAM_NAME} is already installed. $\n$\nClick `OK` to remove the \
  previous version or `Cancel` to cancel this installation." \
  IDOK uninstPismere
  Abort
  
;Run the uninstaller
uninstPismere:
  Call GetWindowsVersion
  Pop $R0
  StrCmp $R0 "2000" uninstPismere_2000

  ClearErrors
  ExecWait 'MSIEXEC /x{83977767-388D-4DF8-BB08-3BF2401635BD} /passive /promptrestart'

  IfErrors no_remove_uninstaller
    ;You can either use Delete /REBOOTOK in the uninstaller or add some code
    ;here to remove the uninstaller. Use a registry key to check
    ;whether the user has chosen to uninstall. If you are using an uninstaller
    ;components page, make sure all sections are uninstalled.

  Push $R1
  Call RestartRequired
  Pop $R1
  StrCmp $R1 "1" Restart DoNotRestart

uninstPismere_2000:
  ClearErrors
  ExecWait 'MSIEXEC /x{83977767-388D-4DF8-BB08-3BF2401635BD}'

  IfErrors no_remove_uninstaller
    ;You can either use Delete /REBOOTOK in the uninstaller or add some code
    ;here to remove the uninstaller. Use a registry key to check
    ;whether the user has chosen to uninstall. If you are using an uninstaller
    ;components page, make sure all sections are uninstalled.

  Push $R1
  Call RestartRequired
  Pop $R1
  StrCmp $R1 "1" Restart DoNotRestart


Restart:
   MessageBox MB_OK|MB_ICONSTOP|MB_TOPMOST "Please reboot and then restart the installer."
   Abort
   MessageBox MB_OK|MB_ICONSTOP|MB_TOPMOST "Abort failed"
 
DoNotRestart:
no_remove_uninstaller:

contInstall:
   ; Never install debug symbols unless explicitly selected, except in DEBUG mode
!IFNDEF DEBUG
   SectionGetFlags ${secDebug} $0
   IntOp $0 $0 & ${SECTION_OFF}
   SectionSetFlags ${secDebug} $0
!ELSE
   SectionGetFlags ${secDebug} $0
   IntOp $0 $0 | ${SF_SELECTED}
   SectionSetFlags ${secDebug} $0
!ENDIF

   ; Our logic should be like this.
   ;     1) If no KfW components are installed, we do a clean install with default options. (Client/Docs)
   ;     2) If existing modules are installed, we keep them selected
   ;     3) If it is an upgrade, we set the text accordingly, else we mark it as a re-install
   ;  TODO: Downgrade?
   Call IsAnyKfWInstalled
   Pop $R0
   StrCmp $R0 "0" DefaultOptions
   
   Call ShouldClientInstall
   Pop $R2
   
   StrCmp $R2 "0" NoClient
   StrCmp $R2 "1" ReinstallClient
   StrCmp $R2 "2" UpgradeClient
   StrCmp $R2 "3" DowngradeClient
   
	SectionGetFlags ${secClient} $0
	IntOp $0 $0 | ${SF_SELECTED}
	SectionSetFlags ${secClient} $0
    ;# !insertmacro SelectSection ${secClient}
   goto skipClient
NoClient:
	;StrCpy $1 ${secClient} ; Gotta remember which section we are at now...
	SectionGetFlags ${secClient} $0
	IntOp $0 $0 & ${SECTION_OFF}
	SectionSetFlags ${secClient} $0
   goto skipClient
UpgradeClient:
	SectionGetFlags ${secClient} $0
	IntOp $0 $0 | ${SF_SELECTED}
	SectionSetFlags ${secClient} $0
   SectionSetText ${secClient} $(UPGRADE_CLIENT)
   goto skipClient
ReinstallClient:
	SectionGetFlags ${secClient} $0
	IntOp $0 $0 | ${SF_SELECTED}
	SectionSetFlags ${secClient} $0
   SectionSetText ${secClient} $(REINSTALL_CLIENT)
   goto skipClient
DowngradeClient:
	SectionGetFlags ${secClient} $0
	IntOp $0 $0 | ${SF_SELECTED}
	SectionSetFlags ${secClient} $0
   SectionSetText ${secClient} $(DOWNGRADE_CLIENT)
   goto skipClient

   
skipClient:   
   
   Call ShouldSDKInstall
   Pop $R2
   StrCmp $R2 "0" NoSDK
   StrCmp $R2 "1" ReinstallSDK
   StrCmp $R2 "2" UpgradeSDK
   StrCmp $R2 "3" DowngradeSDK
   
	SectionGetFlags ${secSDK} $0
	IntOp $0 $0 | ${SF_SELECTED}
	SectionSetFlags ${secSDK} $0
	;# !insertmacro UnselectSection ${secSDK}
   goto skipSDK

UpgradeSDK:
   SectionGetFlags ${secSDK} $0
   IntOp $0 $0 | ${SF_SELECTED}
   SectionSetFlags ${secSDK} $0
   SectionSetText ${secSDK} $(UPGRADE_SDK)
   goto skipSDK

ReinstallSDK:
   SectionGetFlags ${secSDK} $0
   IntOp $0 $0 | ${SF_SELECTED}
   SectionSetFlags ${secSDK} $0
   SectionSetText ${secSDK} $(REINSTALL_SDK)
   goto skipSDK

DowngradeSDK:
   SectionGetFlags ${secSDK} $0
   IntOp $0 $0 | ${SF_SELECTED}
   SectionSetFlags ${secSDK} $0
   SectionSetText ${secSDK} $(DOWNGRADE_SDK)
   goto skipSDK
   
NoSDK:
	SectionGetFlags ${secSDK} $0
	IntOp $0 $0 & ${SECTION_OFF}
	SectionSetFlags ${secSDK} $0
	;# !insertmacro UnselectSection ${secSDK}
   goto skipSDK
   
skipSDK:

   Call ShouldDocumentationInstall
   Pop $R2
   StrCmp $R2 "0" NoDocumentation
   StrCmp $R2 "1" ReinstallDocumentation
   StrCmp $R2 "2" UpgradeDocumentation
   StrCmp $R2 "3" DowngradeDocumentation
   
	SectionGetFlags ${secDocs} $0
	IntOp $0 $0 | ${SF_SELECTED}
	SectionSetFlags ${secDocs} $0
	;# !insertmacro UnselectSection ${secDocs}
   goto skipDocumentation

UpgradeDocumentation:
   SectionGetFlags ${secDocs} $0
   IntOp $0 $0 | ${SF_SELECTED}
   SectionSetFlags ${secDocs} $0
   SectionSetText ${secDocs} $(UPGRADE_DOCS)
   goto skipDocumentation

ReinstallDocumentation:
   SectionGetFlags ${secDocs} $0
   IntOp $0 $0 | ${SF_SELECTED}
   SectionSetFlags ${secDocs} $0
   SectionSetText ${secDocs} $(REINSTALL_DOCS)
   goto skipDocumentation

DowngradeDocumentation:
   SectionGetFlags ${secDocs} $0
   IntOp $0 $0 | ${SF_SELECTED}
   SectionSetFlags ${secDocs} $0
   SectionSetText ${secDocs} $(DOWNGRADE_DOCS)
   goto skipDocumentation
   
NoDocumentation:
	SectionGetFlags ${secDocs} $0
	IntOp $0 $0 & ${SECTION_OFF}
	SectionSetFlags ${secDocs} $0
	;# !insertmacro UnselectSection ${secDocs}
   goto skipDocumentation
   
skipDocumentation:
   goto end
   
DefaultOptions:
   ; Client Selected
	SectionGetFlags ${secClient} $0
	IntOp $0 $0 | ${SF_SELECTED}
	SectionSetFlags ${secClient} $0

   ; SDK NOT selected
	SectionGetFlags ${secSDK} $0
	IntOp $0 $0 & ${SECTION_OFF}
	SectionSetFlags ${secSDK} $0
   
   ; Documentation selected
	SectionGetFlags ${secDocs} $0
	IntOp $0 $0 | ${SF_SELECTED}
	SectionSetFlags ${secDocs} $0
   goto end

end:
	Pop $0
  
   Push $R0
  
  ; See if we can set a default installation path...
  ReadRegStr $R0 HKLM "${KFW_REGKEY_ROOT}\Client\CurrentVersion" "PathName"
  StrCmp $R0 "" TrySDK
  StrCpy $INSTDIR $R0
  goto Nope
  
TrySDK:
  ReadRegStr $R0 HKLM "${KFW_REGKEY_ROOT}\SDK\CurrentVersion" "PathName"
  StrCmp $R0 "" TryDocs
  StrCpy $INSTDIR $R0
  goto Nope

TryDocs:
  ReadRegStr $R0 HKLM "${KFW_REGKEY_ROOT}\Documentation\CurrentVersion" "PathName"
  StrCmp $R0 "" TryRoot
  StrCpy $INSTDIR $R0
  goto Nope

TryRoot:
  ReadRegStr $R0 HKLM "${KFW_REGKEY_ROOT}" "InstallDir"
  StrCmp $R0 "" Nope
  StrCpy $INSTDIR $R0
  
Nope:
  Pop $R0
  
  GetTempFilename $0
  File /oname=$0 KfWConfigPage.ini
  GetTempFilename $1
  File /oname=$1 KfWConfigPage2.ini
  
FunctionEnd


;--------------------------------
; These are our cleanup functions
Function .onInstFailed
Delete $0
Delete $1
FunctionEnd

Function .onInstSuccess
Delete $0
Delete $1
FunctionEnd


;--------------------------------
;Descriptions

  !insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${secClient} $(DESC_secClient)
  !insertmacro MUI_DESCRIPTION_TEXT ${secSDK} $(DESC_secSDK)
  !insertmacro MUI_DESCRIPTION_TEXT ${secDocs} $(DESC_secDocs)
  !insertmacro MUI_DESCRIPTION_TEXT ${secDebug} $(DESC_secDebug)
  !insertmacro MUI_FUNCTION_DESCRIPTION_END
 
;--------------------------------
;Uninstaller Section

Section "Uninstall"
  ; Make sure the user REALLY wants to do this, unless they did a silent uninstall, in which case...let them!
  IfSilent StartRemove     ; New in v2.0b4
  MessageBox MB_YESNO "Are you sure you want to remove MIT ${PROGRAM_NAME} from this machine?" IDYES StartRemove
  abort
  
StartRemove:
  
  SetShellVarContext all
  ; Stop the running processes
  GetTempFileName $R0
  File /oname=$R0 "Killer.exe"
  nsExec::Exec '$R0 netidmgr.exe'
  nsExec::Exec '$R0 krbcc32s.exe'

  Push "$INSTDIR\bin"
  Call un.RemoveFromSystemPath
  
  ; Delete documentation
  Delete "$INSTDIR\doc\relnotes.html"
  Delete "$INSTDIR\doc\netidmgr_userdoc.pdf"
  Delete "$INSTDIR\doc\netiddev.chm"
 
   Delete /REBOOTOK "$INSTDIR\bin\comerr32.dll"
   Delete /REBOOTOK "$INSTDIR\bin\gss.exe"
   Delete /REBOOTOK "$INSTDIR\bin\gss-client.exe"
   Delete /REBOOTOK "$INSTDIR\bin\gss-server.exe"
   Delete /REBOOTOK "$INSTDIR\bin\gssapi32.dll"
   Delete /REBOOTOK "$INSTDIR\bin\k524init.exe"
   Delete /REBOOTOK "$INSTDIR\bin\kclnt32.dll"
   Delete /REBOOTOK "$INSTDIR\bin\kdestroy.exe"
   Delete /REBOOTOK "$INSTDIR\bin\kinit.exe"
   Delete /REBOOTOK "$INSTDIR\bin\klist.exe"   
   Delete /REBOOTOK "$INSTDIR\bin\kpasswd.exe"   
   Delete /REBOOTOK "$INSTDIR\bin\kvno.exe"   
   Delete /REBOOTOK "$INSTDIR\bin\krb5_32.dll" 
   Delete /REBOOTOK "$INSTDIR\bin\k5sprt32.dll" 
   Delete /REBOOTOK "$INSTDIR\bin\krb524.dll"  
   Delete /REBOOTOK "$INSTDIR\bin\krbcc32.dll" 
   Delete /REBOOTOK "$INSTDIR\bin\krbcc32s.exe"
   Delete /REBOOTOK "$INSTDIR\bin\krbv4w32.dll"
   Delete /REBOOTOK "$INSTDIR\bin\netidmgr.exe"      
   Delete /REBOOTOK "$INSTDIR\bin\netidmgr.chm"      
   Delete /REBOOTOK "$INSTDIR\bin\nidmgr32.dll"      
   Delete /REBOOTOK "$INSTDIR\bin\krb4cred.dll"      
   Delete /REBOOTOK "$INSTDIR\bin\krb5cred.dll"      
   Delete /REBOOTOK "$INSTDIR\bin\krb4cred_en_us.dll"
   Delete /REBOOTOK "$INSTDIR\bin\krb5cred_en_us.dll"
   Delete /REBOOTOK "$INSTDIR\bin\leashw32.dll"
   Delete /REBOOTOK "$INSTDIR\bin\ms2mit.exe"  
   Delete /REBOOTOK "$INSTDIR\bin\mit2ms.exe"  
   Delete /REBOOTOK "$INSTDIR\bin\kcpytkt.exe"  
   Delete /REBOOTOK "$INSTDIR\bin\kdeltkt.exe"  
   Delete /REBOOTOK "$INSTDIR\bin\wshelp32.dll"
   Delete /REBOOTOK "$INSTDIR\bin\xpprof32.dll"
   Delete /REBOOTOK "$SYSDIR\bin\kfwlogon.dll"
   Delete /REBOOTOK "$SYSDIR\bin\kfwcpcc.exe"

   Delete /REBOOTOK "$INSTDIR\bin\comerr32.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\gss.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\gss-client.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\gss-server.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\gssapi32.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\k524init.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\kclnt32.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\kdestroy.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\kinit.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\klist.pdb"   
   Delete /REBOOTOK "$INSTDIR\bin\kpasswd.pdb"   
   Delete /REBOOTOK "$INSTDIR\bin\kvno.pdb"   
   Delete /REBOOTOK "$INSTDIR\bin\krb5_32.pdb" 
   Delete /REBOOTOK "$INSTDIR\bin\k5sprt32.pdb" 
   Delete /REBOOTOK "$INSTDIR\bin\krb524.pdb"  
   Delete /REBOOTOK "$INSTDIR\bin\krbcc32.pdb" 
   Delete /REBOOTOK "$INSTDIR\bin\krbcc32s.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\krbv4w32.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\netidmgr.pdb"      
   Delete /REBOOTOK "$INSTDIR\bin\nidmgr32.pdb"      
   Delete /REBOOTOK "$INSTDIR\bin\krb4cred.pdb"      
   Delete /REBOOTOK "$INSTDIR\bin\krb5cred.pdb"      
   Delete /REBOOTOK "$INSTDIR\bin\leashw32.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\ms2mit.pdb"  
   Delete /REBOOTOK "$INSTDIR\bin\mit2ms.pdb"  
   Delete /REBOOTOK "$INSTDIR\bin\kcpytkt.pdb"  
   Delete /REBOOTOK "$INSTDIR\bin\kdeltkt.pdb"  
   Delete /REBOOTOK "$INSTDIR\bin\wshelp32.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\xpprof32.pdb"
   Delete /REBOOTOK "$SYSDIR\bin\kfwlogon.pdb"
   Delete /REBOOTOK "$SYSDIR\bin\kfwcpcc.pdb"

!IFDEF DEBUG
!IFDEF CL_1400
   Delete /REBOOTOK "$INSTDIR\bin\msvcr80d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcr80d.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp80d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp80d.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\mfc80d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\mfc80d.pdb"
!ELSE
!IFDEF CL_1310
   Delete /REBOOTOK "$INSTDIR\bin\msvcr71d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcr71d.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp71d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp71d.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\mfc71d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\mfc71d.pdb"
!ELSE
!IFDEF CL_1300
   Delete /REBOOTOK "$INSTDIR\bin\msvcr70d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcr70d.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp70d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp70d.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\mfc70d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\mfc70d.pdb"
!ELSE
   Delete /REBOOTOK "$INSTDIR\bin\mfc42d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\mfc42d.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp60d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp60d.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\msvcrtd.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcrtd.pdb"
!ENDIF
!ENDIF
!ENDIF
!ELSE
!IFDEF CL_1400
   Delete /REBOOTOK "$INSTDIR\bin\mfc80.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcr80.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp80.dll"
   Delete /REBOOTOK "$INSTDIR\bin\MFC80CHS.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC80CHT.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC80DEU.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC80ENU.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC80ESP.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC80FRA.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC80ITA.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC80JPN.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC80KOR.DLL"
!ELSE
!IFDEF CL_1310
   Delete /REBOOTOK "$INSTDIR\bin\mfc71.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcr71.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp71.dll"
   Delete /REBOOTOK "$INSTDIR\bin\MFC71CHS.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC71CHT.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC71DEU.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC71ENU.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC71ESP.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC71FRA.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC71ITA.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC71JPN.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC71KOR.DLL"
!ELSE
!IFDEF CL_1300
   Delete /REBOOTOK "$INSTDIR\bin\mfc70.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcr70.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp70.dll"
   Delete /REBOOTOK "$INSTDIR\bin\MFC70CHS.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC70CHT.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC70DEU.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC70ENU.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC70ESP.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC70FRA.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC70ITA.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC70JPN.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC70KOR.DLL"
!ELSE
   Delete /REBOOTOK "$INSTDIR\bin\mfc42.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp60.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcrt.dll"
!ENDIF
!ENDIF
!ENDIF
!ENDIF
   Delete /REBOOTOK "$INSTDIR\bin\psapi.dll"

  RMDir  "$INSTDIR\bin"
  RmDir  "$INSTDIR\doc"
  RmDir  "$INSTDIR\lib"
  RmDir  "$INSTDIR\inc"
  RmDir  "$INSTDIR\install"
  RMDir  "$INSTDIR"
  
  Delete  "$SMPROGRAMS\${PROGRAM_NAME}\Uninstall ${PROGRAM_NAME}.lnk"
  Delete  "$SMPROGRAMS\${PROGRAM_NAME}\Network Identity Manager.lnk"
  Delete  "$SMPROGRAMS\${PROGRAM_NAME}\Release Notes.lnk"
  Delete  "$SMPROGRAMS\${PROGRAM_NAME}\Network Identity Manager User Documentation.lnk"
  Delete  "$SMPROGRAMS\${PROGRAM_NAME}\Network Identity Developer Documentation.lnk"
  RmDir   "$SMPROGRAMS\${PROGRAM_NAME}"
  Delete  "$SMSTARTUP\Network Identity Manager.lnk"

   IfSilent SkipAsk
;  IfFileExists "$WINDIR\krb5.ini" CellExists SkipDelAsk
;  RealmExists:
  MessageBox MB_YESNO "Would you like to keep your configuration files?" IDYES SkipDel
  SkipAsk:
  Delete "$WINDIR\krb5.ini"
  Delete "$WINDIR\krb.con"
  Delete "$WINDIR\krbrealm.con"
  
  SkipDel:
  Delete "$INSTDIR\Uninstall.exe"

  ; Restore previous value of AllowTGTSessionKey 
  ReadRegDWORD $R0 HKLM "${KFW_REGKEY_ROOT}\Client\${KFW_VERSION}" "AllowTGTSessionKeyBackup"
  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Control\Lsa\Kerberos\Parameters" "AllowTGTSessionKey" $R0
  ReadRegDWORD $R0 HKLM "${KFW_REGKEY_ROOT}\Client\${KFW_VERSION}" "AllowTGTSessionKeyBackup2"
  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Control\Lsa\Kerberos" "AllowTGTSessionKey" $R0

  ; The following are keys added for Terminal Server compatibility
  DeleteRegKey HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\netidmgr"
  DeleteRegKey HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\kinit"
  DeleteRegKey HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\klist"
  DeleteRegKey HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\kdestroy"
  DeleteRegKey HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\gss"
  DeleteRegKey HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\gss-client"
  DeleteRegKey HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\gss-server"
  DeleteRegKey HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\k524init"
  DeleteRegKey HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\kpasswd"
  DeleteRegKey HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\kvno"
  DeleteRegKey HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\ms2mit"
  DeleteRegKey HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\mit2ms"
  DeleteRegKey HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\kcpytkt"
  DeleteRegKey HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\kdeltkt"
  DeleteRegKey HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\k95"
  DeleteRegKey HKLM "Software\Microsoft\Windows NT\CurrentVersion\Terminal Server\Compatibility\Applications\k95g"

  DeleteRegKey HKLM "${KFW_REGKEY_ROOT}\Client\CurrentVersion"
  DeleteRegKey HKLM "${KFW_REGKEY_ROOT}\Client"
  DeleteRegKey HKLM "${KFW_REGKEY_ROOT}\Documentation\CurrentVersion"
  DeleteRegKey HKLM "${KFW_REGKEY_ROOT}\Documentation"
  DeleteRegKey HKLM "${KFW_REGKEY_ROOT}\SDK\CurrentVersion"
  DeleteRegKey HKLM "${KFW_REGKEY_ROOT}\SDK"
  DeleteRegKey /ifempty HKLM "${KFW_REGKEY_ROOT}"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PROGRAM_NAME}"

  ; NIM Registry Keys
  DeleteRegKey HKLM "${NIM_REGKEY_ROOT}\PluginManager\Modules\MITKrb5"
  DeleteRegKey HKLM "${NIM_REGKEY_ROOT}\PluginManager\Modules\MITKrb4"
  DeleteRegKey HKLM "${NIM_REGKEY_ROOT}\PluginManager\Plugins\Krb5Cred"
  DeleteRegKey HKLM "${NIM_REGKEY_ROOT}\PluginManager\Plugins\Krb5Ident"
  DeleteRegKey HKLM "${NIM_REGKEY_ROOT}\PluginManager\Plugins\Krb4Cred"
  DeleteRegKey /ifempty HKLM "${NIM_REGKEY_ROOT}\PluginManager\Modules"
  DeleteRegKey /ifempty HKLM "${NIM_REGKEY_ROOT}\PluginManager\Plugins"
  DeleteRegKey /ifempty HKLM "${NIM_REGKEY_ROOT}\PluginManager"
  DeleteRegKey /ifempty HKLM "${NIM_REGKEY_ROOT}"
 
  ; WinLogon Event Notification
  DeleteRegKey HKLM "Software\Microsoft\Windows NT\CurrentVersion\Winlogon\Notify\MIT_KFW"
  DeleteRegKey HKLM "SYSTEM\CurrentControlSet\Services\MIT Kerberos"

  RMDir  "$INSTDIR"

SectionEnd

;--------------------------------
;Uninstaller Functions

Function un.onInit

  ;Get language from registry
  ReadRegStr $LANGUAGE ${MUI_LANGDLL_REGISTRY_ROOT} "${MUI_LANGDLL_REGISTRY_KEY}" "${MUI_LANGDLL_REGISTRY_VALUENAME}"
                                                    
FunctionEnd

Function un.onUninstSuccess

   MessageBox MB_OK "Please reboot your machine to complete uninstallation of the software"

FunctionEnd

;------------------------------
; Get the Configurations files from the Internet

Function kfw.GetConfigFiles

;Check if we should download Config Files
ReadINIStr $R0 $0 "Field 4" "State"
StrCmp $R0 "1" DoDownload

;Do nothing if we're keeping the existing file
ReadINIStr $R0 $0 "Field 2" "State"
StrCmp $R0 "1" done

ReadINIStr $R0 $0 "Field 3" "State"
StrCmp $R0 "1" UsePackaged

; If none of these, grab file from other location
goto CheckOther

DoDownload:
   ReadINIStr $R0 $0 "Field 5" "State"
   NSISdl::download "$R0/krb5.ini" "$WINDIR\krb5.ini"
   NSISdl::download "$R0/krb.con" "$WINDIR\krb.con"
   NSISdl::download "$R0/krbrealm.con" "$WINDIR\krbrealm.con"
   Pop $R0 ;Get the return value
   StrCmp $R0 "success" done
   MessageBox MB_OK|MB_ICONSTOP "Download failed: $R0"
   goto done

UsePackaged:
   SetOutPath "$WINDIR"
   File "${KFW_CONFIG_DIR}\sample\krb5.ini"
   File "${KFW_CONFIG_DIR}\sample\krb.con"
   File "${KFW_CONFIG_DIR}\sample\krbrealm.con"
   goto done
   
CheckOther:
   ReadINIStr $R0 $0 "Field 7" "State"
   StrCmp $R0 "" done
   CopyFiles "$R0\krb5.ini" "$WINDIR\krb5.ini"
   CopyFiles "$R0\krb.con" "$WINDIR\krb.con"
   CopyFiles "$R0\krbrealm.con" "$WINDIR\krbrealm.con"
   
done:

FunctionEnd



;-------------------------------
;Do the page to get the Config files

Function KFWPageGetConfigFiles
  ; Skip this page if we are not installing the client
  SectionGetFlags ${secClient} $R0
  IntOp $R0 $R0 & ${SF_SELECTED}
  StrCmp $R0 "0" Skip
  
  ; Set the install options here
  
startOver:
  WriteINIStr $0 "Field 2" "Flags" "DISABLED"
  WriteINIStr $0 "Field 3" "State" "1"
  WriteINIStr $0 "Field 4" "State" "0"
  WriteINIStr $0 "Field 6" "State" "0"
  WriteINIStr $0 "Field 3" "Text"  "Use packaged configuration files for the ${SAMPLE_CONFIG_REALM} realm."
  WriteINIStr $0 "Field 5" "State"  "${HTTP_CONFIG_URL}"  

  ; If there is an existing krb5.ini file, allow the user to choose it and make it default
  IfFileExists "$WINDIR\krb5.ini" +1 notpresent
  WriteINIStr $0 "Field 2" "Flags" "ENABLED"
  WriteINIStr $0 "Field 2" "State" "1"
  WriteINIStr $0 "Field 3" "State" "0"
  
  notpresent:
  
  !insertmacro MUI_HEADER_TEXT "Kerberos Configuration" "Please choose a method for installing the Kerberos Configuration files:" 
  InstallOptions::dialog $0
  Pop $R1
  StrCmp $R1 "cancel" exit
  StrCmp $R1 "back" done
  StrCmp $R1 "success" done
exit: Quit
done:

   ; Check that if a file is set, a valid filename is entered...
   ReadINIStr $R0 $0 "Field 6" "State"
   StrCmp $R0 "1" CheckFileName
   
   ;Check if a URL is specified, one *IS* specified
   ReadINIStr $R0 $0 "Field 4" "State"
   StrCmp $R0 "1" CheckURL Skip
   
   CheckURL:
   ReadINIStr $R0 $0 "Field 5" "State"
   StrCmp $R0 "" +1 Skip
   MessageBox MB_OK|MB_ICONSTOP $(URLError)
   WriteINIStr $0 "Field 4" "State" "0"
   goto startOver
   
   CheckFileName:
   ReadINIStr $R0 $0 "Field 7" "State"
   IfFileExists "$R0\krb5.ini" Skip

   MessageBox MB_OK|MB_ICONSTOP $(ConfigFileError)
   WriteINIStr $0 "Field 6" "State" "0"
   goto startOver
   
   Skip:
   
FunctionEnd


;-------------------------------
;Do the page to get the Startup Configuration

Function KFWPageGetStartupConfig
  ; Skip this page if we are not installing the client
  SectionGetFlags ${secClient} $R0
  IntOp $R0 $R0 & ${SF_SELECTED}
  StrCmp $R0 "0" Skip
  
  ; Set the install options here
  
  !insertmacro MUI_HEADER_TEXT "Network Identity Manager Setup" "Please select Network Identity ticket manager setup options:" 
  InstallOptions::dialog $1
  Pop $R1
  StrCmp $R1 "cancel" exit
  StrCmp $R1 "back" done
  StrCmp $R1 "success" done
exit: 
  Quit
done:
skip:
   
FunctionEnd


;-------------
; Common install routines for each module
Function KFWCommon.Install

  WriteRegStr HKLM "${KFW_REGKEY_ROOT}" "InstallDir" $INSTDIR

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PROGRAM_NAME}" "DisplayName" "${PROGRAM_NAME}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PROGRAM_NAME}" "UninstallString" "$INSTDIR\uninstall.exe"
!ifndef DEBUG
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PROGRAM_NAME}" "DisplayVersion" "${KFW_VERSION}"
!else
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PROGRAM_NAME}" "DisplayVersion" "${KFW_VERSION} Checked/Debug"
!endif
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PROGRAM_NAME}" "URLInfoAbout" "http://web.mit.edu/kerberos/"

!ifdef DEBUG
  WriteRegDWORD HKLM "${KFW_REGKEY_ROOT}\CurrentVersion" "Debug" 1
  WriteRegDWORD HKLM "${KFW_REGKEY_ROOT}\${KFW_VERSION}" "Debug" 1
!else
   ; Delete the DEBUG string
   DeleteRegValue HKLM "${KFW_REGKEY_ROOT}\CurrentVersion" "Debug"
   DeleteRegValue HKLM "${KFW_REGKEY_ROOT}\${KFW_VERSION}" "Debug"
!endif

  WriteUninstaller "$INSTDIR\Uninstall.exe"
FunctionEnd


;-------------------------------
; Check if the client should be checked for default install
Function ShouldClientInstall
   Push $R0
   StrCpy $R2 "Client"
   Call GetInstalledVersion
   Pop $R0
   
   StrCmp $R0 "" NotInstalled
   ; Now we see if it's an older or newer version

   Call GetInstalledVersionMajor
   Pop $R0
   IntCmpU $R0 ${KFW_MAJORVERSION} +1 Upgrade Downgrade

   Call GetInstalledVersionMinor
   Pop $R0
   IntCmpU $R0 ${KFW_MINORVERSION} +1 Upgrade Downgrade
   
   Call GetInstalledVersionPatch
   Pop $R0
   IntCmpU $R0 ${KFW_PATCHLEVEL} Reinstall Upgrade Downgrade
   
Reinstall:
   StrCpy $R0 "1"
   Exch $R0
   goto end
   
Upgrade:
   StrCpy $R0 "2"
   Exch $R0
   goto end
   
Downgrade:
   StrCpy $R0 "3"
   Exch $R0
   goto end
   
NotInstalled:
   StrCpy $R0 "0"
   Exch $R0
end:   
FunctionEnd

;-------------------------------
; Check how the Documentation options should be set
Function ShouldDocumentationInstall
   Push $R0
   StrCpy $R2 "Documentation"
   Call GetInstalledVersion
   Pop $R0
   
   StrCmp $R0 "" NotInstalled
   ; Now we see if it's an older or newer version

   Call GetInstalledVersionMajor
   Pop $R0
   IntCmpU $R0 ${KFW_MAJORVERSION} +1 Upgrade Downgrade

   Call GetInstalledVersionMinor
   Pop $R0
   IntCmpU $R0 ${KFW_MINORVERSION} +1 Upgrade Downgrade
   
   Call GetInstalledVersionPatch
   Pop $R0
   IntCmpU $R0 ${KFW_PATCHLEVEL} Reinstall Upgrade Downgrade
   
Reinstall:
   StrCpy $R0 "1"
   Exch $R0
   goto end
   
Upgrade:
   StrCpy $R0 "2"
   Exch $R0
   goto end
   
Downgrade:
   StrCpy $R0 "3"
   Exch $R0
   goto end
   
   
NotInstalled:
   StrCpy $R0 "0"
   Exch $R0
end:   
FunctionEnd


;-------------------------------
; Check how the SDK options should be set
Function ShouldSDKInstall
   Push $R0
   StrCpy $R2 "SDK"
   Call GetInstalledVersion
   Pop $R0
   
   StrCmp $R0 "" NotInstalled
   ; Now we see if it's an older or newer version

   Call GetInstalledVersionMajor
   Pop $R0
   IntCmpU $R0 ${KFW_MAJORVERSION} +1 Upgrade Downgrade

   Call GetInstalledVersionMinor
   Pop $R0
   IntCmpU $R0 ${KFW_MINORVERSION} +1 Upgrade Downgrade
   
   Call GetInstalledVersionPatch
   Pop $R0
   IntCmpU $R0 ${KFW_PATCHLEVEL} Reinstall Upgrade Downgrade
   
Reinstall:
   StrCpy $R0 "1"
   Exch $R0
   goto end
   
Upgrade:
   StrCpy $R0 "2"
   Exch $R0
   goto end
   
Downgrade:
   StrCpy $R0 "3"
   Exch $R0
   goto end
   
   
NotInstalled:
   StrCpy $R0 "0"
   Exch $R0
end:   
FunctionEnd

; See if KfW SDK is installed
; Returns: "1" if it is, 0 if it is not (on the stack)
Function IsSDKInstalled
   Push $R0
   StrCpy $R2 "SDK"
   Call GetInstalledVersion
   Pop $R0
   
   StrCmp $R0 "" NotInstalled
   
   StrCpy $R0 "1"
   Exch $R0
   goto end
   
NotInstalled:
   StrCpy $R0 "0"
   Exch $R0
end:   
FunctionEnd


; See if KfW Client is installed
; Returns: "1" if it is, 0 if it is not (on the stack)
Function IsClientInstalled
   Push $R0
   StrCpy $R2 "Client"
   Call GetInstalledVersion
   Pop $R0
   
   StrCmp $R0 "" NotInstalled
   
   StrCpy $R0 "1"
   Exch $R0
   goto end
   
NotInstalled:
   StrCpy $R0 "0"
   Exch $R0
end:   
FunctionEnd



; See if KfW Documentation is installed
; Returns: "1" if it is, 0 if it is not (on the stack)
Function IsDocumentationInstalled
   Push $R0
   StrCpy $R2 "Documentation"
   Call GetInstalledVersion
   Pop $R0
   
   StrCmp $R0 "" NotInstalled
   
   StrCpy $R0 "1"
   Exch $R0
   goto end
   
NotInstalled:
   StrCpy $R0 "0"
   Exch $R0
end:   
FunctionEnd



;Check to see if any KfW component is installed
;Returns: Value on stack: "1" if it is, "0" if it is not
Function IsAnyKfWInstalled
   Push $R0
   Push $R1
   Push $R2
   Call IsClientInstalled
   Pop $R0
   Call IsSDKInstalled
   Pop $R1
   Call IsDocumentationInstalled
   Pop $R2
   ; Now we must see if ANY of the $Rn values are 1
   StrCmp $R0 "1" SomethingInstalled
   StrCmp $R1 "1" SomethingInstalled
   StrCmp $R2 "1" SomethingInstalled
   ;Nothing installed
   StrCpy $R0 "0"
   goto end
SomethingInstalled:
   StrCpy $R0 "1"
end:
   Pop $R2
   Pop $R1
   Exch $R0
FunctionEnd

;--------------------------------
;Handle what must and what must not be installed
Function .onSelChange
   ; If they install the SDK, they MUST install the client
   SectionGetFlags ${secSDK} $R0
   IntOp $R0 $R0 & ${SF_SELECTED}
   StrCmp $R0 "1" MakeClientSelected
   goto end
   
MakeClientSelected:
   SectionGetFlags ${secClient} $R0
   IntOp $R0 $R0 | ${SF_SELECTED}
   SectionSetFlags ${secClient} $R0
   
end:
FunctionEnd

Function AddProvider
   Push $R0
   Push $R1
   ReadRegStr $R0 HKLM "SYSTEM\CurrentControlSet\Control\NetworkProvider\HWOrder" "ProviderOrder"
   Push $R0
   StrCpy $R0 "MIT Kerberos"
   Push $R0
   Call StrStr
   Pop $R0
   StrCmp $R0 "" DoOther +1
   ReadRegStr $R1 HKLM "SYSTEM\CurrentControlSet\Control\NetworkProvider\HWOrder" "ProviderOrder"
   StrCpy $R0 "$R1,MIT Kerberos"
   WriteRegStr HKLM "SYSTEM\CurrentControlSet\Control\NetworkProvider\HWOrder" "ProviderOrder" $R0
DoOther:
   ReadRegStr $R0 HKLM "SYSTEM\CurrentControlSet\Control\NetworkProvider\Order" "ProviderOrder"
   Push $R0
   StrCpy $R0 "MIT Kerberos"
   Push $R0
   Call StrStr
   Pop $R0
   StrCmp $R0 "" +1 End
   ReadRegStr $R1 HKLM "SYSTEM\CurrentControlSet\Control\NetworkProvider\Order" "ProviderOrder"
   StrCpy $R0 "$R1,MIT Kerberos"
   WriteRegStr HKLM "SYSTEM\CurrentControlSet\Control\NetworkProvider\Order" "ProviderOrder" $R0
End:
   Pop $R1
   Pop $R0
FunctionEnd

Function un.RemoveProvider
   Push $R0
   StrCpy $R0 "MIT Kerberos"
   Push $R0
   StrCpy $R0 "SYSTEM\CurrentControlSet\Control\NetworkProvider\HWOrder" 
   Call un.RemoveFromProvider
   StrCpy $R0 "MIT Kerberos"
   Push $R0
   StrCpy $R0 "SYSTEM\CurrentControlSet\Control\NetworkProvider\Order"
   Call un.RemoveFromProvider
   Pop $R0
FunctionEnd

Function un.RemoveFromProvider
  Exch $0
  Push $1
  Push $2
  Push $3
  Push $4
  Push $5
  Push $6

  ReadRegStr $1 HKLM "$R0" "ProviderOrder"
    StrCpy $5 $1 1 -1 # copy last char
    StrCmp $5 "," +2 # if last char != ,
      StrCpy $1 "$1," # append ,
    Push $1
    Push "$0,"
    Call un.StrStr ; Find `$0,` in $1
    Pop $2 ; pos of our dir
    StrCmp $2 "" unRemoveFromPath_done
      ; else, it is in path
      # $0 - path to add
      # $1 - path var
      StrLen $3 "$0,"
      StrLen $4 $2
      StrCpy $5 $1 -$4 # $5 is now the part before the path to remove
      StrCpy $6 $2 "" $3 # $6 is now the part after the path to remove
      StrCpy $3 $5$6

      StrCpy $5 $3 1 -1 # copy last char
      StrCmp $5 "," 0 +2 # if last char == ,
        StrCpy $3 $3 -1 # remove last char

      WriteRegStr HKLM "$R0" "ProviderOrder" $3

  unRemoveFromPath_done:
    Pop $6
    Pop $5
    Pop $4
    Pop $3
    Pop $2
    Pop $1
    Pop $0
FunctionEnd
