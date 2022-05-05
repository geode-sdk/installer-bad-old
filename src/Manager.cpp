#include "Manager.hpp"
#include <fstream>
#include "objc.h"
#include <wx/zipstrm.h>
#include <wx/wfstream.h>
#include <wx/stdpaths.h>
#include "include/json.hpp"

#define INSTALL_DATA_JSON "installer.json"

#ifdef _WIN32

#include <Shlobj_core.h>
#include <wx/msw/registry.h>

#define REGKEY_GEODE "Software\\GeodeSDK"
#define REGSUB_INSTALLATIONS "Installations"
#define REGVAL_SDKDIR "SDKDirectory"
#define REGVAL_INSTPATH "Path"
#define REGVAL_INSTEXE "Exe"

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

Result<> Manager::unzipTo(
    ghc::filesystem::path const& zipLocation,
    ghc::filesystem::path const& targetLocation
) {
    wxFileInputStream fis(zipLocation.wstring());
    if (!fis.IsOk()) {
        return Err("Unable to open zip");
    }
    wxZipInputStream zip(fis);
    if (!zip.IsOk()) {
        return Err("Unable to read zip");
    }
    std::unique_ptr<wxZipEntry> entry;
    while (entry.reset(zip.GetNextEntry()), entry) {
        auto path = targetLocation / entry->GetName().ToStdWstring();
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
    }
    return Ok();
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
                        return this->webRequest(
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

void Manager::downloadAPI(
    DownloadErrorFunc errorFunc,
    DownloadProgressFunc progressFunc,
    DownloadFinishFunc finishFunc
) {
    this->webRequest(
        "https://api.github.com/repos/geode-sdk/api/releases/latest",
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
                    if (name.find(".geode") != std::string::npos) {
                        return this->webRequest(
                            asset["browser_download_url"].get<std::string>(),
                            true,
                            errorFunc,
                            progressFunc,
                            finishFunc
                        );
                    }
                }
                if (errorFunc) {
                    errorFunc("No .geode file release asset found");
                }
            } catch(std::exception& e) {
                if (errorFunc) {
                    errorFunc("Unable to parse JSON: " + std::string(e.what()));
                }
            }
        }
    );
}


ghc::filesystem::path Manager::getDefaultSDKDirectory() const {
    #ifdef _WIN32

    TCHAR pf[MAX_PATH];
    SHGetSpecialFolderPath(
        nullptr,
        pf,
        CSIDL_PROGRAM_FILES,
        false
    );
    return ghc::filesystem::path(pf) / "GeodeSDK";
    
    #elif defined(__APPLE__)

    FSRef ref;
    OSType folderType = kApplicationSupportFolderType;
    char path[PATH_MAX];

    FSFindFolder(kUserDomain, folderType, kCreateFolder, &ref);
    FSRefMakePath(&ref, (UInt8*)&path, PATH_MAX);

    return ghc::filesystem::path(path) / "GeodeSDK";

    #else
    #error "Implement Manager::getDefaultGeodeDirectory"
    #endif
}

ghc::filesystem::path const& Manager::getSDKDirectory() const {
    return m_sdkDirectory;
}

void Manager::setSDKDirectory(ghc::filesystem::path const& path) {
    m_sdkDirectory = path;
}

std::set<Installation> const& Manager::getInstallations() const {
    return m_installations;
}

bool Manager::isFirstTime() const {
    return !m_dataLoaded;
}


Result<> Manager::loadData() {
    m_sdkDirectory = this->getDefaultSDKDirectory();

    #ifdef _WIN32

    wxRegKey key(wxRegKey::HKLM, REGKEY_GEODE);
    if (key.Exists()) {
        m_dataLoaded = true;
        if (key.HasValue(REGVAL_SDKDIR)) {
            wxString value;
            key.QueryValue(REGVAL_SDKDIR, value);
            m_sdkDirectory = value.ToStdWstring();
            m_sdkInstalled = true;
        }
        if (key.HasSubKey(REGSUB_INSTALLATIONS)) {
            wxRegKey sub(wxRegKey::HKLM, REGKEY_GEODE "\\" REGSUB_INSTALLATIONS);
            wxString subKey;
            long index = 0;
            for (
                auto cont = sub.GetFirstKey(subKey, index);
                cont;
                cont = sub.GetNextKey(subKey, index)
            ) {
                wxRegKey instKey(
                    wxRegKey::HKLM,
                    REGKEY_GEODE "\\" REGSUB_INSTALLATIONS "\\" + subKey
                );
                Installation inst;
                
                wxString value;
                instKey.QueryValue(REGVAL_INSTPATH, value);
                inst.m_path = value.ToStdWstring();
                instKey.QueryValue(REGVAL_INSTEXE, value);
                inst.m_exe = value;
                
                m_installations.insert(inst);
            }
        }
    }

    #else
    #error "Implement Manager::loadData"
    #endif

    return Ok();
}

Result<> Manager::saveData() {
    #ifdef _WIN32

    #define UHHH_WELP(err) \
        return Err(err " - the installer wont be able to uninstall Geode!")

    wxRegKey key(wxRegKey::HKLM, REGKEY_GEODE);
    if (!key.Create()) {
        UHHH_WELP("Unable to create " REGKEY_GEODE);
    }
    if (m_sdkInstalled) {
        if (!key.SetValue(REGVAL_SDKDIR, m_sdkDirectory.wstring())) {
            UHHH_WELP("Unable to save " REGKEY_GEODE "\\" REGVAL_SDKDIR);
        }
    }
    wxRegKey subKey(wxRegKey::HKLM, REGKEY_GEODE "\\" REGSUB_INSTALLATIONS);
    if (!subKey.Create()) {
        UHHH_WELP("Unable to create " REGKEY_GEODE "\\" REGSUB_INSTALLATIONS);
    }
    size_t ix = 0;
    for (auto& inst : m_installations) {
        auto keyName = REGKEY_GEODE "\\" REGSUB_INSTALLATIONS + 
            std::string("\\") + std::to_string(ix);

        wxRegKey instKey(wxRegKey::HKLM, keyName);
        if (!instKey.Create()) {
            UHHH_WELP("Unable to create " + keyName + "");
        }
        if (!instKey.SetValue(REGVAL_INSTPATH, inst.m_path.wstring())) {
            UHHH_WELP("Unable to save " + keyName + "\\" REGVAL_INSTPATH);
        }
        if (!instKey.SetValue(REGVAL_INSTEXE, inst.m_exe)) {
            UHHH_WELP("Unable to save " + keyName + "\\" REGVAL_INSTEXE);
        }
        ix++;
    }

    #else
    #error "Implement Manager::saveData"
    #endif

    return Ok();
}

Result<> Manager::deleteData() {
    #ifdef _WIN32
    wxRegKey key(wxRegKey::HKLM, REGKEY_GEODE);
    if (!key.DeleteSelf()) {
        return Err("Unable to delete registry key");
    }
    #else
    #error "Implement Manager::deleteData"
    #endif

    return Ok();
}


Result<> Manager::installSDK(
    DevBranch branch,
    DownloadErrorFunc errorFunc,
    DownloadProgressFunc progressFunc,
    CloneFinishFunc finishFunc
) {
    return Ok();
}

bool Manager::isSDKInstalled() const {
    return m_sdkInstalled;
}

Result<> Manager::uninstallSDK() {
    if (ghc::filesystem::exists(m_sdkDirectory)) {
        if (!ghc::filesystem::remove_all(m_sdkDirectory)) {
            return Err("Unable to delete Geode directory");
        }
    }
    return Ok();
}


Result<Installation> Manager::installLoaderFor(
    ghc::filesystem::path const& gdExePath,
    ghc::filesystem::path const& zipLocation
) {
    Installation inst;
    inst.m_exe = gdExePath.filename().wstring();
    inst.m_path = gdExePath.parent_path();

    #ifdef _WIN32

    auto ures = this->unzipTo(zipLocation, inst.m_path);
    if (!ures) {
        return Err("Loader unzip error: " + ures.error());
    }

    #elif defined(__APPLE__)
    #error "Do you just unzip the Geode dylib to the GD folder on mac? or where do you put it"
    #else
    static_assert(false, "Implement installation proper for this platform");
    #endif

    m_installations.insert(inst);

    return Ok(inst);
}

Result<> Manager::installAPIFor(
    Installation const& inst,
    ghc::filesystem::path const& zipLocation,
    wxString const& filename
) {
    auto targetDir = inst.m_path / "geode" / "mods";
    if (
        !ghc::filesystem::exists(targetDir) &&
        !ghc::filesystem::create_directories(targetDir)
    ) {
        return Err("Unable to create Geode mods directory under " + targetDir.string());
    }
    try {
        if (!ghc::filesystem::copy_file(
            zipLocation,
            targetDir / filename.ToStdWstring(),
            ghc::filesystem::copy_options::overwrite_existing
        )) {
            return Err("Unable to copy Geode API");
        }
    } catch(std::exception& e) {
        return Err("Unable to copy Geode API: " + std::string(e.what()));
    }
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
