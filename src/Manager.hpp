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

using DownloadErrorFunc = std::function<void(std::string const&)>;
using DownloadProgressFunc = std::function<void(std::string const&, int)>;
using DownloadFinishFunc = std::function<void(wxWebResponse const&)>;

class Manager : public wxEvtHandler {
protected:
    ghc::filesystem::path m_geodeDirectory;
    std::set<Installation> m_installations;
    bool m_geodeDirectorySet = false;

    void webRequest(
        std::string const& url,
        bool downloadFile,
        DownloadErrorFunc errorFunc,
        DownloadProgressFunc progressFunc,
        DownloadFinishFunc finishFunc
    );

public:
    static Manager* get();

    bool isFirstTime() const;
    ghc::filesystem::path const& getGeodeDirectory() const;
    std::set<Installation> const& getInstallations() const;

    Result<> loadData();
    Result<> saveData();
    Result<> deleteData();

    void downloadLoader(
        DownloadErrorFunc errorFunc,
        DownloadProgressFunc progressFunc,
        DownloadFinishFunc finishFunc
    );

    Result<> installFor(
        ghc::filesystem::path const& gdExePath,
        ghc::filesystem::path const& zipLocation
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
