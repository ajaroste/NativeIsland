# 0.6.1 — native menu and companion window stacking

The overlay previously inserted itself at HWND_TOPMOST during every animation resize and periodic fullscreen check. This could cover transient native menus. Settings, clipboard search and text/image previews were independent unowned windows and could also appear behind the topmost overlay.

Movement and resizing now preserve Z-order with SWP_NOZORDER. Fullscreen polling only repairs a missing WS_EX_TOPMOST style instead of repeatedly raising the overlay. All companion windows explicitly use the overlay as their owner, preserving their position above it. Native menu-loop messages suspend automatic collapse, and fullscreen detection excludes the application's own process.

Verification (`--render-previews`) opens actual settings/search/text/image preview windows and checks their ownership and Z-order. It exercises a real native TrackPopupMenu loop, checks stacking after position/fullscreen polling, closes the test menu without selecting a command, and asserts the overlay remains topmost. These checks passed alongside all existing animation/render tests and the core unit suite. Test windows do not change saved settings, clipboard history or hardware audio settings.

Executable: `out/build/stacking-x64/Release/NativeIsland.exe`. Exit any older instance from its tray menu before starting the updated version.
