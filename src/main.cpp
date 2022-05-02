#include "MainFrame.hpp"

class GeodeInstallerApp : public wxApp {
public:
    virtual bool OnInit();
};

wxIMPLEMENT_APP(GeodeInstallerApp);

bool GeodeInstallerApp::OnInit() {
    auto frame = new MainFrame();
    frame->Show(true);
    return true;
}
