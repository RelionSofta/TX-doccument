#include "Application.h"

// ============================================================
//  main.cpp
//
//  Single responsibility: create Application and hand control
//  to it.  All subsystem ownership lives in Application.
// ============================================================

int main(int /*argc*/, char* /*argv*/[])
{
    Application app;
    return app.run();
}
