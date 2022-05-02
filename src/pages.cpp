#include "MainFrame.hpp"
#include "eula.hpp"
#include <wx/progdlg.h>
#include <wx/webrequest.h>
#include <wx/file.h>
#include <wx/dataview.h>
#include "json.hpp"
#include <fstream>

#ifdef _WIN32
#define PLATFORM_ASSET_IDENTIFIER "win"
#define PLATFORM_NAME "Windows"
#else
#error "Define PLATFORM_ASSET_IDENTIFIER & PLATFORM_NAME"
#endif

void MainFrame::SetupPages() {
    this->PickPage(m_firstTime ? ID_Install : ID_GDPS);

    auto page404 = new Page(this);

    page404->m_sizer->Add(new wxStaticText(page404, wxID_ANY,
        "404 Not Found"
    ), 0, wxALL, 10);

    m_pages.insert({ PageID::NotFound, page404 });

    //////////////////

    if (m_firstTime) {
        auto pageFirstStart = new Page(this);

        pageFirstStart->m_sizer->Add(new wxStaticText(pageFirstStart, wxID_ANY,
            "Welcome to the Geode installer!"
        ), 0, wxALL, 10);

        pageFirstStart->m_sizer->Add(new wxStaticText(pageFirstStart, wxID_ANY,
            "This installer will guide you through the setup process "
            "of getting the Geode mod loader for Geometry Dash."
        ), 1, wxALL, 10);

        pageFirstStart->m_canContinue = true;

        m_pages.insert({ PageID::Start, pageFirstStart });
    } else {
        class PageStart : public Page {
        protected:
            void OnSelect(wxCommandEvent& event) {
                m_frame->PickPage(event.GetId());
            }

        public:
            PageStart(MainFrame* parent) : Page(parent) {
                m_sizer->Add(new wxStaticText(this, wxID_ANY,
                    "Welcome to the Geode installer!"
                ), 0, wxALL, 10);

                m_sizer->Add(new wxStaticText(this, wxID_ANY,
                    "Please select what you want to do: "
                ), 0, wxALL, 10);

                auto gdps = new wxRadioButton(this, ID_GDPS,
                    "Install Geode on a GDPS (Private Server)",
                    wxDefaultPosition, wxDefaultSize, wxRB_GROUP
                );
                gdps->Bind(wxEVT_RADIOBUTTON, &PageStart::OnSelect, this);
                m_sizer->Add(gdps, 0, wxALL, 10);
                
                // auto dev = new wxRadioButton(this, ID_Dev,
                //     "Install the Geode developer tools"
                // );
                // dev->Bind(wxEVT_RADIOBUTTON, &PageStart::OnSelect, this);
                // m_sizer->Add(dev, 0, wxALL, 10);
                
                auto uninstall = new wxRadioButton(this, ID_Uninstall,
                    "Uninstall Geode"
                );
                uninstall->Bind(wxEVT_RADIOBUTTON, &PageStart::OnSelect, this);
                m_sizer->Add(uninstall, 0, wxALL, 10);

                auto contact = new wxButton(this, wxID_ANY, "Support Discord Server");
                contact->Bind(wxEVT_BUTTON, &MainFrame::OnDiscord, m_frame);
                m_sizer->Add(contact, 0, wxALL, 10);

                m_canContinue = true;
            }
        };  
        m_pages.insert({ PageID::Start, new PageStart(this) });
    }

    //////////////////

    class PageGDPSInfo : public Page {
    public:
        PageGDPSInfo(MainFrame* parent) : Page(parent) {
            m_sizer->Add(new wxStaticText(this, wxID_ANY,
                "Please note that Geode currently only supports "
                "Geometry Dash version 2.113. While support for "
                "other versions is planned in the future, for now "
                "it is advised not to try to install Geode on an "
                "older version of GD as this may break the game."
            ), 1, wxALL, 10);

            m_canContinue = true;
        }
    };
    m_pageGens.insert({
        PageID::InstallGDPSInfo,
        [](auto f) -> Page* { return new PageGDPSInfo(f); }
    });

    //////////////////

    class PageEULA : public Page {
    protected:
        wxCheckBox* m_agreeBox;

        void OnUpdate(wxCommandEvent&) {
            m_canContinue = m_agreeBox->IsChecked();
            m_frame->UpdateCanContinue();
        }

    public:
        PageEULA(MainFrame* parent) : Page(parent) {
            m_sizer->Add(new wxStaticText(this, wxID_ANY,
                "Please agree to our finely crafted EULA :)"
            ), 0, wxALL, 10);

            m_sizer->Add(new wxTextCtrl(
                this, wxID_ANY, g_eula,
                wxDefaultPosition, { 1000, 1000 },
                wxTE_MULTILINE | wxTE_READONLY
            ), 1, wxALL, 10);

            m_sizer->Add((m_agreeBox = new wxCheckBox(
                this, wxID_ANY, "I Agree to the Terms && Conditions listed above."
            )), 0, wxALL, 10);
            m_agreeBox->Bind(wxEVT_CHECKBOX, &PageEULA::OnUpdate, this);
        }
    };
    m_pageGens.insert({
        PageID::EULA,
        [](auto f) -> Page* { return new PageEULA(f); }
    });

    //////////////////

    class PageSelectGD : public Page {
    protected:
        wxTextCtrl* m_pathInput;
        
        void Enter() override {
            this->UpdateContinue();
        }
        
        void Leave() override {
            auto path = std::filesystem::path(
                m_pathInput->GetValue().ToStdWstring()
            );
            if (path.has_parent_path()) {
                m_frame->m_targetExeName = path.filename().string();
                m_frame->m_installingTo = path.parent_path();
            }
        }

        void OnBrowse(wxCommandEvent&) {
            wxFileDialog ofd(
                this, "Select Geometry Dash", "", "",
                "Executable files (*.exe)|*.exe",
                wxFD_OPEN | wxFD_FILE_MUST_EXIST 
            );
            if (ofd.ShowModal() == wxID_CANCEL) return;
            m_pathInput->SetValue(ofd.GetPath());
        }

        void OnText(wxCommandEvent&) {
            this->UpdateContinue();
        }

        void UpdateContinue() {
            auto path = m_pathInput->GetValue().ToStdWstring();
            m_canContinue =
                std::filesystem::exists(path) &&
                std::filesystem::is_regular_file(path);
            m_frame->UpdateCanContinue();
        }

    public:
        PageSelectGD(MainFrame* parent) : Page(parent) {
            auto path = m_frame->m_firstTime ? FigureOutGDPath() : "";

            if (m_frame->m_firstTime) {
                if (path.string().size()) {
                    m_sizer->Add(new wxStaticText(this, wxID_ANY,
                        "Automatically detected Geometry Dash path! "
                        "Please verify that the path below is correct, and "
                        "select a different one if it is not."
                    ), 1, wxALL, 10);
                } else {
                    m_sizer->Add(new wxStaticText(this, wxID_ANY,
                        "Unable to automatically detect Geometry Dash path. "
                        "Please enter the path below:"
                    ), 1, wxALL, 10);
                }
            } else {
                m_sizer->Add(new wxStaticText(this, wxID_ANY,
                    "Please enter the path to Geometry Dash. "
                    "For the vanilla game, this should be the file "
                    "called \"GeometryDash.exe\", however if you're "
                    "installing on a GDPS, the name may differ."
                ), 1, wxALL, 10);
            }

            m_sizer->Add(
                (m_pathInput = new wxTextCtrl(this, wxID_ANY, path.string())
            ), 0, wxALL | wxEXPAND, 10);
            m_pathInput->Bind(wxEVT_TEXT, &PageSelectGD::OnText, this);

            auto browse = new wxButton(this, wxID_ANY, "Browse");
            browse->Bind(wxEVT_BUTTON, &PageSelectGD::OnBrowse, this);
            m_sizer->Add(browse, 0, wxALL, 10);
        }
    };
    m_pageGens.insert({
        PageID::InstallSelectGD,
        [](auto f) -> Page* { return new PageSelectGD(f); }
    });

    //////////////////

    class PageCheckModLoaders : public Page {
    protected:
        wxTextCtrl* m_pathInput;
        
    public:
        PageCheckModLoaders(MainFrame* parent) : Page(parent) {
            auto others = parent->DetectOtherModLoaders();

            if (others.None()) {
                m_sizer->Add(new wxStaticText(this, wxID_ANY,
                    "Press \"Next\" to install Geode. "
                    "Please note that you can not have any "
                    "other mod loaders alongside Geode."
                ), 1, wxALL, 10);
            } else {
                std::string info = "";
                if (others.m_hasMH) {
                    info +=
                        "It seems like you have MegaHack v" +
                        std::to_string(others.m_hasMH) + " installed. "
                        "This installer will uninstall MegaHack, but "
                        "you can install it back as a Geode mod using "
                        "<INSERT MEGAHACK REINSTALL METHOD HERE> :)\n\n";
                }
                if (others.m_hasGDHM) {
                    info +=
                        "It seems like you have GD HackerMode "
                        "installed. This installer will uninstall it, "
                        "as you can not run any other modloaders or "
                        "standalone mods alongside Geode. Browse the "
                        "Geode Marketplace to find alternatives for GDHM :)\n\n";
                }
                if (others.m_hasGeode) {
                    info +=
                        "It seems like you already have Geode installed. "
                        "This installer will reinstall it from scratch.\n\n";
                }
                if (others.m_hasMoreUnknown) {
                    info +=
                        "It seems like you already have some mods installed. "
                        "Please note that you can "
                        "not run any other modloaders or standalone mods "
                        "alongsidse Geode. Browse the Geode Marketplace "
                        "in-game to find ports / alternatives for the mods "
                        "you had previously :)\n\n";
                }
                if (others.m_others.size()) {
                    std::string list = "";
                    for (auto& o : others.m_others) {
                        list += o + ", ";
                    }
                    list.pop_back();
                    list.pop_back();
                    info +=
                        "It seems like you have the following modloaders "
                        "installed: " + list + ". Please note that you can "
                        "not run any other modloaders or standalone mods "
                        "alongsidse Geode. Browse the Geode Marketplace "
                        "in-game to find ports / alternatives for the mods "
                        "you had previously :)\n\n";
                }

                m_sizer->Add(new wxStaticText(this, wxID_ANY,
                    info + "Press \"Next\" to install Geode. "
                ), 1, wxALL, 10);
            }
       
            m_canContinue = true;
        }
    };
    m_pageGens.insert({
        PageID::InstallCheckForModLoaders,
        [](auto f) -> Page* { return new PageCheckModLoaders(f); }
    });

    //////////////////

    class PageInstalling : public Page {
    protected:
        wxStaticText* m_conProgress;
        wxGauge* m_connecting;
        wxStaticText* m_downProgress;
        wxGauge* m_downloading;
        wxStaticText* m_instProgress;
        wxGauge* m_installing;
        bool m_begun = false;

        void Enter() override {
            if (m_begun) return;
            this->StepOneYouMustCreateASenseOfScarcity();
            m_begun = true;
        }

        void SheSellsSeaShellsOnTheSeaShore(
            std::string const& url,
            wxStaticText* text,
            wxGauge* gauge,
            void (PageInstalling::*finish)(wxWebResponse const&),
            bool file = false
        ) {
            wxWebRequest request = wxWebSession::GetDefault().CreateRequest(this, url);
            
            if (!request.IsOk()) {
                wxMessageBox(
                    "Fatal: Unable to create web request :(\n"
                    "Report this bug to the Geode developers; "
                    "you may have to manually install Geode if "
                    "the problem persists.",
                    "Error Installing",
                    wxICON_ERROR
                );
                return;
            }

            if (file) {
                request.SetStorage(wxWebRequest::Storage_File);
            }
            
            this->Bind(wxEVT_WEBREQUEST_STATE, [text, gauge, finish, this](wxWebRequestEvent& evt) {
                switch (evt.GetState()) {
                    case wxWebRequest::State_Completed: {
                        text->SetLabel("Finished");
                        gauge->SetValue(100);

                        auto res = evt.GetResponse();

                        if (!res.IsOk()) {
                            wxMessageBox(
                                "Fatal: Web request response is not OK :(\n"
                                "Report this bug to the Geode developers; "
                                "you may have to manually install Geode if "
                                "the problem persists.",
                                "Error Installing",
                                wxICON_ERROR
                            );
                            return;
                        }

                        if (res.GetStatus() != 200) {
                            wxMessageBox(
                                "Fatal: Web request returned " + std::to_string(res.GetStatus()) +
                                " :(\nReport this bug to the Geode developers; "
                                "you may have to manually install Geode if "
                                "the problem persists.",
                                "Error Installing",
                                wxICON_ERROR
                            );
                            return;
                        }

                        (this->*finish)(res);
                    } break;

                    case wxWebRequest::State_Active: {
                        auto got = evt.GetRequest().GetBytesReceived() / 1000.0;
                        auto left = evt.GetRequest().GetBytesExpectedToReceive() / 1000.0;
                        text->SetLabel(
                            std::to_string(got) + "kb / " + std::to_string(left) + "kb"
                        );
                        gauge->SetValue(static_cast<int>(got / left * 100.0));
                    } break;

                    case wxWebRequest::State_Idle: {
                        text->SetLabel("Waiting");
                    } break;

                    case wxWebRequest::State_Unauthorized: {
                        wxMessageBox(
                            "Fatal: Unauthorized to create web request :(\n"
                            "Report this bug to the Geode developers; "
                            "you may have to manually install Geode if "
                            "the problem persists.",
                            "Error Installing",
                            wxICON_ERROR
                        );
                    } break;

                    case wxWebRequest::State_Failed: {
                        wxMessageBox(
                            "Fatal: Web request failed :(\n"
                            "Report this bug to the Geode developers; "
                            "you may have to manually install Geode if "
                            "the problem persists.",
                            "Error Installing",
                            wxICON_ERROR
                        );
                    } break;

                    case wxWebRequest::State_Cancelled: {
                        wxMessageBox(
                            "Fatal: Web request cancelled :(\n"
                            "Report this bug to the Geode developers; "
                            "you may have to manually install Geode if "
                            "the problem persists.",
                            "Error Installing",
                            wxICON_ERROR
                        );
                    } break;
                }
            });

            request.Start();
        }

        void StepOneYouMustCreateASenseOfScarcity() {
            this->SheSellsSeaShellsOnTheSeaShore(
                "https://api.github.com/repos/geode-sdk/loader/releases/latest",
                m_conProgress,
                m_connecting,
                &PageInstalling::ShellsWillSellMuchBetterIfThePeopleThinkTheyreRareYouSee
            );
        }

        void ShellsWillSellMuchBetterIfThePeopleThinkTheyreRareYouSee(
            wxWebResponse const& res
        ) {
            try {
                auto json = nlohmann::json::parse(res.AsString());

                auto tagName = json["tag_name"].get<std::string>();
                m_conProgress->SetLabel("Got " + tagName);

                for (auto& asset : json["assets"]) {
                    auto name = asset["name"].get<std::string>();
                    if (name.find(PLATFORM_ASSET_IDENTIFIER) != std::string::npos) {
                        return StepTwoYouGottaMakeThePeopleThinkThatTheyWantEm(
                            asset["browser_download_url"].get<std::string>()
                        );
                    }
                }

                wxMessageBox(
                    "Fatal: No release asset for " PLATFORM_NAME " found :(\n"
                    "Report this bug to the Geode developers; "
                    "you may have to manually install Geode if "
                    "the problem persists.",
                    "Error Downloading",
                    wxICON_ERROR
                );
            } catch(std::exception& e) {
                wxMessageBox(
                    "Fatal: Unable to parse JSON: " + std::string(e.what()) + "\n"
                    "Report this bug to the Geode developers; "
                    "you may have to manually install Geode if "
                    "the problem persists.",
                    "Error Downloading",
                    wxICON_ERROR
                );
            }
        }

        void StepTwoYouGottaMakeThePeopleThinkThatTheyWantEm(
            std::string const& url
        ) {
            this->SheSellsSeaShellsOnTheSeaShore(
                url,
                m_downProgress,
                m_downloading,
                &PageInstalling::ReallyWantThemReallyFknWantThem,
                true
            );
        }

        void ReallyWantThemReallyFknWantThem(
            wxWebResponse const& res
        ) {
            ThreeItsMonopolyInvestInsideSomeProperty(res.GetDataFile());
        }

        void ThreeItsMonopolyInvestInsideSomeProperty(wxString const& file) {
            m_instProgress->SetLabel("Copying files");

            auto res = m_frame->InstallGeode(file);
            if (res.size()) {
                wxMessageBox(res, "Error Installing", wxICON_ERROR);
                return;
            }

            m_instProgress->SetLabel("Files copied");
            m_installing->SetValue(100);

            m_canContinue = true;
            m_frame->UpdateCanContinue();

            res = m_frame->SaveInstallInfo();
            if (res.size()) {
                wxMessageBox(res, "Error", wxICON_ERROR);
                return;
            }

            m_frame->NextPage();
        }

    public:
        PageInstalling(MainFrame* parent) : Page(parent) {
            m_sizer->Add(new wxStaticText(this, wxID_ANY,
                "Installing Geode..."
            ), 0, wxALL, 10);

            auto sizer = new wxFlexGridSizer(2);
            
            sizer->Add(new wxStaticText(this, wxID_ANY,
                "Step 1. Connecting to GitHub"
            ), 1, wxALL, 10);
            sizer->AddSpacer(2);
            sizer->Add((m_connecting = new wxGauge(this, wxID_ANY, 100)), 1, wxALL, 10);
            sizer->Add((m_conProgress = new wxStaticText(this, wxID_ANY, "")), 1, wxALL, 10);

            sizer->Add(new wxStaticText(this, wxID_ANY,
                "Step 2. Downloading files"
            ), 1, wxALL, 10);
            sizer->AddSpacer(2);
            sizer->Add((m_downloading = new wxGauge(this, wxID_ANY, 100)), 1, wxALL, 10);
            sizer->Add((m_downProgress = new wxStaticText(this, wxID_ANY, "")), 1, wxALL, 10);

            sizer->Add(new wxStaticText(this, wxID_ANY,
                "Step 3. Installing"
            ), 1, wxALL, 10);
            sizer->AddSpacer(2);
            sizer->Add((m_installing = new wxGauge(this, wxID_ANY, 100)), 1, wxALL, 10);
            sizer->Add((m_instProgress = new wxStaticText(this, wxID_ANY, "")), 1, wxALL, 10);

            m_sizer->Add(sizer);

            m_canContinue = false;
            m_canGoBack = false;
        }
    };
    m_pageGens.insert({
        PageID::Install,
        [](auto f) -> Page* { return new PageInstalling(f); }
    });

    //////////////////

    class PageFinished : public Page {
    public:
        PageFinished(MainFrame* parent) : Page(parent) {
            m_sizer->Add(new wxStaticText(this, wxID_ANY,
                "Installing finished! "
                "You can now close this installer && start up Geometry Dash :)"
            ), 1, wxALL, 10);

            auto contact = new wxButton(this, wxID_ANY, "Support Discord Server");
            contact->Bind(wxEVT_BUTTON, &MainFrame::OnDiscord, m_frame);
            m_sizer->Add(contact, 0, wxALL, 10);

            m_canContinue = true;
            m_canGoBack = false;
        }
    };
    m_pageGens.insert({
        PageID::InstallFinished,
        [](auto f) -> Page* { return new PageFinished(f); }
    });

    //////////////////

    class PageUninstallStart : public Page {
    protected:
        void OnSelect(wxCommandEvent& event) {
            m_frame->m_uninstallType = event.GetId();
        }

    public:
        PageUninstallStart(MainFrame* parent) : Page(parent) {
            m_sizer->Add(new wxStaticText(this, wxID_ANY,
                "What would you like to uninstall?"
            ), 0, wxALL, 10);

            auto gdps = new wxRadioButton(this, ID_UninstallAll,
                "Uninstall everything",
                wxDefaultPosition, wxDefaultSize, wxRB_GROUP
            );
            gdps->Bind(wxEVT_RADIOBUTTON, &PageUninstallStart::OnSelect, this);
            m_sizer->Add(gdps, 0, wxALL, 10);
            
            auto dev = new wxRadioButton(this, ID_UninstallSome,
                "Choose which parts to uninstall"
            );
            dev->Bind(wxEVT_RADIOBUTTON, &PageUninstallStart::OnSelect, this);
            m_sizer->Add(dev, 0, wxALL, 10);

            m_frame->m_uninstallType = ID_UninstallAll;
            m_canContinue = true;
        }
    };
    m_pages.insert({ PageID::UninstallStart, new PageUninstallStart(this) });

    //////////////////

    class PageUninstallSelect : public Page {
    protected:
        wxDataViewListCtrl* m_list;
        std::unordered_map<int, std::string> m_items;
        std::set<int> m_selected;

        void Enter() override {
            m_skipThis = m_frame->m_uninstallType == ID_UninstallAll;
        }

        void Leave() override {
            m_frame->m_toUninstall = {};
            for (auto& s : m_selected) {
                m_frame->m_toUninstall.insert(m_items.at(s));
            }
        }

        void OnSelect(wxDataViewEvent& event) {
            auto row = m_list->GetSelectedRow();
            if (m_list->GetToggleValue(row, 1)) {
                m_selected.insert(row);
            } else {
                m_selected.erase(row);
            }
        }

    public:
        PageUninstallSelect(MainFrame* parent) : Page(parent) {
            m_sizer->Add(new wxStaticText(this, wxID_ANY,
                "Select which instances of Geode to uninstall:"
            ), 0, wxALL | wxEXPAND, 10);

            m_list = new wxDataViewListCtrl(this, wxID_ANY);

            m_list->Bind(wxEVT_DATAVIEW_ITEM_VALUE_CHANGED, &PageUninstallSelect::OnSelect, this);

            m_list->AppendTextColumn("Location", wxDATAVIEW_CELL_INERT, m_frame->GetSize().x - 150);
            m_list->AppendToggleColumn("Uninstall");

            auto ix = 0;
            for (auto& i : m_frame->m_installations) {
                wxVector<wxVariant> data;
                data.push_back(wxVariant(i.m_path));
                data.push_back(wxVariant(false));
                m_list->AppendItem(data);
                m_items.insert({ ix, i.m_path });
                ix++;
            }

            m_sizer->Add(m_list, 1, wxALL | wxEXPAND, 10);

            m_canContinue = true;
        }
    };
    m_pages.insert({ PageID::UninstallSelect, new PageUninstallSelect(this) });

    //////////////////

    class PageUninstallDeleteData : public Page {
    protected:
        wxCheckBox* m_box;
        wxStaticText* m_info;

        void OnSelect(wxCommandEvent&) {
            m_frame->m_deleteSaveData = m_box->IsChecked();
        }

        void Enter() override {
            m_info->SetLabel(
                "Do you also want to delete all save data associated "
                "with " + std::string(
                    m_frame->m_uninstallType == ID_UninstallAll ?
                        "Geode" : "the selected parts"
                ) + "? This means that all Geode- and mod-related settings, "
                "save data, etc. will be lost. This will not affect your "
                "normal Geometry Dash save data."
            );
        }

    public:
        PageUninstallDeleteData(MainFrame* parent) : Page(parent) {
            m_sizer->Add((m_info = new wxStaticText(this, wxID_ANY, "")), 1, wxALL | wxEXPAND, 10);
            m_sizer->Add(new wxStaticText(this, wxID_ANY,
                "Note that for some GDPSes this installer may be "
                "unable to accurately detect where Geode's save data "
                "is located. In these cases, you will have to manually "
                "remove the save data. Contact the GDPS's owner for "
                "information on where its save data lies."
            ), 1, wxALL | wxEXPAND, 10);

            m_sizer->Add(
                (m_box = new wxCheckBox(this, wxID_ANY, "Delete all save data")),
            0, wxALL | wxEXPAND, 10);
            m_box->SetValue(true);

            m_canContinue = true;
        }
    };
    m_pages.insert({ PageID::UninstallDeleteData, new PageUninstallDeleteData(this) });

    //////////////////

    class PageUninstall : public Page {
    protected:
        wxGauge* m_gauge;

        void Enter() override {
            if (m_frame->m_uninstallType == ID_UninstallAll) {
                m_frame->m_toUninstall = {};
                for (auto& i : m_frame->m_installations) {
                    m_frame->m_toUninstall.insert(i.m_path);
                }
            }
            size_t ix = 0;
            std::vector<typename decltype(m_frame->m_installations)::iterator> rem;
            for (auto i = m_frame->m_installations.begin(); i != m_frame->m_installations.end(); i++) {
                if (m_frame->m_toUninstall.count(i->m_path)) {
                    if (!m_frame->UninstallGeode(*i)) {
                        wxMessageBox(
                            "Unable to uninstall Geode from " + i->m_path + "! "
                            "You may need to manually remove the files; contact "
                            "the Geode Development Team for more information.",
                            "Error",
                            wxICON_ERROR
                        );
                    }
                    if (m_frame->m_deleteSaveData) {
                        if (!m_frame->DeleteSaveData(*i)) {
                            wxMessageBox(
                                "Unable to delete save data for Geode from " + i->m_path + "! "
                                "You may need to manually remove the files; if the given "
                                "installation is a GDPS, contact its owner for help. Otherwise, "
                                "contact the Geode Development Team for more information.",
                                "Error",
                                wxICON_ERROR
                            );
                        }
                    }
                    rem.push_back(i);
                    ix++;
                    m_gauge->SetValue(static_cast<int>(
                        static_cast<double>(ix) / m_frame->m_toUninstall.size() * 100
                    ));
                    m_frame->Update();
                }
            }
            for (auto& u : rem) {
                m_frame->m_installations.erase(u);
            }
            if (m_frame->m_uninstallType == ID_UninstallAll) {
                m_frame->DeleteInstallInfo();
            }
            m_canContinue = true;
            m_skipThis = true;
        }

    public:
        PageUninstall(MainFrame* parent) : Page(parent) {
            auto sizer = new wxFlexGridSizer(2);

            sizer->Add(new wxStaticText(this, wxID_ANY, "Uninstalling..."), 1, wxALL, 10);
            sizer->Add((m_gauge = new wxGauge(this, wxID_ANY, 100)), 1, wxALL, 10);

            m_sizer->Add(sizer);

            m_canContinue = false;
            m_canGoBack = false;
        }
    };
    m_pages.insert({ PageID::Uninstall, new PageUninstall(this) });
    
    //////////////////

    class PageUninstallFinished : public Page {
    public:
        PageUninstallFinished(MainFrame* parent) : Page(parent) {
            m_sizer->Add(new wxStaticText(this, wxID_ANY,
                "Uninstalling finished! "
                "Geode should now be removed from " + std::string(
                    m_frame->m_uninstallType == ID_UninstallAll ?
                        "your computer" : "the selected parts"
                ) + " :)"
            ), 1, wxALL, 10);

            m_sizer->Add(new wxStaticText(this, wxID_ANY,
                "If you find parts of Geode still lying around, "
                "contact the Geode Development Team for more "
                "information."
            ), 1, wxALL, 10);

            auto contact = new wxButton(this, wxID_ANY, "Support Discord Server");
            contact->Bind(wxEVT_BUTTON, &MainFrame::OnDiscord, m_frame);
            m_sizer->Add(contact, 0, wxALL, 10);

            m_canContinue = true;
            m_canGoBack = false;
        }
    };
    m_pageGens.insert({
        PageID::UninstallFinished,
        [](auto f) -> Page* { return new PageUninstallFinished(f); }
    });
}
