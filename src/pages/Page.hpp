#pragma once

#include "../include/wx.hpp"

class Page;
class MainFrame;

enum class PageID {
    Start,
    FirstStart,
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

using PageGen = Page*(*)(MainFrame*);

class Page : public wxPanel {
protected:
    MainFrame* m_frame;
    wxSizer* m_sizer;
    std::unordered_map<wxStaticText*, wxString> m_labels;
    bool m_canContinue = false;
    bool m_canGoBack = true;
    bool m_skipThis = false;

    virtual void enter();
    virtual void leave();
    virtual void resize();
    virtual void onSelect(wxCommandEvent& e);

    wxStaticText* addText(wxString const& text);
    wxTextCtrl* addLongText(wxString const& text);
    void addSelect(std::initializer_list<wxString> const& select);
    template<auto Func = nullptr>
    wxTextCtrl* addInput(wxString const& text) {
        auto input = new wxTextCtrl(this, wxID_ANY, text);
        m_sizer->Add(input, 0, wxALL | wxEXPAND, 10);
        if constexpr (Func != nullptr) {
            input->Bind(wxEVT_TEXT, Func, this);
        }
        return input;
    }
    template<auto Func = nullptr>
    wxButton* addButton(wxString const& text) {
        auto btn = new wxButton(this, wxID_ANY, text);
        m_sizer->Add(btn, 0, wxALL, 10);
        if constexpr (Func != nullptr) {
            btn->Bind(wxEVT_BUTTON, Func, this);
        }
        return btn;
    }

    friend class MainFrame;

public:
    Page(MainFrame* parent);
    virtual ~Page() = default;

    static Page* getPage(PageID id, MainFrame* frame);
    static void registerPage(PageID id, PageGen gen);

    template<class T>
    static T* get(PageID id, MainFrame* frame) {
        return reinterpret_cast<T*>(Page::getPage(id, frame));
    }
};

template<PageID ID, typename P>
struct RegisterPage {
    RegisterPage() {
        Page::registerPage(ID, [](MainFrame* frame) -> Page* { return new P(frame); });
    }
};
#define REGISTER_PAGE(pg) static RegisterPage<PageID::pg, Page##pg> reg##pg;
