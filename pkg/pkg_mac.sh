#!/bin/sh

pkg_dir=$(dirname $0)
exec_file=$1
bin_dir=$(dirname $exec_file)

printf "Version: "
read ver

if [ "$bin_dir" ]; then
	cp -r $pkg_dir/macOS $bin_dir/Geode\ Installer.app
	cp $exec_file $bin_dir/Geode\ Installer.app/Contents/MacOS
	zip $bin_dir/GeodeInstaller-mac-$ver.zip $bin_dir/Geode\ Installer.app
fi