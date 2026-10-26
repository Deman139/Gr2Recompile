; Gravity Rush 2 port installer (Inno Setup 6). Built by package_gr2.sh from its staging folder:
;   ISCC /DAppVersion=4.72e /DNumericVersion=4.72 /DSourceDir=...\out\gr2-stage\Gravity Rush 2 /DOutputDir=...\dist /DPortDir=... gr2_setup.iss
; Installs for the current user without administrator rights: the launcher keeps its settings,
; saves, logs and the game/, game\addcont\ and sys_modules\ folders next to itself, so it goes into
; a folder the player can write (default %LOCALAPPDATA%\Programs\Gravity Rush 2), never Program Files.
; No game files, DLC or PS4 system modules are included. Uninstalling removes only what was
; installed; saves and settings (user\) go only when the player says so; the game, DLC and modules
; the player copied in stay.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
; The Windows file version takes digits only (4.72e -> 4.72).
#ifndef NumericVersion
  #define NumericVersion "0.0.0"
#endif
#ifndef SourceDir
  #error Pass /DSourceDir=<staging folder> (package_gr2.sh does)
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif
#ifndef PortDir
  #define PortDir "..\.."
#endif
#define AppName "Gravity Rush 2"
#define AppExe "Gravity Rush 2.exe"

[Setup]
AppId={{2EDC43A3-424A-47EA-939A-548A0C676859}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} (PC port) {#AppVersion}
AppPublisher=Gr2Recompile
VersionInfoVersion={#NumericVersion}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
UsePreviousAppDir=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputDir}
OutputBaseFilename=GravityRush2-Setup-v{#AppVersion}
SetupIconFile={#PortDir}\launcher\gr2_launcher.ico
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName} (PC port)
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
LicenseFile={#PortDir}\LICENSE

[Languages]
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
ru.DesktopIcon=Ярлык на рабочем столе
ru.LaunchNow=Открыть лаунчер
ru.ProtectedFolder=Эта папка защищена Windows (Program Files или Windows): лаунчер не сможет сохранять там настройки, сохранения и логи. Выбери другую папку, например в %LOCALAPPDATA%\Programs или на диске D:.
ru.RemoveUser=Удалить также сохранения, настройки, кеш шейдеров и логи (папка user)?%n%nИгра, DLC и системные модули в папках game и sys_modules в любом случае остаются.
ru.ReadyNote=После установки скопируй игру в папку game, DLC (.pkg) в game\addcont, а системные модули PS4 (.sprx) в sys_modules, или укажи свои папки в лаунчере.
en.DesktopIcon=Desktop shortcut
en.LaunchNow=Open the launcher
en.ProtectedFolder=Windows protects this folder (Program Files or Windows): the launcher could not keep its settings, saves and logs there. Choose another folder, for example in %LOCALAPPDATA%\Programs or on drive D:.
en.RemoveUser=Also remove the saves, settings, shader cache and logs (the user folder)?%n%nThe game, DLC and system modules in the game and sys_modules folders stay in any case.
en.ReadyNote=After installing, copy the game into the game folder, the DLC (.pkg) into game\addcont and the PS4 system modules (.sprx) into sys_modules, or point the launcher at your own folders.

[Tasks]
Name: "desktopicon"; Description: "{cm:DesktopIcon}"

[Dirs]
; Made by the installer, never removed by the uninstaller: the player's own files live there.
Name: "{app}\game"; Flags: uninsneveruninstall
Name: "{app}\game\addcont"; Flags: uninsneveruninstall
Name: "{app}\sys_modules"; Flags: uninsneveruninstall

[Files]
; Everything staged, the notes in game\, game\addcont\ and sys_modules\ included.
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"; WorkingDir: "{app}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchNow}"; WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Made while playing next to the installed files (Python bytecode); user\ is asked about below.
Type: filesandordirs; Name: "{app}\scripts\__pycache__"

[Code]
function IsProtected(Dir: String): Boolean;
var
  D: String;
begin
  D := Lowercase(AddBackslash(Dir));
  Result := (Pos(Lowercase(AddBackslash(ExpandConstant('{commonpf64}'))), D) = 1) or
            (Pos(Lowercase(AddBackslash(ExpandConstant('{commonpf32}'))), D) = 1) or
            (Pos(Lowercase(AddBackslash(ExpandConstant('{win}'))), D) = 1);
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = wpSelectDir) and IsProtected(WizardDirValue) then
  begin
    MsgBox(ExpandConstant('{cm:ProtectedFolder}'), mbError, MB_OK);
    Result := False;
  end;
end;

function UpdateReadyMemo(Space, NewLine, MemoUserInfoInfo, MemoDirInfo, MemoTypeInfo,
  MemoComponentsInfo, MemoGroupInfo, MemoTasksInfo: String): String;
begin
  Result := MemoDirInfo + NewLine + NewLine;
  if MemoTasksInfo <> '' then
    Result := Result + MemoTasksInfo + NewLine + NewLine;
  Result := Result + ExpandConstant('{cm:ReadyNote}');
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  UserDir: String;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    UserDir := ExpandConstant('{app}\user');
    if DirExists(UserDir) and not UninstallSilent and
       (MsgBox(ExpandConstant('{cm:RemoveUser}'), mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES) then
      DelTree(UserDir, True, True, True);
    { The install folder goes when nothing the player put there is left. }
    RemoveDir(ExpandConstant('{app}\game\addcont'));
    RemoveDir(ExpandConstant('{app}\game'));
    RemoveDir(ExpandConstant('{app}\sys_modules'));
    RemoveDir(ExpandConstant('{app}'));
  end;
end;
