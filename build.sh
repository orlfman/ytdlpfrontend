#!/bin/bash

PROJECT_DIR="$(pwd)"
BUILD_DIR="$PROJECT_DIR/build"
BINARY_NAME="YTDLPFrontend"
INSTALL_DIR="/usr/local/bin"
DESKTOP_FILE="$PROJECT_DIR/ytdlpfrontend.desktop"
DESKTOP_INSTALL_DIR="/usr/share/applications"
ICON_FILE="$PROJECT_DIR/YTDLPFrontend.png"
ICON_INSTALL_DIR="/usr/share/icons"

check_dependency() {
    if ! command -v "$1" &> /dev/null; then
        echo "Error: $1 is not installed. Please install it."
        exit 1
    fi
}

check_python_dependency() {
    local module_name="$1"
    local package_name="$2"
    if ! python3 -c "import $module_name" 2>/dev/null; then
        echo "Error: Python module '${package_name:-$module_name}' is not installed. Please install it ('pip3 install ${package_name:-$module_name} or check your distro repos')."
        exit 1
    fi
}

echo "Checking dependencies..."
check_dependency qmake6
check_dependency make
check_dependency yt-dlp
check_dependency aria2c
check_dependency atomicparsley
check_dependency rtmpdump

check_python_dependency brotli python-brotli
check_python_dependency mutagen python-mutagen
check_python_dependency Crypto python-pycryptodome
check_python_dependency xattr python-pyxattr
check_python_dependency secretstorage python-secretstorage
check_python_dependency websockets python-websockets

if ! qmake6 --version | grep -q "Qt version 6"; then
    echo "Error: Qt 6 is required. Please install Qt 6 libraries."
    exit 1
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR" || { echo "Error: Cannot change to build directory"; exit 1; }

if [ -n "$(ls -A)" ]; then
    echo "Cleaning build directory..."
    rm -rf *
fi

sleep 2s
echo "Running qmake6..."
qmake6 "$PROJECT_DIR/YTDLPFrontend.pro" || { echo "Error: qmake6 failed"; exit 1; }

echo "Running make..."
make || { echo "Error: make failed"; exit 1; }

if [ ! -f "$BINARY_NAME" ]; then
    echo "Error: Binary '$BINARY_NAME' not found in $BUILD_DIR"
    BINARY_PATH=$(find "$BUILD_DIR" -type f -name "$BINARY_NAME" -executable)
    if [ -n "$BINARY_PATH" ]; then
        echo "Found binary at $BINARY_PATH"
        cp "$BINARY_PATH" .
    else
        echo "Could not locate binary"
        exit 1
    fi
fi

if [ -f "$INSTALL_DIR/$BINARY_NAME" ]; then
    echo "Warning: '$BINARY_NAME' already exists in $INSTALL_DIR."
    read -p "Do you want to overwrite it? [y/N] " REPLY
    if [ "$REPLY" != "y" ] && [ "$REPLY" != "Y" ]; then
        echo "Installation aborted."
        exit 0
    fi
fi

echo "Installing binary to $INSTALL_DIR..."
sudo cp "$BINARY_NAME" "$INSTALL_DIR/" || { echo "Error: Failed to copy binary to $INSTALL_DIR"; exit 1; }

if [ -f "$DESKTOP_FILE" ]; then
    if [ -f "$DESKTOP_INSTALL_DIR/ytdlpfrontend.desktop" ]; then
        echo "Warning: 'ytdlpfrontend.desktop' already exists in $DESKTOP_INSTALL_DIR."
        read -p "Do you want to overwrite it? [y/N] " REPLY
        if [ "$REPLY" = "y" ] || [ "$REPLY" = "Y" ]; then
            sudo cp "$DESKTOP_FILE" "$DESKTOP_INSTALL_DIR/" || { echo "Error: Failed to copy .desktop file"; exit 1; }
        else
            echo "Skipping .desktop file installation."
        fi
    else
        sudo cp "$DESKTOP_FILE" "$DESKTOP_INSTALL_DIR/" || { echo "Error: Failed to copy .desktop file"; exit 1; }
    fi
else
    echo "Warning: .desktop file not found in $PROJECT_DIR"
fi

if [ -f "$ICON_FILE" ]; then
    if [ -f "$ICON_INSTALL_DIR/YTDLPFrontend.png" ]; then
        echo "Warning: 'YTDLPFrontend.png' already exists in $ICON_INSTALL_DIR."
        read -p "Do you want to overwrite it? [y/N] " REPLY
        if [ "$REPLY" = "y" ] || [ "$REPLY" = "Y" ]; then
            sudo cp "$ICON_FILE" "$ICON_INSTALL_DIR/" || { echo "Error: Failed to copy .png file"; exit 1; }
        else
            echo "Skipping .png file installation."
        fi
    else
        sudo cp "$ICON_FILE" "$ICON_INSTALL_DIR/" || { echo "Error: Failed to copy .png file"; exit 1; }
    fi
else
    echo "Warning: .png file not found in $PROJECT_DIR"
fi

echo "Build and installation completed successfully!"
