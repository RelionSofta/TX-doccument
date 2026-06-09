#include "Application.h"

// WIN32 subsystem use karne pe WinMain entry point chahiye
// Isse CMD window bilkul nahi dikhega background mein bhi
#ifdef _WIN32
#include <windows.h>

int WINAPI WinMain(
    HINSTANCE /*hInstance*/,
    HINSTANCE /*hPrevInstance*/,
    LPSTR     /*lpCmdLine*/,
    int       /*nCmdShow*/)
{
    Application app;
    return app.run();
}

#else

int main(int /*argc*/, char* /*argv*/[])
{
    Application app;
    return app.run();
}

#endif