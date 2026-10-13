# 0.6.2 — settings dropdowns

The settings WM_COMMAND handler previously called changed() and refresh() for every combo notification, including CBN_DROPDOWN and focus events. refresh() sent CB_SETCURSEL back to the open control, immediately closing its dropdown.

Animation, monitor and clipboard expiry combo notifications now have a dedicated path: only valid CBN_SELCHANGE updates Config and calls changed() once; other notifications do nothing. No form refresh occurs during selection. The existing changed callback still applies and persists the selected setting.

The built-in --render-previews regression opens each of the three actual native controls, verifies the dropdown remains open and above the island without changing settings, closes it, then checks every available selection and callback count. It uses isolated verification configuration without saving settings. Monitor choices are limited to monitors attached during the test.

Updated executable: out/build/combo-x64/Release/NativeIsland.exe. Close an old instance before launching this version.
