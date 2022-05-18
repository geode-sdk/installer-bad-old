#include "Manager.hpp"
#include <fstream>
#include "objc.h"
#include <wx/zipstrm.h>
#include <wx/wfstream.h>
#include <wx/stdpaths.h>
#include <thread>
#include <chrono>
#include <algorithm>

#define INSTALL_DATA_JSON "config.json"
#define GEODE_DIR "Geode"
#define GEODE_SUITE_ENV "GEODE_SUITE"

#ifdef _WIN32

#include <Shlobj_core.h>
#include <wx/msw/registry.h>

#define PLATFORM_ASSET_IDENTIFIER "win"
#define PLATFORM_NAME "Windows"

#elif defined(__APPLE__)

#include <dlfcn.h>
#include <CoreServices/CoreServices.h>
#include "objc.h"
#define PLATFORM_ASSET_IDENTIFIER "mac"
#define PLATFORM_NAME "MacOS"

#else
#warning "Define PLATFORM_ASSET_IDENTIFIER & PLATFORM_NAME"
#endif

wxDEFINE_EVENT(CALL_ON_MAIN, CallOnMainEvent);

Manager* Manager::get() {
    static auto m = new Manager;
    return m;
}

void Manager::onSyncThreadCall(CallOnMainEvent& e) {
    e.invoke();
}

void Manager::addInstallation(Installation const& inst) {
    auto old = std::find(m_installations.begin(), m_installations.end(), inst);
    if (old != m_installations.end()) {
        *old = inst;
    } else {
        m_installations.push_back(inst);
    }
}

Result<> Manager::addSuiteEnv() {
    #ifdef _WIN32

    wxRegKey key(wxRegKey::HKLM, "System\\CurrentControlSet\\Control\\Session Manager\\Environment");
    if (!key.SetValue(GEODE_SUITE_ENV, m_suiteDirectory.wstring())) {
        return Err("Unable to set " GEODE_SUITE_ENV " environment variable");
    }
    SendMessageTimeout(
        HWND_BROADCAST,
        WM_SETTINGCHANGE,
        0,
        reinterpret_cast<LPARAM>(L"Environment"),
        SMTO_ABORTIFHUNG,
        5000,
        nullptr
    );
    return Ok();

    #else

    // nothing we rly need to do
    return Ok();

    #endif
}


InstallerMode Manager::getInstallerMode() const {
    return m_mode;
}

ghc::filesystem::path const& Manager::getLoaderUpdatePath() const {
    return m_loaderUpdatePath;
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
        [errorFunc, progressFunc, finishFunc](wxWebRequestEvent& evt) -> void {
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

void Manager::downloadCLI(
    DownloadErrorFunc errorFunc,
    DownloadProgressFunc progressFunc,
    DownloadFinishFunc finishFunc
) {
    this->webRequest(
        "https://api.github.com/repos/geode-sdk/cli/releases/latest",
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

void Manager::checkForUpdates(
    Installation const& installation,
    DownloadErrorFunc errorFunc,
    UpdateCheckFinishFunc finishFunc
) {
    auto installedVersion = VersionInfo(0, 0, 0);

    #ifdef _WIN32
    auto exe = (installation.m_path / "Geode.dll").wstring();

    DWORD verHandle = 0;
    unsigned int size = 0;
    LPBYTE lpBuffer = nullptr;
    auto verSize = GetFileVersionInfoSizeW(
        exe.c_str(),
        &verHandle
    );

    if (verSize != 0) {
        auto verData = new char[verSize];
        if (GetFileVersionInfo(exe.c_str(), verHandle, verSize, verData)) {
            if (VerQueryValue(verData, L"\\", (VOID FAR* FAR*)&lpBuffer, &size)) {
                if (size) {
                    VS_FIXEDFILEINFO *verInfo = (VS_FIXEDFILEINFO *)lpBuffer;
                    if (verInfo->dwSignature == 0xfeef04bd) {
                        installedVersion = {
                            ( verInfo->dwFileVersionMS >> 16 ) & 0xffff,
                            ( verInfo->dwFileVersionMS >>  0 ) & 0xffff,
                            ( verInfo->dwFileVersionLS >> 16 ) & 0xffff
                        };
                    }
                }
            }
        }
        delete[] verData;
    }
    #else
    return finishFunc(
        installedVersion,
        { 0, 0, 0 }
    );
    #endif

    this->webRequest(
        "https://api.github.com/repos/geode-sdk/loader/releases/latest",
        false,
        errorFunc,
        nullptr,
        [this, errorFunc, finishFunc, installedVersion](
            wxWebResponse const& res
        ) -> void {
            try {
                auto json = nlohmann::json::parse(res.AsString());
                auto availableVersion = VersionInfo(json["tag_name"].get<std::string>());
                finishFunc(installedVersion, availableVersion);
            } catch(std::exception& e) {
                if (errorFunc) {
                    errorFunc("Unable to parse JSON: " + std::string(e.what()));
                }
            }
        }
    );
}

void Manager::checkCLIForUpdates(
    DownloadErrorFunc errorFunc,
    UpdateCheckFinishFunc finishFunc
) {
    auto installedVersion = VersionInfo(0, 0, 0);

    #ifdef _WIN32
    auto exe = (m_binDirectory / "Geode.exe").wstring();

    DWORD verHandle = 0;
    unsigned int size = 0;
    LPBYTE lpBuffer = nullptr;
    auto verSize = GetFileVersionInfoSizeW(
        exe.c_str(),
        &verHandle
    );

    if (verSize != 0) {
        auto verData = new char[verSize];
        if (GetFileVersionInfo(exe.c_str(), verHandle, verSize, verData)) {
            if (VerQueryValue(verData, L"\\", (VOID FAR* FAR*)&lpBuffer, &size)) {
                if (size) {
                    VS_FIXEDFILEINFO *verInfo = (VS_FIXEDFILEINFO *)lpBuffer;
                    if (verInfo->dwSignature == 0xfeef04bd) {
                        installedVersion = {
                            ( verInfo->dwFileVersionMS >> 16 ) & 0xffff,
                            ( verInfo->dwFileVersionMS >>  0 ) & 0xffff,
                            ( verInfo->dwFileVersionLS >> 16 ) & 0xffff
                        };
                    }
                }
            }
        }
        delete[] verData;
    }
    #else
    return finishFunc(
        installedVersion,
        { 0, 0, 0 }
    );
    #endif

    this->webRequest(
        "https://api.github.com/repos/geode-sdk/cli/releases/latest",
        false,
        errorFunc,
        nullptr,
        [this, errorFunc, finishFunc, installedVersion](
            wxWebResponse const& res
        ) -> void {
            try {
                auto json = nlohmann::json::parse(res.AsString());
                auto availableVersion = VersionInfo(json["tag_name"].get<std::string>());
                finishFunc(installedVersion, availableVersion);
            } catch(std::exception& e) {
                if (errorFunc) {
                    errorFunc("Unable to parse JSON: " + std::string(e.what()));
                }
            }
        }
    );
}


ghc::filesystem::path const& Manager::getDataDirectory() const {
    return m_dataDirectory;
}

ghc::filesystem::path Manager::getDefaultDataDirectory() const {
    #ifdef _WIN32

    // parent_path because wxWidgets returns 
    // %localappdata%/%appname%
    return ghc::filesystem::path(
        wxStandardPaths::Get().GetUserLocalDataDir().ToStdWstring()
    ).parent_path() / GEODE_DIR;
    
    #elif defined(__APPLE__)

    return "/Users/Shared/" GEODE_DIR; 

    #endif
}


ghc::filesystem::path const& Manager::getBinDirectory() const {
    return m_binDirectory;
}

ghc::filesystem::path Manager::getDefaultBinDirectory() const {
    return this->getDefaultDataDirectory() / "bin";
}


ghc::filesystem::path const& Manager::getSuiteDirectory() const {
    return m_suiteDirectory;
}

ghc::filesystem::path Manager::getDefaultSuiteDirectory() const {
    return this->getDefaultDataDirectory() / "suite";
}


void Manager::setSuiteDirectory(ghc::filesystem::path const& path) {
    m_suiteDirectory = path;
}

std::vector<Installation> const& Manager::getInstallations() const {
    return m_installations;
}

size_t Manager::getDefaultInstallation() const {
    return m_defaultInstallation;
}

bool Manager::isFirstTime() const {
    return !m_dataLoaded;
}


Result<> Manager::loadData() {
    m_suiteDirectory = this->getDefaultSuiteDirectory();
    m_dataDirectory = this->getDefaultDataDirectory();
    m_binDirectory = this->getDefaultBinDirectory();

    auto configFile = m_dataDirectory / INSTALL_DATA_JSON;

    auto suite = getenv(GEODE_SUITE_ENV);
    if (suite != nullptr) {
        m_suiteDirectory = suite;
    }
    m_suiteInstalled = suite;

    if (!wxFile::Exists(configFile.wstring())) {
        return Ok();
    }

    m_dataLoaded = true;

    wxFile file;
    file.Open(configFile.wstring());

    wxString x;
    file.ReadAll(&x);

    try {
        auto json = nlohmann::json::parse(std::string(x));
        m_loadedConfigJson = json;

        for (auto install : json["installations"]) {
            Installation inst;
            inst.m_path = std::string(install["path"]);
            inst.m_exe = std::string(install["executable"]);
            inst.m_branch =
                install.contains("nightly") && install["nightly"].get<bool>() ?
                DevBranch::Nightly : DevBranch::Stable;
            this->addInstallation(inst);
        }

        if (json.contains("default-installation")) {
            m_defaultInstallation = json["default-installation"];
        }
    } catch(std::exception& e) {
        return Err("Unable to parse " INSTALL_DATA_JSON ": " + std::string(e.what()));
    }

    return Ok();
}

Result<> Manager::saveData() {
    if (!ghc::filesystem::exists(m_dataDirectory)) {
        ghc::filesystem::create_directories(m_dataDirectory);
    }

    std::ofstream ofs(m_dataDirectory / INSTALL_DATA_JSON);

    if (m_installations.size()) {
        m_loadedConfigJson["default-installation"] = m_defaultInstallation;
    }

    m_loadedConfigJson["installations"] = nlohmann::json::array();
    for (auto x : m_installations) {
        nlohmann::json inst;
        inst["path"] = x.m_path.string();
        inst["executable"] = x.m_exe;
        inst["nightly"] = x.m_branch == DevBranch::Nightly;
        m_loadedConfigJson["installations"].push_back(inst);
    }

    ofs << m_loadedConfigJson.dump(4); 
    ofs.close();

    return Ok();
}

Result<> Manager::deleteData() {
    try {
        if (!ghc::filesystem::remove_all(m_dataDirectory)) {
            return Err("Unable to delete data");
        }
    } catch(std::exception& e) {
        return Err("Error deleting data: " + std::string(e.what()));
    }
    return Ok();
}


Result<> Manager::installCLI(
    ghc::filesystem::path const& cliZipPath
) {
    auto targetDir = m_binDirectory;
    if (
        !ghc::filesystem::exists(targetDir) &&
        !ghc::filesystem::create_directories(targetDir)
    ) {
        return Err("Unable to create directory " + targetDir.string());
    }
    return this->unzipTo(cliZipPath, targetDir);
}

Result<> Manager::addCLIToPath() {
    #ifdef _WIN32
    wxRegKey key(wxRegKey::HKLM, "System\\CurrentControlSet\\Control\\Session Manager\\Environment");
    wxString path;
    if (!key.QueryValue("Path", path)) {
        return Err("Unable to read Path environment variable");
    }
    auto toAdd = m_binDirectory.wstring() + ";";
    if (path.Contains(toAdd)) {
        return Ok();
    }
    path += toAdd;
    if (!key.SetValue("Path", path)) {
        return Err("Unable to save Path environment variable");
    }
    SendMessageTimeout(
        HWND_BROADCAST,
        WM_SETTINGCHANGE,
        0,
        reinterpret_cast<LPARAM>(L"Environment"),
        SMTO_ABORTIFHUNG,
        5000,
        nullptr
    );
    return Ok();
    #else
    return Ok();
    // changing environment variables??? in *this* political landscape???
    #endif
}

bool Manager::isGeodeUtilsInstalled() const {
    #ifdef _WIN32
    return ghc::filesystem::exists(m_binDirectory / "geodeutils.dll");
    #else
    return ghc::filesystem::exists(m_binDirectory / "libgeodeutils.dylib");
    #endif
}

void* Manager::loadFunctionFromUtilsLib(const char* name) {
    #if _WIN32
    auto lib = LoadLibraryW((m_binDirectory / "geodeutils.dll").wstring().c_str());
    if (!lib) return nullptr;
    return GetProcAddress(lib, name);
    #else
    auto lib = dlopen((m_binDirectory / "libgeodeutils.dylib").string().c_str(), RTLD_LAZY);
    if (!lib) return nullptr;
    return dlsym(lib, name);
    #endif
}

Result<> Manager::installSuite(
    DevBranch branch,
    DownloadErrorFunc errorFunc,
    DownloadProgressFunc progressFunc,
    CloneFinishFunc finishFunc
) {
    if (!this->isGeodeUtilsInstalled()) {
        return Err("Geode CLI seems to not have been installed");
    }

    this->Bind(CALL_ON_MAIN, &Manager::onSyncThreadCall, this);

    std::thread t([this, branch, errorFunc, progressFunc, finishFunc]() -> void {
        auto throwError = [errorFunc, this](std::string const& msg) -> void {
            wxQueueEvent(this, new CallOnMainEvent(
                [errorFunc, msg]() -> void {
                    if (errorFunc) errorFunc(msg);
                },
                CALL_ON_MAIN,
                wxID_ANY
            ));
        };

        auto installSuite = utilsFunc<cli::geode_install_suite>("geode_install_suite");

        if (
            !ghc::filesystem::exists(m_suiteDirectory) &&
            !ghc::filesystem::create_directories(m_suiteDirectory)
        ) {
            throwError("Unable to create directory at " + m_suiteDirectory.string());
            return;
        }

        static DownloadProgressFunc progFunc;
        progFunc = progressFunc;

        auto res = installSuite(
            m_suiteDirectory.string().c_str(),
            branch == DevBranch::Nightly,
            [](const char* status, int per) -> void {
                // limit window update rate
                static auto time = std::chrono::high_resolution_clock::now();
                auto now = std::chrono::high_resolution_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - time).count() > 300) {
                    wxQueueEvent(Manager::get(), new CallOnMainEvent(
                        [status, per]() -> void {
                            if (progFunc) progFunc(status, per);
                        },
                        CALL_ON_MAIN,
                        wxID_ANY
                    ));
                    time = now;
                }
            }
        );
        if (res) {
            throwError(res);
        } else {
            wxQueueEvent(Manager::get(), new CallOnMainEvent(
                [this, finishFunc]() -> void {
                    m_suiteInstalled = true;
                    this->addSuiteEnv();
                    if (finishFunc) finishFunc();
                },
                CALL_ON_MAIN,
                wxID_ANY
            ));
        }
    });
    t.detach();
    return Ok();
}

bool Manager::isSuiteInstalled() const {
    return m_suiteInstalled && ghc::filesystem::exists(m_suiteDirectory);
}

Result<> Manager::uninstallSuite() {
    try {
        if (
            ghc::filesystem::exists(m_suiteDirectory) &&
            !ghc::filesystem::remove_all(m_suiteDirectory)
        ) {
            return Err("Unable to delete the Geode Suite directory");
        }
    } catch(std::exception& e) {
        return Err("Error deleting data: " + std::string(e.what()));
    }
    #ifdef _WIN32
    wxRegKey key(wxRegKey::HKLM, "System\\CurrentControlSet\\Control\\Session Manager\\Environment");
    wxString path;
    if (!key.QueryValue("Path", path)) {
        return Err("Unable to read Path environment variable");
    }
    auto toRemove = (m_binDirectory).wstring() + ";";
    path.Replace(toRemove, "");
    if (!key.SetValue("Path", path)) {
        return Err("Unable to save Path environment variable");
    }
    SendMessageTimeout(
        HWND_BROADCAST,
        WM_SETTINGCHANGE,
        0,
        reinterpret_cast<LPARAM>(L"Environment"),
        SMTO_ABORTIFHUNG,
        5000,
        nullptr
    );
    #else
    // no environment variable lol
    #endif
    return Ok();
}


void Manager::installGeodeUtilsLib(
    bool update,
    DevBranch branch,
    DownloadErrorFunc errorFunc,
    DownloadProgressFunc progressFunc,
    CloneFinishFunc finishFunc
) {
    if (!update && this->isGeodeUtilsInstalled()) {
        return finishFunc();
    }
    this->webRequest(
    #ifdef _WIN32
    branch == DevBranch::Nightly ? 
        "https://github.com/geode-sdk/suite/raw/nightly/windows/geodeutils.dll" : 
        "https://github.com/geode-sdk/suite/raw/main/windows/geodeutils.dll",
    #elif defined(__APPLE__)
    branch == DevBranch::Nightly ? 
        "https://github.com/geode-sdk/suite/raw/nightly/macos/libgeodeutils.dylib" :
        "https://github.com/geode-sdk/suite/raw/main/macos/libgeodeutils.dylib",
    #else
        #error "Define download URL for geodeutils"
    #endif
        true,
        errorFunc,
        progressFunc,
        [this, errorFunc, finishFunc](wxWebResponse const& res) -> void {
            try {
                if (
                    !ghc::filesystem::exists(m_binDirectory) &&
                    !ghc::filesystem::create_directories(m_binDirectory)
                ) {
                    return errorFunc("Unable to create directory at " + m_binDirectory.string());
                }
                if (!ghc::filesystem::copy_file(
                    res.GetDataFile().ToStdWstring(),
                    m_binDirectory / res.GetSuggestedFileName().ToStdWstring(),
                    ghc::filesystem::copy_options::overwrite_existing
                )) {
                    return errorFunc("Unable to copy geodeutils dll!");
                }
                finishFunc();
            } catch(std::exception& e) {
                return errorFunc(e.what());
            }
        }
    );
}

Result<> Manager::installGeodeFor(
    ghc::filesystem::path const& gdExePath,
    DevBranch branch,
    DownloadErrorFunc errorFunc,
    DownloadProgressFunc progressFunc,
    CloneFinishFunc finishFunc
) {
    if (!this->isGeodeUtilsInstalled()) {
        return Err("Geode Utility Library seems to not have been installed");
    }

    this->Bind(CALL_ON_MAIN, &Manager::onSyncThreadCall, this);

    std::thread t([this, gdExePath, branch, errorFunc, progressFunc, finishFunc]() -> void {
        auto throwError = [errorFunc, this](std::string const& msg) -> void {
            wxQueueEvent(this, new CallOnMainEvent(
                [errorFunc, msg]() -> void {
                    if (errorFunc) errorFunc(msg);
                },
                CALL_ON_MAIN,
                wxID_ANY
            ));
        };

        auto installGeode = utilsFunc<cli::geode_install_geode>("geode_install_geode");

        static DownloadProgressFunc progFunc;
        progFunc = progressFunc;

        auto res = installGeode(
            gdExePath.string().c_str(),
            branch == DevBranch::Nightly,
            true,
            [](const char* status, int per) -> void {
                wxQueueEvent(Manager::get(), new CallOnMainEvent(
                    [status, per]() -> void {
                        if (progFunc) progFunc(status, per);
                    },
                    CALL_ON_MAIN,
                    wxID_ANY
                ));
            }
        );
        if (res) {
            throwError(res);
        } else {
            wxQueueEvent(Manager::get(), new CallOnMainEvent(
                [finishFunc]() -> void {
                    if (finishFunc) finishFunc();
                },
                CALL_ON_MAIN,
                wxID_ANY
            ));
        }
    });
    t.detach();
    return Ok();
}

Result<> Manager::uninstallGeodeFrom(Installation const& inst) {
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
    std::cout << "me when the...\n";
    return Ok();
    #else
    #warning "Implement MainFrame::UninstallGeode"
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

    FSRef ref;
    char path[PATH_MAX];
    FSFindFolder( kUserDomain, kApplicationSupportFolderType, kCreateFolder, &ref );
    FSRefMakePath( &ref, (UInt8*)&path, PATH_MAX );
    ghc::filesystem::path appSupport(path);
    appSupport = appSupport / "GeometryDash" / "geode";

    if (ghc::filesystem::exists(appSupport)) {
        ghc::filesystem::remove_all(appSupport);
        return Ok();
    }
    return Err("Save data directory not found!");
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

    return FigureOutGDPathMac();
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
    #endif
}

void Manager::launch(ghc::filesystem::path const& path) {
    wxExecuteEnv env;
    env.cwd = path.parent_path().wstring();
    if (!wxExecute(path.wstring(), 0, nullptr, &env)) {
        wxMessageBox(
            "Unable to automatically restart GD, please "
            "open the game yourself.",
            "Error Starting GD",
            wxICON_ERROR
        );
    }
}

bool Manager::needRequestAdminPriviledges() const {
    #ifdef _WIN32

    auto ret = true;
    HANDLE token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION Elevation;
        DWORD cbSize = sizeof( TOKEN_ELEVATION );
        if (GetTokenInformation(token, TokenElevation, &Elevation, sizeof(Elevation), &cbSize)) {
            ret = !Elevation.TokenIsElevated;
        }
    }
    if (token) {
        CloseHandle(token);
    }
    return ret;

    #else
    
    return false;

    #endif
}
