#!/bin/sh

set -e 

check_cmd() {
	command -v "$1" > /dev/null 2>&1
}

error() {
	printf 'Error: %s\n' "$1"
	exit 1
}

need_cmd() {
	if ! check_cmd "$1"; then
		error "need '$1' (command not found)"
	fi
}

download() {
	local url=$1
	local dest_path=$2

	if check_cmd wget; then
		wget --quiet --output-document "$dest_path" "$url"
	elif check_cmd curl; then
		curl --silent --output "$dest_path" "$url"
	else
		error "Unable to download, you must have either curl or wget installed."
	fi
}

need_cmd tar
need_cmd realpath
need_cmd printf
need_cmd uname

echo "Welcome to Orca!"

install_path=$1
if [ -z "$install_path" ]; then
	read -p "Enter the directory where you want to install orca: " install_path
fi
install_path=$(realpath $install_path)

# TODO: if orca already exists at this install path, should we warn the user that orca is already installed?

cd $install_path
mkdir -p orca orca/bin orca/versions

echo "Installing Orca CLI tool..."

os_type="$(uname -s)"

if [ $os_type = "Darwin" ]; then 
	cli_name=orca-mac-x64
	tar_name="${cli_name}.tar"
	#download "https://github.com/orca-app/orca/releases/latest/download/$tar_name" "$install_path/orca/bin/$tar_name"
	download "https://github.com/bitwitch/orca/releases/latest/download/$tar_name" "$install_path/orca/bin/$tar_name"
	cd orca/bin
	tar -xf $tar_name 
	mv $cli_name orca
	rm $tar_name
elif [ $os_type = "Linux" ]; then
	error "Linux currently not supported"
fi


echo "The Orca CLI tool has been installed! ($install_path/orca/bin/orca)"

# TODO: help people get orca on their PATH, for now just printing out the path
echo "\nAdd this to your PATH in order to have access the orca cli tool:"
echo "$install_path/orca/bin\n"


