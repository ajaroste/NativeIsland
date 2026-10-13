# NativeIsland development history

0.8.6 uses the available header space to show full tab labels, including Uygulamalar at the minimum panel width. Latest build: `out/build/tasks-fixes-x64/Release-labels/NativeIsland.exe`.

0.8.5 hides completed tasks (including cached entries), fixes edit-change events incorrectly submitting the new-task form, and adds confirmed Google Tasks deletion. Only the Ekle button submits a task. Latest build: `out/build/tasks-fixes-x64/Release/NativeIsland.exe`.

0.8.4 removes the separate Tasks satellite and adds a checklist tab beside the notification bell in the main header. Header controls remain separated at 465–650 DIP. Latest build: `out/build/tasks-header-x64/Release/NativeIsland.exe`.

0.8.3 replaces the detached Tasks circle with a 26 DIP rounded-square checklist badge, a pending-task dot, subtle hover highlight and a tighter 4 DIP gap. Hover preview and click-to-promote remain unchanged. Latest build: `out/build/task-badge-x64/Release/NativeIsland.exe`.

0.8.2 distinguishes the master-volume card from application sessions, adds persistent application stars (favorites appear first while present), and hides the satellite without a connected Google Tasks account. Favorites live in `%LOCALAPPDATA%/NativeIsland/audio-favorites.json`, outside the install folder. Latest build: `out/build/audio-favorites-x64/Release/NativeIsland.exe`.

0.8.1 reduces the satellite circle from 40 to 30 DIP and its icon from 20 to 16 DIP, vertically centers it against the compact island and keeps hit testing matched to the visible circle. Latest executable: `out/build/satellite-small-x64/Release/NativeIsland.exe`.

0.8.0 adds selectable top-attached Droplet styling, a separate speaker/mixer page with master and per-session application volume, and a Google Tasks satellite island with hover preview / click-to-promote. Tasks list selection, add and completion use opt-in Google authorization; encrypted connection/cache and existing reminders/settings remain in the user's data directory. Latest build: `out/build/tasks-x64/Release/NativeIsland.exe`. Updated single-file installer: `installer/Output/NativeIsland-Kurulum.exe`. See [Tasks and sound setup](docs/tasks-and-sound.md).

Single-file notification installer: installer/Output/NativeIsland-Kurulum.exe embeds the signed MSIX and public certificate, asks for certificate-trust consent, elevates only that trust step when needed and installs for the original user. Existing/newer packages are left unchanged. See [single-file installer](docs/single-file-installer.md).

0.7.8 adds opt-in Windows notification mirroring with permission controls in Settings → Notifications, a transient 6-second card, a bounded queue and content privacy options. Windows notification access requires the signed MSIX edition (Start menu → NativeIsland); the portable/Inno edition explicitly reports missing package identity. See [Windows notification setup](docs/windows-notifications.md).

0.7.7 fixes stale collapsed reminder summaries: the compact island and daily notifications now use only today's incomplete, unexpired entries, independently of the selected reminder day. Completed/expired entries remain in the day list. Latest executable: `out/build/reminder-summary-fix-x64/Release/NativeIsland.exe`.

0.7.6 adds Yesterday / Today / Tomorrow reminder navigation, completed-entry visibility and selected-day defaults for adding reminders. Google sync includes yesterday; notifications still use today only. Latest executable: `out/build/reminder-days-x64/Release/NativeIsland.exe`. See [reminder day navigation](docs/reminder-day-navigation.md).

0.7.5 redesigns Settings as four native sidebar pages (Appearance, Behavior, Clipboard, Google Calendar), with rounded cards/buttons, accent-colored sliders, visible values and dark owner-drawn dropdown items. Existing controls, persistence and Google authorization remain unchanged. Latest executable: `out/build/settings-x64/Release/NativeIsland.exe`. See [settings redesign](docs/settings-redesign.md).

0.7.4 removes Windows tray reminder balloons (island bell notifications remain), moves Google connect/sync/disconnect/calendar selection and daily summary preferences into Settings, and replaces the old reminder-management form with a compact title/date/time/all-day Add/Cancel entry. Google authorization and saved user data are unchanged.

0.7.3 fixes startup/reopen failure with a full saved settings.json: recursive regular-expression validation has been replaced by bounded, iterative flat-JSON parsing. The original user's valid 706-byte settings triggered regex_error(error_stack) in 0.7.2; loading those settings now succeeds without resetting them. Current executable: `out/build/startup-fix-x64/Release/NativeIsland.exe`; the previously reported `startup-x64/Release/NativeIsland.exe` is also updated after verification.

0.7.2 introduced admin-launch failure/cancellation fallback, removed the failing linked-token duplication path, and added worker exception containment (including Google service initialization). Explicit elevated entry opens directly, with a warning about Explorer drag/drop restrictions. See [startup details](docs/startup-update.md).

0.7.0 adds persistent local reminders, a bell/today page, configurable daily summaries and read-only Google Calendar OAuth integration. Existing settings and reminders live outside the installation directory and are preserved on upgrade. See [Google Calendar setup and verification limitations](docs/google-calendar-setup.md).

0.6.2 fixes settings dropdowns closing immediately: only CBN_SELCHANGE applies combo values, without refreshing the open control.

0.6.1 fixes native menu/dialog stacking above the topmost island. Settings, search and clipboard previews are now owned by the island; movement and animation preserve Z-order, and fullscreen polling no longer raises the island over menus.

0.6.0 adds animated transient audio/device/battery notifications, real endpoint controls in Media, and clipboard search/pinning/expiry. See [smart update details and limitations](docs/smart-update.md).

0.5.0 introduced smooth/spring/fast/off animation choices and a 120–600 ms duration setting. A reserved backing texture removes per-frame swap-chain resizing; DXGI frame readiness paces active transitions. See [animation implementation and verification](docs/animation-update.md).

0.4.0 adds scrollable full-text/image clipboard previews, four-column application tiles, real shelf file icons, horizontal dragging/alignment, monitor selection, a configurable hide/show hotkey and an administrator startup supervisor. The desktop UI intentionally runs unelevated to preserve Explorer drag/drop. See [V4 usage and verification](docs/v4-update.md).

V3 ports the supplied Windows 11 HTML design to native Direct2D/DirectComposition: six palettes, a 228 × 37 DIP capsule, a 465 × 334 DIP panel, icon tabs, real media controls/artwork/timeline, system cards, file shelf and private text clipboard panel. Appearance settings apply live and persist atomically in JSON.

NativeIsland is a native C++20, non-activating Windows 10/11 desktop overlay. It never attaches to Explorer, WorkerW or Progman and never changes desktop icons, display settings or taskbar state.

## Build

Requirements: Visual Studio 2022 Build Tools with Desktop C++, Windows 10/11 SDK and CMake.

```powershell
cmake --preset vs2022-x64
cmake --build --preset release
ctest --preset release
```

Run `out/build/startup-fix-x64/Release/NativeIsland.exe` (0.7.3) with a normal double-click; close an older instance first. The app requests an admin monitoring host, but cancellation/failure now falls back to normal UI instead of exiting. Avoid forcing administrator compatibility for the EXE/shortcut if Explorer drag/drop is needed. Hover expands the island; leaving it collapses after 430 ms. A click on the capsule/background pins/unpins expansion when "Tıklayınca açık tut" is enabled; background dragging changes horizontal position only. Settings also expose "Windows başladığında aç". Use the five top tabs, bell/today page and settings icon. Bell → + opens reminder management, morning-summary preferences and Google Calendar setup. Scroll file/clipboard/reminder entries or application grid rows with the mouse wheel. Drag files into the capsule to open Files, then drag shelf rows out to copy them. Applications' plus button adds a program or shortcut with its real icon. Ctrl+Alt+I hides/restores the island; the tray remains available.

Settings are stored at `%LOCALAPPDATA%\NativeIsland\settings.json`. Clipboard history is off by default, opt-in, memory-only, capped at 100 entries / 64 MiB, and cleared when disabled or the process exits. Search/pin entries and choose timed expiry in Settings. Click an entry for full text/image preview; see V4 notes for per-entry safety limits.

## Implemented

- DirectComposition flip-model composition surface with Direct2D/DirectWrite rendering and device-loss recovery
- Per-monitor DPI V2, monitor fallback and non-activating topmost tool window
- Primary-monitor top-center anchoring with a 7 DIP default gap and stable center during animation
- Cancelable hover state machine with delayed collapse and click-to-pin
- Timer-on-demand spring animation; no animation redraw loop while idle
- CPU, RAM, adapter and network throughput sampling on a worker thread
- Windows GSMTC current-session metadata and previous/play/next commands
- GSMTC event subscriptions, cached WIC artwork, supported seek and control capability states
- Windows GPU Engine counters and system disk usage on the telemetry worker
- Six shared theme palettes, bundled vector icons and separate native appearance settings
- Tray controls, optional current-user startup, fullscreen auto-hide
- Local drag-and-drop file shelf that stores references only and never moves/deletes sources
- Explicitly opt-in, memory-only text clipboard history

## Deliberate limits

Windows does not expose unrestricted notification history to an ordinary unpackaged desktop process. NativeIsland therefore does not claim to mirror every Windows notification. GPU temperature/VRAM are also not fabricated when no vendor-neutral sensor API exists. Third-party plugin execution and encrypted LAN transfer are deferred until their sandbox, identity, pairing and update trust models can be implemented and audited.

See `docs/` for architecture, security, performance and roadmap details.

See [V3 verification and manual checklist](docs/v3-verification.md). `--render-previews` creates native PNG exports and runs HWND geometry checks; `--smoke-test` exits cleanly after 30 seconds. These commands do not validate real mouse interaction or third-party media players.

