#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace ni {
// No network requests. Reads the Windows toast application's registered icon,
// its AppsFolder shell icon, or a matching running classic Win32 executable.
std::vector<std::uint8_t> notification_shell_icon(const std::wstring& app_id,
                                                    const std::wstring& display_name);
}
