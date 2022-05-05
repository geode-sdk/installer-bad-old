#include "MainFrame.hpp"

class GeodeInstallerApp : public wxApp {
public:
    virtual bool OnInit();
};

// #ifdef _DEBUG
// wxIMPLEMENT_APP_CONSOLE(GeodeInstallerApp);
// #else
wxIMPLEMENT_APP(GeodeInstallerApp);
// #endif

bool GeodeInstallerApp::OnInit() {
    auto frame = new MainFrame();
    frame->Show(true);
    return true;
}
