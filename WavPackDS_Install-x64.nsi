; ---------------------------------------------------------------------------
; WavPackDS install script for NSIS
; ---------------------------------------------------------------------------

!define NAME "CoreWavPack DirectShow Filters (x64)"
!define VERSION "1.6.0"
!define OUTFILE "Release\CoreWavPack-${VERSION}-Setup-x64.exe"
!define INPUT_PATH "x64\Release\"
!define FILTER_FILE1 "WavPackDSDecoder.ax"
!define FILTER_FILE2 "WavPackDSSplitter.ax"
!define UNINST_NAME "CoreWavPack-uninstall-x64.exe"

!include x64.nsh

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
  ${If} ${RunningX64}
  MessageBox MB_YESNO "This will install ${NAME}. Do you wish to continue?" IDYES gogogo
    Abort
  gogogo:
  ${Else}
  MessageBox MB_OK "This installer is for 64-bit Windows systems only"
  Abort
  ${EndIf}
FunctionEnd

; ---------------------------------------------------------------------------

Section "" ; (default section)
	SetOutPath "$SYSDIR"
	; add files / whatever that need to be installed here.
    ${DisableX64FSRedirection}
	File "${INPUT_PATH}${FILTER_FILE1}"
	File "${INPUT_PATH}${FILTER_FILE2}"
    ${EnableX64FSRedirection}

	; write out uninstaller
	WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\${NAME}" "DisplayName" "${NAME} (remove only)"
	WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\${NAME}" "UninstallString" '"$SYSDIR\${UNINST_NAME}"'
	WriteUninstaller "$SYSDIR\${UNINST_NAME}"

    ${DisableX64FSRedirection}
	ExecWait '"$SYSDIR\regsvr32.exe" /s /u "$SYSDIR\${FILTER_FILE1}"'
	ExecWait '"$SYSDIR\regsvr32.exe" /s /u "$SYSDIR\${FILTER_FILE2}"'
	ExecWait '"$SYSDIR\regsvr32.exe" /s "$SYSDIR\${FILTER_FILE1}"'
	ExecWait '"$SYSDIR\regsvr32.exe" /s "$SYSDIR\${FILTER_FILE2}"'
    ${EnableX64FSRedirection}

SectionEnd ; end of default section

; ---------------------------------------------------------------------------

; begin uninstall settings/section
UninstallText "This will uninstall ${NAME} from your system"

Section Uninstall
    ${DisableX64FSRedirection}
	ExecWait '"$SYSDIR\regsvr32.exe" /s /u "$SYSDIR\${FILTER_FILE1}"'
	ExecWait '"$SYSDIR\regsvr32.exe" /s /u "$SYSDIR\${FILTER_FILE2}"'
    ${EnableX64FSRedirection}

	; add delete commands to delete whatever files/registry keys/etc you installed here.
    ${DisableX64FSRedirection}
	Delete /REBOOTOK "$SYSDIR\${FILTER_FILE1}"
	Delete /REBOOTOK "$SYSDIR\${FILTER_FILE2}"
    ${EnableX64FSRedirection}

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
