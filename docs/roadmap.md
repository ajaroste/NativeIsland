# Roadmap

## V3 delivered

Reference-derived DIP layout and 380 ms easing, 430 ms hover leave delay, four functional icon tabs, all six theme palettes with custom accent/system follow, appearance settings/live preview, shadows and opacity, GPU Engine/disk cards, event-subscribed GSMTC artwork/timeline/seek, file and clipboard row actions, OLE auto-expand drop target, atomic background settings writer and native PNG/geometry test mode.

Real-player interoperability, mixed-monitor interaction, long-duration ETW and GPU reset checks remain manual; see `v3-verification.md`.

## Completed foundation

Native overlay and composition renderer, DPI/display recovery, spring states, tray/startup, fullscreen hiding, CPU/RAM/network data, GSMTC media controls, local file shelf and opt-in in-memory clipboard text history.

## V2 Phase A completed

- Primary-monitor top-center placement with configurable 0–40 DIP top gap
- DPI-scaled 170 × 34 DIP collapsed geometry
- Center-preserving, downward spring expansion
- Hover expansion, cancelable 350 ms leave delay and click-to-pin state
- Monitor/top-gap snap after dragging; no persistent horizontal offset

## Next audited milestones

1. UI Automation provider and keyboard navigation/accessibility audit.
2. Vendor-neutral GPU counters plus optional signed sensor-provider hosts.
3. Packaged identity and user-consented notification access where Windows supports it.
4. Versioned named-pipe widget host with AppContainer/restricted-token isolation.
5. Paired TLS LAN transfer with resumable writes, collision UI and SHA-256 verification.
6. SQLite history only for explicitly enabled non-sensitive data, installer/uninstaller and long-duration ETW testing.

