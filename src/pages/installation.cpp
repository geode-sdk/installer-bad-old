#include "Page.hpp"
#include "../MainFrame.hpp"
#include "../Manager.hpp"

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
        wxFileDialog ofd(
            this, "Select Geometry Dash", "", "",
            "Executable files (*.exe)|*.exe",
            wxFD_OPEN | wxFD_FILE_MUST_EXIST 
        );
        if (ofd.ShowModal() == wxID_CANCEL) return;
        m_pathInput->SetValue(ofd.GetPath());
    }

    void onText(wxCommandEvent&) {
        this->updateContinue();
    }

    void updateContinue() {
        auto path = m_pathInput->GetValue().ToStdWstring();
        m_canContinue =
            ghc::filesystem::exists(path) &&
            ghc::filesystem::is_regular_file(path);
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

        m_pathInput = this->addInput<&PageInstallSelectGD::onText>(path.wstring());

        this->addButton<&PageInstallSelectGD::onBrowse>("Browse");
    }
};
REGISTER_PAGE(InstallSelectGD);

/////////////////

