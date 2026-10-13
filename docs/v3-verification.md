# NativeIsland V3 verification

Reference: the supplied `NativeIsland-Windows11-Themes.html` design.

## Design mapping

| Reference CSS | Native implementation |
| --- | --- |
| 228 × 37, top 7 | DIP geometry, selected monitor top-center anchor |
| 465 × 334, radius 30 | Expanded DIP geometry with animated radius |
| padding 13 / 16 | Shared navigation and page layout |
| cubic-bezier(.2,.9,.18,1.08), 380 ms | AnimationManager, time-axis solved cubic easing |
| leave delay 430 ms | Cancelable Win32 timer |
| six CSS token sets | `ui/theme.hpp`; custom accent and matching soft accent |
| SVG / character icons | Bundled Lucide vector geometry rendered with Direct2D SVG |
| demo media / metrics | GSMTC and real Windows counters; unavailable states |

The shadow uses layered vector rounded rectangles in a transparent 16 DIP margin, rather than the browser's Gaussian CSS shadow. PNG dimensions include that margin; the visible capsule retains the reference dimensions. No WebView is used.

## Automated and renderer verification

`NativeIsland.exe --render-previews` exports 30 PNGs under `out/previews` and checks actual HWND parent/owner, stable center/top edge during expand, expanded width, and restoration of the collapsed rectangle. Test mode disables clipboard collection and does not save theme/page changes.

`NativeIsland.exe --smoke-test` runs the normal message loop for 30 seconds and then closes through the application shutdown path. `ctest --preset release` tests configuration corruption/range handling/migration, palette overrides, DPI geometry, easing endpoints and spring convergence.

Native PNGs were inspected for navigation alignment, six-theme contrast, clipping, corner geometry and real system metrics. Browser comparison is unavailable in this session: browser inventory returned no connected browsers, and the native Computer Use helper was unavailable in earlier verification. Renderer exports are not desktop screenshots and do not prove real pointer/drag interaction.

Release x64 build and CTest passed. Native preview/geometry mode returned exit code 0 and produced 30 PNGs. The 30-second smoke run returned clean exit code 0; samples near 8/16/24 seconds showed 61.8 MiB working set and 512 handles, with CPU time 0.703125/0.734375/0.78125 seconds. These are short, machine-specific observations, not performance guarantees or a one-hour leak test.

## Manual screenshot and interaction checklist (not marked passed)

Use browser zoom 100%, a fixed Windows scale, and the same selected monitor. Do not change desktop resolution or DPI through the app.

- Compare the collapsed capsule against the HTML at 100%, 125% and 150%.
- Compare Widgets, Files, Media and Clipboard with all six themes.
- Open native appearance settings: preview boxes, custom color, follow-Windows-theme, opacity, shadow, animation, gap and width. Confirm live changes and persistence after exit/relaunch.
- Play Spotify, Chrome/YouTube and Edge media: metadata, cover, pause/previous/next, timeline and supported seek. Close the player and confirm stale metadata disappears.
- Enter/leave the capsule repeatedly; confirm a 430 ms cancelable collapse, pinning and stable center.
- Drag one/multiple files from Explorer. Confirm auto expansion and highlight, remove references and reveal sources. Check sources remain untouched.
- Enable text clipboard history voluntarily. Copy/click/delete/scroll items, disable collection and confirm the memory-only list clears. Images are not collected.
- Move between differently scaled monitors and disconnect a selected monitor. Confirm primary fallback and anchored geometry.
- Enter a fullscreen app on the island's monitor; verify hide/restore and lower telemetry sampling. Fullscreen on another monitor should not hide the island.
- Run at least one hour and capture ETW/WPR idle/animation profiles; GPU reset, sleep/resume and resource leak testing remain hardware/manual checks.

GPU utilization is the busiest Windows GPU Engine counter, not the sum of all engine percentages. Temperature/VRAM and ping are not fabricated and are not exposed as measured values.
