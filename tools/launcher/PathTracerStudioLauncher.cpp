#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>
#include <vector>

namespace
{
    void ShowLaunchError(const std::wstring& message)
    {
        MessageBoxW(nullptr, message.c_str(), L"PathTracer Studio", MB_OK | MB_ICONERROR);
    }
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    std::vector<wchar_t> modulePath(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, modulePath.data(),
                                            static_cast<DWORD>(modulePath.size()));
    if (length == 0 || length == modulePath.size())
    {
        ShowLaunchError(L"Unable to locate the application directory.");
        return 1;
    }

    std::wstring rootDirectory(modulePath.data(), length);
    const std::wstring::size_type separator = rootDirectory.find_last_of(L"\\/");
    if (separator == std::wstring::npos)
    {
        ShowLaunchError(L"Unable to resolve the application directory.");
        return 1;
    }
    rootDirectory.resize(separator);

    const std::wstring binDirectory = rootDirectory + L"\\bin";
    std::wstring rendererPath = binDirectory + L"\\PathTracerRenderer_research.exe";
    if (GetFileAttributesW(rendererPath.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        const std::wstring portableFallback = binDirectory + L"\\PathTracerStudioCore.exe";
        if (GetFileAttributesW(portableFallback.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            ShowLaunchError(L"The Research renderer executable is missing. Please keep the launcher next to the complete bin, assets, and src folders.");
            return 1;
        }
        rendererPath = portableFallback;
    }

    std::wstring commandLine = L"\"" + rendererPath + L"\"";
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};

    const BOOL started = CreateProcessW(
        rendererPath.c_str(), mutableCommand.data(), nullptr, nullptr, FALSE, 0,
        nullptr, binDirectory.c_str(), &startupInfo, &processInfo);

    if (!started)
    {
        const DWORD errorCode = GetLastError();
        ShowLaunchError(L"The renderer could not be started. Windows error code: " +
                        std::to_wstring(errorCode));
        return 1;
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return 0;
}
