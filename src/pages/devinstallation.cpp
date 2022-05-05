#include "Page.hpp"
#include "../MainFrame.hpp"
#include "../Manager.hpp"

class PageDevInstallSelectSDK : public Page {
protected:
    wxTextCtrl* m_pathInput;
    ghc::filesystem::path m_path;
    
    void enter() override {
        this->updateContinue();
    }
    
    void leave() override {
        auto path = ghc::filesystem::path(
            m_pathInput->GetValue().ToStdWstring()
        );
        m_path = path;
        Manager::get()->setSDKDirectory(path);
    }

    void onBrowse(wxCommandEvent&) {
        wxDirDialog dlg(
            this, "Select Directory for the Geode SDK", 
            "", wxDD_DEFAULT_STYLE | wxDD_SHOW_HIDDEN
        );
        if (dlg.ShowModal() == wxID_CANCEL) return;
        m_pathInput->SetValue(dlg.GetPath());
    }

    void onText(wxCommandEvent&) {
        this->updateContinue();
    }

    void updateContinue() {
        auto path = m_pathInput->GetValue().ToStdWstring();
        m_canContinue =
            !ghc::filesystem::exists(path) ||
            (ghc::filesystem::is_directory(path) && ghc::filesystem::is_empty(path));
        m_frame->updateControls();
    }

public:
    PageDevInstallSelectSDK(MainFrame* parent) : Page(parent) {
        this->addText("Please select where to install the Geode SDK.");

        m_pathInput = this->addInput(
            Manager::get()->getSDKDirectory().wstring(),
            &PageDevInstallSelectSDK::onText
        );

        this->addButton("Browse", &PageDevInstallSelectSDK::onBrowse);
    }

    ghc::filesystem::path getPath() const { return m_path; }
};
REGISTER_PAGE(DevInstallSelectSDK);

/////////////////
