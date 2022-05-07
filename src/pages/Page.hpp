#pragma once

#include "../include/wx.hpp"
#include <unordered_map>

class Page;
class MainFrame;

enum class PageID {
    Start,
    FirstStart,
    EULA,
    NotFound,

    InstallGDPSInfo,
    InstallSelectGD,
    InstallCheckMods,
    Install,
    InstallFinished,

    DevInstallSelectSDK,
    DevInstallBranch,
    DevInstallAddToPath,
    DevInstall,
    DevInstallFinished,

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

    void setText(wxStaticText* text, wxString const& newText);
    wxStaticText* addText(wxString const& text);
    wxTextCtrl* addLongText(wxString const& text);
    wxGauge* addProgressBar();
    void addSelect(std::initializer_list<wxString> const& select);
    template<class Class>
    wxTextCtrl* addInput(wxString const& text, void(Class::*func)(wxCommandEvent&)) {
        auto input = new wxTextCtrl(this, wxID_ANY, text);
        m_sizer->Add(input, 0, wxALL | wxEXPAND, 10);
        if (func) {
            input->Bind(wxEVT_TEXT, func, reinterpret_cast<Class*>(this));
        }
        return input;
    }
    template<class Class>
    wxButton* addButton(wxString const& text, void(Class::*func)(wxCommandEvent&), Class* ptr = nullptr) {
        auto btn = new wxButton(this, wxID_ANY, text);
        m_sizer->Add(btn, 0, wxALL, 10);
        if (func) {
            if (!ptr) ptr = reinterpret_cast<Class*>(this);
            btn->Bind(wxEVT_BUTTON, func, ptr);
        }
        return btn;
    }
    template<class Class>
    wxCheckBox* addToggle(wxString const& text, void(Class::*func)(wxCommandEvent&), Class* ptr = nullptr) {
        auto btn = new wxCheckBox(this, wxID_ANY, text);
        m_sizer->Add(btn, 0, wxALL, 10);
        if (func) {
            if (!ptr) ptr = reinterpret_cast<Class*>(this);
            btn->Bind(wxEVT_CHECKBOX, func, ptr);
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
#define GET_EARLIER_PAGE(pg) Page::get<Page##pg>(PageID::pg, m_frame)
