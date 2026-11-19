; Inno Setup script. Run from the project root after a Release build:  ISCC Installer\AFTERIMAGE.iss
#define AppName "AFTERIMAGE"
#define AppVersion "1.0.0"

[Setup]
AppId={{B3C9A1E2-7D4F-4C58-9A21-AF7E21A6C001}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=AFTERIMAGE Audio
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir=Output
OutputBaseFilename=AFTERIMAGE-{#AppVersion}-Setup
Compression=lzma2
SolidCompression=yes
UninstallFilesDir={commonappdata}\AFTERIMAGE

[Files]
; A Windows VST3 is a folder bundle: AFTERIMAGE.vst3\Contents\x86_64-win\AFTERIMAGE.vst3
Source: "..\build\AFTERIMAGE_artefacts\Release\VST3\AFTERIMAGE.vst3\*"; DestDir: "{commoncf64}\VST3\AFTERIMAGE.vst3"; Flags: recursesubdirs ignoreversion
Source: "..\README.md"; DestDir: "{commonappdata}\AFTERIMAGE"; Flags: ignoreversion
Source: "..\Presets\*"; DestDir: "{commonappdata}\AFTERIMAGE\FactoryPresetFiles"; Flags: recursesubdirs ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\AFTERIMAGE.vst3"
; User presets in %APPDATA%\AFTERIMAGE\Presets are deliberately left in place.
