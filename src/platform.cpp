#include "MainFrame.hpp"
#include <fstream>
#include "json.hpp"
#include "objc.h"
#include <wx/zipstrm.h>
#include <wx/wfstream.h>
#include <wx/stdpaths.h>
#ifdef _WIN32
#include <wx/msw/registry.h>
#elif defined(__APPLE__)
#include <CoreServices/CoreServices.h>
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
    #elif defined(__APPLE__)
    FSRef ref;
    OSType folderType = kApplicationSupportFolderType;
    char path[PATH_MAX];

    FSFindFolder(kUserDomain, folderType, kCreateFolder, &ref);
    FSRefMakePath(&ref, (UInt8*)&path, PATH_MAX);

    ghc::filesystem::path parent(path);
    parent = parent / "GeodeSDK";
    std::string installInfo = "";
    if (ghc::filesystem::exists(parent)) {
        m_firstTime = false;
        installInfo = (parent / "GeodeInstallationInfo.json").string();
    }
    #else
    static_assert(false, "Implement MainFrame::LoadInstallInfo!");
    // get json file location
    #endif

    if (installInfo.size()) {
        ghc::filesystem::path path(installInfo);
        if (ghc::filesystem::exists(path)) {
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
}

std::string MainFrame::SaveInstallInfo() {
    #ifdef _WIN32
    auto path = ghc::filesystem::absolute("GeodeInstallationInfo.json");
    #elif defined(__APPLE__)
    FSRef ref;
    OSType folderType = kApplicationSupportFolderType;
    char pth[PATH_MAX];

    FSFindFolder(kUserDomain, folderType, kCreateFolder, &ref);
    FSRefMakePath(&ref, (UInt8*)&pth, PATH_MAX);

    ghc::filesystem::path path(pth);
    path = path / "GeodeSDK";
    if (!ghc::filesystem::exists(path)) {
        ghc::filesystem::create_directory(path);
    }
    path = path / "GeodeInstallationInfo.json";
    #endif

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

    #ifdef _WIN32
    wxRegKey key(wxRegKey::HKLM, "Software\\GeodeSDK");
    if (!key.Create()) {
        return "Unable to create Registry Key - the installer wont be able to uninstall Geode!";
    }
    if (!key.SetValue("InstallInfo", path.wstring())) {
        return "Unable to save Registry Key - the installer wont be able to uninstall Geode!";
    }
    #endif
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
        ghc::filesystem::path path(installInfo);
        if (ghc::filesystem::exists(path)) {
            ghc::filesystem::remove(path);
        }
    }
    key.DeleteSelf();
    #elif __APPLE__
    FSRef ref;
    OSType folderType = kApplicationSupportFolderType;
    char pth[PATH_MAX];

    FSFindFolder(kUserDomain, folderType, kCreateFolder, &ref);
    FSRefMakePath(&ref, (UInt8*)&pth, PATH_MAX);

    ghc::filesystem::path path(pth);
    path = path / "GeodeSDK" / "GeodeInstallationInfo.json";
    if (ghc::filesystem::exists(path)) {
        ghc::filesystem::remove(path);
    }
    #else
    static_assert(false, "Implement MainFrame::DeleteInstallInfo!");
    #endif
}

ghc::filesystem::path MainFrame::FigureOutGDPath() {
    #ifdef _WIN32
    wxRegKey key(wxRegKey::HKLM, "Software\\WOW6432Node\\Valve\\Steam");
    if (key.HasValue("InstallPath")) {
        wxString value;
        key.QueryValue("InstallPath", value);
        
        while (value.Contains("\\\\")) {
            value.Replace("\\\\", "\\");
        }

        ghc::filesystem::path firstTest(value.ToStdWstring());
        firstTest /= "steamapps/common/Geometry Dash/GeometryDash.exe";

        if (ghc::filesystem::exists(firstTest) && ghc::filesystem::is_regular_file(firstTest)) {
            return firstTest.make_preferred();
        }

        ghc::filesystem::path configPath(value.ToStdWstring());
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

                    ghc::filesystem::path test(val.ToStdWstring());
                    test /= "steamapps/common/Geometry Dash/GeometryDash.exe";

                    if (ghc::filesystem::exists(test) && ghc::filesystem::is_regular_file(test)) {
                        return test.make_preferred();
                    }
                }
            }
        }
    }
    return "";
    #elif defined(__APPLE__)
    return figureOutGdPath();
    #else
    static_assert(false, "Implement MainFrame::FigureOutGDPath!");
    // If there's no automatic path figure-outing here, just return ""
    #endif
}

OtherModLoaderInfo MainFrame::DetectOtherModLoaders() {
    OtherModLoaderInfo found;
    #ifdef _WIN32
    if (ghc::filesystem::exists(m_installingTo / "absoluteldr.dll")) {
        found.m_hasMH = 6;
    }
    if (ghc::filesystem::exists(m_installingTo / "hackproldr.dll")) {
        found.m_hasMH = 7;
    }
    if (ghc::filesystem::exists(m_installingTo / "ToastedMarshmellow.dll")) {
        found.m_hasGDHM = true;
    }
    if (ghc::filesystem::exists(m_installingTo / "Geode.dll")) {
        found.m_hasGeode = true;
    }
    if (ghc::filesystem::exists(m_installingTo / "quickldr.dll")) {
        found.m_others.push_back("QuickLdr");
    }
    if (ghc::filesystem::exists(m_installingTo / "GDDLLLoader.dll")) {
        found.m_others.push_back("GDDLLLoader");
    }
    if (ghc::filesystem::exists(m_installingTo / "ModLdr.dll")) {
        found.m_others.push_back("ModLdr");
    }
    if (ghc::filesystem::exists(m_installingTo / "minhook.x32.dll")) {
        found.m_hasMoreUnknown = true;
    }
    if (ghc::filesystem::exists(m_installingTo / "XInput9_1_0.dll")) {
        found.m_hasMoreUnknown = true;
    }
    return found;
    #elif defined(__APPLE__)
    return found; // there are no other conflicts
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
    #elif defined(__APPLE__)
    std::cout << "balls.\n";
    #else
    static_assert(false, "Implement installation proper for this platform");
    #endif
    return "";
}

bool MainFrame::UninstallGeode(Installation const& inst) {
    #ifdef _WIN32
    ghc::filesystem::path path(inst.m_path);
    if (ghc::filesystem::exists(path / "geode")) {
        ghc::filesystem::remove_all(path / "geode");
    }
    if (ghc::filesystem::exists(path / "XInput9_1_0.dll")) {
        ghc::filesystem::remove(path / "XInput9_1_0.dll");
    }
    if (ghc::filesystem::exists(path / "Geode.dll")) {
        ghc::filesystem::remove(path / "Geode.dll");
    }
    return true;
    #elif defined(__APPLE__)
    std::cout << "balls 2\n";
    #else
    #error "Implement MainFrame::UninstallGeode"
    #endif
}

bool MainFrame::DeleteSaveData(Installation const& inst) {
    #ifdef _WIN32
    ghc::filesystem::path path(
        wxStandardPaths::Get().GetUserLocalDataDir().ToStdWstring()
    );
    path = path.parent_path() / ghc::filesystem::path(inst.m_exe).replace_extension() / "geode";
    if (ghc::filesystem::exists(path)) {
        ghc::filesystem::remove_all(path);
        return true;
    }
    return false;
    #elif defined(__APPLE__)
    std::cout << "cocks\n";
    #else
    #error "Implement MainFrame::DeleteSaveData"
    #endif
}
