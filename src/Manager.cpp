#include "Manager.hpp"
#include <fstream>
#include "include/json.hpp"
#include "objc.h"
#include <wx/zipstrm.h>
#include <wx/wfstream.h>
#include <wx/stdpaths.h>

#define INSTALL_DATA_JSON "installer.json"

#ifdef _WIN32

#include <Shlobj_core.h>
#include <wx/msw/registry.h>

#define REGKEY_GEODE "Software\\GeodeSDK"
#define REGVAL_INSTALLDIR "InstallDirectory"

#define PLATFORM_ASSET_IDENTIFIER "win"
#define PLATFORM_NAME "Windows"

#elif defined(__APPLE__)

#include <CoreServices/CoreServices.h>

#define PLATFORM_ASSET_IDENTIFIER "mac"
#define PLATFORM_NAME "MacOS"

#else
#error "Define PLATFORM_ASSET_IDENTIFIER & PLATFORM_NAME"
#endif

Manager* Manager::get() {
    static auto m = new Manager;
    return m;
}


void Manager::webRequest(
    std::string const& url,
    bool downloadFile,
    DownloadErrorFunc errorFunc,
    DownloadProgressFunc progressFunc,
    DownloadFinishFunc finishFunc
) {
    auto request = wxWebSession::GetDefault().CreateRequest(this, url);
    if (!request.IsOk()) {
        if (!errorFunc) return;
        return errorFunc("Unable to create web request");
    }
    if (downloadFile) {
        request.SetStorage(wxWebRequest::Storage_File);
    }
    this->Bind(
        wxEVT_WEBREQUEST_STATE,
        [errorFunc, progressFunc, finishFunc](wxWebRequestEvent& evt)
     -> void {
        switch (evt.GetState()) {
            case wxWebRequest::State_Completed: {
                auto res = evt.GetResponse();
                if (!res.IsOk()) {
                    if (!errorFunc) return;
                    return errorFunc("Web request returned not OK");
                }
                if (res.GetStatus() != 200) {
                    if (!errorFunc) return;
                    return errorFunc("Web request returned " + std::to_string(res.GetStatus()));
                }
                if (finishFunc) finishFunc(res);
            } break;

            case wxWebRequest::State_Active: {
                if (!progressFunc) return;
                if (evt.GetRequest().GetBytesExpectedToReceive() == -1) {
                    return progressFunc("Beginning download", 0);
                }
                progressFunc(
                    "Downloading",
                    static_cast<int>(
                        static_cast<double>(evt.GetRequest().GetBytesReceived()) /
                        evt.GetRequest().GetBytesExpectedToReceive() * 100.0
                    )
                );
            } break;

            case wxWebRequest::State_Idle: {
                if (progressFunc) progressFunc("Waiting", 0);
            } break;

            case wxWebRequest::State_Unauthorized: {
                if (errorFunc) errorFunc("Unauthorized to do web request");
            } break;

            case wxWebRequest::State_Failed: {
                if (errorFunc) errorFunc("Web request failed");
            } break;

            case wxWebRequest::State_Cancelled: {
                if (errorFunc) errorFunc("Web request cancelled");
            } break;
        }
    });
    request.Start();
}

void Manager::downloadLoader(
    DownloadErrorFunc errorFunc,
    DownloadProgressFunc progressFunc,
    DownloadFinishFunc finishFunc
) {
    this->webRequest(
        "https://api.github.com/repos/geode-sdk/loader/releases/latest",
        false,
        errorFunc,
        nullptr,
        [this, errorFunc, progressFunc, finishFunc](wxWebResponse const& res) -> void {
            try {
                auto json = nlohmann::json::parse(res.AsString());

                auto tagName = json["tag_name"].get<std::string>();
                if (progressFunc) progressFunc("Downloading version " + tagName, 0);

                for (auto& asset : json["assets"]) {
                    auto name = asset["name"].get<std::string>();
                    if (name.find(PLATFORM_ASSET_IDENTIFIER) != std::string::npos) {
                        this->webRequest(
                            asset["browser_download_url"].get<std::string>(),
                            true,
                            errorFunc,
                            progressFunc,
                            finishFunc
                        );
                    }
                }
                if (errorFunc) {
                    errorFunc("No release asset for " PLATFORM_NAME " found");
                }
            } catch(std::exception& e) {
                if (errorFunc) {
                    errorFunc("Unable to parse JSON: " + std::string(e.what()));
                }
            }
        }
    );
}


ghc::filesystem::path const& Manager::getGeodeDirectory() const {
    return m_geodeDirectory;
}

std::set<Installation> const& Manager::getInstallations() const {
    return m_installations;
}

bool Manager::isFirstTime() const {
    return !m_geodeDirectorySet;
}


Result<> Manager::loadData() {
    #ifdef _WIN32

    wxRegKey key(wxRegKey::HKLM, REGKEY_GEODE);
    if (key.HasValue(REGVAL_INSTALLDIR)) {
        wxString value;
        key.QueryValue(REGVAL_INSTALLDIR, value);
        m_geodeDirectory = value.ToStdWstring();
        m_geodeDirectorySet = true;
    } else {
        TCHAR pf[MAX_PATH];
        SHGetSpecialFolderPath(
            nullptr,
            pf,
            CSIDL_PROGRAM_FILES,
            false
        );
        m_geodeDirectory = ghc::filesystem::path(pf) / "GeodeSDK";
        m_geodeDirectorySet = false;
    }

    #elif defined(__APPLE__)

    FSRef ref;
    OSType folderType = kApplicationSupportFolderType;
    char path[PATH_MAX];

    FSFindFolder(kUserDomain, folderType, kCreateFolder, &ref);
    FSRefMakePath(&ref, (UInt8*)&path, PATH_MAX);

    ghc::filesystem::path parent(path);
    parent = parent / "GeodeSDK";
    std::string geodeDirectory = "";
    if (ghc::filesystem::exists(parent)) {
        m_geodeDirectorySet = true;
        m_geodeDirectory = parent;
    }

    #else
    static_assert(false, "Implement Manager::LoadData!");
    // get Geode dir location or set default
    #endif

    auto installJsonPath = m_geodeDirectory / INSTALL_DATA_JSON;
    if (ghc::filesystem::exists(installJsonPath)) {
        std::ifstream file(installJsonPath);
        if (!file.is_open()) return Err("Unable to save installation info");
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
            return Err("Unable to load installation info: " + std::string(e.what()));
        }
    }
    
    return Ok();
}

Result<> Manager::saveData() {
    ghc::filesystem::path installJsonPath = m_geodeDirectory / INSTALL_DATA_JSON;

    auto json = nlohmann::json::object();
    try {
        json["installations"] = nlohmann::json::array();
        for (auto& i : m_installations) {
            auto obj = nlohmann::json::object();
            obj["path"] = i.m_path.string();
            obj["exe"] = i.m_exe.ToStdString();
            json["installations"].push_back(obj);
        }
    } catch(...) {}

    {
        std::wofstream file(installJsonPath);
        if (!file.is_open()) return Err(
            "Unable to save installation data - "
            "the installer will be unable to uninstall Geode!"
        );
        file << json.dump(4);
    }

    #ifdef _WIN32
    wxRegKey key(wxRegKey::HKLM, "Software\\GeodeSDK");
    if (!key.Create()) {
        return Err(
            "Unable to create Registry Key - "
            "the installer wont be able to uninstall Geode!"
        );
    }
    if (!key.SetValue("InstallInfo", installJsonPath.wstring())) {
        return Err(
            "Unable to save Registry Key - "
            "the installer wont be able to uninstall Geode!"
        );
    }
    #endif

    return Ok();
}

Result<> Manager::deleteData() {
    #ifdef _WIN32
    wxRegKey key(wxRegKey::HKLM, REGKEY_GEODE);
    if (!key.DeleteSelf()) {
        return Err("Unable to delete registry key");
    }
    #endif

    if (ghc::filesystem::exists(m_geodeDirectory)) {
        if (!ghc::filesystem::remove_all(m_geodeDirectory)) {
            return Err("Unable to delete Geode directory");
        }
    }
    return Ok();
}


Result<> Manager::installFor(
    ghc::filesystem::path const& gdExePath,
    ghc::filesystem::path const& zipLocation
) {
    #ifdef _WIN32

    Installation inst;
    inst.m_exe = gdExePath.filename().wstring();
    inst.m_path = gdExePath.parent_path();

    wxFileInputStream fis(zipLocation.wstring());
    if (!fis.IsOk()) {
        return Err("Unable to open loader zip");
    }
    wxZipInputStream zip(fis);
    if (!zip.IsOk()) {
        return Err("Unable to read loader zip");
    }
    m_installations.insert(inst);
    std::unique_ptr<wxZipEntry> entry;
    int ix = 1;
    while (entry.reset(zip.GetNextEntry()), entry) {
        auto path = inst.m_path / entry->GetName().ToStdWstring();
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
            return Err(
                "Unable to read the zip entry \"" +
                entry->GetName().ToStdString() + "\""
            );
        }

        wxFileOutputStream out(path.wstring());

        if (!out.IsOk()) {
            return Err("Unable to create file \"" + path.string() + "\"");
        }

        zip.Read(out);

        ix++;
    }

    #elif defined(__APPLE__)
    std::cout << "balls.\n";
    #else
    static_assert(false, "Implement installation proper for this platform");
    #endif

    return Ok();
}

Result<> Manager::uninstallFrom(Installation const& inst) {
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
    return Ok();

    #elif defined(__APPLE__)
    std::cout << "balls 2\n";
    #else
    #error "Implement MainFrame::UninstallGeode"
    #endif
}

Result<> Manager::deleteSaveDataFrom(Installation const& inst) {
    #ifdef _WIN32

    ghc::filesystem::path path(
        wxStandardPaths::Get().GetUserLocalDataDir().ToStdWstring()
    );
    path = path.parent_path() / ghc::filesystem::path(inst.m_exe.ToStdString()).replace_extension() / "geode";
    if (ghc::filesystem::exists(path)) {
        ghc::filesystem::remove_all(path);
        return Ok();
    }
    return Err("Save data directory not found!");

    #elif defined(__APPLE__)
    std::cout << "cocks\n";
    #else
    #error "Implement MainFrame::DeleteSaveData"
    #endif
}


std::optional<ghc::filesystem::path> Manager::findDefaultGDPath() const {
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
    return std::nullopt;

    #elif defined(__APPLE__)

    return figureOutGdPath();

    #else
    
    static_assert(false, "Implement Manager::FindDefaultGDPath!");
    // If there's no automatic path figure-outing here, just return ""
    
    #endif
}

int Manager::doesDirectoryContainOtherMods(
    ghc::filesystem::path const& path
) const {
    int flags = OMF_None;

    #ifdef _WIN32

    if (ghc::filesystem::exists(path / "absoluteldr.dll")) {
        flags |= OMF_MHv6;
    }
    if (ghc::filesystem::exists(path / "hackproldr.dll")) {
        flags |= OMF_MHv7;
    }
    if (ghc::filesystem::exists(path / "ToastedMarshmellow.dll")) {
        flags |= OMF_GDHM;
    }
    if (
        ghc::filesystem::exists(path / "Geode.dll") ||
        ghc::filesystem::exists(path / "quickldr.dll") ||
        ghc::filesystem::exists(path / "GDDLLLoader.dll") ||
        ghc::filesystem::exists(path / "ModLdr.dll") ||
        ghc::filesystem::exists(path / "minhook.dll") ||
        ghc::filesystem::exists(path / "XInput9_1_0.dll")
    ) {
        flags |= OMF_Some;
    }
    return flags;

    #elif defined(__APPLE__)

    return flags; // there are no other conflicts

    #else
    static_assert(false, "Implement MainFrame::DetectOtherModLoaders!");
    // Return a list of known mods if found (if possible, update
    // the page to say "please uninstall other loaders first" if 
    // this platform doesn't have any way of detecting existing ones)
    #endif
}
