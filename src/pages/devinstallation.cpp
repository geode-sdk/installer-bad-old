#include "Page.hpp"
#include "../MainFrame.hpp"
#include "../Manager.hpp"

class PageDevInstallSelectSDK : public Page {
protected:
    wxStaticText* m_info;
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
        m_info->Show(!m_canContinue);
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

        m_info = this->addText(
            "Please select a path to a folder that doesn't "
            "exist yet or which is empty."
        );
        this->updateContinue();
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

class PageDevInstallAddToPath : public Page {
protected:
    wxCheckBox* m_box;
    bool m_addToPath = true;

    void leave() override {
        m_addToPath = m_box->IsChecked();
    }

public:
    PageDevInstallAddToPath(MainFrame* frame) : Page(frame) {
        this->addText(
            "Would you like to add the Geode CLI to your "
            "PATH environment variable? This will make "
            "Geode usable from the command line anywhere "
            "on your computer."
        );

        m_box = this->addToggle<PageDevInstallAddToPath>("Add CLI to Path", nullptr);
        m_box->SetValue(m_addToPath);

        m_canContinue = true;
    }

    bool shouldAddToPath() const {
        return m_addToPath;
    }
};
REGISTER_PAGE(DevInstallAddToPath);

/////////////////

class PageDevInstall : public Page {
protected:
    wxStaticText* m_status;
    wxGauge* m_gauge;

    void enter() override {
        Manager::get()->downloadCLI(
            [this](std::string const& str) -> void {
                wxMessageBox(
                    "Error downloading the Geode CLI: " + str + 
                    ". Try again, and if the problem persists, contact "
                    "the Geode Development team for more help.",
                    "Error Installing",
                    wxICON_ERROR
                );
                this->setText(m_status, "Error: " + str);
            },
            [this](std::string const& text, int prog) -> void {
                this->setText(m_status, "Downloading Geode CLI: " + text);
                m_gauge->SetValue(prog);
            },
            [this](wxWebResponse const& wres, auto) -> void {
                auto installRes = Manager::get()->installCLI(
                    wres.GetDataFile().ToStdWstring()
                );
                if (!installRes) {
                    wxMessageBox(
                        "Error installing Geode CLI: " + installRes.error() + ". Try "
                        "again, and if the problem persists, contact "
                        "the Geode Development team for more help.",
                        "Error Installing",
                        wxICON_ERROR
                    );
                } else {
                    auto res = Manager::get()->installSDK(
                        GET_EARLIER_PAGE(DevInstallBranch)->getBranch(),
                        [this](std::string const& err) -> void {
                            wxMessageBox(
                                "Error installing the Geode SDK: " + err + 
                                ". Try again, and if the problem persists, contact "
                                "the Geode Development team for more help.",
                                "Error Installing",
                                wxICON_ERROR
                            );
                            this->setText(m_status, "Error: " + err);
                        },
                        [this](std::string const& text, int prog) -> void {
                            m_gauge->SetValue(prog);
                            this->setText(m_status, "Cloning SDK: " + text);
                        },
                        [this]() -> void {
                            if (GET_EARLIER_PAGE(DevInstallAddToPath)->shouldAddToPath()) {
                                auto res = Manager::get()->addCLIToPath();
                                if (!res) {
                                    wxMessageBox(
                                        "Error adding Geode CLI to Path: " + res.error(),
                                        "Error Installing",
                                        wxICON_ERROR
                                    );
                                }
                            }
                            m_frame->nextPage();
                        }
                    );
                    if (!res) {
                        wxMessageBox("Error installing SDK: " + res.error());
                    }
                }
            }
        );
    }

public:
    PageDevInstall(MainFrame* frame) : Page(frame) {
        this->addText("Installing the Geode SDK...");

        m_status = this->addText("Connecting..."); 
        m_gauge = this->addProgressBar();
        this->addText(
            "Installing may take a while; please do not "
            "close the installer. The progress bar may "
            "also start from 0 again a few times while "
            "the installer is cloning submodules."
        ); 

        m_canContinue = false;
        m_canGoBack = false;
    }
};
REGISTER_PAGE(DevInstall);

/////////////////

class PageDevInstallFinished : public Page {
protected:
    void enter() override {
        auto res = Manager::get()->saveData();
        if (!res) {
            wxMessageBox(
                "Unable to save installer data: " + res.error() + " - "
                "the installer will be unable to uninstall Geode!",
                "Error Saving",
                wxICON_ERROR
            );
        }
    }

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
        this->addButton("Support Discord Server", &MainFrame::onDiscord, m_frame);
        m_canContinue = true;
        m_canGoBack = false;
    }
};
REGISTER_PAGE(DevInstallFinished);
