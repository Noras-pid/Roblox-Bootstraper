#include "../Header Files/menu.hh"
using namespace ImGui;
#include "../Header Files/globals.hh"
#include "../imgui/imgui_internal.h"
#include "../imgui/imgui.h"
#include <string>
#include <vector>
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <winhttp.h>
#include <urlmon.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <thread>
#include <mutex>
#include <atomic>
#include <shellapi.h>
#include <regex>
#include <new>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")

using json = nlohmann::json;
namespace fs = std::filesystem;

static std::string rbxV = "";
static NOTIFYICONDATA nid = {};
static std::atomic<bool> is_minimized_to_tray{ false };
static HHOOK keyboard_hook = nullptr;
static HWND main_window = nullptr;

static constexpr int VK_CODES[] = {
    VK_INSERT, VK_DELETE, VK_HOME, VK_END, VK_PRIOR, VK_NEXT,
    VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6, VK_F7, VK_F8, VK_F9, VK_F10, VK_F11, VK_F12,
    VK_NUMPAD0, VK_NUMPAD1, VK_NUMPAD2, VK_NUMPAD3, VK_NUMPAD4, VK_NUMPAD5, VK_NUMPAD6, VK_NUMPAD7, VK_NUMPAD8, VK_NUMPAD9
};

static constexpr const char* VK_NAMES[] = {
    "Insert", "Delete", "Home", "End", "Page Up", "Page Down",
    "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
    "Numpad 0", "Numpad 1", "Numpad 2", "Numpad 3", "Numpad 4", "Numpad 5", "Numpad 6", "Numpad 7", "Numpad 8", "Numpad 9"
};

struct VersionManager {
    char version_input[256] = "";
    char install_path[512] = "C:\\RustStrap";
    char selected_launch_version[256] = "";
    char latest_version[256] = "Fetching...";
    char latest_version_display[512] = "Latest: Fetching...";
    char last_exe_path[512] = "";
    char current_download_version[256] = "";
    char download_status[512] = "";
    char current_file_name[256] = "";
    char version_to_delete[256] = "";
    std::vector<std::string> installed_versions;
    std::atomic<bool> download_latest{ true };
    std::atomic<bool> is_downloading{ false };
    std::atomic<bool> is_installing{ false };
    std::atomic<bool> is_fetching_version{ false };
    std::atomic<bool> show_delete_confirmation{ false };
    std::atomic<bool> minimize_on_launch{ false };
    std::atomic<bool> minimize_on_startup{ false };
    std::atomic<bool> show_settings{ false };
    std::atomic<int> toggle_key_index{ 0 };
    std::atomic<int> selected_version_index{ 0 };
    std::atomic<bool> settings_loaded{ false };
    std::atomic<bool> first_run{ true };
    std::atomic<bool> auto_startup{ false };
    std::atomic<bool> waiting_for_key{ false };
    std::atomic<float> download_progress{ 0.0f };
} static vm;

static std::mutex mtx;

struct DownloadData {
    std::string version, install_path;
    DownloadData(const std::string& v, const std::string& p) : version(v), install_path(p) {}
};

static bool DrawButton(const char* label, ImVec2 size, int style = 0, bool disabled = false);
static void DrawProgressBar(ImVec2 pos, ImVec2 size, float progress);
static bool SelectFolder(char* buffer, size_t bufferSize);
static void ScanInstalledVersions();
static void LaunchVersion(const char* version);
static void DeleteVersion(const char* version);
static void SaveSettings();
static void LoadSettings();
static void ResetSettings();
static void FetchLatestVersion();
static void CreateShortcuts();
static void UpdateShortcuts();
static void CleanupTemp(const std::string& path);
static void MinimizeToTray();
static void RestoreFromTray();
static void CreateTrayIcon();
static void RemoveTrayIcon();
static void SetupHook();
static void RemoveHook();
static LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
static std::string GetExePath();
static std::string GetStartMenuPath();
static bool ValidateHash(const std::string& version);
static std::string NormalizeHash(const std::string& version);
static DWORD WINAPI DownloadThread(LPVOID lpParam);
static DWORD WINAPI FetchThread(LPVOID lpParam);
static void InstallRoblox(const std::string& path, const std::string& version);
static std::string FetchVersion();
static void UpdateAutoStartup();
static bool ProcessKeyInput();

void ui::renderMenu() {
    if (!globals.active && !is_minimized_to_tray.load()) return;

    if (!vm.settings_loaded.load()) {
        LoadSettings();
        ScanInstalledVersions();
        FetchLatestVersion();
        UpdateShortcuts();
        CreateTrayIcon();
        SetupHook();

        if (vm.minimize_on_startup.load() && vm.first_run.load()) {
            MinimizeToTray();
            vm.first_run.store(false);
        }
        vm.settings_loaded.store(true);
    }

    if (is_minimized_to_tray.load()) return;

    ImGui::SetNextWindowPos(ImVec2(window_pos.x, window_pos.y), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(660, 580));

    ImGui::Begin("RustStrap Installer", &globals.active, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
        ImGui::Text("Roblox Installation Manager");
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 200);
        int key_idx = vm.toggle_key_index.load();
        if (key_idx >= 0 && key_idx < 30) {
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Press %s to toggle tray", VK_NAMES[key_idx]);
        }

        ImGui::Separator();
        ImGui::Spacing();

        ImGui::BeginChild("StatusPanel", ImVec2(0, 75), true, ImGuiWindowFlags_NoScrollbar);
        {
            ImGui::Columns(2, "StatusColumns", false);
            ImGui::SetColumnWidth(0, 320);

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.9f, 0.3f, 1.0f));
            ImGui::Text("Status: ONLINE");
            ImGui::PopStyleColor();
            ImGui::Text("CDN: setup-aws.rbxcdn.com");

            ImGui::NextColumn();
            ImGui::Text("Installed: %zu versions", vm.installed_versions.size());
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s", vm.latest_version_display);
            ImGui::Columns(1);
        }
        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
        ImGui::Text("INSTALLATION");
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Columns(3, "VersionColumns", false);
        ImGui::SetColumnWidth(0, 120);
        ImGui::SetColumnWidth(1, 140);

        bool dl_latest = vm.download_latest.load();
        if (ImGui::RadioButton("Latest", dl_latest)) {
            vm.download_latest.store(true);
            SaveSettings();
        }

        ImGui::NextColumn();
        if (ImGui::RadioButton("Custom", !dl_latest)) {
            vm.download_latest.store(false);
            SaveSettings();
        }

        ImGui::NextColumn();
        if (DrawButton("Refresh Latest", ImVec2(130, 0), 2, vm.is_fetching_version.load())) {
            FetchLatestVersion();
        }
        ImGui::Columns(1);
        ImGui::Spacing();

        if (!dl_latest) {
            ImGui::Text("Custom Version Hash:");
            ImGui::PushItemWidth(-1);
            if (ImGui::InputTextWithHint("##version_input", "Enter version hash (e.g., version-1234567890abcdef)",
                vm.version_input, sizeof(vm.version_input))) {
                SaveSettings();
            }
            ImGui::PopItemWidth();

            std::string input = vm.version_input;
            if (!input.empty()) {
                if (ValidateHash(input)) {
                    ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "Valid version format");
                }
                else {
                    ImGui::TextColored(ImVec4(0.8f, 0.0f, 0.0f, 1.0f), "Invalid format! Use: version-1234567890abcdef");
                }
            }
            else {
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Tip: Get version hash from Roblox deployment logs");
            }
            ImGui::Spacing();
        }

        ImGui::Text("Installation Path:");
        ImGui::PushItemWidth(-100);
        if (ImGui::InputText("##install_path", vm.install_path, sizeof(vm.install_path))) {
            SaveSettings();
            ScanInstalledVersions();
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (DrawButton("Browse", ImVec2(90, 0))) {
            if (SelectFolder(vm.install_path, sizeof(vm.install_path))) {
                SaveSettings();
                ScanInstalledVersions();
            }
        }

        ImGui::Spacing();

        {
            std::lock_guard<std::mutex> lock(mtx);
            if (vm.is_downloading.load() || vm.is_installing.load()) {
                ImGui::BeginChild("ProgressPanel", ImVec2(0, 65), true, ImGuiWindowFlags_NoScrollbar);
                {
                    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "%s", vm.download_status);
                    ImGui::Spacing();
                    DrawProgressBar(ImGui::GetCursorScreenPos(), ImVec2(ImGui::GetContentRegionAvail().x, 22), vm.download_progress.load());
                    ImGui::Dummy(ImVec2(0, 27));
                }
                ImGui::EndChild();
            }
        }

        bool install_disabled = vm.is_downloading.load() || vm.is_installing.load();
        if (!dl_latest) {
            std::string input = vm.version_input;
            install_disabled = install_disabled || input.empty() || !ValidateHash(input);
        }

        if (DrawButton("Install Roblox", ImVec2(150, 35), 1, install_disabled)) {
            std::string version = dl_latest ? vm.latest_version : NormalizeHash(vm.version_input);
            auto data = new DownloadData(version, vm.install_path);
            CreateThread(NULL, 0, DownloadThread, data, 0, NULL);
        }

        ImGui::Spacing();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.8f, 1.0f));
        ImGui::Text("VERSION MANAGEMENT");
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Select Version:");
        ImGui::PushItemWidth(-200);

        if (vm.installed_versions.empty()) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No versions installed");
        }
        else {
            std::vector<const char*> items;
            for (const auto& v : vm.installed_versions) items.push_back(v.c_str());

            int sel_idx = vm.selected_version_index.load();
            if (ImGui::Combo("##version_select", &sel_idx, items.data(), (int)items.size())) {
                vm.selected_version_index.store(sel_idx);
                if (sel_idx >= 0 && sel_idx < (int)vm.installed_versions.size()) {
                    strcpy_s(vm.selected_launch_version, vm.installed_versions[sel_idx].c_str());
                    SaveSettings();
                }
            }
        }
        ImGui::PopItemWidth();

        ImGui::SameLine();
        if (DrawButton("Refresh", ImVec2(70, 0))) {
            ScanInstalledVersions();
        }

        ImGui::SameLine();
        bool del_disabled = vm.installed_versions.empty();
        if (DrawButton("Delete", ImVec2(70, 0), 3, del_disabled)) {
            int sel_idx = vm.selected_version_index.load();
            if (!del_disabled && sel_idx >= 0 && sel_idx < (int)vm.installed_versions.size()) {
                strcpy_s(vm.version_to_delete, vm.installed_versions[sel_idx].c_str());
                vm.show_delete_confirmation.store(true);
            }
        }

        ImGui::Spacing();

        ImGui::Columns(3, "ActionColumns", false);
        ImGui::SetColumnWidth(0, 200);
        ImGui::SetColumnWidth(1, 200);

        bool launch_disabled = vm.installed_versions.empty();
        if (DrawButton("Launch Roblox", ImVec2(150, 35), 0, launch_disabled)) {
            int sel_idx = vm.selected_version_index.load();
            if (!launch_disabled && sel_idx >= 0 && sel_idx < (int)vm.installed_versions.size()) {
                LaunchVersion(vm.installed_versions[sel_idx].c_str());
            }
        }

        ImGui::NextColumn();
        if (DrawButton("Settings", ImVec2(100, 35), 2)) {
            vm.show_settings.store(true);
        }

        ImGui::NextColumn();
        if (DrawButton("Exit", ImVec2(80, 35), 3)) {
            SaveSettings();
            RemoveTrayIcon();
            RemoveHook();
            globals.active = false;
        }
        ImGui::Columns(1);

        {
            std::lock_guard<std::mutex> lock(mtx);
            if (strlen(vm.download_status) > 0) {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "Status: %s", vm.download_status);
            }
        }
    }
    ImGui::End();

    if (vm.show_settings.load()) {
        ImGui::SetNextWindowPos(ImVec2(window_pos.x + 50, window_pos.y + 50), ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(ImVec2(400, 420));

        bool show_settings = true;
        ImGui::Begin("Settings", &show_settings, ImGuiWindowFlags_NoResize);
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            ImGui::Text("GENERAL SETTINGS");
            ImGui::PopStyleColor();
            ImGui::Separator();
            ImGui::Spacing();

            bool startup = vm.minimize_on_startup.load();
            if (ImGui::Checkbox("Minimize on startup", &startup)) {
                vm.minimize_on_startup.store(startup);
                SaveSettings();
            }
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(Start minimized to tray)");

            bool launch = vm.minimize_on_launch.load();
            if (ImGui::Checkbox("Minimize on launch", &launch)) {
                vm.minimize_on_launch.store(launch);
                SaveSettings();
            }
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(Minimize when launching Roblox)");

            bool autostart = vm.auto_startup.load();
            if (ImGui::Checkbox("Run on Windows startup", &autostart)) {
                vm.auto_startup.store(autostart);
                UpdateAutoStartup();
                SaveSettings();
            }
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(Start with Windows)");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.8f, 1.0f));
            ImGui::Text("HOTKEYS");
            ImGui::PopStyleColor();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("Toggle tray hotkey:");
            ImGui::PushItemWidth(200);
            if (vm.waiting_for_key.load()) {
                ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "Press a key...");
                if (ProcessKeyInput()) {
                    std::lock_guard<std::mutex> lock(mtx);
                    RemoveHook();
                    SetupHook();
                    SaveSettings();
                    if (nid.hWnd && vm.toggle_key_index.load() >= 0 && vm.toggle_key_index.load() < 30) {
                        sprintf_s(nid.szTip, "RustStrap - Press %s to toggle", VK_NAMES[vm.toggle_key_index.load()]);
                        Shell_NotifyIcon(NIM_MODIFY, &nid);
                    }
                }
            }
            else {
                int key_idx = vm.toggle_key_index.load();
                std::string label = key_idx >= 0 && key_idx < 30 ? std::string("Set Key (Current: ") + VK_NAMES[key_idx] + ")" : "Set Key";
                if (DrawButton(label.c_str(), ImVec2(200, 0))) {
                    vm.waiting_for_key.store(true);
                }
            }
            ImGui::PopItemWidth();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Press this key to show/hide RustStrap");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
            ImGui::Text("SHORTCUTS");
            ImGui::PopStyleColor();
            ImGui::Separator();
            ImGui::Spacing();

            if (DrawButton("Create Desktop & Start Menu Shortcuts", ImVec2(300, 30), 1)) {
                CreateShortcuts();
            }
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Creates shortcuts for easy access");

            ImGui::Spacing();

            if (DrawButton("Register 'ruststrap' command", ImVec2(300, 30), 2)) {
                CreateShortcuts();
            }
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Type 'ruststrap' in Windows search");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
            ImGui::Text("CONFIGURATION");
            ImGui::PopStyleColor();
            ImGui::Separator();
            ImGui::Spacing();

            if (DrawButton("Reset Configuration", ImVec2(300, 30), 3)) {
                ResetSettings();
                LoadSettings();
            }
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Resets all settings to default");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Columns(2, "SettingsButtons", false);
            if (DrawButton("Apply & Close", ImVec2(120, 30), 1)) {
                SaveSettings();
                vm.show_settings.store(false);
            }

            ImGui::NextColumn();
            if (DrawButton("Cancel", ImVec2(80, 30))) {
                LoadSettings();
                vm.show_settings.store(false);
            }
            ImGui::Columns(1);
        }
        ImGui::End();

        if (!show_settings) vm.show_settings.store(false);
    }

    if (vm.show_delete_confirmation.load()) {
        ImGui::OpenPopup("Delete Version");
    }

    if (ImGui::BeginPopupModal("Delete Version", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Are you sure you want to delete version:");
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", vm.version_to_delete);
        ImGui::Text("This action cannot be undone!");
        ImGui::Separator();
        ImGui::Spacing();

        if (DrawButton("Yes, Delete", ImVec2(120, 0), 3)) {
            DeleteVersion(vm.version_to_delete);
            vm.show_delete_confirmation.store(false);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (DrawButton("Cancel", ImVec2(80, 0))) {
            vm.show_delete_confirmation.store(false);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

static LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && wParam == WM_KEYDOWN) {
        auto* kb = (KBDLLHOOKSTRUCT*)lParam;
        int key_idx = vm.toggle_key_index.load();
        if (kb && key_idx >= 0 && key_idx < 30 && kb->vkCode == VK_CODES[key_idx]) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                if (is_minimized_to_tray.load()) RestoreFromTray();
                else MinimizeToTray();
            }
            return 1;
        }
    }
    return CallNextHookEx(keyboard_hook, nCode, wParam, lParam);
}

static void SetupHook() {
    if (!keyboard_hook) {
        keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandle(NULL), 0);
        if (!keyboard_hook) {
            std::lock_guard<std::mutex> lock(mtx);
            strcpy_s(vm.download_status, "Failed to set keyboard hook!");
            std::thread([] {
                Sleep(3000);
                std::lock_guard<std::mutex> lock(mtx);
                vm.download_status[0] = '\0';
                }).detach();
        }
    }
}

static void RemoveHook() {
    if (keyboard_hook) {
        UnhookWindowsHookEx(keyboard_hook);
        keyboard_hook = nullptr;
    }
}

static bool ValidateHash(const std::string& version) {
    if (version.empty()) return false;
    try {
        std::regex regex("^version-[a-fA-F0-9]{16}$");
        return std::regex_match(version, regex);
    }
    catch (...) { return false; }
}

static std::string NormalizeHash(const std::string& version) {
    std::string norm = version;
    try {
        norm.erase(std::remove_if(norm.begin(), norm.end(), ::isspace), norm.end());
        std::transform(norm.begin(), norm.end(), norm.begin(), ::tolower);
        if (norm.find("version-") != 0 && norm.length() == 16) {
            norm = "version-" + norm;
        }
    }
    catch (...) { return version; }
    return norm;
}

static bool ProcessKeyInput() {
    for (int i = 0; i < 256; ++i) {
        if (ImGui::IsKeyPressed(i)) {
            for (int j = 0; j < 30; ++j) {
                if (i == VK_CODES[j]) {
                    std::lock_guard<std::mutex> lock(mtx);
                    vm.toggle_key_index.store(j);
                    vm.waiting_for_key.store(false);
                    return true;
                }
            }
            std::lock_guard<std::mutex> lock(mtx);
            strcpy_s(vm.download_status, "Invalid key! Use Insert, F1-F12, Numpad 0-9, etc.");
            vm.waiting_for_key.store(false);
            std::thread([] {
                Sleep(3000);
                std::lock_guard<std::mutex> lock(mtx);
                vm.download_status[0] = '\0';
                }).detach();
            return true;
        }
    }
    return false;
}

static std::string GetStartMenuPath() {
    char path[MAX_PATH];
    return (SHGetFolderPathA(NULL, CSIDL_PROGRAMS, NULL, SHGFP_TYPE_CURRENT, path) == S_OK) ? std::string(path) : "";
}

static void CleanupTemp(const std::string& path) {
    try {
        if (!fs::exists(path)) return;
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.is_regular_file()) {
                std::string name = entry.path().filename().string();
                if (name.find("temp") != std::string::npos && name.find(".zip") != std::string::npos) {
                    std::error_code ec;
                    fs::remove(entry.path(), ec);
                }
            }
        }
        std::error_code ec;
        fs::remove(path + "\\temp.zip", ec);
        for (int i = 0; i < 25; i++) {
            fs::remove(path + "\\temp_" + std::to_string(i) + ".zip", ec);
        }
    }
    catch (...) {}
}

static void CreateTrayIcon() {
    main_window = GetActiveWindow();
    if (!main_window) return;

    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = main_window;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_USER + 1;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    int key_idx = vm.toggle_key_index.load();
    if (key_idx >= 0 && key_idx < 30) {
        sprintf_s(nid.szTip, "RustStrap - Press %s to toggle", VK_NAMES[key_idx]);
    }
    else {
        strcpy_s(nid.szTip, "RustStrap Installer");
    }
    Shell_NotifyIcon(NIM_ADD, &nid);
}

static void RemoveTrayIcon() {
    if (nid.hWnd) {
        Shell_NotifyIcon(NIM_DELETE, &nid);
        ZeroMemory(&nid, sizeof(nid));
    }
}

static void MinimizeToTray() {
    is_minimized_to_tray.store(true);
    if (main_window) {
        ShowWindow(main_window, SW_HIDE);
        int key_idx = vm.toggle_key_index.load();
        if (key_idx >= 0 && key_idx < 30) {
            sprintf_s(nid.szTip, "RustStrap - Press %s to toggle", VK_NAMES[key_idx]);
            Shell_NotifyIcon(NIM_MODIFY, &nid);
        }
    }
}

static void RestoreFromTray() {
    is_minimized_to_tray.store(false);
    if (main_window) {
        ShowWindow(main_window, SW_SHOW);
        SetForegroundWindow(main_window);
    }
}

static bool DrawButton(const char* label, ImVec2 size, int style, bool disabled) {
    if (!label) return false;

    ImVec4 colors[4];
    if (disabled) {
        colors[0] = colors[1] = colors[2] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
        colors[3] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
    }
    else {
        switch (style) {
        case 1:
            colors[0] = ImVec4(0.2f, 0.6f, 0.2f, 1.0f);
            colors[1] = ImVec4(0.3f, 0.7f, 0.3f, 1.0f);
            colors[2] = ImVec4(0.1f, 0.5f, 0.1f, 1.0f);
            break;
        case 2:
            colors[0] = ImVec4(0.5f, 0.2f, 0.7f, 1.0f);
            colors[1] = ImVec4(0.6f, 0.3f, 0.8f, 1.0f);
            colors[2] = ImVec4(0.4f, 0.1f, 0.6f, 1.0f);
            break;
        case 3:
            colors[0] = ImVec4(0.7f, 0.2f, 0.2f, 1.0f);
            colors[1] = ImVec4(0.8f, 0.3f, 0.3f, 1.0f);
            colors[2] = ImVec4(0.6f, 0.1f, 0.1f, 1.0f);
            break;
        default:
            colors[0] = ImVec4(0.2f, 0.4f, 0.7f, 1.0f);
            colors[1] = ImVec4(0.3f, 0.5f, 0.8f, 1.0f);
            colors[2] = ImVec4(0.1f, 0.3f, 0.6f, 1.0f);
            break;
        }
        colors[3] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    ImGui::PushStyleColor(ImGuiCol_Button, colors[0]);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colors[1]);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, colors[2]);
    ImGui::PushStyleColor(ImGuiCol_Text, colors[3]);

    bool clicked = ImGui::Button(label, size) && !disabled;
    ImGui::PopStyleColor(4);
    return clicked;
}

static void DrawProgressBar(ImVec2 pos, ImVec2 size, float progress) {
    auto* draw = ImGui::GetWindowDrawList();
    if (!draw) return;

    progress = std::clamp(progress, 0.0f, 1.0f);

    draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(35, 35, 35, 255), 6.0f);

    if (progress > 0.0f) {
        float fill = size.x * progress;
        draw->AddRectFilled(pos, ImVec2(pos.x + fill, pos.y + size.y), IM_COL32(0, 140, 255, 255), 6.0f);
    }

    draw->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(70, 70, 70, 255), 6.0f, 0, 1.5f);

    char text[32];
    sprintf_s(text, "%.1f%%", progress * 100.0f);
    ImVec2 text_size = ImGui::CalcTextSize(text);
    ImVec2 text_pos = ImVec2(pos.x + (size.x - text_size.x) * 0.5f, pos.y + (size.y - text_size.y) * 0.5f);
    draw->AddText(text_pos, IM_COL32(255, 255, 255, 255), text);
}

static std::string GetExePath() {
    char path[MAX_PATH];
    return (GetModuleFileNameA(NULL, path, MAX_PATH) > 0) ? std::string(path) : "";
}

static void CreateShortcuts() {
    std::string exe = GetExePath();
    if (exe.empty()) return;

    bool success = false;

    try {
        char desktop[MAX_PATH];
        if (SHGetFolderPathA(NULL, CSIDL_DESKTOP, NULL, SHGFP_TYPE_CURRENT, desktop) == S_OK) {
            std::string link = std::string(desktop) + "\\ruststrap.lnk";

            IShellLinkA* shell = nullptr;
            if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkA, (void**)&shell))) {
                shell->SetPath(exe.c_str());
                shell->SetDescription("RustStrap Roblox Installer");
                shell->SetWorkingDirectory(fs::path(exe).parent_path().string().c_str());

                IPersistFile* persist = nullptr;
                if (SUCCEEDED(shell->QueryInterface(IID_IPersistFile, (void**)&persist))) {
                    std::wstring wide(link.begin(), link.end());
                    if (SUCCEEDED(persist->Save(wide.c_str(), TRUE))) success = true;
                    persist->Release();
                }
                shell->Release();
            }
        }

        std::string start_menu = GetStartMenuPath();
        if (!start_menu.empty()) {
            std::string link = start_menu + "\\ruststrap.lnk";

            IShellLinkA* shell = nullptr;
            if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkA, (void**)&shell))) {
                shell->SetPath(exe.c_str());
                shell->SetDescription("RustStrap Roblox Installer");
                shell->SetWorkingDirectory(fs::path(exe).parent_path().string().c_str());

                IPersistFile* persist = nullptr;
                if (SUCCEEDED(shell->QueryInterface(IID_IPersistFile, (void**)&persist))) {
                    std::wstring wide(link.begin(), link.end());
                    if (SUCCEEDED(persist->Save(wide.c_str(), TRUE))) success = true;
                    persist->Release();
                }
                shell->Release();
            }
        }

        HKEY key;
        if (RegCreateKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\ruststrap.exe",
            0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &key, NULL) == ERROR_SUCCESS) {
            if (RegSetValueExA(key, "", 0, REG_SZ, (BYTE*)exe.c_str(), (DWORD)exe.length() + 1) == ERROR_SUCCESS) {
                std::string parent = fs::path(exe).parent_path().string();
                RegSetValueExA(key, "Path", 0, REG_SZ, (BYTE*)parent.c_str(), (DWORD)parent.length() + 1);
                success = true;
            }
            RegCloseKey(key);
        }
    }
    catch (...) { success = false; }

    {
        std::lock_guard<std::mutex> lock(mtx);
        strcpy_s(vm.download_status, success ? "Shortcuts created successfully!" : "Failed to create shortcuts.");
    }

    std::thread([] {
        Sleep(3000);
        std::lock_guard<std::mutex> lock(mtx);
        vm.download_status[0] = '\0';
        }).detach();
}

static void UpdateAutoStartup() {
    HKEY key;
    std::string exe = GetExePath();
    if (exe.empty()) return;

    bool success = false;

    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &key) == ERROR_SUCCESS) {
        if (vm.auto_startup.load()) {
            if (RegSetValueExA(key, "RustStrap", 0, REG_SZ, (BYTE*)exe.c_str(), (DWORD)exe.length() + 1) == ERROR_SUCCESS) {
                success = true;
            }
        }
        else {
            if (RegDeleteValueA(key, "RustStrap") == ERROR_SUCCESS || GetLastError() == ERROR_FILE_NOT_FOUND) {
                success = true;
            }
        }
        RegCloseKey(key);
    }

    {
        std::lock_guard<std::mutex> lock(mtx);
        strcpy_s(vm.download_status, success ? "Auto-startup updated successfully!" : "Failed to update auto-startup.");
    }

    std::thread([] {
        Sleep(3000);
        std::lock_guard<std::mutex> lock(mtx);
        vm.download_status[0] = '\0';
        }).detach();
}

static void UpdateShortcuts() {
    std::string current = GetExePath();
    if (current.empty()) return;

    if (strlen(vm.last_exe_path) > 0 && current != vm.last_exe_path) {
        CreateShortcuts();
        strcpy_s(vm.last_exe_path, current.c_str());
        SaveSettings();
    }
    else if (strlen(vm.last_exe_path) == 0) {
        strcpy_s(vm.last_exe_path, current.c_str());
        SaveSettings();
    }
}

static std::string FetchVersion() {
    HINTERNET session = nullptr, connect = nullptr, request = nullptr;
    std::string result;

    try {
        session = WinHttpOpen(L"RustStrap/3.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session) throw std::runtime_error("Session failed");

        connect = WinHttpConnect(session, L"clientsettingscdn.roblox.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!connect) throw std::runtime_error("Connect failed");

        request = WinHttpOpenRequest(connect, L"GET", L"/v2/client-version/WindowsPlayer/", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!request) throw std::runtime_error("Request failed");

        if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
            !WinHttpReceiveResponse(request, NULL)) throw std::runtime_error("Send/Receive failed");

        std::string data;
        BYTE buffer[4096];
        DWORD read;

        while (WinHttpReadData(request, buffer, sizeof(buffer), &read) && read > 0) {
            data.append((char*)buffer, read);
        }

        json resp = json::parse(data);
        if (resp.contains("clientVersionUpload")) {
            result = resp["clientVersionUpload"].get<std::string>();
        }
    }
    catch (...) {}

    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    if (session) WinHttpCloseHandle(session);

    return result;
}

static void FetchLatestVersion() {
    CreateThread(NULL, 0, FetchThread, NULL, 0, NULL);
}

static DWORD WINAPI FetchThread(LPVOID) {
    {
        std::lock_guard<std::mutex> lock(mtx);
        vm.is_fetching_version.store(true);
        strcpy_s(vm.latest_version, "Fetching...");
        strcpy_s(vm.latest_version_display, "Latest: Fetching...");
    }

    std::string version = FetchVersion();

    {
        std::lock_guard<std::mutex> lock(mtx);
        if (!version.empty()) {
            strcpy_s(vm.latest_version, version.c_str());
            std::string display = "Latest: " + version;
            strcpy_s(vm.latest_version_display, display.c_str());
        }
        else {
            strcpy_s(vm.latest_version, "Error");
            strcpy_s(vm.latest_version_display, "Latest: Error");
        }
        vm.is_fetching_version.store(false);
    }

    if (!version.empty()) SaveSettings();
    return 0;
}

static void InstallRoblox(const std::string& path, const std::string& version) {
    if (version.empty() || path.empty()) return;

    try {
        CleanupTemp(path);
        if (!fs::exists(path)) fs::create_directories(path);

        rbxV = version;
        std::string ver_path = path + "\\Versions\\" + rbxV;
        if (!fs::exists(ver_path)) fs::create_directories(ver_path);

        const std::string host = "https://setup-aws.rbxcdn.com/";
        const std::array<std::pair<std::string, std::string>, 22> files{ {
            {host + rbxV + "-RobloxApp.zip", ""},
            {host + rbxV + "-redist.zip", ""},
            {host + rbxV + "-shaders.zip", "shaders\\"},
            {host + rbxV + "-ssl.zip", "ssl\\"},
            {host + rbxV + "-WebView2.zip", ""},
            {host + rbxV + "-WebView2RuntimeInstaller.zip", "WebView2RuntimeInstaller\\"},
            {host + rbxV + "-content-avatar.zip", "content\\avatar\\"},
            {host + rbxV + "-content-configs.zip", "content\\configs\\"},
            {host + rbxV + "-content-fonts.zip", "content\\fonts\\"},
            {host + rbxV + "-content-sky.zip", "content\\sky\\"},
            {host + rbxV + "-content-sounds.zip", "content\\sounds\\"},
            {host + rbxV + "-content-textures2.zip", "content\\textures\\"},
            {host + rbxV + "-content-models.zip", "content\\models\\"},
            {host + rbxV + "-content-platform-fonts.zip", "PlatformContent\\pc\\fonts\\"},
            {host + rbxV + "-content-platform-dictionaries.zip", "PlatformContent\\pc\\shared_compression_dictionaries\\"},
            {host + rbxV + "-content-terrain.zip", "PlatformContent\\pc\\terrain\\"},
            {host + rbxV + "-content-textures3.zip", "PlatformContent\\pc\\textures\\"},
            {host + rbxV + "-extracontent-luapackages.zip", "ExtraContent\\LuaPackages\\"},
            {host + rbxV + "-extracontent-translations.zip", "ExtraContent\\translations\\"},
            {host + rbxV + "-extracontent-models.zip", "ExtraContent\\models\\"},
            {host + rbxV + "-extracontent-textures.zip", "ExtraContent\\textures\\"},
            {host + rbxV + "-extracontent-places.zip", "ExtraContent\\places\\"}
        } };

        {
            std::lock_guard<std::mutex> lock(mtx);
            strcpy_s(vm.download_status, "Starting installation...");
            vm.download_progress.store(0.0f);
        }

        for (size_t i = 0; i < files.size(); i++) {
            const auto& [url, dest] = files[i];

            {
                std::lock_guard<std::mutex> lock(mtx);
                std::string name = fs::path(url).filename().string();
                strcpy_s(vm.current_file_name, name.c_str());
                std::string status = "Downloading " + name + " (" + std::to_string(i + 1) + "/" + std::to_string(files.size()) + ")";
                strcpy_s(vm.download_status, status.c_str());
                vm.download_progress.store((float)i / (float)files.size());
            }

            std::wstring wide_url(url.begin(), url.end());
            std::wstring temp_path = std::wstring(path.begin(), path.end()) + L"\\temp.zip";

            HRESULT hr = URLDownloadToFileW(NULL, wide_url.c_str(), temp_path.c_str(), 0, NULL);

            if (SUCCEEDED(hr)) {
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    std::string status = "Extracting " + std::string(vm.current_file_name) + " (" + std::to_string(i + 1) + "/" + std::to_string(files.size()) + ")";
                    strcpy_s(vm.download_status, status.c_str());
                }

                std::string extract_path = ver_path + "\\" + dest;
                if (!fs::exists(extract_path)) fs::create_directories(extract_path);

                std::string cmd = "powershell -Command \"Expand-Archive -Path '" + path + "\\temp.zip' -DestinationPath '" + extract_path + "' -Force\"";
                system(cmd.c_str());

                std::error_code ec;
                fs::remove(path + "\\temp.zip", ec);
            }
            else {
                Sleep(2000);
                hr = URLDownloadToFileW(NULL, wide_url.c_str(), temp_path.c_str(), 0, NULL);
                if (SUCCEEDED(hr)) {
                    std::string extract_path = ver_path + "\\" + dest;
                    if (!fs::exists(extract_path)) fs::create_directories(extract_path);
                    std::string cmd = "powershell -Command \"Expand-Archive -Path '" + path + "\\temp.zip' -DestinationPath '" + extract_path + "' -Force\"";
                    system(cmd.c_str());
                    std::error_code ec;
                    fs::remove(path + "\\temp.zip", ec);
                }
            }
            Sleep(100);
        }

        {
            std::lock_guard<std::mutex> lock(mtx);
            vm.is_downloading.store(false);
            vm.is_installing.store(true);
            strcpy_s(vm.download_status, "Creating configuration files...");
            vm.download_progress.store(1.0f);
        }

        std::ofstream settings(ver_path + "\\AppSettings.xml");
        if (settings.is_open()) {
            settings << R"(<?xml version="1.0" encoding="UTF-8"?>
<Settings>
	<ContentFolder>content</ContentFolder>
	<BaseUrl>http://www.roblox.com</BaseUrl>
</Settings>)";
            settings.close();
        }

        CleanupTemp(path);

        {
            std::lock_guard<std::mutex> lock(mtx);
            strcpy_s(vm.download_status, "Installation completed successfully!");
        }

        Sleep(3000);

        {
            std::lock_guard<std::mutex> lock(mtx);
            vm.is_installing.store(false);
            vm.download_status[0] = '\0';
            vm.download_progress.store(0.0f);
        }
    }
    catch (...) {
        std::lock_guard<std::mutex> lock(mtx);
        vm.is_downloading.store(false);
        vm.is_installing.store(false);
        strcpy_s(vm.download_status, "Installation failed!");
    }
}

static DWORD WINAPI DownloadThread(LPVOID lpParam) {
    auto* data = (DownloadData*)lpParam;

    {
        std::lock_guard<std::mutex> lock(mtx);
        vm.is_downloading.store(true);
        vm.is_installing.store(false);
        strcpy_s(vm.current_download_version, data->version.c_str());
        strcpy_s(vm.download_status, "Initializing installation...");
        vm.download_progress.store(0.0f);
    }

    InstallRoblox(data->install_path, data->version);
    ScanInstalledVersions();
    SaveSettings();

    delete data;
    return 0;
}

static void DeleteVersion(const char* version) {
    std::string ver_path = std::string(vm.install_path) + "\\Versions\\" + version;

    if (fs::exists(ver_path)) {
        try {
            fs::remove_all(ver_path);

            {
                std::lock_guard<std::mutex> lock(mtx);
                std::string msg = "Deleted version " + std::string(version) + " successfully!";
                strcpy_s(vm.download_status, msg.c_str());
            }

            ScanInstalledVersions();
            SaveSettings();

            std::thread([] {
                Sleep(3000);
                std::lock_guard<std::mutex> lock(mtx);
                vm.download_status[0] = '\0';
                }).detach();

        }
        catch (...) {
            std::lock_guard<std::mutex> lock(mtx);
            std::string msg = "Failed to delete version " + std::string(version);
            strcpy_s(vm.download_status, msg.c_str());
        }
    }
}

static void ScanInstalledVersions() {
    vm.installed_versions.clear();
    std::string ver_path = std::string(vm.install_path) + "\\Versions";

    if (!fs::exists(ver_path)) return;

    try {
        for (const auto& entry : fs::directory_iterator(ver_path)) {
            if (entry.is_directory()) {
                std::string name = entry.path().filename().string();
                std::string exe = entry.path().string() + "\\RobloxPlayerBeta.exe";
                if (fs::exists(exe)) {
                    vm.installed_versions.push_back(name);
                }
            }
        }
    }
    catch (...) {}

    std::sort(vm.installed_versions.begin(), vm.installed_versions.end());

    if (!vm.installed_versions.empty()) {
        auto it = std::find(vm.installed_versions.begin(), vm.installed_versions.end(), vm.selected_launch_version);
        if (it != vm.installed_versions.end()) {
            vm.selected_version_index.store((int)(it - vm.installed_versions.begin()));
        }
        else {
            vm.selected_version_index.store(0);
            strcpy_s(vm.selected_launch_version, vm.installed_versions[0].c_str());
        }
    }
}

static void LaunchVersion(const char* version) {
    std::string exe = std::string(vm.install_path) + "\\Versions\\" + version + "\\RobloxPlayerBeta.exe";

    if (!fs::exists(exe)) {
        MessageBoxA(NULL, "RobloxPlayerBeta.exe not found!", "Launch Error", MB_OK | MB_ICONERROR);
        return;
    }

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    if (CreateProcessA(exe.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        {
            std::lock_guard<std::mutex> lock(mtx);
            std::string msg = "Launched version " + std::string(version) + " successfully!";
            strcpy_s(vm.download_status, msg.c_str());
        }

        if (vm.minimize_on_launch.load()) MinimizeToTray();

        std::thread([] {
            Sleep(3000);
            std::lock_guard<std::mutex> lock(mtx);
            vm.download_status[0] = '\0';
            }).detach();

    }
    else {
        MessageBoxA(NULL, "Failed to launch RobloxPlayerBeta.exe", "Launch Error", MB_OK | MB_ICONERROR);
    }
}

static bool SelectFolder(char* buffer, size_t size) {
    BROWSEINFO bi = { 0 };
    bi.lpszTitle = "Select Installation Folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
    if (pidl) {
        char path[MAX_PATH];
        if (SHGetPathFromIDList(pidl, path)) {
            strcpy_s(buffer, size, path);
            CoTaskMemFree(pidl);
            return true;
        }
        CoTaskMemFree(pidl);
    }
    return false;
}

static void SaveSettings() {
    try {
        json j;
        j["install_path"] = vm.install_path;
        j["download_latest"] = vm.download_latest.load();
        j["version_input"] = vm.version_input;
        j["selected_launch_version"] = vm.selected_launch_version;
        j["selected_version_index"] = vm.selected_version_index.load();
        j["latest_version"] = vm.latest_version;
        j["latest_version_display"] = vm.latest_version_display;
        j["last_exe_path"] = vm.last_exe_path;
        j["minimize_on_launch"] = vm.minimize_on_launch.load();
        j["minimize_on_startup"] = vm.minimize_on_startup.load();
        j["toggle_key_index"] = vm.toggle_key_index.load();
        j["first_run"] = vm.first_run.load();
        j["auto_startup"] = vm.auto_startup.load();

        std::string config_dir = "C:\\RustStrap";
        if (!fs::exists(config_dir)) fs::create_directories(config_dir);

        std::ofstream file(config_dir + "\\ruststrap_settings.json");
        if (file.is_open()) {
            file << j.dump(4);
            file.close();
        }
    }
    catch (...) {}
}

static void LoadSettings() {
    try {
        std::string config_path = "C:\\RustStrap\\ruststrap_settings.json";
        std::ifstream file(config_path);
        if (file.is_open()) {
            json j;
            file >> j;
            file.close();

            if (j.contains("install_path")) strcpy_s(vm.install_path, j["install_path"].get<std::string>().c_str());
            if (j.contains("download_latest")) vm.download_latest.store(j["download_latest"].get<bool>());
            if (j.contains("version_input")) strcpy_s(vm.version_input, j["version_input"].get<std::string>().c_str());
            if (j.contains("selected_launch_version")) strcpy_s(vm.selected_launch_version, j["selected_launch_version"].get<std::string>().c_str());
            if (j.contains("selected_version_index")) vm.selected_version_index.store(j["selected_version_index"].get<int>());
            if (j.contains("latest_version")) strcpy_s(vm.latest_version, j["latest_version"].get<std::string>().c_str());
            if (j.contains("latest_version_display")) strcpy_s(vm.latest_version_display, j["latest_version_display"].get<std::string>().c_str());
            if (j.contains("last_exe_path")) strcpy_s(vm.last_exe_path, j["last_exe_path"].get<std::string>().c_str());
            if (j.contains("minimize_on_launch")) vm.minimize_on_launch.store(j["minimize_on_launch"].get<bool>());
            if (j.contains("minimize_on_startup")) vm.minimize_on_startup.store(j["minimize_on_startup"].get<bool>());
            if (j.contains("toggle_key_index")) vm.toggle_key_index.store(j["toggle_key_index"].get<int>());
            if (j.contains("first_run")) vm.first_run.store(j["first_run"].get<bool>());
            if (j.contains("auto_startup")) vm.auto_startup.store(j["auto_startup"].get<bool>());
        }
    }
    catch (...) {}

    UpdateAutoStartup();
}

static void ResetSettings() {
    try {
        std::string config_path = "C:\\RustStrap\\ruststrap_settings.json";
        if (fs::exists(config_path)) fs::remove(config_path);

        {
            std::lock_guard<std::mutex> lock(mtx);
            new (&vm) VersionManager();
        }

        SaveSettings();

        {
            std::lock_guard<std::mutex> lock(mtx);
            strcpy_s(vm.download_status, "Configuration reset successfully!");
        }

        std::thread([] {
            Sleep(3000);
            std::lock_guard<std::mutex> lock(mtx);
            vm.download_status[0] = '\0';
            }).detach();
    }
    catch (...) {
        std::lock_guard<std::mutex> lock(mtx);
        strcpy_s(vm.download_status, "Failed to reset configuration!");
    }
}

void ui::init(LPDIRECT3DDEVICE9 device) {
    dev = device;
    ui::RunStyle();

    if (window_pos.x == 0) {
        RECT screen{};
        GetWindowRect(GetDesktopWindow(), &screen);
        screen_res = ImVec2(float(screen.right), float(screen.bottom));
        window_pos = (screen_res - ImVec2(660, 580)) * 0.5f;
    }

    CoInitialize(NULL);
}

void ui::RunStyle() {
    ImGuiStyle& style = ImGui::GetStyle();

    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.95f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.35f, 0.35f, 0.35f, 0.60f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.60f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.42f, 0.42f, 0.42f, 1.00f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.20f, 0.55f, 0.90f, 1.0f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.20f, 0.50f, 0.80f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.25f, 0.60f, 0.95f, 1.00f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.20f, 0.50f, 0.80f, 0.50f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.60f, 0.95f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.15f, 0.40f, 0.70f, 1.00f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.20f, 0.50f, 0.80f, 0.40f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.60f, 0.95f, 0.80f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.50f, 0.80f, 1.00f);
    style.Colors[ImGuiCol_Separator] = ImVec4(0.35f, 0.35f, 0.35f, 0.60f);
    style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.20f, 0.50f, 0.80f, 0.80f);
    style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.25f, 0.60f, 0.95f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.20f, 0.50f, 0.80f, 0.30f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.25f, 0.60f, 0.95f, 0.70f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.20f, 0.50f, 0.80f, 1.00f);
    style.Colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.30f, 0.50f, 0.90f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.25f, 0.60f, 0.95f, 0.80f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.18f, 0.35f, 0.60f, 1.00f);
    style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.06f, 0.08f, 0.12f, 0.95f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.12f, 0.20f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.20f, 0.50f, 0.80f, 0.40f);

    style.WindowRounding = 6.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 3.0f;
    style.TabRounding = 3.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.TabBorderSize = 0.0f;
    style.WindowPadding = ImVec2(10, 10);
    style.FramePadding = ImVec2(6, 3);
    style.ItemSpacing = ImVec2(6, 4);
    style.ItemInnerSpacing = ImVec2(4, 3);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 10.0f;
}