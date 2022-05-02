#include "MainFrame.hpp"

void MainFrame::OnMouseLeftDown(wxMouseEvent& event) {
    if (!m_dragging) {
        this->Bind(wxEVT_LEFT_UP, &MainFrame::OnMouseLeftUp, this);
        this->Bind(wxEVT_MOTION, &MainFrame::OnMouseMotion, this);
        m_dragging = true;

        wxPoint clientStart = event.GetPosition();
        m_dragStartMouse = this->ClientToScreen(clientStart);
        m_dragStartWindow = this->GetPosition();
        this->CaptureMouse();
    }
}

void MainFrame::OnMouseLeftUp(wxMouseEvent& event) {
    this->FinishDrag();
}

void MainFrame::OnMouseMotion(wxMouseEvent& event) {
    wxPoint curClientPsn = event.GetPosition();
    wxPoint curScreenPsn = ClientToScreen(curClientPsn);
    wxPoint movementVector = curScreenPsn - m_dragStartMouse;

    this->SetPosition(m_dragStartWindow + movementVector);
}

void MainFrame::OnMouseCaptureLost(wxMouseCaptureLostEvent&) {
    this->FinishDrag();
}

void MainFrame::FinishDrag() {
    if ( m_dragging ) {
        Unbind(wxEVT_LEFT_UP, &MainFrame::OnMouseLeftUp, this);
        Unbind(wxEVT_MOTION, &MainFrame::OnMouseMotion, this);
        m_dragging = false;
    }

    if (HasCapture()) {
        ReleaseMouse();
    }
}

MainFrame::MainFrame() : wxFrame(
    nullptr,
    wxID_ANY,
    "Geode Installer",
    wxDefaultPosition,
    { 440, 380 }
) {
    this->SetBackgroundColour({ 255, 255, 255 });
    this->CenterOnScreen();

    this->Bind(wxEVT_LEFT_DOWN, &MainFrame::OnMouseLeftDown, this);
    this->Bind(wxEVT_MOUSE_CAPTURE_LOST, &MainFrame::OnMouseCaptureLost, this);
    this->Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnClose, this);
    this->Bind(wxEVT_SIZE, &MainFrame::OnResize, this);

    auto mainSizer = new wxBoxSizer(wxVERTICAL);

    m_contentSizer = new wxBoxSizer(wxVERTICAL);
    mainSizer->Add(m_contentSizer, 1, wxALL | wxEXPAND, 0);

    auto bottomControls = new wxControl(this, wxID_ANY);

    auto bottomSizer = new wxStaticBoxSizer(
        new wxStaticBox(bottomControls, wxID_ANY, "(c) Geode Team 2022"),
        wxHORIZONTAL
    );
    
    auto quitBtn = new wxButton(bottomControls, wxID_ANY, "Quit");
    quitBtn->Bind(wxEVT_BUTTON, &MainFrame::OnQuit, this);
    bottomSizer->Add(quitBtn, 0, wxALL, 10);

    bottomSizer->AddStretchSpacer();

    m_prevBtn = new wxButton(bottomControls, wxID_ANY, "Back");
    m_prevBtn->Bind(wxEVT_BUTTON, &MainFrame::OnPrev, this);
    bottomSizer->Add(m_prevBtn, 0, wxALL, 10);

    m_nextBtn = new wxButton(bottomControls, wxID_ANY, "Next");
    m_nextBtn->Bind(wxEVT_BUTTON, &MainFrame::OnNext, this);
    bottomSizer->Add(m_nextBtn, 0, wxALL, 10);

    bottomControls->SetSizer(bottomSizer);

    mainSizer->Add(bottomControls, 0, wxALL | wxEXPAND, 0);

    this->SetSizer(mainSizer);

    this->LoadInstallInfo();
    this->SetupPages();
    this->GoToPage(PageID::Start);

    this->Layout();
}

void MainFrame::OnNext(wxCommandEvent& event) {
    this->NextPage();
}

void MainFrame::OnPrev(wxCommandEvent& event) {
    this->PrevPage();
}

void MainFrame::OnQuit(wxCommandEvent& event) {
    this->Close();
}

void MainFrame::OnDiscord(wxCommandEvent&) {
    wxLaunchDefaultBrowser("https://discord.gg/9e43WMKzhp");
}

void MainFrame::OnClose(wxCloseEvent& event) {
    if (m_page < m_structure.size() && event.CanVeto()) {
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

void MainFrame::OnResize(wxSizeEvent& event) {
    this->Layout();
}

void MainFrame::GoToPage(PageID id) {
    if (m_current) {
        m_current->Leave();
        m_current->Hide();
    }
    while (m_contentSizer->GetItemCount()) {
        m_contentSizer->Remove(0);
    }
    if (!m_pages.count(id)) {
        if (m_pageGens.count(id)) {
            m_pages.insert({ id, m_pageGens.at(id)(this) });
        }
    }
    if (m_pages.count(id)) {
        m_current = m_pages.at(id);
        m_current->Enter();
        m_current->Show();
        m_contentSizer->Add(m_current, 1, wxALL | wxEXPAND, 10);
        
        this->UpdateCanContinue();

        this->Layout();
    } else if (id != PageID::NotFound) {
        this->GoToPage(PageID::NotFound);
    }
    this->SetTitle(
        m_page ?
            "Geode Installer (" +
                std::to_string(m_page) + "/" +
                std::to_string(m_structure.size()) + ")" :
            "Geode Installer"
    );
}

void MainFrame::PrevPage() {
    if (m_page) m_page--;
    if (!m_page) return this->GoToPage(PageID::Start);
    this->GoToPage(m_structure.at(m_page - 1));
    if (m_current->m_skipThis) {
        this->PrevPage();
    }
}

void MainFrame::NextPage() {
    if (!m_structure.size()) this->GoToPage(PageID::NotFound);
    if (m_page < m_structure.size()) {
        m_page++;
        this->GoToPage(m_structure.at(m_page - 1));
        if (m_current->m_skipThis) {
            this->NextPage();
        }
    } else {
        this->Close();
    }
}

void MainFrame::UpdateCanContinue() {
    m_prevBtn->Enable(m_page && m_current->m_canGoBack);
    if (m_current->m_canContinue) {
        m_nextBtn->Enable(m_current->m_canContinue);
    } else {
        m_nextBtn->Disable();
    }
    m_nextBtn->SetLabel(
        m_page == m_structure.size() ? 
            "Finish" :
            "Next"
    );
}

void MainFrame::PickPage(int id) {
    switch (id) {
        case ID_Install: {
            m_structure = {
                PageID::EULA,
                PageID::InstallSelectGD,
                PageID::InstallCheckForModLoaders,
                PageID::Install,
                PageID::InstallFinished,
            };
        } break;

        case ID_GDPS: {
            m_structure = {
                PageID::InstallGDPSInfo,
                PageID::EULA,
                PageID::InstallSelectGD,
                PageID::InstallCheckForModLoaders,
                PageID::Install,
                PageID::InstallFinished,
            };
        } break;

        case ID_Dev: {
            m_structure = {
                PageID::DevInstall,
            };
        } break;

        case ID_Uninstall: {
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

Page::Page(MainFrame* parent) : wxPanel(parent) {
    m_frame = parent;
    m_sizer = new wxBoxSizer(wxVERTICAL);
    this->SetSizer(m_sizer);
    this->Hide();
}
