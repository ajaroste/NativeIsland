# Compact interaction update — 0.3.1

## Changes

- The island cannot be moved by mouse dragging. Background presses only pin/unpin it; placement stays anchored to the selected monitor's top center.
- Expanded height follows content: Media 252 DIP; one shelf file 226 DIP; empty Applications 194 DIP. Widgets keep their populated grid. Bluetooth adds a 26 DIP status row when detected. Five labeled tabs fit a 500 DIP minimum width.
- Playing media shows animated bars, with an approximately 180 ms transition to three dots when paused. Wave rendering runs only while relevant and visible, stops after pause, and respects reduced motion. Raw Spotify IDs are displayed as Spotify.
- Page content is clipped to the same rounded capsule geometry as its background, including transitions.
- OLE drag-enter immediately selects Files and opens the shelf. Shelf rows can be dragged outward using a real Shell IDataObject and `DROPEFFECT_COPY` only. Explorer/destination applications perform the drop; source files/folders are untouched. Collision handling belongs to the destination's normal copy UI.
- Applications has a plus button for local .exe/.lnk/.appref-ms selection. Real Windows icons load on a worker; click launches the shortcut, X removes only its saved entry. UTF-8 paths persist atomically in `%LOCALAPPDATA%\NativeIsland\applications.txt`.
- Active audio endpoints are matched to present remote Bluetooth device containers. Host radio nodes and GUID_NULL are excluded so wired computer audio is not mislabeled as Bluetooth. Devices whose drivers do not expose the matching container may not be identified.
- Desktop Explorer/DefView/ListView foreground windows are excluded from fullscreen hiding. The overlay periodically reasserts topmost visibility without stealing focus.

## Verification

Release x64 build, configuration/layout tests, actual folder CF_HDROP retrieval, and real application-icon loading/removal tests pass. The preview mode produces 36 native PNGs (six palettes × collapsed plus five pages) and passes actual HWND center/top/collapse geometry checks. Preview rows include a clearly named repository test folder and the actual NativeIsland executable icon; they are not saved to the user's shelves/shortcuts. Actual Spotify metadata/cover and paused state were present in the media PNG.

The 30-second smoke test exits cleanly with code 0. Samples near 8/16/24 seconds showed 70.8 MiB working set and 645 handles. This short run is not a long-term leak/performance guarantee. Windows SDK registry access required running build/tests outside the restricted tool sandbox; no approval review was rejected.

Manual checks still needed: physical outward drag to an Explorer destination and resulting copy; actual application launch through the plus menu; real pointer hover/wave motion; connected Bluetooth headphone/speaker hardware; desktop-focus restoration and multi-monitor DPI behavior on the real desktop. No external applications were launched or user files copied automatically during tests.

The old running Release executable is protected from process termination/WM_CLOSE in this session and remains locked. The verified update is built under `out/build/compact-x64/Release/NativeIsland.exe`. Exit the old instance through its tray menu before running the update. The installer is compiled from the updated binary through its AppBinary override.
