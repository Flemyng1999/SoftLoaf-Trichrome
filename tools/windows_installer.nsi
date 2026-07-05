Unicode true
ManifestDPIAware true
RequestExecutionLevel admin

!ifndef APP_VERSION
  !define APP_VERSION "0.2.0-beta.3"
!endif
!ifndef APP_VERSION_QUAD
  !define APP_VERSION_QUAD "0.2.0.3"
!endif
!ifndef APP_STAGE
  !error "APP_STAGE must be provided"
!endif
!ifndef APP_OUT
  !define APP_OUT "SoftLoaf-Trichrome-${APP_VERSION}-Windows-x64-Setup.exe"
!endif
!ifndef APP_ICON
  !define APP_ICON "..\resources\AppIcon.ico"
!endif

!define APP_NAME "SoftLoaf Trichrome"
!define APP_PUBLISHER "SoftLoaf"
!define APP_EXE "SoftLoaf Trichrome.exe"
!define APP_REG_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoftLoaf Trichrome"

Name "${APP_NAME}"
OutFile "${APP_OUT}"
InstallDir "$PROGRAMFILES64\${APP_NAME}"
InstallDirRegKey HKLM "${APP_REG_KEY}" "InstallLocation"
Icon "${APP_ICON}"
UninstallIcon "${APP_ICON}"

VIProductVersion "${APP_VERSION_QUAD}"
VIAddVersionKey "ProductName" "${APP_NAME}"
VIAddVersionKey "CompanyName" "${APP_PUBLISHER}"
VIAddVersionKey "FileDescription" "${APP_NAME} Installer"
VIAddVersionKey "FileVersion" "${APP_VERSION}"
VIAddVersionKey "ProductVersion" "${APP_VERSION}"

Page directory
Page instfiles
UninstPage uninstConfirm
UninstPage instfiles

Section "Install"
  SetShellVarContext all
  SetOutPath "$INSTDIR"
  File /r "${APP_STAGE}\*"

  CreateDirectory "$SMPROGRAMS\${APP_NAME}"
  CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"
  CreateShortcut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"

  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "${APP_REG_KEY}" "DisplayName" "${APP_NAME}"
  WriteRegStr HKLM "${APP_REG_KEY}" "DisplayVersion" "${APP_VERSION}"
  WriteRegStr HKLM "${APP_REG_KEY}" "Publisher" "${APP_PUBLISHER}"
  WriteRegStr HKLM "${APP_REG_KEY}" "DisplayIcon" "$INSTDIR\${APP_EXE}"
  WriteRegStr HKLM "${APP_REG_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${APP_REG_KEY}" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegDWORD HKLM "${APP_REG_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${APP_REG_KEY}" "NoRepair" 1
SectionEnd

Section "Uninstall"
  SetShellVarContext all
  Delete "$DESKTOP\${APP_NAME}.lnk"
  RMDir /r "$SMPROGRAMS\${APP_NAME}"
  DeleteRegKey HKLM "${APP_REG_KEY}"
  RMDir /r "$INSTDIR"
SectionEnd
