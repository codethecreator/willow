#define MyAppName "LunarField"
#define MyAppVersion "0.4.0"
[Setup]
AppId={{BD99DBFA-0BC1-4B47-B6A1-32CFE0E08642}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher=Independent Audio
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=LunarField-Setup-Windows-x64
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
[Files]
Source: "..\build\LunarField_artefacts\Release\VST3\LunarField.vst3\*"; DestDir: "{app}\LunarField.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
