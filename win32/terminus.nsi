#
# Copyright (c) 2020 Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#

!include nsDialogs.nsh
!include LogicLib.nsh
!include FileFunc.nsh
!include WinVer.nsh

Name "Terminus Font"
OutFile terminus-font-4.49.exe

XPStyle on
CRCCheck force
RequestExecutionLevel admin

InstallDir "$EXEDIR\terminus-font-4.49"
InstallButtonText "Proceed"

Var apply_ao2
Var apply_dv1
Var apply_ge2
Var apply_gq2
Var apply_ij1
Var apply_ka2
Var apply_ll2
Var apply_td1
Var apply_hi2

Var install
Var unpack
Var hamster
Var directory
Var browse

Var ao2
Var dv1
Var ge2
Var gq2
Var ij1
Var ka2
Var ll2
Var td1
Var hi2

Var instate

Page custom ter_dialog_page ter_dialog_page_leave
Page instfiles

Function install_clicked
	EnableWindow $directory 0
	EnableWindow $browse 0
	${NSD_SetText} $directory $FONTS
FunctionEnd

Function unpack_clicked
	${NSD_SetText} $directory $INSTDIR
	EnableWindow $directory 1
	EnableWindow $browse 1
FunctionEnd

Function hamster_clicked
	ExecShell "open" "http://terminus-font.sourceforge.net#variants"
	ToolTips::Classic $hamster "http://terminus-font.sourceforge.net#variants"
FunctionEnd

Function browse_clicked
	nsDialogs::SelectFolderDialog Directory $INSTDIR

	Pop $0
	${If} $0 != error
		StrCpy $INSTDIR $0
		${NSD_SetText} $directory $INSTDIR
	${EndIf}
FunctionEnd

Function ter_dialog_page
	nsDialogs::Create 1018
	Pop $0
	${If} $0 == error
		MessageBox MB_ICONSTOP|MB_OK "Failed to create installation dialog."
		Abort
	${EndIf}

	${NSD_CreateLink} 2% 1 11% 10u "Variants"
	Pop $hamster
	${NSD_OnClick} $hamster hamster_clicked
	ToolTips::Classic $hamster "http://terminus-font.sourceforge.net#variants"
	${NSD_CreateGroupBox} 0 0 100% 25u ""
	${NSD_CreateCheckBox} 2% 10u 11% 12u "ao2"
	Pop $apply_ao2
	${NSD_CreateCheckBox} 13% 10u 11% 12u "dv1"
	Pop $apply_dv1
	${NSD_CreateCheckBox} 24% 10u 11% 12u "ge2"
	Pop $apply_ge2
	${NSD_CreateCheckBox} 35% 10u 11% 12u "gq2"
	Pop $apply_gq2
	${NSD_CreateCheckBox} 46% 10u 10% 12u "ij1"
	Pop $apply_ij1
	${NSD_CreateCheckBox} 56% 10u 11% 12u "ka2"
	Pop $apply_ka2
	${NSD_CreateCheckBox} 67% 10u 10% 12u "ll2"
	Pop $apply_ll2
	${NSD_CreateCheckBox} 77% 10u 11% 12u "td1"
	Pop $apply_td1
	${NSD_CreateCheckBox} 88% 10u 11% 12u "hi2"
	Pop $apply_hi2

	${NSD_CreateRadioButton} 0 28u 14% 12u "Install"
	Pop $install
	${NSD_AddStyle} $install ${WS_GROUP}
	${NSD_Check} $install
	${NSD_OnClick} $install install_clicked
	${NSD_CreateRadioButton} 15% 28u 35% 12u "Unpack and patch only"
	Pop $unpack
	${NSD_UnCheck} $unpack
	${NSD_OnClick} $unpack unpack_clicked
	${NSD_CreateFileRequest} 0% 42u 95% 12u ""
	Pop $directory
	${NSD_CreateBrowseButton} 95% 42u 5% 12u "..."
	Pop $browse
	${NSD_OnClick} $browse browse_clicked
	Call install_clicked

	${NSD_CreateHLine} 0 57u 100% 1u
	Pop $0
	${NSD_AddStyle} $0 ${WS_GROUP}
	${NSD_CreateLabel} 2% 63u 96% 8u "Terminus Font is licensed under \
		the SIL Open Font License, Version 1.1."
	${NSD_CreateLabel} 2% 71u 96% 12u "The license is available with a \
		FAQ at: http://scripts.sil.org/OFL"
	${NSD_CreateLabel} 2% 83u 96% 24u "Note: the Windows code pages \
		contain a total of 384 characters. All other characters \
		(math, pseudographics etc.) are not currently available."
	${NSD_CreateLabel} 2% 107u 96% 12u "Terminus Font 4.49, \
		Copyright (C) 2020 Dimitar Toshkov Zhekov."
	${NSD_CreateLabel} 2% 119u 96% 12u "Report bugs to \
		<dimitar.zhekov@gmail.com>"

	nsDialogs::Show
FunctionEnd

Function ter_dialog_page_leave
	${NSD_GetState} $apply_ao2 $0
	${IfThen} $0 == ${BST_CHECKED} ${|} StrCpy $ao2 "ao2" ${|}
	${NSD_GetState} $apply_dv1 $0
	${IfThen} $0 == ${BST_CHECKED} ${|} StrCpy $dv1 "dv1" ${|}
	${NSD_GetState} $apply_ge2 $0
	${IfThen} $0 == ${BST_CHECKED} ${|} StrCpy $ge2 "ge2" ${|}
	${NSD_GetState} $apply_gq2 $0
	${IfThen} $0 == ${BST_CHECKED} ${|} StrCpy $gq2 "gq2" ${|}
	${NSD_GetState} $apply_ij1 $0
	${IfThen} $0 == ${BST_CHECKED} ${|} StrCpy $ij1 "ij1" ${|}
	${NSD_GetState} $apply_ka2 $0
	${IfThen} $0 == ${BST_CHECKED} ${|} StrCpy $ka2 "ka2" ${|}
	${NSD_GetState} $apply_ll2 $0
	${IfThen} $0 == ${BST_CHECKED} ${|} StrCpy $ll2 "ll2" ${|}
	${NSD_GetState} $apply_td1 $0
	${IfThen} $0 == ${BST_CHECKED} ${|} StrCpy $td1 "td1" ${|}

	${NSD_GetState} $apply_hi2 $0
	${If} $0 == ${BST_CHECKED}
		StrCpy $hi2 "hi2"
		${IfThen} $dv1 != "" ${|} StrCpy $dv1 "hi2-dv1" ${|}
		${IfThen} $ka2 != "" ${|} StrCpy $ka2 "hi2-ka2" ${|}
	${EndIf}

	${NSD_GetState} $install $instate
	${If} $instate == ${BST_UNCHECKED}
		${NSD_GetText} $directory $INSTDIR
		${If} $INSTDIR == ""
			MessageBox MB_ICONSTOP|MB_OK "Unpack directory name required"
			Abort
		${EndIf}
	${EndIf}
FunctionEnd

Function patch
	Pop $1
	${If} $1 != ""
		ClearErrors
		ExecWait '"$OUTDIR\fcpw.exe" 4100 terminus.fon $1.txt' $R0
		${If} ${Errors}
			MessageBox MB_OK|MB_ICONEXCLAMATION "Can't run $OUTDIR\fcpw.exe"
			Abort "Can't run $OUTDIR\fcpw.exe"
		${EndIf}
		${IfThen} $R0 != 0 ${|} Abort "fcpw.exe failed with exit code $R0" ${|}
	${EndIf}
FunctionEnd

!macro PATCH arg
	push ${arg}
	Call patch
!macroend

!define Patch `!insertmacro PATCH`

Section "Install"
	${If} $instate == ${BST_CHECKED}
		InitPluginsDir
		SetOutPath $PLUGINSDIR
	${Else}
		SetOutPath $INSTDIR
	${EndIf}

	File "terminus.fon"
	File "fcpw.exe"
	File "ao2.txt"
	File "dv1.txt"
	File "ge2.txt"
	File "gq2.txt"
	File "ij1.txt"
	File "ka2.txt"
	File "ll2.txt"
	File "td1.txt"
	File "hi2.txt"
	File "hi2-dv1.txt"
	File "hi2-ka2.txt"

	${Patch} $ao2
	${Patch} $ge2
	${Patch} $gq2
	${Patch} $ij1
	${Patch} $hi2
	${Patch} $dv1
	${Patch} $ka2
	${Patch} $ll2
	${Patch} $td1

	${If} $instate == ${BST_CHECKED}
		${For} $R0 1 15
			Push "$FONTS\terminus.fon"
			System::Call "Gdi32::RemoveFontResource(t s) i.r0"
			${IfThen} $0 == 0 ${|} ${ExitFor} ${|}
		${Next}
		SendMessage ${HWND_BROADCAST} ${WM_FONTCHANGE} 0 0 /TIMEOUT=250

		${Do}
			ClearErrors
			CopyFiles "$OUTDIR\terminus.fon" "$FONTS\terminus.fon"
			${IfNotThen} ${Errors} ${|} ${Break} ${|}
			MessageBox MB_RETRYCANCEL|MB_ICONSTOP "Can't copy terminus.fon$\n\
				$\n\
				All programs using the font should be stopped." IDRETRY +2
			Abort "Can't copy terminus.fon"
		${Loop}

		${For} $R1 1 $R0
			Push "$FONTS\terminus.fon"
			System::Call "Gdi32::AddFontResource(t s) i.r0"
			${IfThen} $0 == 0 ${|} ${ExitFor} ${|}
		${Next}
		SendMessage ${HWND_BROADCAST} ${WM_FONTCHANGE} 0 0 /TIMEOUT=250

		${If} ${IsNT}
			StrCpy $1 "SOFTWARE\Microsoft\Windows NT\CurrentVersion\Fonts"
		${Else}
			StrCpy $1 "SOFTWARE\Microsoft\Windows\CurrentVersion\Fonts"
		${EndIf}
		WriteRegStr HKLM $1 "Terminus" "terminus.fon"
		ReadRegStr $0 HKLM $1 "Terminus"

		StrCpy $1 ""
		${If} $0 != "terminus.fon"
			StrCpy $1 'If the font is not available after restart, open Control \
			Panel -> Fonts, find "Terminus" and open it.$\n$\n'
		${EndIf}
		MessageBox MB_OK '$1\
			Depending on the Windows font settings, some sizes may be unavailable. \
			For example, with "Medium - 125%" fonts, 8x14 and 12x24 will likely be \
			suppressed by 8x16 and 11x22 respectively. This seems to be Windows GUI \
			problem; if you remove 8x16 and 11x12 with a resource editor, 8x14 and \
			12x24 will work.$\n\
			$\n\
			Uninstallation: stop all programs using Terminus Font, open Control \
			Panel -> Fonts, find "Terminus" and delete it. If you get an Access \
			denied error, re-login or restart the system and try again.'
	${Else}
		File "..\AUTHORS"
		File "..\CHANGES"
		File "..\OFL.TXT"
	${EndIf}
SectionEnd
