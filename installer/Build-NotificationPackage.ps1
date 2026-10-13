param(
  [Parameter(Mandatory=$true)][string]$AppBinary,
  [string]$CertificateThumbprint
)
$ErrorActionPreference='Stop'
$repoRoot=Split-Path $PSScriptRoot -Parent
$binaryPath=(Resolve-Path -LiteralPath $AppBinary).Path
$sdkPath='C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64'
$stagePath=Join-Path $repoRoot ('out\msix-stage-'+[Guid]::NewGuid().ToString('N'))
$outputPath=Join-Path $repoRoot 'installer\Output'
New-Item -ItemType Directory -Path (Join-Path $stagePath 'Assets'),$outputPath -Force | Out-Null
Copy-Item -LiteralPath $binaryPath -Destination (Join-Path $stagePath 'NativeIsland.exe')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'AppxManifest.xml') -Destination (Join-Path $stagePath 'AppxManifest.xml')
Copy-Item -LiteralPath (Join-Path $repoRoot 'assets\LUCIDE-LICENSE.txt') -Destination $stagePath
Copy-Item -LiteralPath (Join-Path $repoRoot 'LICENSE') -Destination $stagePath
Copy-Item -LiteralPath (Join-Path $repoRoot 'docs\google-calendar-setup.md') -Destination $stagePath
Copy-Item -LiteralPath (Join-Path $repoRoot 'docs\tasks-and-sound.md') -Destination $stagePath
# Procedural native packaging icons; no external image or account is needed.
Add-Type -AssemblyName System.Drawing
foreach($entry in @(@('StoreLogo.png',50),@('Square44x44Logo.png',44),@('Square150x150Logo.png',150))){
  $size=[int]$entry[1]
  $bitmap=New-Object System.Drawing.Bitmap($size,$size)
  $graphics=[System.Drawing.Graphics]::FromImage($bitmap)
  $graphics.SmoothingMode=[System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
  $graphics.Clear([System.Drawing.Color]::FromArgb(7,26,39))
  $pen=New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(81,204,255),[single]($size/16))
  $graphics.DrawEllipse($pen,[single]($size*.2),[single]($size*.2),[single]($size*.6),[single]($size*.6))
  $graphics.DrawLine($pen,[single]($size*.5),[single]($size*.32),[single]($size*.5),[single]($size*.68))
  $bitmap.Save((Join-Path $stagePath ('Assets\'+$entry[0])),[System.Drawing.Imaging.ImageFormat]::Png)
  $pen.Dispose();$graphics.Dispose();$bitmap.Dispose()
}
$packagePath=Join-Path $outputPath 'NativeIsland-Notifications.msix'
& (Join-Path $sdkPath 'makeappx.exe') pack /d $stagePath /p $packagePath /o
if($LASTEXITCODE -ne 0){throw 'MSIX validation/packaging failed.'}
if($CertificateThumbprint){
  $certificate=Get-Item -LiteralPath ('Cert:\CurrentUser\My\'+$CertificateThumbprint)
  if($certificate.Subject -ne 'CN=NativeIsland Local Development'){throw 'Certificate publisher does not match the package.'}
  & (Join-Path $sdkPath 'signtool.exe') sign /fd SHA256 /sha1 $certificate.Thumbprint /s My $packagePath
  if($LASTEXITCODE -ne 0){throw 'MSIX signing failed.'}
  Export-Certificate -Cert $certificate -FilePath (Join-Path $outputPath 'NativeIsland-Development.cer') | Out-Null
}
Write-Output $packagePath
