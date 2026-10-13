#ifndef AppBinary
  #define AppBinary "..\out\build\vs2022-x64\Release\NativeIsland.exe"
#endif
[Setup]
AppName=NativeIsland
AppId=NativeIsland
AppVersion=0.8.7
DefaultDirName={localappdata}\Programs\NativeIsland
PrivilegesRequired=lowest
DefaultGroupName=NativeIsland
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputBaseFilename=NativeIsland-Setup
Compression=lzma2
SolidCompression=yes

[Files]
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\assets\LUCIDE-LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\docs\google-calendar-setup.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\docs\tasks-and-sound.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#AppBinary}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\NativeIsland"; Filename: "{app}\NativeIsland.exe"

[Run]
Filename: "{app}\NativeIsland.exe"; Description: "NativeIsland'ı başlat"; Flags: nowait postinstall skipifsilent

