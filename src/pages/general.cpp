#include "Page.hpp"
#include "../MainFrame.hpp"
#include "../include/eula.hpp"
#include "../Manager.hpp"

class PageNotFound : public Page {
public:
    PageNotFound(MainFrame* frame) : Page(frame) {
        this->addText("404 Not Found");
    }
};
REGISTER_PAGE(NotFound);

/////////////////

class PageStart : public Page {
protected:
    void onSelect(wxCommandEvent& e) override {
        switch (e.GetId()) {
            case 0: {
                m_frame->selectPageStructure(InstallType::InstallOnGDPS);
            } break;

            case 1: {
                if (Manager::get()->needRequestAdminPriviledges()) {
                    wxMessageBox(
                        "You need to run the installer as "
                        "administrator in order to install "
                        "the developer tools.",
                        "Admin Priviledges Required",
                        wxICON_ERROR
                    );
                    m_canContinue = false;
                    m_frame->updateControls();
                    return;
                }
                m_frame->selectPageStructure(InstallType::InstallDevTools);
            } break;

            case 2: {
                m_frame->selectPageStructure(InstallType::Manage);
            } break;

            case 3: {
                m_frame->selectPageStructure(InstallType::Uninstall);
            } break;

            default: break;
        }
        m_canContinue = true;
        m_frame->updateControls();
    }

    void onViewInfo(wxCommandEvent&) {
        wxString info = "";

        #ifdef _WIN32
        // this looks neat on windows message boxes 
        // but not on mac
        info += "Installations\n\n";
        #endif

        info += "Data directory: " + Manager::get()->getDataDirectory().wstring() + "\n";
        info += "Bin directory: " + Manager::get()->getBinDirectory().wstring() + "\n";

        if (Manager::get()->isSuiteInstalled()) {
            info += "SDK directory: " + Manager::get()->getSuiteDirectory().wstring() + "\n\n";
            info += "SDK is installed, querying version\n\n";
        } else {
            info += "\n";
            info += "SDK has not been installed\n\n";
        }
        size_t ix = 0;
        for (auto& inst : Manager::get()->getInstallations()) {
            if (ix == Manager::get()->getDefaultInstallation()) {
                info += "(Default) ";
            }
            info += "Geode loader\n";
            info += inst.m_path.wstring() + "\n\n";
            ix++;
        }
        wxMessageBox(info, "Installations");
    }

public:
    PageStart(MainFrame* frame) : Page(frame) {
        this->addText("Welcome to the Geode installer!");
        if (Manager::get()->isSuiteInstalled()) {
            this->addSelectWithIDs({
                { "Install Geode on a GDPS (Private Server)", 0 },
                { "Manage Geode installations", 2 },
                { "Uninstall Geode", 3 },
            });
        } else {
            this->addSelect({
                "Install Geode on a GDPS (Private Server)",
                "Install the Geode developer tools",
                "Manage Geode installations",
                "Uninstall Geode",
            });
        }
        this->addButton("Installations", &PageStart::onViewInfo);
        frame->selectPageStructure(InstallType::InstallOnGDPS);
        m_canContinue = true;
    }
};
REGISTER_PAGE(Start);

/////////////////

class PageFirstStart : public Page {
public:
    PageFirstStart(MainFrame* frame) : Page(frame) {
        this->addText("Welcome to the Geode installer!");
        this->addText(
            "This installer will guide you through the setup process "
            "of getting the Geode mod loader for Geometry Dash."
        );
        m_canContinue = true;
    }
};
REGISTER_PAGE(FirstStart);

/////////////////

class PageEULA : public Page {
protected:
    wxCheckBox* m_agreeBox;

    void OnUpdate(wxCommandEvent&) {
        m_canContinue = m_agreeBox->IsChecked();
        m_frame->updateControls();
    }

public:
    PageEULA(MainFrame* frame) : Page(frame) {
        this->addText("Please agree to our finely crafted EULA :)");
        this->addLongText(g_eula);

        m_sizer->Add((m_agreeBox = new wxCheckBox(
            this, wxID_ANY, "I Agree to the Terms && Conditions listed above."
        )), 0, wxALL, 10);
        m_agreeBox->Bind(wxEVT_CHECKBOX, &PageEULA::OnUpdate, this);
    }
};
REGISTER_PAGE(EULA);

/////////////////


