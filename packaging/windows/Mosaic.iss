; Mosaic — Windows installer (Inno Setup 6)
; Build: scripts\package-installer.bat  or  release.bat

#ifndef AppVersion
  #define AppVersion "0.2.1"
#endif

#ifndef SourceDir
  #define SourceDir "..\..\dist\Mosaic_" + AppVersion + "_win64"
#endif

#ifndef OutputDir
  #define OutputDir "..\..\dist\artifacts"
#endif

#ifndef AppIconFile
  #define AppIconFile "..\..\resources\app-icon.ico"
#endif

[Setup]
AppId={{B7E4A2C1-9D3F-4E8A-B6C5-1F0E2D3A4B5C}
AppName=Mosaic
AppVersion={#AppVersion}
AppVerName=Mosaic {#AppVersion}
AppPublisher=MeowYewy
DefaultDirName={autopf}\Mosaic
DefaultGroupName=Mosaic
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=Mosaic_{#AppVersion}_win64_Setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayName=Mosaic {#AppVersion}
CloseApplications=force
CloseApplicationsFilter=Mosaic.exe
AppMutex=MosaicAppMutex
SetupIconFile={#AppIconFile}
UninstallDisplayIcon={app}\Mosaic.exe

[Languages]
Name: "chinesesimplified"; MessagesFile: "languages\ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Mosaic"; Filename: "{app}\Mosaic.exe"; IconFilename: "{app}\Mosaic.exe"
Name: "{group}\{cm:UninstallProgram,Mosaic}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Mosaic"; Filename: "{app}\Mosaic.exe"; IconFilename: "{app}\Mosaic.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Mosaic.exe"; Description: "{cm:LaunchProgram,Mosaic}"; Flags: nowait postinstall

[UninstallDelete]
Type: filesandordirs; Name: "{app}"
