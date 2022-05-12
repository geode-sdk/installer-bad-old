#include "Page.hpp"
#include "../MainFrame.hpp"
#include "../Manager.hpp"

class PageInstallGDPSInfo : public Page {
public:
    PageInstallGDPSInfo(MainFrame* parent) : Page(parent) {
        this->addText(
            "Please note that Geode currently only supports "
            "Geometry Dash version 2.113. While support for "
            "other versions is planned in the future, for now "
            "it is advised not to try to install Geode on an "
            "older version of GD as this may break the game."
        );
        m_canContinue = true;
    }
};
REGISTER_PAGE(InstallGDPSInfo);

/////////////////

class PageInstallSelectGD : public Page {
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
    }

    void onBrowse(wxCommandEvent&) {
        #if _WIN32
        wxFileDialog ofd(
            this, "Select Geometry Dash", "", "",
            "Executable files (*.exe)|*.exe",
            wxFD_OPEN | wxFD_FILE_MUST_EXIST 
        );
        #else
        wxFileDialog ofd(
            this, "Select Geometry Dash", "", "",
            "Applications (*.app)|*.app",
            wxFD_OPEN | wxFD_FILE_MUST_EXIST 
        );
        #endif
        if (ofd.ShowModal() == wxID_CANCEL) return;
        m_pathInput->SetValue(ofd.GetPath());
    }

    void onText(wxCommandEvent&) {
        this->updateContinue();
    }

    void updateContinue() {
        auto path = m_pathInput->GetValue().ToStdWstring();
        #if _WIN32
        m_canContinue =
            ghc::filesystem::exists(path) &&
            ghc::filesystem::is_regular_file(path);
        #else
        m_canContinue =
            ghc::filesystem::exists(path) &&
            ghc::filesystem::is_directory(path);
        #endif
        m_frame->updateControls();
    }

public:
    PageInstallSelectGD(MainFrame* parent) : Page(parent) {
        ghc::filesystem::path path;

        if (Manager::get()->isFirstTime()) {
            auto gdPath = Manager::get()->findDefaultGDPath();
            if (gdPath.has_value()) {
                this->addText(
                    "Automatically detected Geometry Dash path! "
                    "Please verify that the path below is correct, and "
                    "select a different one if it is not."
                );
                path = gdPath.value();
            } else {
                this->addText(
                    "Unable to automatically detect Geometry Dash path. "
                    "Please enter the path below:"
                );
            }
        } else {
            this->addText(
                "Please enter the path to Geometry Dash. "
                "For the vanilla game, this should be the file "
                "called \"GeometryDash.exe\", however if you're "
                "installing on a GDPS, the name may differ."
            );
        }

        m_pathInput = this->addInput(path.wstring(), &PageInstallSelectGD::onText);

        this->addButton("Browse", &PageInstallSelectGD::onBrowse);
    }

    ghc::filesystem::path getPath() const { return m_path; }
};
REGISTER_PAGE(InstallSelectGD);

/////////////////

class PageInstallCheckMods : public Page {
public:
    PageInstallCheckMods(MainFrame* parent) : Page(parent) {
        auto path = GET_EARLIER_PAGE(InstallSelectGD)->getPath();
        auto others = Manager::get()->doesDirectoryContainOtherMods(path.parent_path());
        if (others) {
            if ((others & OMF_MHv6) || (others & OMF_MHv7)) {
                this->addText(
                    "Looks like you already have MegaHack " +
                    std::string((others & OMF_MHv6) ? "v7" : "v6") + 
                    " installed! This installer will uninstall it, "
                    "however you can get it back as a Geode mod "
                    "through INSERT METHOD FOR GETTING MEGAHACK FOR "
                    "GEODE"
                );
            } else if (others & OMF_GDHM) {
                this->addText(
                    "Looks like you already have GD HackerMode "
                    "installed! Geode is not compatible with other "
                    "external mods or mod loaders, so it will be "
                    "uninstalled. Unfortunately, GDHM has no direct "
                    "Geode-equivalent, but you can browse the Geode "
                    "marketplace in-game to find alternatives."
                );
            } else if (others & OMF_Some) {
                this->addText(
                    "Looks like you already have some mods "
                    "installed! Geode is not compatible with "
                    "other external mods or mod loaders, so "
                    "you will have to uninstall any other "
                    "mods / mod loaders you have. Check out "
                    "the Geode marketplace in-game to find "
                    "ports && alternatives for your current "
                    "mods :)"
                );
            } else {
                this->addText("You should not see this teehehee");
            }
        } else {
            this->addText(
                "You are ready to install Geode! Please "
                "note that Geode is not compatible with "
                "other external mods or mod loaders, so "
                "make sure to uninstall any mods you may "
                "already have installed. Check out "
                "the Geode marketplace in-game to find "
                "mods available for Geode :)"
            );
        }
        this->addText("Press \"Next\" to begin installing Geode.");
        m_canContinue = true;
    }
};
REGISTER_PAGE(InstallCheckMods);

/////////////////

class PageInstall : public Page {
protected:
    wxStaticText* m_status;
    wxGauge* m_gauge;

    void enter() override {
        Manager::get()->downloadLoader(
            [this](std::string const& str) -> void {
                wxMessageBox(
                    "Error downloading the Geode loader: " + str + 
                    ". Try again, and if the problem persists, contact "
                    "the Geode Development team for more help.",
                    "Error Installing",
                    wxICON_ERROR
                );
                this->setText(m_status, "Error: " + str);
            },
            [this](std::string const& text, int prog) -> void {
                this->setText(m_status, "Downloading Geode: " + text);
                m_gauge->SetValue(prog / 2);
            },
            [this](wxWebResponse const& res) -> void {
                auto installRes = Manager::get()->installLoaderFor(
                    GET_EARLIER_PAGE(InstallSelectGD)->getPath(),
                    res.GetDataFile().ToStdWstring()
                );
                if (!installRes) {
                    wxMessageBox(
                        "Error installing Geode: " + installRes.error() + ". Try "
                        "again, and if the problem persists, contact "
                        "the Geode Development team for more help.",
                        "Error Installing",
                        wxICON_ERROR
                    );
                } else {
                    auto installation = installRes.value();
                    Manager::get()->downloadAPI(
                        [this](std::string const& str) -> void {
                            wxMessageBox(
                                "Error downloading the Geode API: " + str + 
                                ". Try again, and if the problem persists, contact "
                                "the Geode Development team for more help.",
                                "Error Installing",
                                wxICON_ERROR
                            );
                            this->setText(m_status, "Error: " + str);
                        },
                        [this](std::string const& text, int prog) -> void {
                            this->setText(m_status, "Downloading API: " + text);
                            m_gauge->SetValue(prog / 2 + 50);
                        },
                        [this, installation](wxWebResponse const& res) -> void {
                            auto apiInstallRes = Manager::get()->installAPIFor(
                                installation,
                                res.GetDataFile().ToStdWstring(),
                                res.GetSuggestedFileName()
                            );
                            if (!apiInstallRes) {
                                wxMessageBox(
                                    "Error installing Geode API: " + apiInstallRes.error() + ". Try "
                                    "again, and if the problem persists, contact "
                                    "the Geode Development team for more help.",
                                    "Error Installing",
                                    wxICON_ERROR
                                );
                            } else {
                                m_frame->nextPage();
                            }
                        }
                    );
                }
            }
        );
    }

public:
    PageInstall(MainFrame* frame) : Page(frame) {
        this->addText("Installing Geode...");

        m_status = this->addText("Connecting..."); 
        m_gauge = this->addProgressBar();

        m_canContinue = false;
        m_canGoBack = false;
    }
};
REGISTER_PAGE(Install);

/////////////////

class PageInstallFinished : public Page {
protected:
    wxCheckBox* m_box;

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

    void leave() override {
        if (m_box->IsChecked()) {
            Manager::get()->launch(GET_EARLIER_PAGE(InstallSelectGD)->getPath());
        }
    }

public:
    PageInstallFinished(MainFrame* frame) : Page(frame) {
        this->addText(
            "Installing finished! "
            "You can now close this installer && start up Geometry Dash :)"
        );
        m_box = this->addToggle<PageInstallFinished>("Launch Geometry Dash", nullptr);
        m_box->SetValue(true);
        this->addButton("Support Discord Server", &MainFrame::onDiscord, m_frame);

        m_canContinue = true;
        m_canGoBack = false;
    }
};
REGISTER_PAGE(InstallFinished);

