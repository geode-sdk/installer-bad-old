#include "MainFrame.hpp"
#include "Manager.hpp"

void MainFrame::onMouseLeftDown(wxMouseEvent& event) {
    if (!m_dragging) {
        this->Bind(wxEVT_LEFT_UP, &MainFrame::onMouseLeftUp, this);
        this->Bind(wxEVT_MOTION, &MainFrame::onMouseMotion, this);
        m_dragging = true;

        wxPoint clientStart = event.GetPosition();
        m_dragStartMouse = this->ClientToScreen(clientStart);
        m_dragStartWindow = this->GetPosition();
        this->CaptureMouse();
    }
}

void MainFrame::onMouseLeftUp(wxMouseEvent& event) {
    this->finishDrag();
}

void MainFrame::onMouseMotion(wxMouseEvent& event) {
    wxPoint curClientPsn = event.GetPosition();
    wxPoint curScreenPsn = ClientToScreen(curClientPsn);
    wxPoint movementVector = curScreenPsn - m_dragStartMouse;

    this->SetPosition(m_dragStartWindow + movementVector);
}

void MainFrame::onMouseCaptureLost(wxMouseCaptureLostEvent&) {
    this->finishDrag();
}

void MainFrame::finishDrag() {
    if ( m_dragging ) {
        Unbind(wxEVT_LEFT_UP, &MainFrame::onMouseLeftUp, this);
        Unbind(wxEVT_MOTION, &MainFrame::onMouseMotion, this);
        m_dragging = false;
    }

    if (HasCapture()) {
        ReleaseMouse();
    }
}

void MainFrame::onNext(wxCommandEvent& event) {
    this->nextPage();
}

void MainFrame::onPrev(wxCommandEvent& event) {
    this->prevPage();
}

void MainFrame::onQuit(wxCommandEvent& event) {
    this->Close();
}

void MainFrame::onDiscord(wxCommandEvent&) {
    wxLaunchDefaultBrowser("https://discord.gg/9e43WMKzhp");
}

void MainFrame::onClose(wxCloseEvent& event) {
    if (m_pageIndex < m_structure.size() && event.CanVeto()) {
        if (wxMessageBox(
            "Are you sure you want to quit the installer?",
            "Quit installation",
            wxICON_WARNING | wxYES_NO
        ) != wxYES ) {
            return event.Veto();
        }
    }
    event.Skip();
}

void MainFrame::onResize(wxSizeEvent& event) {
    if (m_current) m_current->resize();
    this->Layout();
}

void MainFrame::goToPage(PageID id) {
    if (m_current) {
        m_current->leave();
        m_current->Hide();
    }
    while (m_contentSizer->GetItemCount()) {
        m_contentSizer->Remove(0);
    }
    auto page = Page::getPage(id, this);
    if (page) {
        m_current = page;
        m_current->enter();
        m_current->Show();
        m_contentSizer->Add(m_current, 1, wxALL | wxEXPAND, 10);
        
        this->updateControls();

        // for some reason this needs to be done 
        // to make sure labels wrap properly
        this->Layout();
        m_current->resize();
        m_current->Update();
        this->Layout();
    } else if (id != PageID::NotFound) {
        this->goToPage(PageID::NotFound);
    }
    this->SetTitle(
        m_pageIndex ?
            "Geode Installer (" +
                std::to_string(m_pageIndex) + "/" +
                std::to_string(m_structure.size()) + ")" :
            "Geode Installer"
    );
}

void MainFrame::prevPage() {
    if (m_pageIndex) m_pageIndex--;
    if (!m_pageIndex) return this->goToPage(m_startPage);
    this->goToPage(m_structure.at(m_pageIndex - 1));
    if (m_current->m_skipThis) {
        this->prevPage();
    }
}

void MainFrame::nextPage() {
    if (!m_structure.size()) this->goToPage(PageID::NotFound);
    if (m_pageIndex < m_structure.size()) {
        m_pageIndex++;
        this->goToPage(m_structure.at(m_pageIndex - 1));
        if (m_current->m_skipThis) {
            this->nextPage();
        }
    } else {
        this->Close();
    }
}

void MainFrame::updateControls() {
    m_prevBtn->Enable(m_pageIndex && m_current->m_canGoBack);
    if (m_current->m_canContinue) {
        m_nextBtn->Enable(m_current->m_canContinue);
    } else {
        m_nextBtn->Disable();
    }
    m_nextBtn->SetLabel(
        m_pageIndex == m_structure.size() ? 
            "Finish" :
            "Next"
    );
}

void MainFrame::selectPageStructure(InstallType type) {
    switch (type) {
        case InstallType::Install: {
            m_structure = {
                PageID::EULA,
                PageID::InstallSelectGD,
                PageID::InstallCheckMods,
                PageID::Install,
                PageID::InstallFinished,
            };
        } break;

        case InstallType::InstallOnGDPS: {
            m_structure = {
                PageID::InstallGDPSInfo,
                PageID::EULA,
                PageID::InstallSelectGD,
                PageID::InstallCheckMods,
                PageID::Install,
                PageID::InstallFinished,
            };
        } break;

        case InstallType::InstallDevTools: {
            m_structure = {
                PageID::EULA,
                PageID::DevInstallSelectSDK,
                PageID::DevInstall,
                PageID::DevInstallFinished,
            };
        } break;

        case InstallType::Uninstall: {
            m_structure = {
                PageID::UninstallStart,
                PageID::UninstallSelect,
                PageID::UninstallDeleteData,
                PageID::Uninstall,
                PageID::UninstallFinished,
            };
        } break;
    }
}

MainFrame::MainFrame() : wxFrame(
    nullptr,
    wxID_ANY,
    "Geode Installer",
    wxDefaultPosition,
    { 440, 380 }
) {
    auto res = Manager::get()->loadData();
    if (!res) {
        wxMessageBox(
            "Unable to load settings: " + res.error() + ". "
            "The installer may be unable to uninstall Geode!"
        );
    }

    // for some reason the default bg color on windows
    // is ugly grey and wxWidgets doesn't appear to have 
    // dark mode support
    #ifdef _WIN32
    this->SetBackgroundColour({ 255, 255, 255 });
    #endif
    this->CenterOnScreen();

    this->Bind(wxEVT_LEFT_DOWN, &MainFrame::onMouseLeftDown, this);
    this->Bind(wxEVT_MOUSE_CAPTURE_LOST, &MainFrame::onMouseCaptureLost, this);
    this->Bind(wxEVT_CLOSE_WINDOW, &MainFrame::onClose, this);
    this->Bind(wxEVT_SIZE, &MainFrame::onResize, this);

    auto mainSizer = new wxBoxSizer(wxVERTICAL);

    m_contentSizer = new wxBoxSizer(wxVERTICAL);
    mainSizer->Add(m_contentSizer, 1, wxALL | wxEXPAND, 0);

    auto bottomControls = new wxControl(this, wxID_ANY);

    auto bottomSizer = new wxStaticBoxSizer(
        new wxStaticBox(bottomControls, wxID_ANY, "(c) Geode Team 2022"),
        wxHORIZONTAL
    );
    
    auto quitBtn = new wxButton(bottomControls, wxID_ANY, "Quit");
    quitBtn->Bind(wxEVT_BUTTON, &MainFrame::onQuit, this);
    bottomSizer->Add(quitBtn, 0, wxALL, 10);

    bottomSizer->AddStretchSpacer();

    m_prevBtn = new wxButton(bottomControls, wxID_ANY, "Back");
    m_prevBtn->Bind(wxEVT_BUTTON, &MainFrame::onPrev, this);
    bottomSizer->Add(m_prevBtn, 0, wxALL, 10);

    m_nextBtn = new wxButton(bottomControls, wxID_ANY, "Next");
    m_nextBtn->Bind(wxEVT_BUTTON, &MainFrame::onNext, this);
    bottomSizer->Add(m_nextBtn, 0, wxALL, 10);

    bottomControls->SetSizer(bottomSizer);

    mainSizer->Add(bottomControls, 0, wxALL | wxEXPAND, 0);

    if (Manager::get()->isFirstTime()) {
        this->selectPageStructure(InstallType::Install);
        m_startPage = PageID::FirstStart;
    } else {
        this->selectPageStructure(InstallType::InstallOnGDPS);
        m_startPage = PageID::Start;
    }

    this->SetSizer(mainSizer);
    this->SetMinSize({330, 285});
    this->goToPage(m_startPage);
    this->Layout();
}
