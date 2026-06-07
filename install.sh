#!/usr/bin/env bash
# ──────────────────────────────────────────────────────────────────────────────
# Xpress++ installer
#
# What this does:
#   1. Detects your OS and installs all required dependencies
#      (cmake, g++, jsoncpp, openssl, zlib, and Drogon)
#   2. Builds the `xp` CLI from source
#   3. Installs it to ~/.local/bin/xp
#
# Usage:
#   curl -sSfL https://raw.githubusercontent.com/aaryan359/xpresspp/main/install.sh | bash
#   -- or --
#   ./install.sh
# ──────────────────────────────────────────────────────────────────────────────
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_DIR="${HOME}/.local/bin"
XP_BINARY="${ROOT_DIR}/cli/build/xp"
XP_WRAPPER="${INSTALL_DIR}/xp"

# ── Colour helpers ────────────────────────────────────────────────────────────
if [ -t 1 ]; then
    GREEN="\033[32m"; CYAN="\033[36m"; YELLOW="\033[33m"
    RED="\033[31m"; BOLD="\033[1m"; DIM="\033[2m"; RESET="\033[0m"
else
    GREEN=""; CYAN=""; YELLOW=""; RED=""; BOLD=""; DIM=""; RESET=""
fi

step()    { echo -e "${CYAN}  ▸ ${RESET}${1}"; }
ok()      { echo -e "${GREEN}${BOLD}  ✓ ${RESET}${1}"; }
warn()    { echo -e "${YELLOW}${BOLD}  ⚠ ${RESET}${YELLOW}${1}${RESET}"; }
die()     { echo -e "${RED}${BOLD}\n  ✗ ${1}${RESET}\n" >&2; exit 1; }
divider() { echo -e "${DIM}  ────────────────────────────────────────────${RESET}"; }

# ── Detect OS ─────────────────────────────────────────────────────────────────
detect_os() {
    if [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    elif [ -f /etc/os-release ]; then
        # shellcheck disable=SC1091
        source /etc/os-release
        case "$ID" in
            ubuntu|debian|linuxmint|pop)    echo "debian" ;;
            arch|manjaro|endeavouros)       echo "arch"   ;;
            fedora|rhel|centos|almalinux)   echo "fedora" ;;
            opensuse*|sles)                 echo "suse"   ;;
            *)
                if command -v apt-get &>/dev/null; then echo "debian"
                elif command -v pacman &>/dev/null; then echo "arch"
                elif command -v dnf    &>/dev/null; then echo "fedora"
                else die "Unsupported OS: $ID. Please install Drogon manually: https://github.com/drogonframework/drogon/wiki/ENG-02-Installation"
                fi
                ;;
        esac
    elif command -v apt-get &>/dev/null; then echo "debian"
    elif command -v pacman  &>/dev/null; then echo "arch"
    elif command -v dnf     &>/dev/null; then echo "fedora"
    else die "Cannot detect OS. Please install dependencies manually and re-run this script."
    fi
}

# ── Install system build tools and Drogon's dependencies ─────────────────────
install_deps() {
    local os="$1"
    step "Installing build dependencies for ${os}..."

    case "$os" in
        debian)
            sudo apt-get update -qq
            sudo apt-get install -y --no-install-recommends \
                build-essential git cmake \
                libssl-dev libjsoncpp-dev zlib1g-dev \
                libpq-dev uuid-dev libmariadb-dev libsqlite3-dev \
                ca-certificates curl wget
            ;;
        macos)
            if ! command -v brew &>/dev/null; then
                step "Installing Homebrew..."
                /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
            fi
            brew install cmake jsoncpp openssl zlib ossp-uuid
            ;;
        arch)
            sudo pacman -Sy --noconfirm \
                base-devel git cmake \
                jsoncpp openssl zlib uuid
            ;;
        fedora)
            sudo dnf install -y \
                gcc-c++ git cmake \
                openssl-devel jsoncpp-devel zlib-devel \
                libuuid-devel
            ;;
        suse)
            sudo zypper install -y \
                gcc-c++ git cmake \
                libopenssl-devel jsoncpp-devel zlib-devel
            ;;
    esac

    ok "Build dependencies installed."
}

# ── Check if Drogon is already installed ──────────────────────────────────────
drogon_installed() {
    # Check for the cmake config file that Drogon installs
    local search_paths=(
        "/usr/local/lib/cmake/Drogon"
        "/usr/lib/cmake/Drogon"
        "/opt/homebrew/lib/cmake/Drogon"  # macOS ARM
        "/usr/local/Cellar"               # macOS Intel
    )
    for p in "${search_paths[@]}"; do
        [ -d "$p" ] && return 0
    done
    # Try pkg-config or cmake itself
    cmake --find-package -DNAME=Drogon -DCOMPILER_ID=GNU -DLANGUAGE=CXX \
        -DMODE=EXIST &>/dev/null 2>&1 && return 0
    return 1
}

# ── Install Drogon from source ────────────────────────────────────────────────
install_drogon() {
    if drogon_installed; then
        ok "Drogon is already installed — skipping."
        return 0
    fi

    step "Building Drogon from source (this takes a few minutes)..."

    local tmp_dir
    tmp_dir="$(mktemp -d)"
    trap 'rm -rf "$tmp_dir"' RETURN

    git clone --depth 1 --recurse-submodules \
        https://github.com/drogonframework/drogon.git \
        "${tmp_dir}/drogon"

    cmake -S "${tmp_dir}/drogon" -B "${tmp_dir}/drogon/build" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_EXAMPLES=OFF \
        -DBUILD_CTL=OFF \
        -DBUILD_ORM=OFF \
        >/dev/null

    cmake --build "${tmp_dir}/drogon/build" --parallel "$(nproc 2>/dev/null || echo 4)"
    sudo cmake --install "${tmp_dir}/drogon/build" >/dev/null

    ok "Drogon installed successfully."
}

# ── Build the xp CLI ──────────────────────────────────────────────────────────
build_cli() {
    step "Building the xp CLI..."

    cmake -S "${ROOT_DIR}/cli" -B "${ROOT_DIR}/cli/build" \
        -DCMAKE_BUILD_TYPE=Release >/dev/null

    cmake --build "${ROOT_DIR}/cli/build" --parallel "$(nproc 2>/dev/null || echo 4)"

    ok "CLI built at ${XP_BINARY}"
}

# ── Install the wrapper script ─────────────────────────────────────────────────
install_wrapper() {
    mkdir -p "${INSTALL_DIR}"

    cat > "${XP_WRAPPER}" << EOF
#!/usr/bin/env bash
export XPRESSPP_HOME="${ROOT_DIR}"
export XPRESSPP_CLI_PATH="${XP_BINARY}"
exec "${XP_BINARY}" "\$@"
EOF

    chmod +x "${XP_WRAPPER}"
    ok "Installed: ${XP_WRAPPER}"
}

# ── Ensure ~/.local/bin is in PATH ────────────────────────────────────────────
ensure_path() {
    if [[ ":${PATH}:" != *":${INSTALL_DIR}:"* ]]; then
        warn "~/.local/bin is not in your PATH."
        echo ""
        echo "  Add this line to your shell config file:"

        local shell_config
        shell_config="${HOME}/.bashrc"
        if [[ "${SHELL}" == *"zsh"* ]]; then
            shell_config="${HOME}/.zshrc"
        elif [[ "${SHELL}" == *"fish"* ]]; then
            shell_config="${HOME}/.config/fish/config.fish"
        fi

        echo -e "${CYAN}    echo 'export PATH=\"\$HOME/.local/bin:\$PATH\"' >> ${shell_config}${RESET}"
        echo -e "${CYAN}    source ${shell_config}${RESET}"
        echo ""
    fi
}

# ── Main ──────────────────────────────────────────────────────────────────────
main() {
    echo ""
    echo -e "${CYAN}${BOLD}  Xpress++ Installer${RESET}"
    divider

    local os
    os=$(detect_os)
    step "Detected OS: ${os}"
    echo ""

    install_deps "${os}"
    echo ""

    # If running standalone (e.g. via curl | bash or copying just the install.sh file)
    if [ ! -d "${ROOT_DIR}/cli" ]; then
        step "Cloning Xpress++ core repository to ${HOME}/.xpresspp..."
        rm -rf "${HOME}/.xpresspp"
        git clone --depth 1 https://github.com/aaryan359/xpresspp.git "${HOME}/.xpresspp"
        ROOT_DIR="${HOME}/.xpresspp"
        XP_BINARY="${ROOT_DIR}/cli/build/xp"
        XP_WRAPPER="${INSTALL_DIR}/xp"
    fi

    install_drogon
    echo ""
    build_cli
    echo ""
    install_wrapper
    echo ""

    divider
    echo -e "${GREEN}${BOLD}  All done! Xpress++ is installed.${RESET}"
    echo ""
    echo -e "  Run your first project:"
    echo -e "${CYAN}    xp create my-api${RESET}"
    echo -e "${CYAN}    cd my-api${RESET}"
    echo -e "${CYAN}    xp run${RESET}"
    echo ""

    ensure_path
}

main

