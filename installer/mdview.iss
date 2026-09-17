; mdview Inno Setup Script
; Build with Inno Setup 6.3+ (https://jrsoftware.org/isinfo.php)

[Setup]
AppId={{B5E47C8A-3F2D-4E6A-9C1B-7D8F0A2E5B3C}}
AppName=mdview
AppVersion=0.1.0
AppPublisher=iamuday2006
AppPublisherURL=https://github.com/iamuday2006/mdview
AppSupportURL=https://github.com/iamuday2006/mdview
AppUpdatesURL=https://github.com/iamuday2006/mdview/releases
AppCopyright=Copyright (c) 2026 iamuday2006
DefaultGroupName=mdview
DefaultDirName={commonpf}\mdview
LicenseFile=..\LICENSE
OutputDir=..\installer\output
OutputBaseFilename=mdview-win32-x64-0.1.0
SetupIconFile=..\installer\mdview.ico
UninstallDisplayIcon={app}\mdview.exe
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
WizardSizePercent=110
SetupLogging=yes
UninstallLogging=yes
DisableProgramGroupPage=auto
DisableDirPage=auto
AllowCancelDuringInstall=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Messages]
SetupAppTitle=Setup - mdview

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked
Name: "addtopath"; Description: "Add mdview to your &PATH environment variable"; GroupDescription: "System integration:"; Flags: checkedonce
Name: "associate_md"; Description: "Associate &.md files with mdview"; GroupDescription: "File associations:"; Flags: unchecked

[Files]
Source: "..\build\Release\mdview.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\mdview"; Filename: "{app}\mdview.exe"
Name: "{group}\Uninstall mdview"; Filename: "{uninstallexe}"
Name: "{autodesktop}\mdview"; Filename: "{app}\mdview.exe"; Tasks: desktopicon

[Registry]
; File association: .md (with uninsdeletekey for clean uninstall)
Root: HKLM; Subkey: "Software\Classes\.md"; ValueType: string; ValueName: ""; ValueData: "mdview.md"; Flags: createvalueifdoesntexist uninsdeletekey; Tasks: associate_md
Root: HKLM; Subkey: "Software\Classes\mdview.md"; ValueType: string; ValueName: ""; ValueData: "Markdown File"; Flags: createvalueifdoesntexist uninsdeletekey; Tasks: associate_md
Root: HKLM; Subkey: "Software\Classes\mdview.md\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\mdview.exe,0"; Flags: createvalueifdoesntexist uninsdeletekey; Tasks: associate_md
Root: HKLM; Subkey: "Software\Classes\mdview.md\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\mdview.exe"" ""%1"""; Flags: createvalueifdoesntexist uninsdeletekey; Tasks: associate_md

[Run]
Filename: "{app}\mdview.exe"; Parameters: "--help"; Description: "View mdview help"; Flags: postinstall nowait skipifsilent shellexec; Tasks: addtopath

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Code]
const
  PATH_KEY = 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment';
  PATH_VALUE = 'Path';

function PosEx(const SubStr, S: String; StartPos: Integer): Integer;
var
  I: Integer;
begin
  Result := 0;
  if (StartPos < 1) or (StartPos > Length(S)) then Exit;
  for I := StartPos to Length(S) - Length(SubStr) + 1 do
  begin
    if Copy(S, I, Length(SubStr)) = SubStr then
    begin
      Result := I;
      Exit;
    end;
  end;
end;

function IsInPath(const CurrentPath, DirToCheck: String): Boolean;
var
  UpperPath, UpperDir: String;
  P: Integer;
begin
  UpperPath := UpperCase(CurrentPath);
  UpperDir := UpperCase(DirToCheck);

  Result := False;
  P := Pos(UpperDir, UpperPath);
  while P > 0 do
  begin
    { Check that it's a whole segment: preceded by start or ';' }
    if (P = 1) or (CurrentPath[P - 1] = ';') then
    begin
      { Check that it's followed by end or ';' }
      if (P + Length(DirToCheck) - 1 = Length(CurrentPath)) or
         (CurrentPath[P + Length(DirToCheck)] = ';') then
      begin
        Result := True;
        Exit;
      end;
    end;
    P := PosEx(UpperDir, UpperPath, P + 1);
  end;
end;

function RemoveFromPath(const CurrentPath, DirToRemove: String): String;
var
  Cleaned: String;
begin
  Cleaned := CurrentPath;

  { Handle the entry with a trailing semicolon }
  StringChangeEx(Cleaned, ';' + DirToRemove, '', True);

  { Handle the entry without a trailing semicolon (e.g. last entry) }
  StringChangeEx(Cleaned, DirToRemove, '', True);

  { Remove any double semicolons left behind }
  while Pos(';;', Cleaned) > 0 do
    StringChangeEx(Cleaned, ';;', ';', True);

  { Remove leading/trailing semicolons }
  if (Length(Cleaned) > 0) and (Cleaned[1] = ';') then
    Delete(Cleaned, 1, 1);
  if (Length(Cleaned) > 0) and (Cleaned[Length(Cleaned)] = ';') then
    Delete(Cleaned, Length(Cleaned), 1);

  Result := Cleaned;
end;

procedure AddToPath;
var
  OldPath: String;
  NewPath: String;
  InstallDir: String;
begin
  if not WizardIsTaskSelected('addtopath') then Exit;

  InstallDir := ExpandConstant('{app}');

  if not RegQueryStringValue(HKLM, PATH_KEY, PATH_VALUE, OldPath) then
  begin
    Log('Could not read system PATH; skipping add.');
    Exit;
  end;

  if IsInPath(OldPath, InstallDir) then
  begin
    Log('"' + InstallDir + '" is already in system PATH; no changes made.');
    Exit;
  end;

  NewPath := OldPath + ';' + InstallDir;
  if RegWriteExpandStringValue(HKLM, PATH_KEY, PATH_VALUE, NewPath) then
    Log('Added "' + InstallDir + '" to system PATH.')
  else
    Log('Failed to write system PATH.');
end;

procedure RemoveFromPathOnUninstall;
var
  OldPath: String;
  NewPath: String;
  InstallDir: String;
begin
  InstallDir := ExpandConstant('{app}');

  if not RegQueryStringValue(HKLM, PATH_KEY, PATH_VALUE, OldPath) then
  begin
    Log('Could not read system PATH from registry.');
    Exit;
  end;

  NewPath := RemoveFromPath(OldPath, InstallDir);
  if NewPath <> OldPath then
  begin
    if RegWriteExpandStringValue(HKLM, PATH_KEY, PATH_VALUE, NewPath) then
      Log('Removed "' + InstallDir + '" from system PATH.')
    else
      Log('Failed to update system PATH during uninstall.');
  end
  else
    Log('"' + InstallDir + '" was not found in system PATH; no changes made.');
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
    AddToPath;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
    RemoveFromPathOnUninstall;
end;
