; Delex by Piradex — instalador Windows (Inno Setup)
#define MyName "Delex"
#define MyVersion "1.0.0"
#define MyPublisher "Ricardo Amorim / Juninho Piradex"
#ifndef VST3SRC
  #define VST3SRC "out\Delex.vst3"
#endif
#ifndef OUTDIR
  #define OUTDIR "."
#endif
[Setup]
AppName={#MyName}
AppVersion={#MyVersion}
AppPublisher={#MyPublisher}
AppPublisherURL=https://www.beatfreakstudio.com
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
UninstallDisplayName={#MyName} (VST3)
OutputBaseFilename=Delex-Installer-Windows
OutputDir={#OUTDIR}
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
[Messages]
WelcomeLabel2=Este assistente vai instalar o {#MyName} (VST3) no teu sistema.%n%nProfessional Audio Plugin | Beatfreak Music%nCriador e Desenvolvedor: {#MyPublisher}%nVersao {#MyVersion}
[Files]
Source: "{#VST3SRC}\*"; DestDir: "{commoncf64}\VST3\Delex.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Delex.vst3"
