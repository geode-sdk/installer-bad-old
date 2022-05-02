#include "MainFrame.hpp"
#include <fstream>
#include "json.hpp"
#include <wx/zipstrm.h>
#include <wx/wfstream.h>
#include <wx/stdpaths.h>
#ifdef _WIN32
#include <wx/msw/registry.h>
#endif

void MainFrame::LoadInstallInfo() {
    #ifdef _WIN32
    std::wstring installInfo = L"";
    wxRegKey key(wxRegKey::HKLM, "Software\\GeodeSDK");
    if (key.HasValue("InstallInfo")) {
        wxString value;
        key.QueryValue("InstallInfo", value);
        installInfo = value;
        m_firstTime = false;
    }
    if (installInfo.size()) {
        std::filesystem::path path(installInfo);
        if (std::filesystem::exists(path)) {
            std::ifstream file(path);
            if (!file.is_open()) return;
            std::string data(std::istreambuf_iterator<char>{file}, {});
            try {
                auto json = nlohmann::json::parse(data);
                for (auto& i : json["installations"]) {
                    m_installations.insert({
                        i["path"].get<std::string>(),
                        i["exe"].get<std::string>()
                    });
                }
            } catch(std::exception& e) {
                wxMessageBox("Error loading installation info: " + std::string(e.what()));
            }
        }
    }
    #else
    static_assert(false, "Implement MainFrame::LoadInstallInfo!");
    // Just store the path to the install info directory somewhere 
    // and read it back. That's it, the rest is a cross-platform 
    // JSON read
    #endif
}

std::string MainFrame::SaveInstallInfo() {
    auto path = std::filesystem::absolute("GeodeInstallationInfo.json");

    auto json = nlohmann::json::object();
    try {
        json["installations"] = nlohmann::json::array();
        for (auto& i : m_installations) {
            auto obj = nlohmann::json::object();
            obj["path"] = i.m_path;
            obj["exe"] = i.m_exe;
            json["installations"].push_back(obj);
        }
    } catch(...) {}

    {
        std::wofstream file(path);
        if (!file.is_open()) return "Unable to write installation data - the installer wont be able to uninstall Geode!";
        file << json.dump(4);
    }

    wxRegKey key(wxRegKey::HKLM, "Software\\GeodeSDK");
    if (!key.Create()) {
        return "Unable to create Registry Key - the installer wont be able to uninstall Geode!";
    }
    if (!key.SetValue("InstallInfo", path.wstring())) {
        return "Unable to save Registry Key - the installer wont be able to uninstall Geode!";
    }
    return "";
}

void MainFrame::DeleteInstallInfo() {
    #ifdef _WIN32
    std::wstring installInfo = L"";
    wxRegKey key(wxRegKey::HKLM, "Software\\GeodeSDK");
    if (key.HasValue("InstallInfo")) {
        wxString value;
        key.QueryValue("InstallInfo", value);
        installInfo = value;
        m_firstTime = false;
    }
    if (installInfo.size()) {
        std::filesystem::path path(installInfo);
        if (std::filesystem::exists(path)) {
            std::filesystem::remove(path);
        }
    }
    key.DeleteSelf();
    #else
    static_assert(false, "Implement MainFrame::DeleteInstallInfo!");
    #endif
}

std::filesystem::path MainFrame::FigureOutGDPath() {
    #ifdef _WIN32
    wxRegKey key(wxRegKey::HKLM, "Software\\WOW6432Node\\Valve\\Steam");
    if (key.HasValue("InstallPath")) {
        wxString value;
        key.QueryValue("InstallPath", value);
        
        while (value.Contains("\\\\")) {
            value.Replace("\\\\", "\\");
        }

        std::filesystem::path firstTest(value.ToStdWstring());
        firstTest /= "steamapps/common/Geometry Dash/GeometryDash.exe";

        if (std::filesystem::exists(firstTest) && std::filesystem::is_regular_file(firstTest)) {
            return firstTest.make_preferred();
        }

        std::filesystem::path configPath(value.ToStdWstring());
        configPath /= "config/config.vdf";

        std::wifstream config(configPath);
        if (config.is_open()) {
            std::wstring rline;
            while (getline(config, rline)) {
                auto line = wxString(rline);
                if (line.Contains(L"BaseInstallFolder_")) {
                    auto val = line.substr(0, line.find_last_of(L'"'));
                    val = val.substr(val.find_last_of(L'"') + 1);

                    while (val.Contains("\\\\")) {
                        val.Replace("\\\\", "\\");
                    }

                    std::filesystem::path test(val.ToStdWstring());
                    test /= "steamapps/common/Geometry Dash/GeometryDash.exe";

                    if (std::filesystem::exists(test) && std::filesystem::is_regular_file(test)) {
                        return test.make_preferred();
                    }
                }
            }
        }
    }
    return "";
    #else
    static_assert(false, "Implement MainFrame::FigureOutGDPath!");
    // If there's no automatic path figure-outing here, just return ""
    #endif
}

OtherModLoaderInfo MainFrame::DetectOtherModLoaders() {
    #ifdef _WIN32
    OtherModLoaderInfo found;
    if (std::filesystem::exists(m_installingTo / "absoluteldr.dll")) {
        found.m_hasMH = 6;
    }
    if (std::filesystem::exists(m_installingTo / "hackproldr.dll")) {
        found.m_hasMH = 7;
    }
    if (std::filesystem::exists(m_installingTo / "ToastedMarshmellow.dll")) {
        found.m_hasGDHM = true;
    }
    if (std::filesystem::exists(m_installingTo / "Geode.dll")) {
        found.m_hasGeode = true;
    }
    if (std::filesystem::exists(m_installingTo / "quickldr.dll")) {
        found.m_others.push_back("QuickLdr");
    }
    if (std::filesystem::exists(m_installingTo / "GDDLLLoader.dll")) {
        found.m_others.push_back("GDDLLLoader");
    }
    if (std::filesystem::exists(m_installingTo / "ModLdr.dll")) {
        found.m_others.push_back("ModLdr");
    }
    if (std::filesystem::exists(m_installingTo / "minhook.x32.dll")) {
        found.m_hasMoreUnknown = true;
    }
    if (std::filesystem::exists(m_installingTo / "XInput9_1_0.dll")) {
        found.m_hasMoreUnknown = true;
    }
    return found;
    #else
    static_assert(false, "Implement MainFrame::DetectOtherModLoaders!");
    // Return a list of known mods if found (if possible, update
    // the page to say "please uninstall other loaders first" if 
    // this platform doesn't have any way of detecting existing ones)
    #endif
}

std::string MainFrame::InstallGeode(wxString const& file) {
    #ifdef _WIN32
    wxFileInputStream fis(file);
    if (!fis.IsOk()) {
        return 
            "Fatal: Unable to open downloaded zip :(\n"
            "Report this bug to the Geode developers; "
            "you may have to manually install Geode if "
            "the problem persists.";
    }
    wxZipInputStream zip(fis);
    if (!zip.IsOk()) {
        return
            "Fatal: Unable to read downloaded zip :(\n"
            "Report this bug to the Geode developers; "
            "you may have to manually install Geode if "
            "the problem persists.";
    }
    m_installations.insert({
        m_installingTo.string(),
        m_targetExeName
    });
    std::unique_ptr<wxZipEntry> entry;
    int ix = 1;
    while (entry.reset(zip.GetNextEntry()), entry) {
        auto path = m_installingTo / entry->GetName().ToStdWstring();
        wxFileName fn;

        if (entry->IsDir()) {
            fn.AssignDir(path.wstring());
        } else {
            fn.Assign(path.wstring());
        }

        if (!wxDirExists(fn.GetPath())) {
            wxFileName::Mkdir(fn.GetPath(), entry->GetMode(), wxPATH_MKDIR_FULL);
        }

        if (entry->IsDir()) continue;

        if (!zip.CanRead()) {
            return 
                "Fatal: Unable to read the zip entry \"" + 
                entry->GetName().ToStdString() + "\" :(\n"
                "Report this bug to the Geode developers; "
                "you may have to manually install Geode if "
                "the problem persists.";
        }

        wxFileOutputStream out(path.wstring());

        if (!out.IsOk()) {
            return 
                "Fatal: Unable to create file \"" + 
                path.string() + "\"; Try running the installer"
                "as Administrator.\n"
                "Report this bug to the Geode developers; "
                "you may have to manually install Geode if "
                "the problem persists.";
        }

        zip.Read(out);

        ix++;
    }
    #else
    static_assert(false, "Implement installation proper for this platform");
    #endif
    return "";
}

bool MainFrame::UninstallGeode(Installation const& inst) {
    #ifdef _WIN32
    std::filesystem::path path(inst.m_path);
    if (std::filesystem::exists(path / "geode")) {
        std::filesystem::remove_all(path / "geode");
    }
    if (std::filesystem::exists(path / "XInput9_1_0.dll")) {
        std::filesystem::remove(path / "XInput9_1_0.dll");
    }
    if (std::filesystem::exists(path / "Geode.dll")) {
        std::filesystem::remove(path / "Geode.dll");
    }
    return true;
    #else
    #error "Implement MainFrame::UninstallGeode"
    #endif
}

bool MainFrame::DeleteSaveData(Installation const& inst) {
    #ifdef _WIN32
    std::filesystem::path path(
        wxStandardPaths::Get().GetUserLocalDataDir().ToStdWstring()
    );
    path = path.parent_path() / std::filesystem::path(inst.m_exe).replace_extension() / "geode";
    if (std::filesystem::exists(path)) {
        std::filesystem::remove_all(path);
        return true;
    }
    return false;
    #else
    #error "Implement MainFrame::DeleteSaveData"
    #endif
}
