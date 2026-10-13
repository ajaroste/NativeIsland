param(
  [Parameter(Mandatory=$true)][string]$CertificatePath,
  [Parameter(Mandatory=$true)][string]$ExpectedThumbprint,
  [switch]$Install
)
$ErrorActionPreference='Stop'
$env:PSModulePath=(Join-Path $PSHOME 'Modules')+';'+$env:PSModulePath
try {
  Import-Module (Join-Path $PSHOME 'Modules\Microsoft.PowerShell.Security\Microsoft.PowerShell.Security.psd1') -ErrorAction Stop
  $certificate=New-Object System.Security.Cryptography.X509Certificates.X509Certificate2($CertificatePath)
  if($certificate.Subject -ne 'CN=NativeIsland Local Development' -or $certificate.Thumbprint -ne $ExpectedThumbprint){throw 'Beklenmeyen sertifika. Kurulum durduruldu.'}
  if($certificate.NotAfter -le (Get-Date) -or $certificate.NotBefore -gt (Get-Date)){throw 'Sertifikanin gecerlilik tarihi uygun degil.'}
  $trustedPath='Cert:\LocalMachine\TrustedPeople\'+$ExpectedThumbprint
  if(Test-Path -LiteralPath $trustedPath){exit 0}
  if(-not $Install){exit 10}
  # Only the exact end-entity certificate bundled with this installer is trusted.
  # Never modify Trusted Root, security policies or notification access consent.
  Import-Certificate -FilePath $CertificatePath -CertStoreLocation Cert:\LocalMachine\TrustedPeople | Out-Null
  if(-not (Test-Path -LiteralPath $trustedPath)){throw 'Sertifika guven kaydi dogrulanamadi.'}
  exit 0
} catch {
  Write-Error $_.Exception.Message
  exit 1
}
