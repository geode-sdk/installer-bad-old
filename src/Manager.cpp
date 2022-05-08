#include "Manager.hpp"
#include <fstream>
#include "objc.h"
#include <wx/zipstrm.h>
#include <wx/wfstream.h>
#include <wx/stdpaths.h>
#include "include/json.hpp"
#include <thread>
#include <chrono>
#include <algorithm>

#define INSTALL_DATA_JSON "installer.json"

#ifdef _WIN32

#include <Shlobj_core.h>
#include <wx/msw/registry.h>

#define REGKEY_GEODE "Software\\GeodeSDK"
#define REGSUB_INSTALLATIONS "Installations"
#define REGVAL_SDKDIR "SDKDirectory"
#define REGVAL_DEFINST "Default"
#define REGVAL_INSTPATH "Path"
#define REGVAL_INSTEXE "Exe"
#define REGVAL_INSTVERSION "Version"
#define REGVAL_INSTAPIVERSION "APIVersion"

#define ENVVAR_SDK "GEODE_SUITE"

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


InstallerMode Manager::getInstallerMode() const {
    return m_mode;
}

ghc::filesystem::path const& Manager::getLoaderUpdatePath() const {
    return m_loaderUpdatePath;
}


Result<> Manager::finishSDKInstallation() {
    m_sdkInstalled = true;

    #ifdef _WIN32

    wxRegKey key(wxRegKey::HKLM, "System\\CurrentControlSet\\Control\\Session Manager\\Environment");
    if (!key.SetValue(ENVVAR_SDK, (m_sdkDirectory / "suite").wstring())) {
        return Err("Unable to set " ENVVAR_SDK " environment variable");
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

void Manager::webRequest(
    std::string const& url,
    std::string const& version,
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
        [version, errorFunc, progressFunc, finishFunc](wxWebRequestEvent& evt) -> void {
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
                if (finishFunc) finishFunc(res, version);
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
        "",
        false,
        errorFunc,
        nullptr,
        [this, errorFunc, progressFunc, finishFunc](
            wxWebResponse const& res, std::string const&
        ) -> void {
            try {
                auto json = nlohmann::json::parse(res.AsString());

                auto tagName = json["tag_name"].get<std::string>();
                if (progressFunc) progressFunc("Downloading version " + tagName, 0);

                for (auto& asset : json["assets"]) {
                    auto name = asset["name"].get<std::string>();
                    if (name.find(PLATFORM_ASSET_IDENTIFIER) != std::string::npos) {
                        return this->webRequest(
                            asset["browser_download_url"].get<std::string>(),
                            tagName,
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
        "",
        false,
        errorFunc,
        nullptr,
        [this, errorFunc, progressFunc, finishFunc](wxWebResponse const& res, auto) -> void {
            try {
                auto json = nlohmann::json::parse(res.AsString());

                auto tagName = json["tag_name"].get<std::string>();
                if (progressFunc) progressFunc("Downloading version " + tagName, 0);

                for (auto& asset : json["assets"]) {
                    auto name = asset["name"].get<std::string>();
                    if (name.find(".geode") != std::string::npos) {
                        return this->webRequest(
                            asset["browser_download_url"].get<std::string>(),
                            tagName,
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

void Manager::downloadCLI(
    DownloadErrorFunc errorFunc,
    DownloadProgressFunc progressFunc,
    DownloadFinishFunc finishFunc
) {
    this->webRequest(
        "https://api.github.com/repos/geode-sdk/cli/releases/latest",
        "",
        false,
        errorFunc,
        nullptr,
        [this, errorFunc, progressFunc, finishFunc](wxWebResponse const& res, auto) -> void {
            try {
                auto json = nlohmann::json::parse(res.AsString());

                auto tagName = json["tag_name"].get<std::string>();
                if (progressFunc) progressFunc("Downloading version " + tagName, 0);

                for (auto& asset : json["assets"]) {
                    auto name = asset["name"].get<std::string>();
                    if (name.find(PLATFORM_ASSET_IDENTIFIER) != std::string::npos) {
                        return this->webRequest(
                            asset["browser_download_url"].get<std::string>(),
                            tagName,
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

    return "/Users/Shared/Geode/SDK"; 
    // it's literally hardcoded
    // there is no programatic way to get /Users/Shared
    #endif
}

ghc::filesystem::path const& Manager::getSDKDirectory() const {
    return m_sdkDirectory;
}

void Manager::setSDKDirectory(ghc::filesystem::path const& path) {
    m_sdkDirectory = path;
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
                if (instKey.QueryValue(REGVAL_INSTPATH, value)) {
                    inst.m_path = value.ToStdWstring();
                }
                if (instKey.QueryValue(REGVAL_INSTEXE, value)) {
                    inst.m_exe = value;
                }
                if (
                    instKey.HasValue(REGVAL_INSTVERSION) &&
                    instKey.QueryValue(REGVAL_INSTVERSION, value)
                ) {
                    inst.m_version = value;
                }
                
                this->addInstallation(inst);
            }
            if (sub.HasValue(REGVAL_DEFINST)) {
                long long defInst;
                sub.QueryValue64(REGVAL_DEFINST, &defInst);
                m_defaultInstallation = defInst;
            }
        }
    }

    #elif defined(__APPLE__)
    char* suite = getenv("GEODE_SUITE");
    m_dataLoaded = true;
    if (suite != NULL) {
        m_sdkDirectory = suite;
    }

    if (!wxFile::Exists("/Users/Shared/Geode/config.json"))
        return Ok();

    wxFile file;
    file.Open("/Users/Shared/Geode/config.json");


    wxString x;
    file.ReadAll(&x);

    auto json = nlohmann::json::parse(std::string(x));

    for (auto install : json["installations"]) {
        Installation inst;
        inst.m_path = std::string(install["path"]);
        inst.m_exe = std::string(install["executable"]);
        inst.m_version = std::string(install["version"]);
        this->addInstallation(inst);
    }

    m_defaultInstallation = json["default-installation"];
    m_defaultInstallation = json["has-sdk"];

    file.Close();
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
    if (m_installations.size()) {
        subKey.SetValue64(REGVAL_DEFINST, m_defaultInstallation);
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
        if (!instKey.SetValue(REGVAL_INSTVERSION, inst.m_version)) {
            UHHH_WELP("Unable to save " + keyName + "\\" REGVAL_INSTVERSION);
        }
        ix++;
    }
    #elif defined(__APPLE__)
        if (!ghc::filesystem::exists("/Users/Shared/Geode/"))
            ghc::filesystem::create_directories("/Users/Shared/Geode/");

        std::ofstream ofs("/Users/Shared/Geode/config.json");

        nlohmann::json config;
        config["has-sdk"] = m_sdkInstalled;

        if (m_installations.size())
            config["default-installation"] = m_defaultInstallation;

        for (auto x : m_installations) {
            nlohmann::json inst;
            inst["path"] = x.m_path;
            inst["executable"] = x.m_exe;
            inst["version"] = x.m_version;
            config["installations"].push_back(inst);
        }

        ofs << config.dump(4); 
        ofs.close();
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
    ghc::filesystem::remove("/Users/Shared/Geode/config.json");
    #endif

    return Ok();
}


Result<> Manager::installCLI(
    ghc::filesystem::path const& cliZipPath
) {
    auto targetDir = m_sdkDirectory / "bin";
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
    auto toAdd = (m_sdkDirectory / "bin").wstring() + ";";
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

Result<> Manager::installSDK(
    DevBranch branch,
    DownloadErrorFunc errorFunc,
    DownloadProgressFunc progressFunc,
    CloneFinishFunc finishFunc
) {
    #ifdef _WIN32

    if (!ghc::filesystem::exists(m_sdkDirectory / "bin" / "geodeutils.dll")) {
        return Err("Geode CLI seems to not have been installed!");
    }

    #else

    if (!ghc::filesystem::exists(m_sdkDirectory / "bin" / "libgeodeutils.dylib")) {
        return Err("Geode CLI seems to not have been installed!");
    }

    #endif

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

        auto suiteDir = m_sdkDirectory / "suite";

        using SuiteProgressCallback = void(__stdcall*)(const char*, int);
        using geode_install_suite = const char*(__cdecl*)(const char*, bool, SuiteProgressCallback);

        #if _WIN32

        auto lib = LoadLibraryW((m_sdkDirectory / "bin" / "geodeutils.dll").wstring().c_str());
        if (!lib) {
            throwError("Unable to load library");
            return;
        }

        auto installSuite = reinterpret_cast<geode_install_suite>(GetProcAddress(lib, "geode_install_suite"));
        if (!installSuite) {
            throwError("Unable to locate suite installing function");
            return;
        }

        #else

        auto lib = dlopen((m_sdkDirectory / "bin" / "libgeodeutils.dylib").string().c_str(), RTLD_LAZY);
        if (!lib) {
            throwError("Unable to load library");
        }

        auto installSuite = reinterpret_cast<geode_install_suite>(dlsym(lib, "geode_install_suite"));
        if (!installSuite) {
            throwError("Unable to locate suite installing function");
            return;
        }

        #endif

        if (
            !ghc::filesystem::exists(suiteDir) &&
            !ghc::filesystem::create_directories(suiteDir)
        ) {
            throwError("Unable to create directory at " + suiteDir.string());
            return;
        }

        static DownloadProgressFunc progFunc;
        progFunc = progressFunc;

        auto res = installSuite(
            suiteDir.string().c_str(),
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
                    this->finishSDKInstallation();
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

bool Manager::isSDKInstalled() const {
    return m_sdkInstalled;
}

Result<> Manager::uninstallSDK() {
    if (
        ghc::filesystem::exists(m_sdkDirectory) &&
        !ghc::filesystem::remove_all(m_sdkDirectory)
    ) {
        return Err("Unable to delete the GeodeSDK directory");
    }
    #ifdef _WIN32
    wxRegKey key(wxRegKey::HKLM, "System\\CurrentControlSet\\Control\\Session Manager\\Environment");
    if (!key.DeleteValue(ENVVAR_SDK)) {
        return Err("Unable to delete " ENVVAR_SDK " environment variable");
    }
    wxString path;
    if (!key.QueryValue("Path", path)) {
        return Err("Unable to read Path environment variable");
    }
    auto toRemove = (m_sdkDirectory / "bin").wstring() + ";";
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


Result<Installation> Manager::installLoaderFor(
    ghc::filesystem::path const& gdExePath,
    ghc::filesystem::path const& zipLocation,
    std::string const& version
) {
    Installation inst;
    inst.m_exe = gdExePath.filename().wstring();
    #if _WIN32
    inst.m_path = gdExePath.parent_path();
    #else
    inst.m_path = gdExePath / "Contents";
    #endif
    inst.m_version = version;

    #ifdef _WIN32

    auto ures = this->unzipTo(zipLocation, inst.m_path);
    if (!ures) {
        return Err("Loader unzip error: " + ures.error());
    }

    wxFile appid((inst.m_path / "steam_appid.txt").wstring(), wxFile::write);
    appid.Write("322170");

    #elif defined(__APPLE__)
    auto zPath = inst.m_path / "Frameworks";
    std::cout << "path " << zPath.string() << "\n";

    auto ures = this->unzipTo(zipLocation, zPath);
    if (!ures) {
        return Err("Loader unzip error: " + ures.error());
    }

    #else
    static_assert(false, "Implement installation proper for this platform");
    #endif

    if (!m_installations.size()) {
        m_defaultInstallation = 0;
    }
    this->addInstallation(inst);

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
