#define AppName "Windows 11 Power Slider"
#define AppVersion "1.0.0"
#define AppPublisher "Bozhen Peng"
#define AppExeName "PowerModeNative.exe"

[Setup]
AppId={{C6092D9F-9F26-4F4D-A882-D44D82E8C8D0}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={localappdata}\Programs\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\dist
OutputBaseFilename=Windows11PowerSlider-Setup-{#AppVersion}-x64
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
SetupIconFile=..\src\PowerModeNative.ico
CloseApplications=yes
RestartApplications=no
UninstallDisplayIcon={app}\{#AppExeName}
UninstallDisplayName={#AppName}
VersionInfoVersion={#AppVersion}.0
VersionInfoProductName={#AppName}
VersionInfoProductVersion={#AppVersion}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "..\dist\app\{#AppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\assets\fluent\LICENSE.txt"; DestDir: "{app}"; DestName: "Fluent-Icons-LICENSE.txt"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: none; ValueName: "PowerModeNative"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\PowerModeNative"; ValueType: none; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\PowerModeNative"; ValueType: none; ValueName: "Language"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\PowerModeNative"; ValueType: none; ValueName: "StartupInitialized"; Flags: uninsdeletevalue

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
