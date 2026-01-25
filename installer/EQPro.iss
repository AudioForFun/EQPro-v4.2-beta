#define MyAppName "EQ Pro"
#define MyAppVersion "3.7.0-beta"
#define MyAppPublisher "DidAudio"
#define MyAppURL "https://github.com/AudioForFun/EQPro"
#define BuildDir "..\build\EQPro_artefacts\Release"

[Setup]
AppId={{D0D4C7A2-0C42-4E3D-9A20-4E6F25B4F2E1}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
DefaultDirName={commonpf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir={#SourcePath}\Output
OutputBaseFilename=EQPro_Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}";

[Files]
; VST3
Source: "{#BuildDir}\VST3\EQ Pro.vst3\*"; DestDir: "{commoncf}\VST3\EQ Pro.vst3"; Flags: recursesubdirs createallsubdirs

; Standalone
Source: "{#BuildDir}\Standalone\EQ Pro.exe"; DestDir: "{app}"; Flags: ignoreversion

#ifexist "{#BuildDir}\VST2\EQ Pro.dll"
; VST2 (optional if built)
Source: "{#BuildDir}\VST2\EQ Pro.dll"; DestDir: "{commonpf}\VSTPlugins"; Flags: ignoreversion
#endif

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\EQ Pro.exe"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\EQ Pro.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\EQ Pro.exe"; Description: "Launch EQ Pro"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}"
Type: filesandordirs; Name: "{commoncf}\VST3\EQ Pro.vst3"
Type: files; Name: "{commonpf}\VSTPlugins\EQ Pro.dll"


