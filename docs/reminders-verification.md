# 0.7.0 verification

Release build and CTest core suite passed. The extended core suite covers reminder JSON round trips including Turkish text, quotes/newlines and emoji; duplicate IDs; daily summary and due-event deduplication; restart/update preservation; Google-cache merge and local completion preservation; disconnect preserving local records; rollback after write failure; corrupt-file backup recovery and future-schema overwrite refusal. OAuth encoding and the RFC7636 S256 vector, timezone-offset parsing, all-day/recurring/cancelled Calendar response fixtures and existing-settings migration are included.

In the interactive standard desktop session, --render-previews exited 0 and exercised actual native reminder-manager controls (add, complete, delete and opening the morning-hour dropdown), companion window ownership, popup menu stacking, existing settings combos, animations and six pages across six themes. Reminder page captures in dark and light themes were visually inspected. --smoke-test ran for 30 seconds and exited 0.

The same graphical commands launched from the elevated build-tool executor returned fast-fail codes; standard desktop execution passed. Graphical verification should use the normal interactive desktop context, matching the application's --desktop-ui role, not the elevated build executor.

The Inno Setup package was built with the explicit reminders-x64 executable override. AppId and the %LOCALAPPDATA% data-directory paths are stable. The installer contains executable, license and setup guide only, with no user-data overwrite/delete directives. A real installed-to-installed upgrade was not performed on the user's running installation; preservation was checked through store reload tests and installer inspection.

Live Google sign-in, consent, refresh and synchronization remain unverified until the user supplies a Desktop OAuth JSON and authorizes their account. Windows tray-balloon delivery under Focus Assist and real midnight/resume transitions were not manually exercised; scheduling and persistence logic use deterministic timestamps in tests. The application must be running to produce reminders.
