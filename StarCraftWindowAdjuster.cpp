// Windows Desktop Wizard
#define NOMINMAX
#include <windows.h>
#include <commctrl.h> // For Trackbar control (TRACKBAR_CLASS)
#include <string>
#include <vector>     // 虽然当前版本未使用，但保留以备将来扩展
#include <iomanip>    // For std::fixed, std::setprecision
#include <sstream>    // For std::wstringstream
#include <shlwapi.h>  // For PathAppend
#include <Shlobj.h>   // For SHGetFolderPath and CSIDL_* constants
#include <fstream>    // For ini file saving/loading

// --- Link with Comctl32.lib for Trackbar ---
#pragma comment(lib, "Comctl32.lib")
// --- Link with Shlwapi.lib for Path functions ---
#pragma comment(lib, "Shlwapi.lib")


// --- Global Variables for GUI ---
HINSTANCE hInst_g;                       // 当前实例句柄
HWND hTrackbarRatio_g;                   // Trackbar 句柄
HWND hStaticRatioValue_g;                // 显示当前比例值的标签句柄
HWND hButtonApply_g;                     // "应用" 按钮句柄
HWND hButtonDefault_g;                   // "默认" 按钮句柄
HWND hStaticStatus_g;                    // 状态标签
// HWND hStaticAuthor_g;                 // (Optional) Author label handle, not strictly needed if static

const wchar_t CLASS_NAME_G[] = L"StarCraftWindowAdjusterGUI_V2Fixed_GGrush"; // Added author to class name for uniqueness
const wchar_t WINDOW_TITLE_G[] = L"星际窗口调整工具";
const wchar_t AUTHOR_INFO_G[] = L"作者：GGrush"; // Author information

// --- Configuration ---
const wchar_t* DEFAULT_GAME_WINDOW_TITLE_G = L"Brood War";
const double DEFAULT_SIZE_RATIO_G = 0.8;
double currentSizeRatio_g = DEFAULT_SIZE_RATIO_G; // Will be loaded/saved

// 这些将根据屏幕尺寸和比例计算得出
int targetX_g = 0, targetY_g = 0, targetWidth_g = 0, targetHeight_g = 0;

// Trackbar range and calculation
const int TRACKBAR_MIN_VAL = 10; // Represents 0.10 ratio
const int TRACKBAR_MAX_VAL = 100; // Represents 1.00 ratio
const double RATIO_SCALE_FACTOR = 100.0;

// --- For saving settings (using a simple INI file in AppData) ---
const wchar_t* INI_FILE_NAME_G = L"SCWindowAdjusterSettings.ini";
const wchar_t* INI_SECTION_NAME_G = L"Settings";
const wchar_t* INI_KEY_RATIO_G = L"SizeRatio";


// --- Function Declarations ---
LRESULT CALLBACK WndProc_G(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void CalculateAndSetWindowDimensions_G();
void ApplyChanges_G(HWND hwndParent);
void UpdateRatioDisplay_G(double ratio);
void SaveSettings_G();
void LoadSettings_G();
std::wstring GetSettingsFilePath_G();


// --- Utility to convert double to wstring with precision ---
std::wstring DoubleToWString_G(double val, int precision = 2) {
    std::wstringstream ss;
    ss << std::fixed << std::setprecision(precision) << val;
    return ss.str();
}

// --- WinMain ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    hInst_g = hInstance;

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);

    LoadSettings_G();

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc_G;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME_G;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.style = CS_HREDRAW | CS_VREDRAW;

    if (!RegisterClass(&wc)) {
        MessageBox(NULL, L"窗口类注册失败!", L"错误", MB_ICONERROR | MB_OK);
        return 0;
    }

    // Increased window height slightly to accommodate the author label
    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME_G, WINDOW_TITLE_G,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 350, 250, // Adjusted height from 220 to 250
        NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) {
        MessageBox(NULL, L"窗口创建失败!", L"错误", MB_ICONERROR | MB_OK);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    SaveSettings_G();
    return (int)msg.wParam;
}

// --- WndProc_G ---
LRESULT CALLBACK WndProc_G(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
    {
        // Font for better readability (optional, but good for UI consistency)
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        if (!hFont) {
            hFont = (HFONT)GetStockObject(SYSTEM_FONT);
        }

        HWND hCtrl; // Temporary handle for setting font

        hCtrl = CreateWindowEx(0, L"STATIC", L"大小比例:",
            WS_CHILD | WS_VISIBLE,
            10, 20, 80, 20, hwnd, NULL, hInst_g, NULL);
        SendMessage(hCtrl, WM_SETFONT, (WPARAM)hFont, TRUE);


        hTrackbarRatio_g = CreateWindowEx(0, TRACKBAR_CLASS, NULL,
            WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ,
            90, 18, 150, 30,
            hwnd, (HMENU)201, hInst_g, NULL);
        // No need to set font for trackbar usually

        SendMessage(hTrackbarRatio_g, TBM_SETRANGE, (WPARAM)TRUE, (LPARAM)MAKELONG(TRACKBAR_MIN_VAL, TRACKBAR_MAX_VAL));
        SendMessage(hTrackbarRatio_g, TBM_SETPOS, (WPARAM)TRUE, (LPARAM)(currentSizeRatio_g * RATIO_SCALE_FACTOR));
        SendMessage(hTrackbarRatio_g, TBM_SETTICFREQ, 10, 0);

        hStaticRatioValue_g = CreateWindowEx(0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_RIGHT,
            250, 20, 60, 20, hwnd, NULL, hInst_g, NULL);
        SendMessage(hStaticRatioValue_g, WM_SETFONT, (WPARAM)hFont, TRUE);
        UpdateRatioDisplay_G(currentSizeRatio_g);

        hButtonApply_g = CreateWindowEx(0, L"BUTTON", L"应用配置并居中",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            10, 70, 150, 30, hwnd, (HMENU)102, hInst_g, NULL);
        SendMessage(hButtonApply_g, WM_SETFONT, (WPARAM)hFont, TRUE);

        hButtonDefault_g = CreateWindowEx(0, L"BUTTON", L"恢复默认 (0.8)",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            170, 70, 150, 30, hwnd, (HMENU)103, hInst_g, NULL);
        SendMessage(hButtonDefault_g, WM_SETFONT, (WPARAM)hFont, TRUE);

        hStaticStatus_g = CreateWindowEx(0, L"STATIC", L"就绪。请确保星际争霸已运行。",
            WS_CHILD | WS_VISIBLE,
            10, 120, 310, 40, hwnd, (HMENU)104, hInst_g, NULL);
        SendMessage(hStaticStatus_g, WM_SETFONT, (WPARAM)hFont, TRUE);

        // --- Author Label ---
        // Position it below the status label
        // Status label ends at y = 120 + 40 = 160. Give some padding.
        HWND hStaticAuthor = CreateWindowEx(0, L"STATIC", AUTHOR_INFO_G,
            WS_CHILD | WS_VISIBLE | SS_CENTER, // Centered text
            10, 170, 310, 20, // x, y, width, height
            hwnd, (HMENU)105, hInst_g, NULL); // Unique ID (optional, as not interacted with)
        SendMessage(hStaticAuthor, WM_SETFONT, (WPARAM)hFont, TRUE);
        // If you need to access hStaticAuthor later, make it a global variable hStaticAuthor_g.
        // For just displaying text, a local HWND is fine if font is set here.

        break;
    }

    case WM_HSCROLL:
    {
        if ((HWND)lParam == hTrackbarRatio_g) {
            int newPos = static_cast<int>(SendMessage(hTrackbarRatio_g, TBM_GETPOS, 0, 0));
            currentSizeRatio_g = (double)newPos / RATIO_SCALE_FACTOR;
            UpdateRatioDisplay_G(currentSizeRatio_g);
            SetWindowText(hStaticStatus_g, L"比例已更改。点击“应用”生效。");
        }
        break;
    }

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId) {
        case 102:
            ApplyChanges_G(hwnd);
            break;
        case 103:
        {
            currentSizeRatio_g = DEFAULT_SIZE_RATIO_G;
            SendMessage(hTrackbarRatio_g, TBM_SETPOS, (WPARAM)TRUE, (LPARAM)(currentSizeRatio_g * RATIO_SCALE_FACTOR));
            UpdateRatioDisplay_G(currentSizeRatio_g);
            SetWindowText(hStaticStatus_g, L"比例已恢复为默认值。点击“应用”生效。");
            break;
        }
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }
        break;
    }

    case WM_CLOSE:
        if (MessageBox(hwnd, L"确定要退出吗?", L"退出确认", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES) {
            DestroyWindow(hwnd);
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// --- UpdateRatioDisplay_G ---
void UpdateRatioDisplay_G(double ratio) {
    std::wstring displayText = DoubleToWString_G(ratio, 2);
    SetWindowText(hStaticRatioValue_g, displayText.c_str());
}

// --- CalculateAndSetWindowDimensions_G ---
void CalculateAndSetWindowDimensions_G() {
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    if (screenWidth == 0 || screenHeight == 0) {
        // Fallback if screen metrics are somehow unavailable
        targetWidth_g = 800; targetHeight_g = 600;
        targetX_g = 100; targetY_g = 100;
        return;
    }
    targetWidth_g = static_cast<int>(screenWidth * currentSizeRatio_g);
    targetHeight_g = static_cast<int>(screenHeight * currentSizeRatio_g);
    targetX_g = (screenWidth - targetWidth_g) / 2;
    targetY_g = (screenHeight - targetHeight_g) / 2;

    // Ensure window is not positioned off-screen (especially relevant for multi-monitor setups or small ratios)
    if (targetX_g < 0) targetX_g = 0;
    if (targetY_g < 0) targetY_g = 0;
}

// --- ApplyChanges_G ---
void ApplyChanges_G(HWND hwndParent) {
    CalculateAndSetWindowDimensions_G();

    HWND hStarCraftWnd = FindWindowW(NULL, DEFAULT_GAME_WINDOW_TITLE_G);
    if (hStarCraftWnd == NULL) {
        SetWindowText(hStaticStatus_g, L"错误: 未找到星际争霸窗口!");
        MessageBox(hwndParent, L"未找到星际争霸窗口!\n请确保游戏已运行且窗口标题为 \"Brood War\"。", L"错误", MB_ICONERROR | MB_OK);
        return;
    }

    SetWindowText(hStaticStatus_g, L"正在应用...");

    // If minimized, restore it first
    if (IsIconic(hStarCraftWnd)) {
        ShowWindow(hStarCraftWnd, SW_RESTORE);
        Sleep(500); // Give it a moment to restore
    }

    // Bring to foreground to ensure SetWindowPos works reliably, especially if SC is admin and this isn't
    SetForegroundWindow(hStarCraftWnd);
    Sleep(100); // Brief pause

    // Attempt to set window position and size
    if (SetWindowPos(hStarCraftWnd, NULL, targetX_g, targetY_g, targetWidth_g, targetHeight_g, SWP_NOZORDER | SWP_SHOWWINDOW)) {
        SetWindowText(hStaticStatus_g, L"配置已成功应用!");
    }
    else {
        DWORD errorCode = GetLastError();
        std::wstring errorMsg = L"设置窗口失败! 错误代码: " + std::to_wstring(errorCode);
        if (errorCode == 5) { // ERROR_ACCESS_DENIED
            errorMsg += L"\n\n提示: 这可能是权限问题。如果星际争霸以管理员身份运行，本程序也需要以管理员身份运行。";
        }
        SetWindowText(hStaticStatus_g, L"错误: 应用配置失败!");
        MessageBox(hwndParent, errorMsg.c_str(), L"应用失败", MB_ICONERROR | MB_OK);
    }
}

// --- Settings File Path ---
std::wstring GetSettingsFilePath_G() {
    wchar_t path[MAX_PATH];
    // Try to get AppData folder first
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        PathAppend(path, INI_FILE_NAME_G);
        return std::wstring(path);
    }
    // Fallback to current directory if AppData path fails (should be rare)
    GetCurrentDirectory(MAX_PATH, path);
    PathAppend(path, INI_FILE_NAME_G);
    return std::wstring(path);
}

// --- SaveSettings_G ---
void SaveSettings_G() {
    std::wstring filePath = GetSettingsFilePath_G();
    std::wstring ratioStr = DoubleToWString_G(currentSizeRatio_g, 4); // Save with more precision

    std::wofstream iniFile(filePath);
    if (iniFile.is_open()) {
        iniFile << L"[" << INI_SECTION_NAME_G << L"]" << std::endl;
        iniFile << INI_KEY_RATIO_G << L"=" << ratioStr << std::endl;
        iniFile.close();
    }
    // Optionally, add error handling if file cannot be opened/written
}

// --- LoadSettings_G ---
void LoadSettings_G() {
    std::wstring filePath = GetSettingsFilePath_G();
    currentSizeRatio_g = DEFAULT_SIZE_RATIO_G; // Start with default

    std::wifstream iniFile(filePath);
    if (iniFile.is_open()) {
        std::wstring line;
        std::wstring sectionTarget = L"[" + std::wstring(INI_SECTION_NAME_G) + L"]";
        bool inTargetSection = false;

        while (std::getline(iniFile, line)) {
            // Trim whitespace (simple trim for leading/trailing)
            size_t first = line.find_first_not_of(L" \t\n\r\f\v");
            if (std::wstring::npos == first) { // Line is all whitespace
                continue;
            }
            size_t last = line.find_last_not_of(L" \t\n\r\f\v");
            line = line.substr(first, (last - first + 1));

            if (line == sectionTarget) {
                inTargetSection = true;
                continue;
            }

            if (inTargetSection) {
                // Check if it's the start of another section
                if (line.length() > 2 && line[0] == L'[' && line.back() == L']') {
                    inTargetSection = false; // Exited our target section
                    break;
                }

                size_t eqPos = line.find(L'=');
                if (eqPos != std::wstring::npos) {
                    std::wstring key = line.substr(0, eqPos);
                    // Trim key
                    size_t key_last = key.find_last_not_of(L" \t");
                    if (key_last != std::wstring::npos) key = key.substr(0, key_last + 1);


                    if (key == INI_KEY_RATIO_G) {
                        std::wstring valueStr = line.substr(eqPos + 1);
                        // Trim valueStr
                        size_t val_first = valueStr.find_first_not_of(L" \t");
                        if (val_first != std::wstring::npos) {
                            valueStr = valueStr.substr(val_first);
                            size_t val_last = valueStr.find_last_not_of(L" \t");
                            if (val_last != std::wstring::npos) valueStr = valueStr.substr(0, val_last + 1);
                        }


                        try {
                            double loadedRatio = std::stod(valueStr);
                            // Validate loaded ratio against trackbar's effective range
                            if (loadedRatio >= ((double)TRACKBAR_MIN_VAL / RATIO_SCALE_FACTOR) &&
                                loadedRatio <= ((double)TRACKBAR_MAX_VAL / RATIO_SCALE_FACTOR)) {
                                currentSizeRatio_g = loadedRatio;
                            }
                        }
                        catch (const std::invalid_argument&) { /* Malformed, keep default */ }
                        catch (const std::out_of_range&) { /* Out of double range, keep default */ }
                        break; // Found our key, no need to parse further in this section
                    }
                }
            }
        }
        iniFile.close();
    }
}
