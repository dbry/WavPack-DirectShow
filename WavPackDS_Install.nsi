; ---------------------------------------------------------------------------
; WavPackDS install script for NSIS
; ---------------------------------------------------------------------------

!define NAME "CoreWavPack DirectShow Filters"
!define VERSION "1.2.0"
!define OUTFILE "Release\CoreWavPack-${VERSION}-Setup.exe"
!define INPUT_PATH "Release\"
!define FILTER_FILE1 "WavPackDSDecoder.ax"
!define FILTER_FILE2 "WavPackDSSplitter.ax"
!define UNINST_NAME "CoreWavPack-uninstall.exe"

; ---------------------------------------------------------------------------
; NOTE: this .NSI script is designed for NSIS v1.8+
; ---------------------------------------------------------------------------

Name "${NAME}"
OutFile "${OUTFILE}"
BrandingText " "

SetOverwrite ifnewer
ShowInstDetails show
SetDateSave on
ShowUninstDetails show
XpStyle on

InstallColors /windows
InstProgressFlags smooth

; ---------------------------------------------------------------------------

Function .onInit
  MessageBox MB_YESNO "This will install ${NAME}. Do you wish to continue?" IDYES gogogo
    Abort
  gogogo:
FunctionEnd

; ---------------------------------------------------------------------------

Section "" ; (default section)
	SetOutPath "$SYSDIR"
	; add files / whatever that need to be installed here.
	File "${INPUT_PATH}${FILTER_FILE1}"
	File "${INPUT_PATH}${FILTER_FILE2}"

	; write out uninstaller
	WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\${NAME}" "DisplayName" "${NAME} (remove only)"
	WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\${NAME}" "UninstallString" '"$SYSDIR\${UNINST_NAME}"'
	WriteUninstaller "$SYSDIR\${UNINST_NAME}"

	UnRegDLL "$SYSDIR\${FILTER_FILE1}"
	UnRegDLL "$SYSDIR\${FILTER_FILE2}"

	RegDLL "$SYSDIR\${FILTER_FILE1}"
	RegDLL "$SYSDIR\${FILTER_FILE2}"
SectionEnd ; end of default section

; ---------------------------------------------------------------------------

; begin uninstall settings/section
UninstallText "This will uninstall ${NAME} from your system"

Section Uninstall
	UnRegDLL "$SYSDIR\${FILTER_FILE1}"
	UnRegDLL "$SYSDIR\${FILTER_FILE2}"
	; add delete commands to delete whatever files/registry keys/etc you installed here.
	Delete /REBOOTOK "$SYSDIR\${FILTER_FILE1}"
	Delete /REBOOTOK "$SYSDIR\${FILTER_FILE2}"
	Delete "$SYSDIR\${UNINST_NAME}"
   
	DeleteRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\${NAME}"
SectionEnd ; end of uninstall section

; ---------------------------------------------------------------------------

Function un.onUninstSuccess
	IfRebootFlag 0 NoReboot
		MessageBox MB_OK \ 
			"A file couldn't be deleted. It will be deleted at next reboot."
	NoReboot:
FunctionEnd

; ---------------------------------------------------------------------------
; eof
; ---------------------------------------------------------------------------
