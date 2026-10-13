$ErrorActionPreference='Stop'
$certificate=New-Object System.Security.Cryptography.X509Certificates.X509Certificate2((Join-Path $PSScriptRoot 'Output\NativeIsland-Development.cer'))
if($certificate.Subject -ne 'CN=NativeIsland Local Development'){throw 'Unexpected publisher.'}
$packagePath=Join-Path $PSScriptRoot 'Output\NativeIsland-Notifications.msix'
$packageHash=(Get-FileHash -LiteralPath $packagePath -Algorithm SHA256).Hash
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive=[System.IO.Compression.ZipFile]::OpenRead($packagePath)
try {
  $reader=New-Object System.IO.StreamReader($archive.GetEntry('AppxManifest.xml').Open())
  try {$manifest=[xml]$reader.ReadToEnd()} finally {$reader.Dispose()}
} finally {$archive.Dispose()}
$packageVersion=[string]$manifest.Package.Identity.Version
if($manifest.Package.Identity.Publisher -ne $certificate.Subject){throw 'Package/certificate publisher mismatch.'}
& 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe' "/DCertificateThumbprint=$($certificate.Thumbprint)" "/DPackageHash=$packageHash" "/DPackageVersion=$packageVersion" (Join-Path $PSScriptRoot 'NativeIsland-Bootstrap.iss')
if($LASTEXITCODE -ne 0){throw 'Bootstrap compiler failed.'}
