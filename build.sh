#!/bin/sh
#
# build.sh — Static site generator for The Mechanical Researcher
#
# Usage: ./build.sh [-u] [output_dir]
#   -u   Update in-place (skip rm -rf, useful for live preview)
#
# Reads research topic directories, converts markdown to HTML via lowdown,
# and assembles the site using shell-based templates.

set -eu

UPDATE_MODE=false
if [ "${1:-}" = "-u" ]; then
    UPDATE_MODE=true
    shift
fi

SITE_DIR="$(cd "$(dirname "$0")" && pwd)"
OUT_DIR="${1:-$SITE_DIR/_build}"
TEMPLATE_DIR="$SITE_DIR/site/templates"
STATIC_DIR="$SITE_DIR/site/static"
SITE_URL="https://orangetide.github.io/the-mechanical-researcher"

# Clean and prepare output
if [ "$UPDATE_MODE" = false ]; then
    rm -rf "$OUT_DIR"
fi
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

# Generate a TOC from h2 tags in an HTML file; returns empty string if < 3 headings
# $1 = file path
generate_toc() {
    _count="$(grep -cP '<h2 id="' "$1" || true)"
    [ "$_count" -lt 3 ] && return
    _toc='<input type="checkbox" id="toc-toggle" class="toc-state">'
    _toc="$_toc"'<label for="toc-toggle" class="toc-btn">Contents</label>'
    _toc="$_toc"'<div class="toc-panel"><div class="toc-header"><span>Contents</span>'
    _toc="$_toc"'<label for="toc-toggle" class="toc-close">&times;</label></div><ul class="toc-list">'
    _items="$(grep -oP '<h2 id="([^"]+)">([^<]+)</h2>' "$1" | sed 's/<h2 id="\([^"]*\)">\([^<]*\)<\/h2>/<li><a href="#\1">\2<\/a><\/li>/' | tr -d '\n')"
    printf '%s%s</ul></div>' "$_toc" "$_items"
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

CARDS_UNSORTED=""
PACKAGES_JSON=""
PACKAGES_SEP=""
RSS_ITEMS=""

# Find topic directories (contain an index.md)
for index_md in */index.md; do
    [ -f "$index_md" ] || continue

    topic_dir="$(dirname "$index_md")"
    slug="$topic_dir"

    title="$(meta title "$index_md")"
    date="$(meta date "$index_md")"
    revised="$(meta revised "$index_md")"
    abstract="$(meta abstract "$index_md")"
    category="$(meta category "$index_md")"

    [ -z "$title" ] && title="$slug"
    [ -z "$date" ] && date="1970-01-01"
    [ -z "$abstract" ] && abstract=""
    [ -z "$category" ] && category="research"

    date_display="$(format_date "$date")"
    if [ -n "$revised" ]; then
        revised_display="$(format_date "$revised")"
        revised_html="<span class=\"article-revised\">Revised <time datetime=\"${revised}\">${revised_display}</time></span>"
    else
        revised_display=""
        revised_html=""
    fi

    # Build article HTML
    article_body="$(body_html "$index_md")"
    mkdir -p "$OUT_DIR/$slug"

    # Copy supplementary files (SVGs, images, data) — skip source files
    # Uses find to handle subdirectories (e.g. demo/)
    (cd "$topic_dir" && find . -type f \
        ! -name 'index.md' \
        ! -name '*.dot' \
        ! -name '*.msc' \
        ! -name '.gitignore' \
    ) | while IFS= read -r rel; do
        rel="${rel#./}"
        mkdir -p "$OUT_DIR/$slug/$(dirname "$rel")"
        cp "$topic_dir/$rel" "$OUT_DIR/$slug/$rel"
    done

    # Build supplementary .md files as HTML (e.g. demo/README.md)
    find "$topic_dir" -name '*.md' ! -name 'index.md' -type f | while IFS= read -r suppl_md; do
        rel="${suppl_md#$topic_dir/}"
        suppl_html="$OUT_DIR/$slug/${rel%.md}.html"
        suppl_title="$(head -1 "$suppl_md" | sed 's/^#* *//')"
        suppl_body="$(lowdown -thtml --html-no-escapehtml --html-no-skiphtml "$suppl_md" 2>/dev/null)"
        cat "$TEMPLATE_DIR/article.html" \
            | render_template \
                "TITLE" "${suppl_title:-$rel}" \
                "DATE" "$date" \
                "DATE_DISPLAY" "$date_display" \
                "REVISED" "" \
                "CATEGORY" "$category" \
                "SOURCE_ZIP" "" \
                "ROOT" "$(printf '%s' "$rel" | sed 's|[^/]||g; s|/|../|g').." \
            | awk -v body="$suppl_body" '{
                idx = index($0, "{{BODY}}")
                if (idx > 0) {
                    print substr($0, 1, idx-1)
                    print body
                    print substr($0, idx+8)
                } else if (index($0, "{{TOC}}")) {
                    next
                } else {
                    print
                }
            }' > "$suppl_html"
    done

    # Source ZIP link (if demo/ exists)
    if [ -d "$topic_dir/demo" ]; then
        source_zip_html="<a class=\"source-zip\" href=\"${slug}-source.zip\" download>Source ZIP</a>"
    else
        source_zip_html=""
    fi

    # Render article template
    # For the article body which contains HTML, we use a two-pass approach
    cat "$TEMPLATE_DIR/article.html" \
        | render_template \
            "TITLE" "$title" \
            "DATE" "$date" \
            "DATE_DISPLAY" "$date_display" \
            "REVISED" "$revised_html" \
            "CATEGORY" "$category" \
            "SOURCE_ZIP" "$source_zip_html" \
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

    # Generate and inject TOC (from the rendered HTML, so heading IDs are available)
    article_toc="$(generate_toc "$OUT_DIR/$slug/index.html")"
    awk -v toc="$article_toc" '{
        idx = index($0, "{{TOC}}")
        if (idx > 0) {
            print substr($0, 1, idx-1) toc substr($0, idx+7)
        } else {
            print
        }
    }' "$OUT_DIR/$slug/index.html" > "$OUT_DIR/$slug/index.html.tmp"
    mv "$OUT_DIR/$slug/index.html.tmp" "$OUT_DIR/$slug/index.html"

    # Collect card data for the index page (as JS object literal)
    # Escape strings for JS
    js_title="$(printf '%s' "$title" | sed 's/\\/\\\\/g; s/"/\\"/g')"
    js_abstract="$(printf '%s' "$abstract" | sed 's/\\/\\\\/g; s/"/\\"/g')"
    js_category="$(printf '%s' "$category" | sed 's/\\/\\\\/g; s/"/\\"/g')"

    # Prefix with date for sorting (newest first); TAB separates date from JSON
    js_revised_display="$(printf '%s' "$revised_display" | sed 's/\\/\\\\/g; s/"/\\"/g')"

    CARDS_UNSORTED="${CARDS_UNSORTED}
${date}	{\"title\":\"${js_title}\",\"abstract\":\"${js_abstract}\",\"date\":\"${date}\",\"dateDisplay\":\"${date_display}\",\"revisedDisplay\":\"${js_revised_display}\",\"category\":\"${js_category}\",\"url\":\"${slug}/index.html\"}"

    # ZIP source code directories (demo/) for download
    if [ -d "$topic_dir/demo" ]; then
        zip_name="${slug}-source.zip"
        (cd "$topic_dir" && zip -qr - demo) > "$OUT_DIR/$slug/$zip_name"
        zip_size="$(wc -c < "$OUT_DIR/$slug/$zip_name")"
        if [ "$zip_size" -ge 1073741824 ]; then
            zip_display="$(awk "BEGIN{printf \"%.1f GB\", $zip_size/1073741824}")"
        elif [ "$zip_size" -ge 1048576 ]; then
            zip_display="$(awk "BEGIN{printf \"%.1f MB\", $zip_size/1048576}")"
        else
            zip_display="$(awk "BEGIN{printf \"%.0f KB\", $zip_size/1024}")"
        fi
        PACKAGES_JSON="${PACKAGES_JSON}${PACKAGES_SEP}{\"name\":\"${js_title}\",\"zip\":\"${slug}/${zip_name}\",\"date\":\"${date}\",\"dateDisplay\":\"${date_display}\",\"size\":\"${zip_display}\",\"article\":\"${slug}/index.html\"}"
        PACKAGES_SEP=","
    fi

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
# Sort cards by date (newest first)
# --------------------------------------------------------------------------

CARDS_JSON="$(printf '%s\n' "$CARDS_UNSORTED" | grep '	' | sort -t'	' -k1,1r | cut -f2- | paste -sd',')"

# --------------------------------------------------------------------------
# Build index page
# --------------------------------------------------------------------------

# Inject cards and packages JSON into the index template
sed -e "s|{{CARDS}}|${CARDS_JSON}|" \
    -e "s|{{PACKAGES}}|${PACKAGES_JSON}|" \
    "$TEMPLATE_DIR/index.html" > "$OUT_DIR/index.html"

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
