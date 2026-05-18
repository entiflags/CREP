// Entry point

#include "crep.h"
#include "main_window.h"

int APIENTRY wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    MainWindow mainWnd;
    if (!mainWnd.create(hInstance, nCmdShow))
        return 1;

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (mainWnd.getAccel() && TranslateAccelerator(mainWnd.getHwnd(), mainWnd.getAccel(), &msg))
            continue;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
