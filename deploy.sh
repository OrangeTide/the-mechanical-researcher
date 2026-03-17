#!/bin/sh
#
# deploy.sh — Deploy The Mechanical Researcher to GitHub Pages
#
# Builds the site and pushes the output to the gh-pages branch.
#
# Usage: ./deploy.sh
#
# Prerequisites:
#   - A GitHub remote named 'origin' must be configured
#   - The gh-pages branch will be created automatically if it doesn't exist
#

set -eu

SITE_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SITE_DIR/_build"

# --------------------------------------------------------------------------
# Preflight checks
# --------------------------------------------------------------------------

if ! git remote get-url origin >/dev/null 2>&1; then
    echo "Error: no 'origin' remote configured."
    echo "  git remote add origin git@github.com:USER/REPO.git"
    exit 1
fi

# --------------------------------------------------------------------------
# Build
# --------------------------------------------------------------------------

echo "==> Building site..."
"$SITE_DIR/build.sh"
echo ""

# --------------------------------------------------------------------------
# Deploy to gh-pages
# --------------------------------------------------------------------------

echo "==> Deploying to gh-pages..."

# Save current state
ORIG_BRANCH="$(git symbolic-ref --short HEAD 2>/dev/null || git rev-parse --short HEAD)"
ORIG_STASH=""

# Stash any uncommitted changes so checkout is clean
if ! git diff --quiet || ! git diff --cached --quiet; then
    ORIG_STASH="yes"
    git stash push -m "deploy.sh: auto-stash before deploy" --quiet
fi

# Copy build output to a temp directory (outside the worktree)
TMPDIR="$(mktemp -d)"
cp -r "$BUILD_DIR"/. "$TMPDIR/"

# Create or switch to gh-pages
if git show-ref --verify --quiet refs/heads/gh-pages; then
    git checkout gh-pages --quiet
else
    # Create orphan branch (no history from main)
    git checkout --orphan gh-pages --quiet
    git rm -rf . --quiet 2>/dev/null || true
    git clean -fd --quiet 2>/dev/null || true
fi

# Clear existing content (keep .git)
find . -maxdepth 1 ! -name '.' ! -name '.git' -exec rm -rf {} +

# Copy build output into worktree root
cp -r "$TMPDIR"/. ./

# Add a .nojekyll file so GitHub serves raw HTML
touch .nojekyll

# Stage and commit
git add -A
if git diff --cached --quiet; then
    echo "No changes to deploy."
else
    git commit -m "deploy: $(date -u '+%Y-%m-%d %H:%M:%S UTC')" --quiet
    git push origin gh-pages
    echo "Deployed successfully."
fi

# --------------------------------------------------------------------------
# Restore original state
# --------------------------------------------------------------------------

git checkout "$ORIG_BRANCH" --quiet
if [ -n "$ORIG_STASH" ]; then
    git stash pop --quiet
fi

# Clean up
rm -rf "$TMPDIR"

echo "==> Done."
