#!/bin/sh
#
# build.sh — Static site generator for The Mechanical Researcher
#
# Usage: ./build.sh [output_dir]
#
# Reads research topic directories, converts markdown to HTML via lowdown,
# and assembles the site using shell-based templates.

set -eu

SITE_DIR="$(cd "$(dirname "$0")" && pwd)"
OUT_DIR="${1:-$SITE_DIR/_build}"
TEMPLATE_DIR="$SITE_DIR/site/templates"
STATIC_DIR="$SITE_DIR/site/static"
SITE_URL="https://orangetide.github.io/the-mechanical-researcher"

# Clean and prepare output
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR/static"

# Copy static assets
cp -r "$STATIC_DIR/css" "$OUT_DIR/static/" 2>/dev/null || true
cp -r "$STATIC_DIR/js"  "$OUT_DIR/static/" 2>/dev/null || true
cp -r "$STATIC_DIR/fonts" "$OUT_DIR/static/" 2>/dev/null || true

# --------------------------------------------------------------------------
# Helpers
# --------------------------------------------------------------------------

# Extract a metadata field from a markdown file with YAML frontmatter
meta() {
    # $1 = field name, $2 = file path
    lowdown --parse-metadata -X "$1" "$2" 2>/dev/null || echo ""
}

# Convert markdown body (after frontmatter) to HTML
body_html() {
    lowdown --parse-metadata -thtml --html-no-escapehtml --html-no-skiphtml "$1" 2>/dev/null
}

# Format an ISO date (YYYY-MM-DD) for display
format_date() {
    if command -v date >/dev/null 2>&1; then
        date -d "$1" '+%B %-d, %Y' 2>/dev/null || echo "$1"
    else
        echo "$1"
    fi
}

# Simple template substitution: replace {{KEY}} with value
# Reads template from stdin, writes result to stdout
render_template() {
    # Accepts pairs of KEY VALUE arguments
    _input="$(cat)"
    while [ $# -ge 2 ]; do
        _key="$1"
        _val="$2"
        # Use awk for safe substitution (no sed delimiter issues)
        _input="$(printf '%s' "$_input" | awk -v key="{{${_key}}}" -v val="$_val" '{
            idx = index($0, key)
            while (idx > 0) {
                $0 = substr($0, 1, idx-1) val substr($0, idx+length(key))
                idx = index($0, key)
            }
            print
        }')"
        shift 2
    done
    printf '%s\n' "$_input"
}

# --------------------------------------------------------------------------
# Build articles and collect card data
# --------------------------------------------------------------------------

CARDS_JSON=""
CARD_SEP=""
RSS_ITEMS=""

# Find topic directories (contain an index.md)
for index_md in */index.md; do
    [ -f "$index_md" ] || continue

    topic_dir="$(dirname "$index_md")"
    slug="$topic_dir"

    title="$(meta title "$index_md")"
    date="$(meta date "$index_md")"
    abstract="$(meta abstract "$index_md")"
    category="$(meta category "$index_md")"

    [ -z "$title" ] && title="$slug"
    [ -z "$date" ] && date="1970-01-01"
    [ -z "$abstract" ] && abstract=""
    [ -z "$category" ] && category="research"

    date_display="$(format_date "$date")"

    # Build article HTML
    article_body="$(body_html "$index_md")"
    mkdir -p "$OUT_DIR/$slug"

    # Copy supplementary files (SVGs, images, data) — skip index.md
    for asset in "$topic_dir"/*; do
        [ -f "$asset" ] || continue
        case "$asset" in
            */index.md) continue ;;
        esac
        cp "$asset" "$OUT_DIR/$slug/"
    done

    # Render article template
    # For the article body which contains HTML, we use a two-pass approach
    cat "$TEMPLATE_DIR/article.html" \
        | render_template \
            "TITLE" "$title" \
            "DATE" "$date" \
            "DATE_DISPLAY" "$date_display" \
            "CATEGORY" "$category" \
            "ROOT" ".." \
        | awk -v body="$article_body" '{
            idx = index($0, "{{BODY}}")
            if (idx > 0) {
                print substr($0, 1, idx-1)
                print body
                print substr($0, idx+8)
            } else {
                print
            }
        }' > "$OUT_DIR/$slug/index.html"

    # Collect card data for the index page (as JS object literal)
    # Escape strings for JS
    js_title="$(printf '%s' "$title" | sed 's/\\/\\\\/g; s/"/\\"/g')"
    js_abstract="$(printf '%s' "$abstract" | sed 's/\\/\\\\/g; s/"/\\"/g')"
    js_category="$(printf '%s' "$category" | sed 's/\\/\\\\/g; s/"/\\"/g')"

    CARDS_JSON="${CARDS_JSON}${CARD_SEP}{\"title\":\"${js_title}\",\"abstract\":\"${js_abstract}\",\"date\":\"${date}\",\"dateDisplay\":\"${date_display}\",\"category\":\"${js_category}\",\"url\":\"${slug}/index.html\"}"
    CARD_SEP=","

    # Collect RSS item
    rss_date="$(date -d "$date" -R 2>/dev/null || echo "$date")"
    xml_title="$(printf '%s' "$title" | sed 's/&/\&amp;/g; s/</\&lt;/g; s/>/\&gt;/g')"
    xml_abstract="$(printf '%s' "$abstract" | sed 's/&/\&amp;/g; s/</\&lt;/g; s/>/\&gt;/g')"
    RSS_ITEMS="${RSS_ITEMS}
    <item>
      <title>${xml_title}</title>
      <link>${SITE_URL}/${slug}/index.html</link>
      <guid>${SITE_URL}/${slug}/index.html</guid>
      <pubDate>${rss_date}</pubDate>
      <description>${xml_abstract}</description>
      <category>${category}</category>
    </item>"

    echo "  built: $slug"
done

# --------------------------------------------------------------------------
# Sort cards by date (newest first) using a simple approach
# --------------------------------------------------------------------------

# We rely on the glob order above; for proper date sorting we'd need
# to collect and sort. For now, topics are built in directory order.
# TODO: implement date-based sorting

# --------------------------------------------------------------------------
# Build index page
# --------------------------------------------------------------------------

# Inject cards JSON into the index template
sed "s|{{CARDS}}|${CARDS_JSON}|" "$TEMPLATE_DIR/index.html" \
    > "$OUT_DIR/index.html"

# --------------------------------------------------------------------------
# Build RSS feed
# --------------------------------------------------------------------------

cat > "$OUT_DIR/feed.xml" << RSSEOF
<?xml version="1.0" encoding="UTF-8"?>
<rss version="2.0" xmlns:atom="http://www.w3.org/2005/Atom">
  <channel>
    <title>The Mechanical Researcher</title>
    <link>${SITE_URL}/</link>
    <description>Research topics gathered through agentic AI sessions</description>
    <language>en-us</language>
    <atom:link href="${SITE_URL}/feed.xml" rel="self" type="application/rss+xml"/>
    ${RSS_ITEMS}
  </channel>
</rss>
RSSEOF

echo ""
echo "Site built in: $OUT_DIR"
echo "Cards: $(printf '%s' "$CARDS_JSON" | grep -o '{' | wc -l)"
