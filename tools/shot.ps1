# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Yukinoshita-lin
param(
  [string]$Out = "F:\DNA\shots\shot.png",
  [string]$AppArgs = "",
  [int]$DelayMs = 1400
)
Add-Type -AssemblyName System.Windows.Forms,System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class DpiBootstrap {
  [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr ctx);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
}
"@
try { [DpiBootstrap]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null } catch { [DpiBootstrap]::SetProcessDPIAware() | Out-Null }
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class ShotWinApi {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
}
"@
if ([string]::IsNullOrWhiteSpace($AppArgs)) {
  $p = Start-Process -FilePath "F:\DNA\DNAExpressionSim.exe" -PassThru
} else {
  $p = Start-Process -FilePath "F:\DNA\DNAExpressionSim.exe" -ArgumentList $AppArgs -PassThru
}
Start-Sleep -Milliseconds 1000
$null = $p.WaitForInputIdle(3000)
$h = $p.MainWindowHandle
if ($h -eq [IntPtr]::Zero) { Start-Sleep -Milliseconds 800; $p.Refresh(); $h = $p.MainWindowHandle }
[ShotWinApi]::ShowWindow($h, 9) | Out-Null
[ShotWinApi]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds $DelayMs
$r = New-Object ShotWinApi+RECT
[ShotWinApi]::GetWindowRect($h, [ref]$r) | Out-Null
$w = $r.Right - $r.Left; $ht = $r.Bottom - $r.Top
Write-Output "window: $w x $ht at $($r.Left),$($r.Top)"
$bmp = New-Object System.Drawing.Bitmap($w, $ht)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.Left, $r.Top, 0, 0, (New-Object System.Drawing.Size($w, $ht)))
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
$p.CloseMainWindow() | Out-Null
Start-Sleep -Milliseconds 400
if (!$p.HasExited) { $p.Kill() }
Write-Output "saved $Out"
