#define AppName "Windows 11 Power Slider"
#define AppVersion "1.0.3"
#define AppPublisher "Bozhen Peng"
#define AppExeName "PowerModeNative.exe"
#define AppIconName "PowerSlider-glass-dial-1.ico"

[Setup]
AppId={{C6092D9F-9F26-4F4D-A882-D44D82E8C8D0}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL=https://github.com/kiteretsu903/windows-11-power-slider
AppSupportURL=https://github.com/kiteretsu903/windows-11-power-slider/issues
AppUpdatesURL=https://github.com/kiteretsu903/windows-11-power-slider/releases
DefaultDirName={localappdata}\Programs\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
UsePreviousAppDir=no
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
CloseApplicationsFilter={#AppExeName}
RestartApplications=no
; Notify Explorer to invalidate cached icons after replacing the executable.
ChangesAssociations=yes
CreateUninstallRegKey=yes
Uninstallable=yes
UninstallFilesDir={app}
UninstallDisplayIcon={app}\{#AppIconName}
UninstallDisplayName={#AppName}
VersionInfoVersion={#AppVersion}.0
VersionInfoProductName={#AppName}
VersionInfoProductVersion={#AppVersion}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "..\dist\app\{#AppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\src\PowerModeNative.ico"; DestDir: "{app}"; DestName: "{#AppIconName}"; Flags: ignoreversion
Source: "..\assets\fluent\LICENSE.txt"; DestDir: "{app}"; DestName: "Fluent-Icons-LICENSE.txt"; Flags: ignoreversion

[InstallDelete]
Type: filesandordirs; Name: "{localappdata}\Programs\PowerModeNative"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: checkedonce

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"; IconFilename: "{app}\{#AppIconName}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; IconFilename: "{app}\{#AppIconName}"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "PowerModeNative"; ValueData: """{app}\{#AppExeName}"" --startup"; Flags: uninsdeletevalue; Check: ShouldRegisterStartup
Root: HKCU; Subkey: "Software\PowerModeNative"; ValueType: none; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\PowerModeNative"; ValueType: none; ValueName: "Language"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\PowerModeNative"; ValueType: dword; ValueName: "StartupInitialized"; ValueData: "1"; Flags: uninsdeletevalue; Check: ShouldRegisterStartup

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{app}\{#AppExeName}"; Parameters: "--shutdown"; Flags: runhidden waituntilterminated; RunOnceId: "StopPowerSlider"

[Code]
const
  WM_COMMAND = $0111;
  ExitCommand = 1002;

function ShouldRegisterStartup: Boolean;
var
  Initialized: Cardinal;
begin
  Result := RegValueExists(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Run', 'PowerModeNative') or
    (not RegQueryDWordValue(HKCU, 'Software\PowerModeNative', 'StartupInitialized', Initialized)) or
    (Initialized = 0);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  AppWindow: HWND;
  Attempt: Integer;
begin
  Result := '';
  AppWindow := FindWindowByClassName('PowerModeNative.Flyout');
  if AppWindow <> 0 then
  begin
    SendMessage(AppWindow, WM_COMMAND, ExitCommand, 0);
    for Attempt := 1 to 50 do
    begin
      if FindWindowByClassName('PowerModeNative.Flyout') = 0 then
        exit;
      Sleep(100);
    end;
    Result := 'Windows 11 Power Slider could not be closed. Please exit it from the tray and try again.';
  end;
end;
