; Instalator wtyczki Podpis (Watermark). Budowanie: ISCC.exe installer\ZetmarWatermark.iss
#define AppName "Podpis (Watermark) / Signature (Watermark)"
#define AppVer "1.1.0"

[Setup]
AppId={{7A1F3C52-5B1E-4D0C-9E55-5A7B2C0E4D11}
AppName={#AppName}
AppVersion={#AppVer}
AppPublisher=Zetmar - Cyfrowy Przyjaciel
AppPublisherURL=https://zetmar.net.pl
DefaultDirName={commonappdata}\ZetmarWatermark
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
OutputDir=..\dist
OutputBaseFilename=ZetmarWatermark_Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#AppName}

[Languages]
Name: "polish"; MessagesFile: "compiler:Languages\Polish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
polish.PageTitle=Wybierz programy Adobe
polish.PageDesc=Wtyczka zostanie zainstalowana w zaznaczonych programach.
polish.ManualTitle=Folder wtyczek
polish.ManualDesc=Nie wykryto Photoshopa ani Photoshop Elements. Wskaż folder Plug-ins ręcznie.
polish.ManualHint=Wybierz folder Plug-ins (lub Plug-Ins) programu Photoshop / Photoshop Elements:
polish.NothingSelected=Zaznacz co najmniej jeden program.
polish.CopyFail=Nie udało się skopiować wtyczki do:
polish.Docs=Instrukcja instalacji i użycia
polish.AffinityItem=Affinity (folder wtyczek do dodania w ustawieniach Affinity)
polish.AffinityInfo=Wtyczka dla Affinity została skopiowana do:%n%n%1%n%nW Affinity otwórz Ustawienia > Wtyczki Photoshop (Photoshop plugins), dodaj powyższy folder i uruchom Affinity ponownie. Filtr znajdziesz w menu Filtry > Wtyczki > Zetmar.
english.PageTitle=Choose Adobe programs
english.PageDesc=The plug-in will be installed into the selected programs.
english.ManualTitle=Plug-ins folder
english.ManualDesc=Photoshop or Photoshop Elements was not detected. Choose the Plug-ins folder manually.
english.ManualHint=Choose the Plug-ins (or Plug-Ins) folder of Photoshop / Photoshop Elements:
english.NothingSelected=Select at least one program.
english.CopyFail=Could not copy the plug-in to:
english.Docs=Installation and usage guide
english.AffinityItem=Affinity (plug-ins folder to add in the Affinity settings)
english.AffinityInfo=The plug-in for Affinity was copied to:%n%n%1%n%nIn Affinity open Settings > Photoshop plug-ins, add the folder above and restart Affinity. The filter appears under Filters > Plugins > Zetmar.

[Files]
Source: "..\build\ZetmarWatermark_64.8bf"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\ZetmarWatermark_Affinity_64.8bf"; DestDir: "{app}"; Flags: ignoreversion
Source: "INSTRUKCJA_PL.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "INSTALL_EN.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{commonprograms}\Zetmar Watermark\{cm:Docs} (PL)"; Filename: "{app}\INSTRUKCJA_PL.txt"
Name: "{commonprograms}\Zetmar Watermark\{cm:Docs} (EN)"; Filename: "{app}\INSTALL_EN.txt"

[Code]
var
  ListPage: TWizardPage;
  Checks: TNewCheckListBox;
  ManualPage: TInputDirWizardPage;
  Paths: TStringList;
  Kinds: TStringList;      // "A" = Adobe, "F" = Affinity (wariant z klasycznym PiPL)
  Installed: string;

procedure AddPluginsDir(const Root, Caption: string);
var
  P: string;
begin
  P := Root + '\Plug-Ins';
  if not DirExists(P) then P := Root + '\Plug-ins';
  if DirExists(P) then
  begin
    Checks.AddCheckBox(Caption + '  (' + P + ')', '', 0, True, True, False, True, nil);
    Paths.Add(P);
    Kinds.Add('A');
  end;
end;

function AffinityInstalled: Boolean;
var
  FR: TFindRec;
begin
  Result := False;
  if FindFirst(ExpandConstant('{localappdata}\Packages\*Affinity*'), FR) then
  begin
    Result := True;
    FindClose(FR);
  end;
  if not Result then
    Result := DirExists(ExpandConstant('{commonpf64}\Affinity')) or DirExists(ExpandConstant('{commonpf64}\Affinity\Affinity'));
end;

function AffinityFolder: string;
begin
  Result := ExpandConstant('{commoncf64}\Zetmar\AffinityPlugins');
end;

procedure ScanAdobe(const Base: string);
var
  FR: TFindRec;
  N: string;
begin
  if not FindFirst(Base + '\*', FR) then Exit;
  try
    repeat
      if (FR.Attributes and FILE_ATTRIBUTE_DIRECTORY <> 0) and (FR.Name <> '.') and (FR.Name <> '..') then
      begin
        N := FR.Name;
        if (Pos('Photoshop', N) > 0) and (Pos('Organizer', N) = 0) then
          AddPluginsDir(Base + '\' + N, N);
      end;
    until not FindNext(FR);
  finally
    FindClose(FR);
  end;
end;

procedure InitializeWizard;
begin
  Paths := TStringList.Create;
  Kinds := TStringList.Create;
  ListPage := CreateCustomPage(wpWelcome, CustomMessage('PageTitle'), CustomMessage('PageDesc'));
  Checks := TNewCheckListBox.Create(ListPage);
  Checks.Parent := ListPage.Surface;
  Checks.Left := 0;
  Checks.Top := 0;
  Checks.Width := ListPage.SurfaceWidth;
  Checks.Height := ListPage.SurfaceHeight;
  ScanAdobe(ExpandConstant('{commonpf64}\Adobe'));
  ScanAdobe(ExpandConstant('{commonpf32}\Adobe'));
  if AffinityInstalled then
  begin
    Checks.AddCheckBox(CustomMessage('AffinityItem') + '  (' + AffinityFolder + ')', '', 0, True, True, False, True, nil);
    Paths.Add(AffinityFolder);
    Kinds.Add('F');
  end;

  ManualPage := CreateInputDirPage(ListPage.ID, CustomMessage('ManualTitle'), CustomMessage('ManualDesc'),
    CustomMessage('ManualHint'), False, '');
  ManualPage.Add('');
  ManualPage.Values[0] := ExpandConstant('{commonpf64}\Adobe');
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;
  if PageID = ListPage.ID then Result := Paths.Count = 0;
  if PageID = ManualPage.ID then Result := Paths.Count > 0;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  I, N: Integer;
begin
  Result := True;
  if CurPageID = ListPage.ID then
  begin
    N := 0;
    for I := 0 to Checks.Items.Count - 1 do
      if Checks.Checked[I] then N := N + 1;
    if N = 0 then
    begin
      MsgBox(CustomMessage('NothingSelected'), mbError, MB_OK);
      Result := False;
    end;
  end;
  if CurPageID = ManualPage.ID then
  begin
    if not DirExists(ManualPage.Values[0]) then
    begin
      MsgBox(CustomMessage('ManualHint'), mbError, MB_OK);
      Result := False;
    end;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  I: Integer;
  Dst, Src, SrcAff, AffDir: string;
  Targets: TStringList;
  Kind: string;
begin
  if CurStep <> ssPostInstall then Exit;
  Src := ExpandConstant('{app}\ZetmarWatermark_64.8bf');
  SrcAff := ExpandConstant('{app}\ZetmarWatermark_Affinity_64.8bf');
  Targets := TStringList.Create;
  try
    if Paths.Count > 0 then
    begin
      for I := 0 to Paths.Count - 1 do
        if Checks.Checked[I] then Targets.Add(Kinds[I] + Paths[I]);
    end
    else
      Targets.Add('A' + ManualPage.Values[0]);
    for I := 0 to Targets.Count - 1 do
    begin
      Kind := Copy(Targets[I], 1, 1);
      if Kind = 'F' then
      begin
        Dst := Copy(Targets[I], 2, MaxInt);
        ForceDirectories(Dst);
        if FileCopy(SrcAff, Dst + '\ZetmarWatermark_Affinity_64.8bf', False) then
        begin
          Installed := Installed + Dst + ';';
          AffDir := Dst;
        end
        else
          MsgBox(CustomMessage('CopyFail') + #13#10 + Dst, mbError, MB_OK);
      end
      else
      begin
        Dst := Copy(Targets[I], 2, MaxInt) + '\Zetmar';
        ForceDirectories(Dst);
        if FileCopy(Src, Dst + '\ZetmarWatermark_64.8bf', False) then
          Installed := Installed + Dst + ';'
        else
          MsgBox(CustomMessage('CopyFail') + #13#10 + Dst, mbError, MB_OK);
      end;
    end;
  finally
    Targets.Free;
  end;
  RegWriteStringValue(HKLM, 'Software\Zetmar\Watermark', 'Targets', Installed);
  if AffDir <> '' then
    MsgBox(FmtMessage(CustomMessage('AffinityInfo'), [AffDir]), mbInformation, MB_OK);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  S, D: string;
  P: Integer;
begin
  if CurUninstallStep <> usUninstall then Exit;
  if RegQueryStringValue(HKLM, 'Software\Zetmar\Watermark', 'Targets', S) then
  begin
    while S <> '' do
    begin
      P := Pos(';', S);
      if P = 0 then
      begin
        D := S;
        S := '';
      end
      else
      begin
        D := Copy(S, 1, P - 1);
        S := Copy(S, P + 1, MaxInt);
      end;
      if D <> '' then
      begin
        DeleteFile(D + '\ZetmarWatermark_64.8bf');
        DeleteFile(D + '\ZetmarWatermark_Affinity_64.8bf');
        RemoveDir(D);
      end;
    end;
    RegDeleteKeyIncludingSubkeys(HKLM, 'Software\Zetmar\Watermark');
  end;
end;
