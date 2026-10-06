#ifndef AuraVersion
  #define AuraVersion "2.0.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\..\build-windows"
#endif

[Setup]
AppId={{574FDC08-2D0F-4DAD-93EE-221385984BB2}
AppName=Aura
AppVersion={#AuraVersion}
AppPublisher=Aura contributors
AppPublisherURL=https://github.com/Retroalligator/Aura
AppSupportURL=https://github.com/Retroalligator/Aura/issues
AppUpdatesURL=https://github.com/Retroalligator/Aura/releases
DefaultDirName={autopf}\Aura
DefaultGroupName=Aura
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputBaseFilename=Aura-{#AuraVersion}-Windows-x64-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
LicenseFile=..\..\LICENSE
UninstallDisplayIcon={app}\Aura.exe
CloseApplications=no
RestartApplications=no

[Types]
Name: "full"; Description: "VST3 plugin and standalone application"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plugin"; Types: full custom; Flags: fixed
Name: "standalone"; Description: "Standalone application"; Types: full

[Files]
Source: "{#BuildDir}\Aura_artefacts\Release\VST3\Aura.vst3\*"; DestDir: "{commoncf64}\VST3\Aura.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\Aura_artefacts\Release\Standalone\Aura.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "..\..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\ThirdParty\*"; DestDir: "{app}\ThirdParty"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "SOURCE.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Aura"; Filename: "{app}\Aura.exe"; Components: standalone
Name: "{autoprograms}\Uninstall Aura"; Filename: "{uninstallexe}"
