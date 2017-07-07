;-----------------------------------------------
; Common Utility functions not specific to KFW

;-------------------
; Get the currently installed version and place it on the stack
; Modifies: Nothing
Function GetInstalledVersion
   Push $R0
   Push $R1
   Push $R4
   ReadRegStr $R0 HKLM "${KFW_REGKEY_ROOT}\$R2\CurrentVersion" "VersionString"
   StrCmp $R0 "" done
   
done:
   Pop $R4
   Pop $R1
   Exch $R0
FunctionEnd

; Functions to get each component of the version number
Function GetInstalledVersionMajor
   Push $R0
   Push $R1
   Push $R4
   ReadRegDWORD $R0 HKLM "${KFW_REGKEY_ROOT}\$R2\CurrentVersion" "MajorVersion"
   StrCmp $R0 "" done
   
done:
   Pop $R4
   Pop $R1
   Exch $R0
FunctionEnd

Function GetInstalledVersionMinor
   Push $R0
   Push $R1
   Push $R4
   ReadRegDWORD $R0 HKLM "${KFW_REGKEY_ROOT}\$R2\CurrentVersion" "MinorVersion"
   StrCmp $R0 "" done
   
done:
   Pop $R4
   Pop $R1
   Exch $R0
FunctionEnd

Function GetInstalledVersionPatch
   Push $R0
   Push $R1
   Push $R4
   ReadRegDWORD $R0 HKLM "${KFW_REGKEY_ROOT}\$R2\CurrentVersion" "PatchLevel"
   StrCmp $R0 "" done
   
done:
   Pop $R4
   Pop $R1
   Exch $R0
FunctionEnd


;--------------------------------
; Macros

;--------------------------------
; Macros
; Macro - Upgrade DLL File
; Written by Joost Verburg
; ------------------------
;
; Parameters:
; LOCALFILE   - Location of the new DLL file (on the compiler system)
; DESTFILE    - Location of the DLL file that should be upgraded
;              (on the user's system)
; TEMPBASEDIR - Directory on the user's system to store a temporary file
;               when the system has to be rebooted.
;               For Win9x support, this should be on the same volume as the
;               DESTFILE!
;               The Windows temp directory could be located on any volume,
;               so you cannot use  this directory.
;
; Define REPLACEDLL_NOREGISTER if you want to upgrade a DLL that does not
; have to be registered.
;
; Note: If you want to support Win9x, you can only use
;       short filenames (8.3).
;
; Example of usage:
; !insertmacro ReplaceDLL "dllname.dll" "$SYSDIR\dllname.dll" "$SYSDIR"
;

!macro ReplaceDLL LOCALFILE DESTFILE TEMPBASEDIR

  Push $R0
  Push $R1
  Push $R2
  Push $R3
  Push $R4
  Push $R5

  ;------------------------
  ;Unique number for labels

  !define REPLACEDLL_UNIQUE ${__LINE__}

  ;------------------------
  ;Copy the parameters used on run-time to a variable
  ;This allows the usage of variables as paramter

  StrCpy $R4 "${DESTFILE}"
  StrCpy $R5 "${TEMPBASEDIR}"

  ;------------------------
  ;Check file and version
  ;
  IfFileExists $R4 0 replacedll.copy_${REPLACEDLL_UNIQUE}
  
  ;ClearErrors
  ;  GetDLLVersionLocal "${LOCALFILE}" $R0 $R1
  ;  GetDLLVersion $R4 $R2 $R3
  ;IfErrors replacedll.upgrade_${REPLACEDLL_UNIQUE}
  ;
  ;IntCmpU $R0 $R2 0 replacedll.done_${REPLACEDLL_UNIQUE} \
  ;  replacedll.upgrade_${REPLACEDLL_UNIQUE}
  ;IntCmpU $R1 $R3 replacedll.done_${REPLACEDLL_UNIQUE} \
  ;  replacedll.done_${REPLACEDLL_UNIQUE} \
  ;  replacedll.upgrade_${REPLACEDLL_UNIQUE}

  ;------------------------
  ;Let's replace the DLL!

  SetOverwrite try

  ;replacedll.upgrade_${REPLACEDLL_UNIQUE}:
    !ifndef REPLACEDLL_NOREGISTER
      ;Unregister the DLL
      UnRegDLL $R4
    !endif

  ;------------------------
  ;Try to copy the DLL directly

  ClearErrors
    StrCpy $R0 $R4
    Call :replacedll.file_${REPLACEDLL_UNIQUE}
  IfErrors 0 replacedll.noreboot_${REPLACEDLL_UNIQUE}

  ;------------------------
  ;DLL is in use. Copy it to a temp file and Rename it on reboot.

  GetTempFileName $R0 $R5
    Call :replacedll.file_${REPLACEDLL_UNIQUE}
  Rename /REBOOTOK $R0 $R4

  ;------------------------
  ;Register the DLL on reboot

  !ifndef REPLACEDLL_NOREGISTER
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\RunOnce" \
      "Register $R4" 'rundll32.exe "$R4",DllRegisterServer'
  !endif

  Goto replacedll.done_${REPLACEDLL_UNIQUE}

  ;------------------------
  ;DLL does not exist - just extract

  replacedll.copy_${REPLACEDLL_UNIQUE}:
    StrCpy $R0 $R4
    Call :replacedll.file_${REPLACEDLL_UNIQUE}

  ;------------------------
  ;Register the DLL

  replacedll.noreboot_${REPLACEDLL_UNIQUE}:
    !ifndef REPLACEDLL_NOREGISTER
      RegDLL $R4
    !endif

  ;------------------------
  ;Done

  replacedll.done_${REPLACEDLL_UNIQUE}:

  Pop $R5
  Pop $R4
  Pop $R3
  Pop $R2
  Pop $R1
  Pop $R0

  ;------------------------
  ;End

  Goto replacedll.end_${REPLACEDLL_UNIQUE}

  ;------------------------
  ;Called to extract the DLL

  replacedll.file_${REPLACEDLL_UNIQUE}:
    File /oname=$R0 "${LOCALFILE}"
    Return

  replacedll.end_${REPLACEDLL_UNIQUE}:

 ;------------------------
 ;Restore settings

 SetOverwrite lastused
 
 !undef REPLACEDLL_UNIQUE

!macroend


; GetParameters
; input, none
; output, top of stack (replaces, with e.g. whatever)
; modifies no other variables.

Function GetParameters
  Push $R0
  Push $R1
  Push $R2
  StrCpy $R0 $CMDLINE 1
  StrCpy $R1 '"'
  StrCpy $R2 1
  StrCmp $R0 '"' loop
    StrCpy $R1 ' ' ; we're scanning for a space instead of a quote
  loop:
    StrCpy $R0 $CMDLINE 1 $R2
    StrCmp $R0 $R1 loop2
    StrCmp $R0 "" loop2
    IntOp $R2 $R2 + 1
    Goto loop
  loop2:
    IntOp $R2 $R2 + 1
    StrCpy $R0 $CMDLINE 1 $R2
    StrCmp $R0 " " loop2
  StrCpy $R0 $CMDLINE "" $R2
  Pop $R2
  Pop $R1
  Exch $R0
FunctionEnd


!verbose 3
!include "WinMessages.NSH"
!verbose 4

Function GetSystemPath
    Push $0

    Call IsNT
    Pop $0
    StrCmp $0 1 GetPath_NT
    ReadEnvStr $0 PATH
    goto HavePath
GetPath_NT:
    ReadRegStr $0 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "PATH"
HavePath:
    
    Exch $0
FunctionEnd

;====================================================
; AddToSystemPath - Adds the given dir to the search path.
;        Input - head of the stack
;        Note - Win9x systems requires reboot
;====================================================
Function AddToSystemPath
  Exch $0
  Push $1
  Push $2
  Push $3

  # don't add if the path doesn't exist
  IfFileExists $0 "" AddToPath_done

  Call GetSystemPath
  Pop $1
  Push "$1;"
  Push "$0;"
  Call StrStr
  Pop $2
  StrCmp $2 "" 0 AddToPath_done
  Push "$1;"
  Push "$0\;"
  Call StrStr
  Pop $2
  StrCmp $2 "" 0 AddToPath_done
  GetFullPathName /SHORT $3 $0
  Push "$1;"
  Push "$3;"
  Call StrStr
  Pop $2
  StrCmp $2 "" 0 AddToPath_done
  Push "$1;"
  Push "$3\;"
  Call StrStr
  Pop $2
  StrCmp $2 "" 0 AddToPath_done

  Call IsNT
  Pop $1
  StrCmp $1 1 AddToPath_NT
    ; Not on NT
    StrCpy $1 $WINDIR 2
    FileOpen $1 "$1\autoexec.bat" a
    FileSeek $1 -1 END
    FileReadByte $1 $2
    IntCmp $2 26 0 +2 +2 # DOS EOF
      FileSeek $1 -1 END # write over EOF
    FileWrite $1 "$\r$\nSET PATH=%PATH%;$3$\r$\n"
    FileClose $1
    SetRebootFlag true
    Goto AddToPath_done

  AddToPath_NT:
    ReadRegStr $1 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "PATH"
    StrCpy $2 $1 1 -1 # copy last char
    StrCmp $2 ";" 0 +2 # if last char == ;
      StrCpy $1 $1 -1 # remove last char
    StrCmp $1 "" AddToPath_NTdoIt
      StrCpy $0 "$1;$0"
    AddToPath_NTdoIt:
      WriteRegExpandStr HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "PATH" $0
      SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000

  AddToPath_done:
    Pop $3
    Pop $2
    Pop $1
    Pop $0
FunctionEnd

;====================================================
; RemoveFromPath - Remove a given dir from the path
;     Input: head of the stack
;====================================================
Function un.RemoveFromSystemPath
  Exch $0
  Push $1
  Push $2
  Push $3
  Push $4
  Push $5
  Push $6

  IntFmt $6 "%c" 26 # DOS EOF

  Call un.IsNT
  Pop $1
  StrCmp $1 1 unRemoveFromPath_NT
    ; Not on NT
    StrCpy $1 $WINDIR 2
    FileOpen $1 "$1\autoexec.bat" r
    GetTempFileName $4
    FileOpen $2 $4 w
    GetFullPathName /SHORT $0 $0
    StrCpy $0 "SET PATH=%PATH%;$0"
    Goto unRemoveFromPath_dosLoop

    unRemoveFromPath_dosLoop:
      FileRead $1 $3
      StrCpy $5 $3 1 -1 # read last char
      StrCmp $5 $6 0 +2 # if DOS EOF
        StrCpy $3 $3 -1 # remove DOS EOF so we can compare
      StrCmp $3 "$0$\r$\n" unRemoveFromPath_dosLoopRemoveLine
      StrCmp $3 "$0$\n" unRemoveFromPath_dosLoopRemoveLine
      StrCmp $3 "$0" unRemoveFromPath_dosLoopRemoveLine
      StrCmp $3 "" unRemoveFromPath_dosLoopEnd
      FileWrite $2 $3
      Goto unRemoveFromPath_dosLoop
      unRemoveFromPath_dosLoopRemoveLine:
        SetRebootFlag true
        Goto unRemoveFromPath_dosLoop

    unRemoveFromPath_dosLoopEnd:
      FileClose $2
      FileClose $1
      StrCpy $1 $WINDIR 2
      Delete "$1\autoexec.bat"
      CopyFiles /SILENT $4 "$1\autoexec.bat"
      Delete $4
      Goto unRemoveFromPath_done

  unRemoveFromPath_NT:
    ReadRegStr $1 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "PATH"
    StrCpy $5 $1 1 -1 # copy last char
    StrCmp $5 ";" +2 # if last char != ;
      StrCpy $1 "$1;" # append ;
    Push $1
    Push "$0;"
    Call un.StrStr ; Find `$0;` in $1
    Pop $2 ; pos of our dir
    StrCmp $2 "" unRemoveFromPath_done
      ; else, it is in path
      # $0 - path to add
      # $1 - path var
      StrLen $3 "$0;"
      StrLen $4 $2
      StrCpy $5 $1 -$4 # $5 is now the part before the path to remove
      StrCpy $6 $2 "" $3 # $6 is now the part after the path to remove
      StrCpy $3 $5$6

      StrCpy $5 $3 1 -1 # copy last char
      StrCmp $5 ";" 0 +2 # if last char == ;
        StrCpy $3 $3 -1 # remove last char

      WriteRegExpandStr HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "PATH" $3
      SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000

  unRemoveFromPath_done:
    Pop $6
    Pop $5
    Pop $4
    Pop $3
    Pop $2
    Pop $1
    Pop $0
FunctionEnd

;====================================================
; IsNT - Returns 1 if the current system is NT, 0
;        otherwise.
;     Output: head of the stack
;====================================================
!macro IsNT un
Function ${un}IsNT
  Push $0
  ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion" CurrentVersion
  StrCmp $0 "" 0 IsNT_yes
  ; we are not NT.
  Pop $0
  Push 0
  Return

  IsNT_yes:
    ; NT!!!
    Pop $0
    Push 1
FunctionEnd
!macroend
!insertmacro IsNT ""
!insertmacro IsNT "un."

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Uninstall stuff
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;====================================================
; StrStr - Finds a given string in another given string.
;               Returns -1 if not found and the pos if found.
;          Input: head of the stack - string to find
;                      second in the stack - string to find in
;          Output: head of the stack
;====================================================
!macro StrStr un
Function ${un}StrStr
Exch $R1 ; st=haystack,old$R1, $R1=needle
  Exch    ; st=old$R1,haystack
  Exch $R2 ; st=old$R1,old$R2, $R2=haystack
  Push $R3
  Push $R4
  Push $R5
  StrLen $R3 $R1
  StrCpy $R4 0
  ; $R1=needle
  ; $R2=haystack
  ; $R3=len(needle)
  ; $R4=cnt
  ; $R5=tmp
  loop:
    StrCpy $R5 $R2 $R3 $R4
    StrCmp $R5 $R1 done
    StrCmp $R5 "" done
    IntOp $R4 $R4 + 1
    Goto loop
done:
  StrCpy $R1 $R2 "" $R4
  Pop $R5
  Pop $R4
  Pop $R3
  Pop $R2
  Exch $R1
FunctionEnd
!macroend
!insertmacro StrStr ""
!insertmacro StrStr "un."


!ifdef ADDSHAREDDLLUSED
; AddSharedDLL
 ;
 ; Increments a shared DLLs reference count.
 ; Use by passing one item on the stack (the full path of the DLL).
 ;
 ; Usage:
 ;   Push $SYSDIR\myDll.dll
 ;   Call AddSharedDLL
 ;

 Function AddSharedDLL
   Exch $R1
   Push $R0
   ReadRegDword $R0 HKLM Software\Microsoft\Windows\CurrentVersion\SharedDLLs $R1
   IntOp $R0 $R0 + 1
   WriteRegDWORD HKLM Software\Microsoft\Windows\CurrentVersion\SharedDLLs $R1 $R0
   Pop $R0
   Pop $R1
 FunctionEnd

 
; un.RemoveSharedDLL
 ;
 ; Decrements a shared DLLs reference count, and removes if necessary.
 ; Use by passing one item on the stack (the full path of the DLL).
 ; Note: for use in the main installer (not the uninstaller), rename the
 ; function to RemoveSharedDLL.
 ;
 ; Usage:
 ;   Push $SYSDIR\myDll.dll
 ;   Call un.RemoveSharedDLL
 ;

 Function un.RemoveSharedDLL
   Exch $R1
   Push $R0
   ReadRegDword $R0 HKLM Software\Microsoft\Windows\CurrentVersion\SharedDLLs $R1
   StrCmp $R0 "" remove
     IntOp $R0 $R0 - 1
     IntCmp $R0 0 rk rk uk
     rk:
       DeleteRegValue HKLM Software\Microsoft\Windows\CurrentVersion\SharedDLLs $R1
     goto Remove
     uk:
       WriteRegDWORD HKLM Software\Microsoft\Windows\CurrentVersion\SharedDLLs $R1 $R0
     Goto noremove
   remove:
     Delete /REBOOTOK $R1
   noremove:
   Pop $R0
   Pop $R1
 FunctionEnd
!endif


; GetWindowsVersion
;
; Based on Yazno's function, http://yazno.tripod.com/powerpimpit/
; Updated by Joost Verburg
;
; Returns on top of stack
;
; Windows Version (95, 98, ME, NT x.x, 2000, XP, 2003)
; or
; '' (Unknown Windows Version)
;
; Usage:
;   Call GetWindowsVersion
;   Pop $R0
;   ; at this point $R0 is "NT 4.0" or whatnot

Function GetWindowsVersion

  Push $R0
  Push $R1

  ClearErrors

  ReadRegStr $R0 HKLM \
  "SOFTWARE\Microsoft\Windows NT\CurrentVersion" CurrentVersion

  IfErrors 0 lbl_winnt
  
  ; we are not NT
  ReadRegStr $R0 HKLM \
  "SOFTWARE\Microsoft\Windows\CurrentVersion" VersionNumber

  StrCpy $R1 $R0 1
  StrCmp $R1 '4' 0 lbl_error

  StrCpy $R1 $R0 3

  StrCmp $R1 '4.0' lbl_win32_95
  StrCmp $R1 '4.9' lbl_win32_ME lbl_win32_98

  lbl_win32_95:
    StrCpy $R0 '95'
  Goto lbl_done

  lbl_win32_98:
    StrCpy $R0 '98'
  Goto lbl_done

  lbl_win32_ME:
    StrCpy $R0 'ME'
  Goto lbl_done

  lbl_winnt:

  StrCpy $R1 $R0 1

  StrCmp $R1 '3' lbl_winnt_x
  StrCmp $R1 '4' lbl_winnt_x

  StrCpy $R1 $R0 3

  StrCmp $R1 '5.0' lbl_winnt_2000
  StrCmp $R1 '5.1' lbl_winnt_XP
  StrCmp $R1 '5.2' lbl_winnt_2003 lbl_error

  lbl_winnt_x:
    StrCpy $R0 "NT $R0" 6
  Goto lbl_done

  lbl_winnt_2000:
    Strcpy $R0 '2000'
  Goto lbl_done

  lbl_winnt_XP:
    Strcpy $R0 'XP'
  Goto lbl_done

  lbl_winnt_2003:
    Strcpy $R0 '2003'
  Goto lbl_done

  lbl_error:
    Strcpy $R0 ''
  lbl_done:

  Pop $R1
  Exch $R0

FunctionEnd


; Author: Lilla (lilla@earthlink.net) 2003-06-13
; function IsUserAdmin uses plugin \NSIS\PlusgIns\UserInfo.dll
; This function is based upon code in \NSIS\Contrib\UserInfo\UserInfo.nsi
; This function was tested under NSIS 2 beta 4 (latest CVS as of this writing).
;
; Usage:
;   Call IsUserAdmin
;   Pop $R0   ; at this point $R0 is "true" or "false"
;
Function IsUserAdmin
Push $R0
Push $R1
Push $R2

ClearErrors
UserInfo::GetName
IfErrors Win9x
Pop $R1
UserInfo::GetAccountType
Pop $R2

StrCmp $R2 "Admin" 0 Continue
; Observation: I get here when running Win98SE. (Lilla)
; The functions UserInfo.dll looks for are there on Win98 too, 
; but just don't work. So UserInfo.dll, knowing that admin isn't required
; on Win98, returns admin anyway. (per kichik)
; MessageBox MB_OK 'User "$R1" is in the Administrators group'
StrCpy $R0 "true"
Goto Done

Continue:
; You should still check for an empty string because the functions
; UserInfo.dll looks for may not be present on Windows 95. (per kichik)
StrCmp $R2 "" Win9x
StrCpy $R0 "false"
;MessageBox MB_OK 'User "$R1" is in the "$R2" group'
Goto Done

Win9x:
; comment/message below is by UserInfo.nsi author:
; This one means you don't need to care about admin or
; not admin because Windows 9x doesn't either
;MessageBox MB_OK "Error! This DLL can't run under Windows 9x!"
StrCpy $R0 "false"

Done:
;MessageBox MB_OK 'User= "$R1"  AccountType= "$R2"  IsUserAdmin= "$R0"'

Pop $R2
Pop $R1
Exch $R0
FunctionEnd

Function RestartRequired
Push $R1 ;Original Variable
Push $R2
Push $R3 ;Counter Variable

StrCpy $R1 "0" 1 ;initialize variable with 0
StrCpy $R3 "0" 0 ;Counter Variable

;First Check Current User RunOnce Key
EnumRegValue $R2 HKCU "Software\Microsoft\Windows\CurrentVersion\RunOnce" $R3
StrCmp $R2 "" 0 FoundRestart

;Next Check Local Machine RunOnce Key
EnumRegValue $R2 HKLM "Software\Microsoft\Windows\CurrentVersion\RunOnce" $R3
StrCmp $R2 "" 0 FoundRestart

EnumRegValue $R2 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\FileRenameOperations" $R3
StrCmp $R2 "" 0 FoundRestart

NextValue:
EnumRegValue $R2 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager" $R3
StrCmp $R2 "" ExitFunc 0
StrCmp $R2 "PendingFileRenameOperations" FoundRestart 0
IntOp $R3 $R3 + 1
Goto NextValue

FoundRestart:
StrCpy $R1 "1" 1

ExitFunc:
Pop $R3
Pop $R2
Exch $R1
FunctionEnd

; GetParent
 ; input, top of stack  (e.g. C:\Program Files\Poop)
 ; output, top of stack (replaces, with e.g. C:\Program Files)
 ; modifies no other variables.
 ;
 ; Usage:
 ;   Push "C:\Program Files\Directory\Whatever"
 ;   Call GetParent
 ;   Pop $R0
 ;   ; at this point $R0 will equal "C:\Program Files\Directory"

Function GetParent

  Exch $R0
  Push $R1
  Push $R2
  Push $R3
  
  StrCpy $R1 0
  StrLen $R2 $R0
  
  loop:
    IntOp $R1 $R1 + 1
    IntCmp $R1 $R2 get 0 get
    StrCpy $R3 $R0 1 -$R1
    StrCmp $R3 "\" get
  Goto loop
  
  get:
    StrCpy $R0 $R0 -$R1
    
    Pop $R3
    Pop $R2
    Pop $R1
    Exch $R0
    
FunctionEnd

; SearchPath  (path, filename)
; input:
;    top of stack is the filename
;    top of stack minus one is the path
; output:
;    top of stack is a fully qualified path or the number "0" 
;
; Usage:
;    Push "semicolon delimited path"
;    Push "filename"
;    Call SearchPath
;    Pop  $R0 ; fqpn 
;    StrCmp $R0 "" failed
;   
;
Function SearchPath
  Exch $R0  ; input - filename
  Exch 
  Exch $R1  ; input - semicolon delimited path
  Push $R3  ; worker - index to current end character
  Push $R4  ; worker - length of $R1
  Push $R5  ; worker - copy of directory string/fqpn to search for
  Push $R6  ; worker - single charcter copy or find handle
  
  StrCpy $R3 0        ; init character index
  StrLen $R4 $R1      ; determine length of semicolon delimited path
  StrCpy $R5 ""        ; init return value
  
  findDir:  ; find a semi-colon or end of string
  IntCmp $R3 $R4 exit 0 exit   ; we are done if no unprocessed string left

  loop:  
    StrCpy $R6 $R1 1 $R3       ; get the next character
    StrCmp $R6 ";" foundDir    ; if it is semi-colon, we have found a dir
    IntOp $R3 $R3 + 1          ; increment index
    IntCmp $R3 $R4 foundDir    ; if we are at end of string, we have a dir
  Goto loop                    ; still more chars in this dir

  foundDir:
    StrCpy $R5 $R1 $R3     ; copy the dir to $R5
    StrCpy $R5 "$R5\$R0"   ; construct fqpn
    IfFileExists $R5 exit  ; if file exists we are done
    StrCpy $R5 ""           ; reset return value to null string
    IntOp $R4 $R4 - $R3    ; compute maxlen of new delimited path
    IntCmp $R4 0 exit      ; no more path left, exit 
    IntOp $R3 $R3 + 1      ; Increment $R3 past the semi-colon
    StrCpy $R1 $R1 $R4 $R3 ; remove dir from the delimited path
    StrCpy $R3 0           ; index back to start of new delimited path
    goto findDir           ; get another directory to look in

  exit:
    Pop  $R6
    Exch $R5 ; output - fully qualified pathname
    Exch
    Pop  $R4
    Exch
    Pop  $R3
    Exch
    Pop  $R1
    Exch 
    Pop  $R0
FunctionEnd
