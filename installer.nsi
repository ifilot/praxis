; ============================
; PyQInt-GUI NSIS Installer
; ============================
;
; Installs the program for all users. The Python environment with PyQInt is
; *not* part of the installer: it is created per user on first launch (by the
; bundled uv executable) in %LOCALAPPDATA%\IMC\PyQInt-GUI.

!define APP_NAME "PyQInt-GUI"
!ifndef APP_VERSION
  !define APP_VERSION "0.0.0"
!endif
!define APP_PUBLISHER "Ivo Filot"
!define APP_EXE "pyqint-gui.exe"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"
!define USER_DATA_DIR "$LOCALAPPDATA\IMC\${APP_NAME}"

Unicode true
Name "${APP_NAME} ${APP_VERSION}"
OutFile "${APP_NAME}-Windows-Setup.exe"
InstallDir "$PROGRAMFILES64\${APP_NAME}"
InstallDirRegKey HKLM "Software\${APP_NAME}" "InstallDir"
Icon "assets\icons\pyqint-gui.ico"
UninstallIcon "assets\icons\pyqint-gui.ico"

RequestExecutionLevel admin

Page directory
Page instfiles
UninstPage uninstConfirm
UninstPage instfiles

Section "Install"
    SetOutPath "$INSTDIR"

    ; Copy everything from staging directory
    File /r "dist\${APP_NAME}\*"

    ; Write uninstall information
    WriteRegStr HKLM "Software\${APP_NAME}" "InstallDir" "$INSTDIR"
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayName" "${APP_NAME}"
    WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayVersion" "${APP_VERSION}"
    WriteRegStr HKLM "${UNINSTALL_KEY}" "Publisher" "${APP_PUBLISHER}"
    WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\${APP_EXE}"
    WriteRegStr HKLM "${UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoModify" 1
    WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoRepair" 1

    ; Start menu shortcut
    CreateDirectory "$SMPROGRAMS\${APP_NAME}"
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"

    ; Desktop shortcut
    CreateShortcut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"
SectionEnd

Section "Uninstall"
    Delete "$INSTDIR\${APP_EXE}"
    RMDir /r "$INSTDIR"

    Delete "$DESKTOP\${APP_NAME}.lnk"
    RMDir /r "$SMPROGRAMS\${APP_NAME}"

    DeleteRegKey HKLM "Software\${APP_NAME}"
    DeleteRegKey HKLM "${UNINSTALL_KEY}"

    ; The Python environment and the calculation results are stored per user
    IfFileExists "${USER_DATA_DIR}\*.*" 0 done
        MessageBox MB_YESNO|MB_ICONQUESTION \
            "Also remove the Python environment and all calculation results of the current user?$\r$\n$\r$\n${USER_DATA_DIR}" \
            /SD IDNO IDNO done
        RMDir /r "${USER_DATA_DIR}"
    done:
SectionEnd
