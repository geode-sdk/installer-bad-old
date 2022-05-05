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

class PageDevInstallBranch : public Page {
protected:
    DevBranch m_branch = DevBranch::Nightly;

    void onSelect(wxCommandEvent& e) override {
        switch (e.GetId()) {
            case 0: m_branch = DevBranch::Nightly; break;
            case 1: m_branch = DevBranch::Stable; break;
            default: break;
        }
    }

public:
    PageDevInstallBranch(MainFrame* frame) : Page(frame) {
        this->addText(
            "Which branch would you like to use "
            "for the Geode SDK?"
        );
        this->addText(
            "Nightly contains all of the latest && "
            "greatest features of Geode, whereas "
            "Stable is a little behind but contains "
            "only fully tested features."
        );
        this->addSelect({ "Nightly", "Stable" });
        m_canContinue = true;
    }

    DevBranch getBranch() const {
        return m_branch;
    }
};
REGISTER_PAGE(DevInstallBranch);

/////////////////

class PageDevInstall : public Page {
protected:
    wxStaticText* m_status;
    wxStaticText* m_gitStatus;
    wxGauge* m_gauge;

    void enter() override {
        Manager::get()->installSDK(
            GET_EARLIER_PAGE(DevInstallBranch)->getBranch(),
            [this](std::string const& str) -> void {
                wxMessageBox(
                    "Error downloading the Geode SDK: " + str + 
                    ". Try again, and if the problem persists, contact "
                    "the Geode Development team for more help.",
                    "Error Installing",
                    wxICON_ERROR
                );
                this->setText(m_status, "Error: " + str);
            },
            [this](std::string const& text, int prog) -> void {
                m_gauge->SetValue(prog / 2);
                this->setText(m_status, "Downloading SDK: " + text);
            },
            [this]() -> void {
                m_frame->nextPage();
            }
        );
    }

public:
    PageDevInstall(MainFrame* frame) : Page(frame) {
        this->addText("Installing the Geode SDK...");

        m_status = this->addText("Connecting..."); 
        m_gauge = this->addProgressBar();
        m_gitStatus = this->addText(""); 

        m_canContinue = false;
        m_canGoBack = false;
    }
};
REGISTER_PAGE(DevInstall);

/////////////////

class PageDevInstallFinished : public Page {
protected:
    void onDocs(wxCommandEvent&) {
        wxLaunchDefaultBrowser("https://geode-sdk.github.io/docs");
    }

public:
    PageDevInstallFinished(MainFrame* frame) : Page(frame) {
        this->addText(
            "The Geode SDK has been installed! "
            "Happy modding :)"
        );
        this->addText("Check Documentation for help using the SDK.");
        this->addButton("Docs", &PageDevInstallFinished::onDocs);
        m_canContinue = true;
        m_canGoBack = false;
    }
};
REGISTER_PAGE(DevInstallFinished);
