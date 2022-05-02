#pragma once

#include "wx.hpp"
#include <unordered_map>
#include "fs/filesystem.hpp"
#include <set>

enum {
    ID_Install,
    ID_GDPS,
    ID_Dev,
    ID_Uninstall,

    ID_UninstallAll,
    ID_UninstallSome,
};

class MainFrame;

enum class PageID {
    Start,
    EULA,
    NotFound,

    InstallGDPSInfo,
    InstallSelectGD,
    InstallCheckForModLoaders,
    Install,
    InstallFinished,

    DevInstall,

    UninstallStart,
    UninstallSelect,
    UninstallDeleteData,
    Uninstall,
    UninstallFinished,
};

class Page;

class Page : public wxPanel {
protected:
    MainFrame* m_frame;
    wxBoxSizer* m_sizer;
    bool m_canContinue = false;
    bool m_canGoBack = true;
    bool m_skipThis = false;

    friend class MainFrame;

    virtual void Enter() {}
    virtual void Leave() {}

public:
    Page(MainFrame* parent);
};

using PageGen = Page*(*)(MainFrame*);

struct OtherModLoaderInfo {
    int m_hasMH = 0;
    bool m_hasGeode = false;
    bool m_hasGDHM = false;
    bool m_hasMoreUnknown = false;
    std::vector<std::string> m_others;

    inline bool None() const {
        return !m_hasMH && !m_hasGeode &&
            !m_hasGDHM && !m_hasMoreUnknown &&
            !m_others.size();
    }
};

struct Installation {
    std::string m_path;
    std::string m_exe;

    bool operator<(Installation const& other) const {
        return m_path < other.m_path;
    }
};

class MainFrame : public wxFrame {
protected:
    bool m_dragging = false;
    bool m_firstTime = true;
    wxPoint m_dragStartMouse;
    wxPoint m_dragStartWindow;
    wxSizer* m_contentSizer;
    std::unordered_map<PageID, PageGen> m_pageGens;
    std::unordered_map<PageID, Page*> m_pages;
    std::set<Installation> m_installations;
    std::vector<PageID> m_structure;
    size_t m_page = 0;
    Page* m_current = nullptr;
    wxButton* m_nextBtn;
    wxButton* m_prevBtn;
    ghc::filesystem::path m_installingTo;
    std::string m_targetExeName;
    int m_uninstallType;
    bool m_deleteSaveData;
    std::set<std::string> m_toUninstall;

    void OnMouseLeftDown(wxMouseEvent& event);
    void OnMouseLeftUp(wxMouseEvent& event);
    void OnMouseMotion(wxMouseEvent& event);
    void OnMouseCaptureLost(wxMouseCaptureLostEvent&);
    void FinishDrag();
    void OnClose(wxCloseEvent&);
    void OnResize(wxSizeEvent&);

    void SetupPages();

    void OnNext(wxCommandEvent&);
    void OnPrev(wxCommandEvent&);
    void OnQuit(wxCommandEvent&);
    void OnDiscord(wxCommandEvent&);

    void UpdateCanContinue();
    void PickPage(int id);
    void ResizeText(float width);

    // platform-specific

    static ghc::filesystem::path FigureOutGDPath();
    OtherModLoaderInfo DetectOtherModLoaders();

    void LoadInstallInfo();
    std::string SaveInstallInfo();
    void DeleteInstallInfo();

    std::string InstallGeode(wxString const& path);
    bool UninstallGeode(Installation const& inst);
    bool DeleteSaveData(Installation const& inst);

    friend class Page;

public:
    MainFrame();

    void GoToPage(PageID id);
    void NextPage();
    void PrevPage();
};
