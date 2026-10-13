# Architecture

The UI thread owns the HWND, DirectComposition tree and Direct2D context. `SystemMonitor` and `MediaService` own stoppable worker threads and publish immutable snapshots protected by short mutex sections. Workers notify the HWND with posted messages; they never render or block the UI thread.

The overlay is a standalone `WS_POPUP` with `WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST`. It does not enumerate or mutate Explorer desktop windows. DirectComposition presents a premultiplied-alpha flip-model swap chain. Rendering resources are recreated after D2D/DXGI device loss.

Window interaction uses explicit `collapsed`, `expanding`, `expanded`, `collapsing`, `pinned` and `hidden` states. Hover exit arms a cancelable 350 ms timer rather than blocking the UI thread. Geometry is expressed in DIP, converted for the active monitor DPI, and anchored to the physical monitor top edge so expansion proceeds downward without changing the horizontal center.

Future widgets must expose lifecycle methods (initialize/start/suspend/resume/stop/dispose) behind a versioned message contract. Untrusted code must run out-of-process; a manifest is metadata, not a security boundary.

