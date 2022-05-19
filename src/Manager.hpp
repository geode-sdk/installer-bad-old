#pragma once

#include "include/wx.hpp"
#include <unordered_map>
#include "fs/filesystem.hpp"
#include <set>
#include "include/Result.hpp"
#include <optional>
#include <wx/webrequest.h>
#include <functional>
#include "include/VersionInfo.hpp"
#include "include/json.hpp"

enum class DevBranch : bool {
    Stable,
    Nightly,
};

/**
 * Represents an installation of Geode 
 * on some directory. The identifier of 
 * the installation is its directory
 */
struct Installation {
    /**
     * Path to the GD directory
     */
    ghc::filesystem::path m_path;
    /**
     * Binary name; stored to delete the save data 
     * dir under %localappdata%/${m_exe}/geode on 
     * Windows.
     */
    wxString m_exe;
    DevBranch m_branch;

    inline bool operator<(Installation const& other) const {
        return m_path < other.m_path;
    }
    inline bool operator==(Installation const& other) const {
        return m_path == other.m_path;
    }
};

enum OtherModFlags {
    OMF_None = 0b0,
    OMF_Some = 0b1,
    OMF_MHv6 = 0b10,
    OMF_MHv7 = 0b100,
    OMF_GDHM = 0b1000,
};

enum class InstallerMode {
    Normal,
    UpdateLoader,
};

using DownloadErrorFunc = std::function<void(std::string const&)>;
using DownloadProgressFunc = std::function<void(std::string const&, int)>;
using DownloadFinishFunc = std::function<void(wxWebResponse const&)>;
using CloneFinishFunc = std::function<void()>;
using UpdateCheckFinishFunc = std::function<void(VersionInfo const&, VersionInfo const&)>;

class GeodeInstallerApp;

namespace cli {
    using ProgressCallback = void(__stdcall*)(const char*, int);
    using geode_install_geode = const char*(__cdecl*)(const char*, bool, bool, ProgressCallback);
    using geode_install_suite = const char*(__cdecl*)(const char*, bool, ProgressCallback);
}

class CallOnMainEvent : public wxEvent {
protected:
    std::function<void()> m_function;

public:
    inline CallOnMainEvent(std::function<void()> func, wxEventType type, int winid)
      : wxEvent(winid, type), m_function(func) {}
    
    inline void invoke() {
        m_function();
    }

    wxEvent* Clone() const override { return new CallOnMainEvent(*this); }
};

class Manager : public wxEvtHandler {
protected:
    ghc::filesystem::path m_dataDirectory;
    ghc::filesystem::path m_suiteDirectory;
    ghc::filesystem::path m_binDirectory;
    std::vector<Installation> m_installations;
    size_t m_defaultInstallation;
    bool m_dataLoaded = false;
    bool m_suiteInstalled = false;
    InstallerMode m_mode = InstallerMode::Normal;
    ghc::filesystem::path m_loaderUpdatePath;
    nlohmann::json m_loadedConfigJson;

    void* loadFunctionFromUtilsLib(const char* name);
    template<typename Func>
    Func utilsFunc(const char* name) {
        return reinterpret_cast<Func>(this->loadFunctionFromUtilsLib(name));
    }

    void webRequest(
        std::string const& url,
        bool downloadFile,
        DownloadErrorFunc errorFunc,
        DownloadProgressFunc progressFunc,
        DownloadFinishFunc finishFunc
    );
    Result<> unzipTo(
        ghc::filesystem::path const& zip,
        ghc::filesystem::path const& to
    );
    Result<> addSuiteEnv();
 
    void onSyncThreadCall(CallOnMainEvent&);

    void addInstallation(Installation const& inst);

    friend class GeodeInstallerApp;

public:
    static Manager* get();
    
    InstallerMode getInstallerMode() const;
    ghc::filesystem::path const& getLoaderUpdatePath() const;

    bool isFirstTime() const;

    ghc::filesystem::path const& getDataDirectory() const;
    ghc::filesystem::path getDefaultDataDirectory() const;

    ghc::filesystem::path const& getBinDirectory() const;
    ghc::filesystem::path getDefaultBinDirectory() const;

    ghc::filesystem::path const& getSuiteDirectory() const;
    void setSuiteDirectory(ghc::filesystem::path const&);
    ghc::filesystem::path getDefaultSuiteDirectory() const;

    std::vector<Installation> const& getInstallations() const;
    size_t getDefaultInstallation() const;

    Result<> loadData();
    Result<> saveData();
    Result<> deleteData();

    void downloadCLI(
        DownloadErrorFunc errorFunc,
        DownloadProgressFunc progressFunc,
        DownloadFinishFunc finishFunc
    );
    Result<> installCLI(
        ghc::filesystem::path const& cliZipPath
    );
    Result<> addCLIToPath();
    Result<> installSuite(
        DevBranch branch,
        DownloadErrorFunc errorFunc,
        DownloadProgressFunc progressFunc,
        CloneFinishFunc finishFunc
    );
    bool isSuiteInstalled() const;
    Result<> uninstallSuite();

    void checkForUpdates(
        Installation const& installation,
        DownloadErrorFunc errorFunc,
        UpdateCheckFinishFunc finishFunc
    );
    void checkCLIForUpdates(
        DownloadErrorFunc errorFunc,
        UpdateCheckFinishFunc finishFunc
    );

    void installGeodeUtilsLib(
        bool update,
        DevBranch branch,
        DownloadErrorFunc errorFunc,
        DownloadProgressFunc progressFunc,
        CloneFinishFunc finishFunc
    );
    bool isGeodeUtilsInstalled() const;

    Result<> installGeodeFor(
        ghc::filesystem::path const& gdExePath,
        DevBranch branch,
        DownloadErrorFunc errorFunc,
        DownloadProgressFunc progressFunc,
        CloneFinishFunc finishFunc
    );
    Result<> uninstallGeodeFrom(Installation const& installation);
    Result<> deleteSaveDataFrom(Installation const& installation);

    bool needRequestAdminPriviledges() const;

    void launch(ghc::filesystem::path const& path);

    /**
     * Find default installation of GD.
     * @returns On Windows, this returns 
     * the Steam installation location.
     * On MacOS, this returns IDK.
     */
    std::optional<ghc::filesystem::path> findDefaultGDPath() const;

    static bool isValidGD(ghc::filesystem::path const& path);

    /**
     * Check if the given directory contains 
     * other external mods.
     * @returns On Windows, this returns 
     * flags based on OtherModFlags.
     * On MacOS, this always returns 0.
     */
    int doesDirectoryContainOtherMods(
        ghc::filesystem::path const& path
    ) const;
};
