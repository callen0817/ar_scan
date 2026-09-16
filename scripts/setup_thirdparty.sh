#!/usr/bin/env bash
# ==============================================================================
# AV Scan — Third-Party Dependency Setup Script
# Extracts local, isolated Qt Quick / QML modules without requiring root/sudo
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
QML_DIR="${REPO_ROOT}/third_party/qml"
LIB_DIR="${REPO_ROOT}/third_party/lib"

echo "=== AV Scan Third-Party Setup ==="
echo "QML directory: ${QML_DIR}"
echo "Lib directory: ${LIB_DIR}"

mkdir -p "${QML_DIR}" "${LIB_DIR}"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "${TMP_DIR}"' EXIT

echo "Downloading Qt5 QML packages into sandbox..."
cd "${TMP_DIR}"
apt download \
    qml-module-qtquick2 \
    qml-module-qtquick-window2 \
    qml-module-qtquick-controls2 \
    qml-module-qtquick-layouts \
    qml-module-qtquick-templates2 \
    qml-module-qtgraphicaleffects \
    qml-module-qtquick-dialogs \
    qml-module-qtquick-privatewidgets \
    libqt5quicktemplates2-5 \
    libqt5quickcontrols2-5 > /dev/null 2>&1 || true

echo "Extracting runtime modules..."
EXTRACT_DIR="${TMP_DIR}/extract"
mkdir -p "${EXTRACT_DIR}"

for deb in "${TMP_DIR}"/*.deb; do
    if [ -f "${deb}" ]; then
        dpkg -x "${deb}" "${EXTRACT_DIR}"
    fi
done

# Copy QML plugins
if [ -d "${EXTRACT_DIR}/usr/lib/aarch64-linux-gnu/qt5/qml" ]; then
    cp -r "${EXTRACT_DIR}/usr/lib/aarch64-linux-gnu/qt5/qml/"* "${QML_DIR}/"
elif [ -d "${EXTRACT_DIR}/usr/lib/x86_64-linux-gnu/qt5/qml" ]; then
    cp -r "${EXTRACT_DIR}/usr/lib/x86_64-linux-gnu/qt5/qml/"* "${QML_DIR}/"
fi

# Copy supporting shared libraries
if [ -d "${EXTRACT_DIR}/usr/lib/aarch64-linux-gnu" ]; then
    cp -P "${EXTRACT_DIR}/usr/lib/aarch64-linux-gnu/"libQt5Quick*.so* "${LIB_DIR}/" 2>/dev/null || true
elif [ -d "${EXTRACT_DIR}/usr/lib/x86_64-linux-gnu" ]; then
    cp -P "${EXTRACT_DIR}/usr/lib/x86_64-linux-gnu/"libQt5Quick*.so* "${LIB_DIR}/" 2>/dev/null || true
fi

echo "Third-party QML modules and libraries configured successfully."
