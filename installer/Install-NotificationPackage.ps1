param([Parameter(Mandatory=$true)][string]$ExpectedThumbprint)
$ErrorActionPreference='Stop'
$certificatePath=Join-Path $PSScriptRoot 'Output\NativeIsland-Development.cer'
$packagePath=Join-Path $PSScriptRoot 'Output\NativeIsland-Notifications.msix'
$certificate=New-Object System.Security.Cryptography.X509Certificates.X509Certificate2($certificatePath)
if($certificate.Subject -ne 'CN=NativeIsland Local Development' -or $certificate.Thumbprint -ne $ExpectedThumbprint){throw 'Unexpected certificate. Nothing was installed.'}
# User explicitly approves this exact developer certificate and package.
# TrustedPeople trusts this end-entity publisher; Trusted Root is NOT changed.
$resultPath=Join-Path $PSScriptRoot 'Output\notification-install-result.txt'
try {
  Import-Certificate -FilePath $certificatePath -CertStoreLocation Cert:\LocalMachine\TrustedPeople | Out-Null
  Add-AppxPackage -Path $packagePath
  $installed=Get-AppxPackage -Name NativeIsland.Desktop
  if(-not $installed){throw 'Package installation did not return the installed package.'}
  $installed | Select-Object Name,Version,PackageFamilyName | Format-List | Out-String | Set-Content -LiteralPath $resultPath
} catch {
  $_.Exception.Message | Set-Content -LiteralPath $resultPath
  exit 1
}
