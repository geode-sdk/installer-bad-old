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
        if (Manager::get()->isSDKInstalled()) {
            switch (e.GetId()) {
                case 0: m_frame->selectPageStructure(InstallType::InstallOnGDPS); break;
                case 1: m_frame->selectPageStructure(InstallType::Uninstall); break;
                default: break;
            }
        } else {
            switch (e.GetId()) {
                case 0: m_frame->selectPageStructure(InstallType::InstallOnGDPS); break;
                case 1: m_frame->selectPageStructure(InstallType::InstallDevTools); break;
                case 2: m_frame->selectPageStructure(InstallType::Uninstall); break;
                default: break;
            }
        }
    }

    void onViewInfo(wxCommandEvent&) {
        wxString info = "Installations\n\n";
        if (Manager::get()->isSDKInstalled()) {
            info += "SDK is installed, querying version\n\n";
        } else {
            info += "SDK has not been installed\n\n";
        }
        size_t ix = 0;
        for (auto& inst : Manager::get()->getInstallations()) {
            if (ix == Manager::get()->getDefaultInstallation()) {
                info += "(Default) ";
            }
            info += "Geode loader " + inst.m_version + "\n";
            info += inst.m_path.wstring() + "\n\n";
            ix++;
        }
        wxMessageBox(info, "Installations");
    }

public:
    PageStart(MainFrame* frame) : Page(frame) {
        this->addText("Welcome to the Geode installer!");
        if (Manager::get()->isSDKInstalled()) {
            this->addSelect({
                "Install Geode on a GDPS (Private Server)",
                "Uninstall Geode"
            });
        } else {
            this->addSelect({
                "Install Geode on a GDPS (Private Server)",
                "Install the Geode developer tools",
                "Uninstall Geode"
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


