#define MyAppName "TrenchKit"
#define MyAppPublisher "Tapawingo"
#define MyAppURL "https://github.com/Tapawingo/TrenchKit"

#ifndef AppVersion
  #error AppVersion is not defined.
#endif

#ifndef SourceDir
  #error SourceDir is not defined.
#endif

#ifndef OutputDir
  #error OutputDir is not defined.
#endif

#ifndef OutputBaseFilename
  #define OutputBaseFilename "TrenchKit-Setup"
#endif

#ifndef ProjectRoot
  #define ProjectRoot ".."
#endif

[Setup]
AppId={{C9FD03E0-31F7-4E12-90D2-950B57865E23}
AppName={#MyAppName}
AppVersion={#AppVersion}
AppVerName={#MyAppName} {#AppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}/releases
DefaultDirName={localappdata}\Programs\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
LicenseFile={#ProjectRoot}\LICENSE.md
OutputDir={#OutputDir}
OutputBaseFilename={#OutputBaseFilename}
SetupIconFile={#ProjectRoot}\app.ico
UninstallDisplayIcon={app}\TrenchKit.exe
WizardImageFile={#ProjectRoot}\extras\installer\wizard-sidebar.bmp
WizardSmallImageFile={#ProjectRoot}\extras\installer\wizard-small.bmp
Compression=lzma
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
UsePreviousAppDir=yes
UsePreviousTasks=yes
CloseApplications=yes
RestartApplications=no
AppMutex=Global\TrenchKitRunning

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\TrenchKit.exe"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\TrenchKit.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\TrenchKit.exe"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent
