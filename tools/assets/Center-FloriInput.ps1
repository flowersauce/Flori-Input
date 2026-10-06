<#
.SYNOPSIS
Center an open Flori Input window on its monitor for desktop screenshots.
.PARAMETER ProcessId
Select a particular Flori-Input process when several windows are open.
#>
[CmdletBinding()]
param(
    [ValidateRange(0, 2147483647)]
    [int]$ProcessId = 0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$windows = @(Get-Process -Name 'Flori-Input' -ErrorAction SilentlyContinue |
    Where-Object {
        $_.MainWindowHandle -ne [IntPtr]::Zero -and
        ($ProcessId -eq 0 -or $_.Id -eq $ProcessId)
    })
if ($windows.Count -eq 0) {
    throw 'Open Flori Input first. If -ProcessId is specified, check that it identifies an open Flori Input window.'
}
if ($windows.Count -gt 1) {
    $windows | Select-Object Id, MainWindowTitle, Path | Format-Table -AutoSize | Out-Host
    throw 'Several Flori Input windows are open. Run this script with -ProcessId <Id> to select one.'
}

if (-not ('FloriInput.StoreAssets.WindowPosition' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;

namespace FloriInput.StoreAssets
{
    public static class WindowPosition
    {
        [StructLayout(LayoutKind.Sequential)]
        private struct Rect
        {
            public int Left, Top, Right, Bottom;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct MonitorInfo
        {
            public int Size;
            public Rect Monitor, WorkArea;
            public uint Flags;
        }

        [DllImport("user32.dll")]
        private static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern bool GetWindowRect(IntPtr window, out Rect bounds);

        [DllImport("user32.dll")]
        private static extern IntPtr MonitorFromWindow(IntPtr window, uint flags);

        [DllImport("user32.dll", EntryPoint = "GetMonitorInfoW", SetLastError = true)]
        private static extern bool GetMonitorInfo(IntPtr monitor, ref MonitorInfo info);

        [DllImport("dwmapi.dll")]
        private static extern int DwmGetWindowAttribute(IntPtr window, uint attribute, out Rect bounds, int size);

        [DllImport("user32.dll")]
        private static extern bool IsIconic(IntPtr window);

        [DllImport("user32.dll")]
        private static extern bool ShowWindow(IntPtr window, int command);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern bool SetWindowPos(IntPtr window, IntPtr insertAfter, int x, int y, int width, int height, uint flags);

        public static void Center(IntPtr window)
        {
            // Keep monitor coordinates and DWM frame bounds in physical pixels.
            IntPtr previousDpi = SetThreadDpiAwarenessContext(new IntPtr(-4));
            if (previousDpi == IntPtr.Zero)
            {
                throw new InvalidOperationException("Could not enable physical display coordinates.");
            }
            try
            {
                if (IsIconic(window))
                {
                    ShowWindow(window, 9); // SW_RESTORE
                }
                Rect bounds;
                if (!GetWindowRect(window, out bounds))
                {
                    throw new Win32Exception(Marshal.GetLastWin32Error());
                }
                MonitorInfo info = new MonitorInfo();
                info.Size = Marshal.SizeOf(typeof(MonitorInfo));
                if (!GetMonitorInfo(MonitorFromWindow(window, 2), ref info))
                {
                    throw new Win32Exception(Marshal.GetLastWin32Error());
                }
                Rect visible;
                if (DwmGetWindowAttribute(window, 9, out visible, Marshal.SizeOf(typeof(Rect))) != 0 ||
                    visible.Right <= visible.Left || visible.Bottom <= visible.Top)
                {
                    visible = bounds;
                }
                // Center the visible frame, compensating for invisible resize borders.
                int x = info.Monitor.Left + (info.Monitor.Right - info.Monitor.Left - (visible.Right - visible.Left)) / 2 - (visible.Left - bounds.Left);
                int y = info.Monitor.Top + (info.Monitor.Bottom - info.Monitor.Top - (visible.Bottom - visible.Top)) / 2 - (visible.Top - bounds.Top);
                const uint positionFlags = 0x0001 | 0x0004 | 0x0010; // NOSIZE | NOZORDER | NOACTIVATE
                if (!SetWindowPos(window, IntPtr.Zero, x, y, 0, 0, positionFlags))
                {
                    throw new Win32Exception(Marshal.GetLastWin32Error());
                }
            }
            finally
            {
                SetThreadDpiAwarenessContext(previousDpi);
            }
        }
    }
}
'@
}

$targetWindow = $windows[0]
[FloriInput.StoreAssets.WindowPosition]::Center($targetWindow.MainWindowHandle)
Write-Output ('Centered Flori Input window (PID {0}) on its monitor.' -f $targetWindow.Id)
