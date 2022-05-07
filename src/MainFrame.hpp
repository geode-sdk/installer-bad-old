#pragma once

#include "include/wx.hpp"
#include <unordered_map>
#include "fs/filesystem.hpp"
#include <set>
#include "pages/Page.hpp"

enum class InstallType {
    Install,
    InstallOnGDPS,
    InstallDevTools,
    Uninstall,
};

class MainFrame : public wxFrame {
protected:
    bool m_dragging = false;
    wxPoint m_dragStartMouse;
    wxPoint m_dragStartWindow;
    wxSizer* m_contentSizer;
    PageID m_startPage;
    std::vector<PageID> m_structure;
    size_t m_pageIndex = 0;
    Page* m_current = nullptr;
    wxButton* m_nextBtn;
    wxButton* m_prevBtn;

    void onMouseLeftDown(wxMouseEvent& event);
    void onMouseLeftUp(wxMouseEvent& event);
    void onMouseMotion(wxMouseEvent& event);
    void onMouseCaptureLost(wxMouseCaptureLostEvent&);
    void finishDrag();
    void onClose(wxCloseEvent&);
    void onResize(wxSizeEvent&);

    void loaderUpdateWindow();

    void onNext(wxCommandEvent&);
    void onPrev(wxCommandEvent&);
    void onQuit(wxCommandEvent&);
    friend class Page;

public:
    MainFrame();

    void goToPage(PageID id);
    void nextPage();
    void prevPage();

    void updateControls();
    void selectPageStructure(InstallType type);
    
    void onDiscord(wxCommandEvent&);
};
