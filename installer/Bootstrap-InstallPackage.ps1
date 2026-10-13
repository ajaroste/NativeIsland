param(
  [Parameter(Mandatory=$true)][string]$PackagePath,
  [Parameter(Mandatory=$true)][string]$ExpectedHash,
  [Parameter(Mandatory=$true)][version]$PackageVersion,
  [Parameter(Mandatory=$true)][string]$LogPath,
  [switch]$ValidateOnly
)
$ErrorActionPreference='Stop'
$env:PSModulePath=(Join-Path $PSHOME 'Modules')+';'+$env:PSModulePath
try {
  'NativeIsland package verification started.' | Set-Content -LiteralPath $LogPath
  $stream=[System.IO.File]::OpenRead($PackagePath)
  $sha=[System.Security.Cryptography.SHA256]::Create()
  try {$actualHash=[BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','')} finally {$stream.Dispose();$sha.Dispose()}
  if($actualHash -ne $ExpectedHash){throw 'Paket butunluk kontrolu basarisiz. Hicbir uygulama verisi degistirilmedi.'}
  if(-not [Environment]::Is64BitOperatingSystem){throw '64-bit Windows gerekli.'}
  $build=[int](Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion').CurrentBuildNumber
  if($build -lt 19041){throw 'Windows 10 2004 veya daha yeni bir surum gerekli.'}
  if($ValidateOnly){'PASS embedded MSIX SHA256 and Windows requirements.' | Add-Content -LiteralPath $LogPath;exit 0}
  Import-Module (Join-Path $PSHOME 'Modules\Appx\Appx.psd1') -ErrorAction Stop
  # This runs as the original, unelevated installer user. If UAC used a different
  # administrator account for certificate trust, the app is still installed here.
  $installed=Get-AppxPackage -Name NativeIsland.Desktop
  if($installed -and $installed.Publisher -ne 'CN=NativeIsland Local Development'){throw 'Beklenmeyen paket yayincisi.'}
  if($installed -and [version]$installed.Version -ge $PackageVersion){
    'PASS same or newer NativeIsland package already installed; data untouched.' | Add-Content -LiteralPath $LogPath
    exit 0
  }
  Add-AppxPackage -Path $PackagePath
  $installed=Get-AppxPackage -Name NativeIsland.Desktop
  if(-not $installed -or [version]$installed.Version -lt $PackageVersion){throw 'Kurulan paket dogrulanamadi.'}
  ('PASS installed NativeIsland '+$installed.Version+' for current user; settings/reminders were not modified.') | Add-Content -LiteralPath $LogPath
  exit 0
} catch {
  ('ERROR: '+$_.Exception.Message) | Add-Content -LiteralPath $LogPath
  exit 1
}
