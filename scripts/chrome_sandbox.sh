#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
URL="${1:-https://www.baidu.com}"

sudo env \
  DISPLAY="${DISPLAY:-}" \
  WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-}" \
  XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-}" \
  DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-}" \
  LANG="${LANG:-C.UTF-8}" \
  "$PROJECT_DIR/lsandbox" run \
    --name chromebox \
    --rm \
    --net host \
    --mem 4G \
    --pids 512 \
    --cpu 100 \
    --seccomp off \
    -- bash -lc '
      set -e

      export HOME=/tmp/home
      export XDG_CONFIG_HOME=/tmp/config
      export XDG_CACHE_HOME=/tmp/cache
      export XDG_DOWNLOAD_DIR=/tmp/downloads

      mkdir -p \
        /tmp/home \
        /tmp/config \
        /tmp/cache \
        /tmp/downloads \
        /tmp/chrome-profile/Default \
        /tmp/chrome-cache

      cat > /tmp/chrome-profile/Default/Preferences <<EOF
{
  "download": {
    "default_directory": "/tmp/downloads",
    "prompt_for_download": false,
    "directory_upgrade": true
  },
  "savefile": {
    "default_directory": "/tmp/downloads"
  },
  "profile": {
    "default_content_setting_values": {
      "automatic_downloads": 1
    }
  }
}
EOF

      exec google-chrome \
        --ozone-platform=wayland \
        --user-data-dir=/tmp/chrome-profile \
        --disk-cache-dir=/tmp/chrome-cache \
        --no-first-run \
        --disable-dev-shm-usage \
        --lang=zh-CN \
        "$@"
    ' _ "$URL"