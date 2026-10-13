# Performance

V3 animation uses an 8 ms timer only during its 380 ms reference-easing transition; page fade timers stop after 200 ms. Outside transitions rendering follows data/input events. DirectWrite formats, SVG documents, the D2D brush and artwork bitmap are cached. System telemetry samples once per second while visible and once per ten seconds while fullscreen-hidden. GSMTC metadata follows session events; visible playing sessions additionally refresh timeline about once per second. WinRT waits are cancellable and bounded. Network throughput is the aggregate active-adapter counter delta and is labelled as adapter traffic.

Settings changes are coalesced on a background writer and replace JSON atomically. Explorer selection is dispatched through the Windows thread pool. Image decoding occurs on the media worker. The native PNG test mode verifies renderer outputs without a browser dependency.

No CPU, RAM or frame-time guarantees are claimed without ETW/WPR measurements on the target hardware. Recommended release validation: capture idle, animation, fullscreen transition, display hotplug, sleep/resume and GPU reset scenarios with WPR/WPA.

