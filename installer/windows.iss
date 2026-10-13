; Instalador Windows do DELEX by Piradex (Inno Setup 6)
; Compilado no GitHub Actions: ISCC /DVersao=2.0.0 installer\windows.iss

#ifndef Versao
  #define Versao "2.0.0"
#endif

[Setup]
AppId={{B2D7E4A1-5C3F-4E8B-9A61-7D0C2F9E4B35}
AppName=DELEX by Piradex
AppVersion={#Versao}
AppVerName=DELEX by Piradex {#Versao}
AppPublisher=Piradex
AppPublisherURL=https://www.beatfreakstudio.com
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=no
UninstallFilesDir={commonpf64}\Piradex\DELEX
UninstallDisplayName=DELEX by Piradex
OutputDir=..\out-win
OutputBaseFilename=DELEX-by-Piradex-{#Versao}-Windows-Setup
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
MinVersion=10.0
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
InfoBeforeFile=LEIA-ME-Windows.txt

[Languages]
Name: "pt"; MessagesFile: "compiler:Languages\Portuguese.isl"
Name: "ptbr"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "..\build\Delex_artefacts\Release\VST3\DELEX by Piradex.vst3\*"; \
  DestDir: "{commoncf64}\VST3\DELEX by Piradex.vst3"; \
  Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\DELEX by Piradex.vst3"
