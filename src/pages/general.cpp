#include "Page.hpp"
#include "../MainFrame.hpp"
#include "../include/eula.hpp"

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
            case 0: m_frame->selectPageStructure(InstallType::InstallOnGDPS); break;
            case 1: m_frame->selectPageStructure(InstallType::InstallDevTools); break;
            case 2: m_frame->selectPageStructure(InstallType::Uninstall); break;
            default: break;
        }
    }

public:
    PageStart(MainFrame* frame) : Page(frame) {
        this->addText("Welcome to the Geode installer!");
        this->addSelect({
            "Install Geode on a GDPS (Private Server)",
            "Install the Geode developer tools",
            "Uninstall Geode"
        });
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


