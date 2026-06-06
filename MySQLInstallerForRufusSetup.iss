; =============================================================================
;  MySQL Installer for Rufus — Script Inno Setup 6
;  https://jrsoftware.org/isinfo.php
;
;  Prérequis : avoir lancé build_windows.ps1 au préalable.
;  Le dossier source (SourceDir) est passé par build_windows.ps1 via :
;      iscc MySQLInstallerForRufusSetup.iss /DSourceDir=<chemin>
;  Il peut aussi être compilé manuellement depuis l'IDE Inno Setup
;  (dans ce cas, ajustez SourceDir ci-dessous).
; =============================================================================

#ifndef SourceDir
  ; Valeur par défaut si on compile à la main (chemin relatif depuis ce fichier)
  #define SourceDir "build-windows\release"
#endif

#define AppName      "MySQL Installer for Rufus"
#define AppVersion   "1.0"
#define AppPublisher "Rufus / Serge Lainé"
#define AppExeName   "MySQLInstaller.exe"
#define AppURL       "https://github.com/ukinoki/mysqlinstaller-for-rufus"

[Setup]
; GUID unique de l'application — NE PAS modifier après la première distribution
AppId={{F3A7C2B1-8E4D-4F9A-B631-2DC5E8A04F17}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}

; Répertoire d'installation
DefaultDirName={autopf}\MySQLInstallerForRufus
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes

; Sortie
OutputDir=dist
OutputBaseFilename=MySQLInstallerForRufusSetup
SetupIconFile=

; Compression
Compression=lzma2/ultra64
SolidCompression=yes

; UAC — l'installeur demande les droits admin pour écrire dans Program Files
PrivilegesRequired=admin

; Apparence
WizardStyle=modern
WizardResizable=no

; Pas de redémarrage nécessaire en temps normal
RestartIfNeededByRun=no

; Infos affichées dans Paramètres > Applications
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\{#AppExeName}

[Languages]
Name: "french";  MessagesFile: "compiler:Languages\French.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; ── Exécutable principal ──────────────────────────────────────────────────────
Source: "{#SourceDir}\MySQLInstaller.exe"; DestDir: "{app}"; Flags: ignoreversion

; ── DLL Qt + runtime VC++ (copiées par windeployqt) ──────────────────────────
Source: "{#SourceDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

; ── Plugins Qt (sous-dossiers créés par windeployqt) ─────────────────────────
Source: "{#SourceDir}\platforms\*";        DestDir: "{app}\platforms";        Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "{#SourceDir}\styles\*";           DestDir: "{app}\styles";           Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "{#SourceDir}\imageformats\*";     DestDir: "{app}\imageformats";     Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "{#SourceDir}\iconengines\*";      DestDir: "{app}\iconengines";      Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "{#SourceDir}\networkinformation\*"; DestDir: "{app}\networkinformation"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "{#SourceDir}\tls\*";              DestDir: "{app}\tls";              Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "{#SourceDir}\generic\*";          DestDir: "{app}\generic";          Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

[Icons]
; Menu Démarrer
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"; \
    Comment: "Installe et configure MySQL pour Rufus"
Name: "{group}\Désinstaller {#AppName}"; Filename: "{uninstallexe}"

; Raccourci Bureau (optionnel — proposé à l'utilisateur)
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; \
    Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Créer un raccourci sur le Bureau"; \
    GroupDescription: "Raccourcis :"; Flags: unchecked

[Run]
; Proposer de lancer l'application à la fin de l'installation.
; L'exe embarque un manifeste UAC requireAdministrator : Windows affiche
; l'invite UAC automatiquement au lancement.
Filename: "{app}\{#AppExeName}"; \
    Description: "Lancer {#AppName} maintenant"; \
    Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Nettoyage complet du dossier d'installation à la désinstallation
Type: filesandordirs; Name: "{app}"
