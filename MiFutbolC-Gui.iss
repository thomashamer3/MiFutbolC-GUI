; ================================
; MiFutbolC - Instalador Oficial
; Autor: Thomas Hamer
; ================================

#define MyAppName "MiFutbolC-GUI"
#define MyAppVersion "1.0"
#define MyAppPublisher "Thomas Hamer"
#define MyAppURL "https://github.com/thomashamer3/MiFutbolC"
#define MyAppExeName "MiFutbolC_GUI.exe"

[Setup]
AppId={{B8F7E2E3-AD8C-5D9F-C8DA-234567890BCD}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}

AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}

DefaultDirName={localappdata}\Programs\{#MyAppName}
DefaultGroupName={#MyAppName}

UninstallDisplayIcon={app}\MiFutbolC.ico
LicenseFile=LICENSE

PrivilegesRequired=lowest
DisableProgramGroupPage=yes

OutputDir=installer
OutputBaseFilename=MiFutbolC_Setup
SetupIconFile=MiFutbolC.ico

Compression=lzma2
SolidCompression=yes
InternalCompressLevel=max

WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64os

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "Accesos directos"

; ================================
; ARCHIVOS
; ================================

[Files]
Source: "MiFutbolC.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "MiFutbolC.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "Manual_Usuario_MiFutbolC.pdf"; DestDir: "{app}"; Flags: ignoreversion
Source: "README.pdf"; DestDir: "{app}"; Flags: ignoreversion
Source: "LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "Icons\*"; DestDir: "{app}\Icons"; Flags: ignoreversion recursesubdirs createallsubdirs

; ================================
; DIRECTORIOS
; ================================

[Dirs]
Name: "{localappdata}\MiFutbolC\data"

; ================================
; ACCESOS DIRECTOS
; ================================

[Icons]

Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\MiFutbolC.ico"

Name: "{autoprograms}\Manual de Usuario"; Filename: "{app}\Manual_Usuario_MiFutbolC.pdf"

Name: "{autoprograms}\README"; Filename: "{app}\README.pdf"

Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon; IconFilename: "{app}\MiFutbolC.ico"

; ================================
; POST INSTALACIÓN
; ================================

[Run]

Filename: "{app}\Manual_Usuario_MiFutbolC.pdf"; Description: "Abrir manual de usuario"; Flags: postinstall shellexec skipifsilent

Filename: "{app}\{#MyAppExeName}"; Description: "Ejecutar MiFutbolC"; Flags: nowait postinstall skipifsilent

; ================================
; LIMPIEZA AL DESINSTALAR
; ================================

[UninstallDelete]

Type: filesandordirs; Name: "{localappdata}\MiFutbolC"
