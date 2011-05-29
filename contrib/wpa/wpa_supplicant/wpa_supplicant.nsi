!define PRODUCT_NAME "wpa_supplicant"
!define PRODUCT_VERSION "@WPAVER@"
!define PRODUCT_PUBLISHER "Jouni Malinen"

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
outfile "../wpa_supplicant-@WPAVER@.exe"

installDir "$PROGRAMFILES\wpa_supplicant"

Page Directory
Page InstFiles

section -Prerequisites
	SetOutPath $INSTDIR\Prerequisites
	MessageBox MB_YESNO "Install WinPcap?" /SD IDYES IDNO endWinPcap
		File "/opt/Qt-Win/files/WinPcap_4_1_2.exe"
		ExecWait "$INSTDIR\Prerequisites\WinPcap_4_1_2.exe"
		Goto endWinPcap
	endWinPcap:
sectionEnd


section
	setOutPath $INSTDIR

	File wpa_gui.exe
	File wpa_gui_de.qm
	File wpa_cli.exe
	File COPYING
	File README
	File README-Windows.txt
	File win_example.reg
	File win_if_list.exe
	File wpa_passphrase.exe
	File wpa_supplicant.conf
	File wpa_supplicant.exe
	File wpasvc.exe

	File /opt/Qt-Win/files/mingwm10.dll
	File /opt/Qt-Win/files/libgcc_s_dw2-1.dll
	File /opt/Qt-Win/files/QtCore4.dll
	File /opt/Qt-Win/files/QtGui4.dll

	WriteRegDWORD HKLM "Software\wpa_supplicant" "debug_level" 0
	WriteRegDWORD HKLM "Software\wpa_supplicant" "debug_show_keys" 0
	WriteRegDWORD HKLM "Software\wpa_supplicant" "debug_timestamp" 0
	WriteRegDWORD HKLM "Software\wpa_supplicant" "debug_use_file" 0

	WriteRegDWORD HKLM "Software\wpa_supplicant\configs\default" "ap_scan" 2
	WriteRegDWORD HKLM "Software\wpa_supplicant\configs\default" "update_config" 1
	WriteRegDWORD HKLM "Software\wpa_supplicant\configs\default\networks" "dummy" 1
	DeleteRegValue HKLM "Software\wpa_supplicant\configs\default\networks" "dummy"

	WriteRegDWORD HKLM "Software\wpa_supplicant\interfaces" "dummy" 1
	DeleteRegValue HKLM "Software\wpa_supplicant\interfaces" "dummy"

	writeUninstaller "$INSTDIR\uninstall.exe"

	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\wpa_supplicant" \
		"DisplayName" "wpa_supplicant"
WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\wpa_supplicant" \
		"UninstallString" "$INSTDIR\uninstall.exe"

	CreateDirectory "$SMPROGRAMS\wpa_supplicant"
	CreateShortCut "$SMPROGRAMS\wpa_supplicant\wpa_gui.lnk" "$INSTDIR\wpa_gui.exe"
	CreateShortCut "$SMPROGRAMS\wpa_supplicant\Uninstall.lnk" "$INSTDIR\uninstall.exe"

	ExecWait "$INSTDIR\wpasvc.exe reg"
sectionEnd


Function un.onInit
	MessageBox MB_YESNO "This will uninstall wpa_supplicant. Continue?" IDYES NoAbort
	Abort
  NoAbort:
FunctionEnd

section "uninstall"
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\wpa_supplicant"
	delete "$INSTDIR\uninstall.exe"

	ExecWait "$INSTDIR\wpasvc.exe unreg"

	DeleteRegKey HKLM "Software\wpa_supplicant"

	delete "$INSTDIR\wpa_gui.exe"
	delete "$INSTDIR\wpa_gui_de.qm"
	delete "$INSTDIR\wpa_cli.exe"
	delete "$INSTDIR\COPYING"
	delete "$INSTDIR\README"
	delete "$INSTDIR\README-Windows.txt"
	delete "$INSTDIR\win_example.reg"
	delete "$INSTDIR\win_if_list.exe"
	delete "$INSTDIR\wpa_passphrase.exe"
	delete "$INSTDIR\wpa_supplicant.conf"
	delete "$INSTDIR\wpa_supplicant.exe"
	delete "$INSTDIR\wpasvc.exe"

	delete "$INSTDIR\mingwm10.dll"
	delete "$INSTDIR\libgcc_s_dw2-1.dll"
	delete "$INSTDIR\QtCore4.dll"
	delete "$INSTDIR\QtGui4.dll"

	delete "$INSTDIR\Prerequisites\WinPcap_4_1_2.exe"
	rmdir "$INSTDIR\Prerequisites"

	rmdir "$INSTDIR"

	delete "$SMPROGRAMS\wpa_supplicant\wpa_gui.lnk"
	delete "$SMPROGRAMS\wpa_supplicant\Uninstall.lnk"
	rmdir "$SMPROGRAMS\wpa_supplicant"
sectionEnd
