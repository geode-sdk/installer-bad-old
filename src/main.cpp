#include "MainFrame.hpp"
#include <wx/cmdline.h>
#include "Manager.hpp"

class GeodeInstallerApp : public wxApp {
public:
    virtual bool OnInit();

    void OnInitCmdLine(wxCmdLineParser& parser) override;
    bool OnCmdLineParsed(wxCmdLineParser& parser) override;
};

static const wxCmdLineEntryDesc g_cmdLineDesc [] = {
    { wxCMD_LINE_SWITCH, "h", "help", "Displays help on the command line parameters",
        wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
    { wxCMD_LINE_OPTION, "u", "update", "Update loader" },
    { wxCMD_LINE_NONE },
};

wxIMPLEMENT_APP(GeodeInstallerApp);

bool GeodeInstallerApp::OnInit() {
    if (!wxApp::OnInit()) return false;
    auto frame = new MainFrame();
    frame->Show(true);
    return true;
}

void GeodeInstallerApp::OnInitCmdLine(wxCmdLineParser& parser) {
    parser.SetDesc(g_cmdLineDesc);
    parser.SetSwitchChars("-");
}

bool GeodeInstallerApp::OnCmdLineParsed(wxCmdLineParser& parser) {
    wxString value;
    if (parser.Found("u", &value)) {
        Manager::get()->m_mode = InstallerMode::UpdateLoader;
        Manager::get()->m_loaderUpdatePath = value.ToStdWstring();
    }
    return true;
}
