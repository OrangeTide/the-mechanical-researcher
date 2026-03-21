#!/bin/sh
#
# deploy-remote.sh — Deploy site and notes to a remote host via rsync/scp
#
# Usage: ./deploy-remote.sh user@host:/path/to/webroot
#
# Builds the site, builds EPUB notes, then uploads everything to the
# remote host. Uses rsync over SSH when available (faster incremental
# deploys), falls back to scp.
#

set -eu

if [ $# -lt 1 ]; then
    echo "Usage: $0 user@host:/path/to/webroot"
    exit 1
fi

REMOTE="$1"
SITE_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SITE_DIR/_build"

# --------------------------------------------------------------------------
# Build site
# --------------------------------------------------------------------------

echo "==> Building site..."
"$SITE_DIR/build.sh"
echo ""

# --------------------------------------------------------------------------
# Build EPUB notes
# --------------------------------------------------------------------------

echo "==> Building EPUB notes..."
make -C "$SITE_DIR" notes
echo ""

# --------------------------------------------------------------------------
# Deploy
# --------------------------------------------------------------------------

echo "==> Deploying to $REMOTE ..."
if command -v rsync >/dev/null 2>&1; then
    rsync -az --delete -e ssh "$BUILD_DIR/" "$REMOTE"
else
    scp -r "$BUILD_DIR"/. "$REMOTE"
fi

echo "==> Done."
