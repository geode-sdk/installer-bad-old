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
        this->setText(m_info, "");
        auto path = ghc::filesystem::path(m_pathInput->GetValue().ToStdWstring());
        #ifdef _WIN32
        m_canContinue = 
            ghc::filesystem::exists(path) &&
            ghc::filesystem::is_regular_file(path);
        #else
        m_canContinue = 
            ghc::filesystem::exists(path) &&
            ghc::filesystem::is_directory(path);
        #endif
        if (path.string().size()) {
            if (!m_canContinue) {
                this->setText(
                    m_info,
                    "Please enter a path to a valid installation "
                    "of GD 2.113."
                );
            }
            if (!Manager::isValidGD(path)) {
                this->setText(
                    m_info,
                    "This does not seem like a valid installation "
                    "of GD 2.113. Please note that Geode currently "
                    "only supports version 2.113; installing may "
                    "not work, or cause the game to become "
                    "unplayable."
                );
            }
            auto perms = ghc::filesystem::status(path).permissions();
            if (
                (perms & ghc::filesystem::perms::owner_all) == ghc::filesystem::perms::none ||
                (perms & ghc::filesystem::perms::group_all) == ghc::filesystem::perms::none
            ) {
                this->setText(
                    m_info,
                    "It seems like the installer lacks sufficient "
                    "permissions to write files to the provided "
                    "location; please install GD on another path. "
                    #ifdef _WIN32
                    "(Instructions for Steam: "
                    "https://geode-sdk.github.io/docs/movegd.html)"
                    #endif
                );
                m_canContinue = false;
            }
        }
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

        m_info = this->addText("");
        m_info->SetForegroundColour(wxTheColourDatabase->Find("RED"));
    }

    ghc::filesystem::path getPath() const { return m_path; }
};
REGISTER_PAGE(InstallSelectGD);

/////////////////

class PageInstallOptBeta : public Page {
protected:
    wxCheckBox* m_check;

public:
    PageInstallOptBeta(MainFrame* parent) : Page(parent) {
        this->addText(
            "Would you like to use the beta version "
            "(nightly channel) of the Geode loader? "
            "Beta versions may be less stable and have "
            "more bugs, but using beta versions lets "
            "you try out new features ahead of time."
        );
        m_check = this->addToggle("Use beta version for Loader");
        m_canContinue = true;
    }
    
    DevBranch getBranch() const {
        return m_check->IsChecked() ? DevBranch::Nightly : DevBranch::Stable;
    }
};
REGISTER_PAGE(InstallOptBeta);

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
        Manager::get()->installGeodeUtilsLib(
            false,
            GET_EARLIER_PAGE(InstallOptBeta)->getBranch(),
            [this](std::string const& str) -> void {
                wxMessageBox(
                    "Error downloading the Geode Utility Library: " + str + 
                    ". Try again, and if the problem persists, contact "
                    "the Geode Development team for more help.",
                    "Error Installing",
                    wxICON_ERROR
                );
                this->setText(m_status, "Error: " + str);
            },
            [this](std::string const& text, int prog) -> void {
                this->setText(m_status, "Downloading Geode Utility Library: " + text);
                m_gauge->SetValue(prog / 2);
            },
            [this]() -> void {
                this->setText(m_status, "Waiting to download Geode...");
                m_gauge->SetValue(50);
                auto res = Manager::get()->installGeodeFor(
                    GET_EARLIER_PAGE(InstallSelectGD)->getPath(),
                    GET_EARLIER_PAGE(InstallOptBeta)->getBranch(),
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
                        m_gauge->SetValue(prog / 2 + 50);
                    },
                    [this]() -> void {
                        m_frame->nextPage();
                    }
                );
                if (!res) {
                    wxMessageBox(
                        "Error downloading the Geode loader: " + res.error() + 
                        ". Try again, and if the problem persists, contact "
                        "the Geode Development team for more help.",
                        "Error Installing",
                        wxICON_ERROR
                    );
                    this->setText(m_status, "Error: " + res.error());
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

