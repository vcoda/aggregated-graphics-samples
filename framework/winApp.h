#pragma once
#include "application.h"
#include "debugOutputStream.h"

class Win32App : public BaseApp
{
public:
    Win32App(const AppEntry& entry, const core::tstring& caption, uint32_t width, uint32_t height);
    virtual ~Win32App();
    virtual void setWindowCaption(const core::tstring& caption) override;
    virtual void show() const override;
    virtual void run() override;
    virtual void onKeyUp(char, int, uint32_t) override {}
    virtual void onMouseMove(int x, int y) override;
    virtual void onMouseLButton(bool down, int x, int y) override;
    virtual void onMouseRButton(bool, int, int)  override {}
    virtual void onMouseMButton(bool, int, int)  override {}
    virtual void onMouseWheel(float)  override {}

protected:
    HINSTANCE hInstance;
    HWND hWnd;

private:
    virtual char translateKey(int code) const override;
    static LRESULT WINAPI wndProc(HWND, UINT, WPARAM, LPARAM);

    static Win32App *self;
    static DebugOutputStream dostream;
    POINT lastPos = {0L, 0L};
    POINT currPos = {0L, 0L};
};
