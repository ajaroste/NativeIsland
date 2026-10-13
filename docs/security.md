# Security and privacy

- The app never modifies desktop files/icons, Explorer, taskbar, resolution or DPI.
- Dropped files remain at their original paths; clearing the shelf removes references only.
- Clipboard monitoring is disabled by default. Opt-in text is held only in process memory, capped and never persisted.
- Startup registration is per-user (`HKCU`) and reversible.
- No DLL injection, process patching, anti-cheat interaction or uncontrolled plugin DLL loading is used.
- LAN transfer and third-party plugins are not enabled until authenticated, encrypted and sandboxed designs are implemented.

