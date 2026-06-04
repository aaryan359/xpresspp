#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_DIR="${HOME}/.local/bin"
XP_BINARY="${ROOT_DIR}/cli/build/xp"
XP_WRAPPER="${INSTALL_DIR}/xp"

cmake -S "${ROOT_DIR}/cli" -B "${ROOT_DIR}/cli/build"
cmake --build "${ROOT_DIR}/cli/build"

mkdir -p "${INSTALL_DIR}"

cat > "${XP_WRAPPER}" <<EOF
#!/usr/bin/env bash
export XPRESSPP_HOME="${ROOT_DIR}"
export XPRESSPP_CLI_PATH="${XP_BINARY}"
exec "${XP_BINARY}" "\$@"
EOF

chmod +x "${XP_WRAPPER}"

echo "Installed xp to ${XP_WRAPPER}"
echo
echo "If your shell cannot find xp yet, add this to ~/.bashrc:"
echo "  export PATH=\"\$HOME/.local/bin:\$PATH\""
