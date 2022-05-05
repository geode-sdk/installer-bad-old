#pragma once

#include "include/wx.hpp"
#include <unordered_map>
#include "fs/filesystem.hpp"
#include <set>
#include "include/Result.hpp"
#include <optional>
#include <wx/webrequest.h>
#include <functional>

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

enum class DevBranch {
    Stable,
    Nightly,
};

using DownloadErrorFunc = std::function<void(std::string const&)>;
using DownloadProgressFunc = std::function<void(std::string const&, int)>;
using DownloadFinishFunc = std::function<void(wxWebResponse const&)>;
using CloneFinishFunc = std::function<void()>;

class Manager : public wxEvtHandler {
protected:
    ghc::filesystem::path m_sdkDirectory;
    std::set<Installation> m_installations;
    bool m_dataLoaded = false;
    bool m_sdkInstalled = false;

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

public:
    static Manager* get();

    bool isFirstTime() const;
    ghc::filesystem::path const& getSDKDirectory() const;
    void setSDKDirectory(ghc::filesystem::path const&);
    ghc::filesystem::path getDefaultSDKDirectory() const;
    std::set<Installation> const& getInstallations() const;

    Result<> loadData();
    Result<> saveData();
    Result<> deleteData();

    void downloadCLI(
        DownloadErrorFunc errorFunc,
        DownloadProgressFunc progressFunc,
        DownloadFinishFunc finishFunc
    );
    Result<> installSDK(
        DevBranch branch, 
        DownloadErrorFunc errorFunc,
        DownloadProgressFunc progressFunc,
        CloneFinishFunc finishFunc
    );
    bool isSDKInstalled() const;
    Result<> uninstallSDK();

    void downloadLoader(
        DownloadErrorFunc errorFunc,
        DownloadProgressFunc progressFunc,
        DownloadFinishFunc finishFunc
    );
    void downloadAPI(
        DownloadErrorFunc errorFunc,
        DownloadProgressFunc progressFunc,
        DownloadFinishFunc finishFunc
    );

    Result<Installation> installLoaderFor(
        ghc::filesystem::path const& gdExePath,
        ghc::filesystem::path const& zipLocation
    );
    Result<> installAPIFor(
        Installation const& installation,
        ghc::filesystem::path const& zipLocation,
        wxString const& filename
    );
    Result<> uninstallFrom(Installation const& installation);
    Result<> deleteSaveDataFrom(Installation const& installation);

    /**
     * Find default installation of GD.
     * @returns On Windows, this returns 
     * the Steam installation location.
     * On MacOS, this returns IDK.
     */
    std::optional<ghc::filesystem::path> findDefaultGDPath() const;
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
