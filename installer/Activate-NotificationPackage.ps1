param([string]$Arguments='--windows-notification-probe')
$ErrorActionPreference='Stop'
$package=Get-AppxPackage -Name NativeIsland.Desktop
if(-not $package){throw 'NativeIsland notification package is not installed.'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
[ComImport, Guid("2e941141-7f97-4756-ba1d-9decde894a3d"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IApplicationActivationManager {
  uint ActivateApplication([MarshalAs(UnmanagedType.LPWStr)] string app, [MarshalAs(UnmanagedType.LPWStr)] string args, uint options);
  uint ActivateForFile([MarshalAs(UnmanagedType.LPWStr)] string app, IntPtr items, [MarshalAs(UnmanagedType.LPWStr)] string verb);
  uint ActivateForProtocol([MarshalAs(UnmanagedType.LPWStr)] string app, IntPtr items);
}
public static class NativeIslandPackageActivation {
  public static uint Launch(string app, string args) {
    var type=Type.GetTypeFromCLSID(new Guid("45BA127D-10A8-46EA-8AB7-56EA9078943C"));
    object instance=Activator.CreateInstance(type);
    try { return ((IApplicationActivationManager)instance).ActivateApplication(app,args,0); }
    finally { Marshal.ReleaseComObject(instance); }
  }
}
'@
[NativeIslandPackageActivation]::Launch(($package.PackageFamilyName+'!App'),$Arguments)
