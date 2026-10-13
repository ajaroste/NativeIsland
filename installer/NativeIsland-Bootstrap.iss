#ifndef CertificateThumbprint
  #error CertificateThumbprint must be supplied by Build-Bootstrap.ps1.
#endif
#ifndef PackageHash
  #error PackageHash must be supplied by Build-Bootstrap.ps1.
#endif
#ifndef PackageVersion
  #error PackageVersion must be supplied by Build-Bootstrap.ps1.
#endif
[Setup]
AppName=NativeIsland
AppId=NativeIsland.NotificationBootstrap
AppVersion={#PackageVersion}
AppPublisher=NativeIsland Local Development
PrivilegesRequired=lowest
CreateAppDir=no
Uninstallable=no
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableWelcomePage=no
WizardStyle=modern
MinVersion=10.0.19041
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=Output
OutputBaseFilename=NativeIsland-Kurulum
Compression=lzma2
SolidCompression=yes
SetupLogging=yes

[Files]
Source: "Output\NativeIsland-Notifications.msix"; Flags: dontcopy
Source: "Output\NativeIsland-Development.cer"; Flags: dontcopy
Source: "Bootstrap-TrustCertificate.ps1"; Flags: dontcopy
Source: "Bootstrap-InstallPackage.ps1"; Flags: dontcopy

[Run]
Filename: "{win}\explorer.exe"; Parameters: "shell:AppsFolder\NativeIsland.Desktop_fyjwr1apyye3e!App"; Description: "NativeIsland'ı aç"; Flags: postinstall nowait skipifsilent runasoriginaluser

[Code]
var
  ConsentPage: TInputOptionWizardPage;

function PowerShellPath: String;
begin
  Result := ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe');
end;

function Quoted(const Value: String): String;
begin
  Result := '"' + Value + '"';
end;

function HasSwitch(const Value: String): Boolean;
var I: Integer;
begin
  Result := False;
  for I := 1 to ParamCount do
    if CompareText(ParamStr(I), Value) = 0 then Result := True;
end;

procedure InitializeWizard;
begin
  ConsentPage := CreateInputOptionPage(wpWelcome,
    'Sertifika ve Windows bildirim erişimi',
    'Bu kurulum yerel test sertifikasıyla imzalanmış NativeIsland paketini yükler.',
    'Yalnızca NativeIsland Local Development sertifikası, Güvenilen Kişiler deposuna eklenir. ' +
    'Güvenilen kök sertifikalar ve Windows güvenlik ayarları değiştirilmez.' + #13#10#13#10 +
    'Mevcut ayarlar ve hatırlatıcı dosyaları korunur. Kurulumdan sonra Ayarlar > Bildirimler ' +
    'üzerinden Windows bildirim erişimine ayrıca izin vermelisiniz.' + #13#10#13#10 +
    'Uygulamayı Windows Ayarları > Uygulamalar üzerinden kaldırabilirsiniz.',
    False, False);
  ConsentPage.Add('NativeIsland test sertifikasına güvenilmesini ve paketin kurulmasını onaylıyorum.');
  ConsentPage.Values[0] := HasSwitch('/ACCEPTCERTIFICATE');
  WizardForm.FinishedLabel.Caption :=
    'NativeIsland kuruldu. Ayarlar > Bildirimler bölümünde Windows bildirimlerini açıp erişim izni verin. ' +
    'Bu izin sizin onayınız olmadan verilmez.';
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = ConsentPage.ID) and (not HasSwitch('/VERIFYONLY')) and (not ConsentPage.Values[0]) then begin
    if not WizardSilent then
      MsgBox('Devam etmek için sertifika ve paket kurulumunu onaylamanız gerekiyor.', mbInformation, MB_OK)
    else Log('Certificate consent missing; unattended installation refused.');
    Result := False;
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var ExitCode: Integer; Args, PackagePath, CertPath, LogPath: String; Details: AnsiString;
begin
  Result := '';
  if (not HasSwitch('/VERIFYONLY')) and (not ConsentPage.Values[0]) then begin
    Result := 'Sertifika kurulum onayı verilmedi.'; Exit;
  end;
  ExtractTemporaryFile('NativeIsland-Notifications.msix');
  ExtractTemporaryFile('NativeIsland-Development.cer');
  ExtractTemporaryFile('Bootstrap-TrustCertificate.ps1');
  ExtractTemporaryFile('Bootstrap-InstallPackage.ps1');
  PackagePath := ExpandConstant('{tmp}\NativeIsland-Notifications.msix');
  CertPath := ExpandConstant('{tmp}\NativeIsland-Development.cer');
  LogPath := ExpandConstant('{tmp}\NativeIsland-package-install.log');
  Args := '-NoProfile -NonInteractive -ExecutionPolicy Bypass -File ' +
    Quoted(ExpandConstant('{tmp}\Bootstrap-InstallPackage.ps1')) +
    ' -PackagePath ' + Quoted(PackagePath) + ' -ExpectedHash {#PackageHash}' +
    ' -PackageVersion {#PackageVersion} -LogPath ' + Quoted(LogPath) + ' -ValidateOnly';
  if (not Exec(PowerShellPath, Args, '', SW_HIDE, ewWaitUntilTerminated, ExitCode)) or (ExitCode <> 0) then begin
    LoadStringFromFile(LogPath, Details);
    Result := 'Paket doğrulanamadı.' + #13#10 + Details; Exit;
  end;
  if HasSwitch('/VERIFYONLY') then begin Log('PASS verification only: no certificate or package changes.'); Exit; end;
  Args := '-NoProfile -NonInteractive -ExecutionPolicy Bypass -File ' +
    Quoted(ExpandConstant('{tmp}\Bootstrap-TrustCertificate.ps1')) +
    ' -CertificatePath ' + Quoted(CertPath) + ' -ExpectedThumbprint {#CertificateThumbprint}';
  if not Exec(PowerShellPath, Args, '', SW_HIDE, ewWaitUntilTerminated, ExitCode) then begin
    Result := 'Sertifika doğrulaması başlatılamadı.'; Exit;
  end;
  if ExitCode = 10 then begin
    if (not ShellExec('runas', PowerShellPath, Args + ' -Install', '', SW_HIDE, ewWaitUntilTerminated, ExitCode)) or (ExitCode <> 0) then begin
      Result := 'Yönetici onayı veya sertifika kurulumu tamamlanamadı. Uygulama verilerine dokunulmadı.'; Exit;
    end;
  end else if ExitCode <> 0 then begin
    Result := 'Sertifika doğrulanamadı. Kurulum durduruldu.'; Exit;
  end;
  Args := '-NoProfile -NonInteractive -ExecutionPolicy Bypass -File ' +
    Quoted(ExpandConstant('{tmp}\Bootstrap-InstallPackage.ps1')) +
    ' -PackagePath ' + Quoted(PackagePath) + ' -ExpectedHash {#PackageHash}' +
    ' -PackageVersion {#PackageVersion} -LogPath ' + Quoted(LogPath);
  if (not Exec(PowerShellPath, Args, '', SW_HIDE, ewWaitUntilTerminated, ExitCode)) or (ExitCode <> 0) then begin
    LoadStringFromFile(LogPath, Details);
    Result := 'MSIX kurulamadı. NativeIsland açıksa tepsiden kapatıp yeniden deneyin.' + #13#10 + Details; Exit;
  end;
  LoadStringFromFile(LogPath, Details); Log(Details);
end;
