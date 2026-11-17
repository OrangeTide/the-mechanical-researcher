#!/bin/sh
#
# build.sh — Static site generator for The Mechanical Researcher
#
# Usage: ./build.sh [-u] [-a] [output_dir]
#   -u   Update in-place (skip rm -rf, useful for live preview)
#   -a   Build all articles, including unpublished
#
# Reads research topic directories, converts markdown to HTML via lowdown,
# and assembles the site using shell-based templates.

set -eu

UPDATE_MODE=false
BUILD_ALL=false
while [ $# -gt 0 ]; do
    case "$1" in
        -u) UPDATE_MODE=true; shift ;;
        -a) BUILD_ALL=true; shift ;;
        *)  break ;;
    esac
done

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
    # Strip surrounding YAML quotes (lowdown -X preserves them verbatim)
    _val="$(lowdown --parse-metadata -X "$1" "$2" 2>/dev/null || echo "")"
    case "$_val" in
        \"*\") _val="${_val#\"}"; _val="${_val%\"}" ;;
        \'*\') _val="${_val#\'}"; _val="${_val%\'}" ;;
    esac
    printf '%s' "$_val"
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
        # Pass val via ENVIRON to avoid awk -v interpreting backslash escapes
        export _AWK_VAL="$_val"
        _input="$(printf '%s' "$_input" | awk -v key="{{${_key}}}" 'BEGIN{val=ENVIRON["_AWK_VAL"]} {
            idx = index($0, key)
            while (idx > 0) {
                $0 = substr($0, 1, idx-1) val substr($0, idx+length(key))
                idx = index($0, key)
            }
            print
        }')"
        unset _AWK_VAL
        shift 2
    done
    printf '%s\n' "$_input"
}

# --------------------------------------------------------------------------
# Load published list
# --------------------------------------------------------------------------

PUBLISHED_FILE="$SITE_DIR/published.txt"
if [ -f "$PUBLISHED_FILE" ]; then
    PUBLISHED_LIST="$(sed 's/#.*//' "$PUBLISHED_FILE" | awk 'NF{$1=$1; print}' | tr '\n' ' ')"
else
    PUBLISHED_LIST=""
fi

is_published() {
    # Returns 0 (true) if slug is in PUBLISHED_LIST, -a flag is set, or no published.txt exists
    [ "$BUILD_ALL" = true ] && return 0
    [ -z "$PUBLISHED_LIST" ] && return 0
    case " $PUBLISHED_LIST " in
        *" $1 "*) return 0 ;;
        *) return 1 ;;
    esac
}

# --------------------------------------------------------------------------
# Build articles and collect card data
# --------------------------------------------------------------------------

POSTS_UNSORTED=""
RSS_ITEMS=""

# Escape &, <, > for safe inclusion in HTML text
html_escape() {
    printf '%s' "$1" | sed 's/&/\&amp;/g; s/</\&lt;/g; s/>/\&gt;/g'
}

# Find topic directories (contain an index.md)
for index_md in */index.md; do
    [ -f "$index_md" ] || continue

    topic_dir="$(dirname "$index_md")"
    slug="$topic_dir"

    # Skip unpublished articles entirely
    if ! is_published "$slug"; then
        echo "  skipped: $slug (unpublished)"
        continue
    fi

    title="$(meta title "$index_md")"
    date="$(meta date "$index_md")"
    revised="$(meta revised "$index_md")"
    abstract="$(meta abstract "$index_md")"
    category="$(meta category "$index_md")"
    license="$(meta license "$index_md")"

    [ -z "$title" ] && title="$slug"
    [ -z "$date" ] && date="1970-01-01"
    [ -z "$abstract" ] && abstract=""
    [ -z "$category" ] && category="research"
    [ -z "$license" ] && license="CC0-1.0"

    # License notice shown in the article footer
    case "$license" in
        CC0-1.0|CC0)
            license_html="<span class=\"article-license\">Licensed <a href=\"https://creativecommons.org/publicdomain/zero/1.0/\" rel=\"license\">CC0 1.0</a></span>" ;;
        none|"")
            license_html="" ;;
        *)
            license_html="<span class=\"article-license\">Licensed ${license}</span>" ;;
    esac

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
                "WASM_DEMO" "" \
                "LICENSE" "$license_html" \
                "ROOT" "$(printf '%s' "$rel" | sed 's|[^/]||g; s|/|../|g').." \
            | _AWK_BODY="$suppl_body" awk 'BEGIN{body=ENVIRON["_AWK_BODY"]} {
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

    # Browser demonstration. A topic that ships demo/wasm/build-wasm.sh has
    # that script run here, and the self-contained page it writes is
    # published at /<slug>/wasm/. The script builds into a scratch
    # directory so its intermediates stay out of the site and out of the
    # source tree; only the page itself is deployed.
    #
    # The toolchain it needs is not required to build the site. If it is
    # missing the demo is skipped with a warning and everything else still
    # builds, so a plain checkout without a cross-compiler works.
    wasm_demo_html=""
    if [ -x "$topic_dir/demo/wasm/build-wasm.sh" ]; then
        wasm_tmp="$OUT_DIR/.wasm-build-$slug"
        mkdir -p "$wasm_tmp"
        if OUT="$wasm_tmp" "$topic_dir/demo/wasm/build-wasm.sh" \
                > "$wasm_tmp/build.log" 2>&1 &&
           [ -f "$wasm_tmp/index.html" ]; then
            mkdir -p "$OUT_DIR/$slug/wasm"
            cp "$wasm_tmp/index.html" "$OUT_DIR/$slug/wasm/index.html"
            wasm_demo_html="<a class=\"wasm-demo\" href=\"wasm/\">Run in browser</a>"
            echo "  wasm demo: $slug ($(wc -c < "$wasm_tmp/index.html") bytes)"
        else
            echo "  wasm demo: $slug SKIPPED (build failed; see log below)" >&2
            tail -5 "$wasm_tmp/build.log" >&2 || true
        fi
        rm -rf "$wasm_tmp"
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
            "WASM_DEMO" "$wasm_demo_html" \
            "LICENSE" "$license_html" \
            "ROOT" ".." \
        | _AWK_BODY="$article_body" awk 'BEGIN{body=ENVIRON["_AWK_BODY"]} {
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
    _AWK_TOC="$article_toc" awk 'BEGIN{toc=ENVIRON["_AWK_TOC"]} {
        idx = index($0, "{{TOC}}")
        if (idx > 0) {
            print substr($0, 1, idx-1) toc substr($0, idx+7)
        } else {
            print
        }
    }' "$OUT_DIR/$slug/index.html" > "$OUT_DIR/$slug/index.html.tmp"
    mv "$OUT_DIR/$slug/index.html.tmp" "$OUT_DIR/$slug/index.html"

    # ZIP source code directories (demo/) for download. Prefer the tracked
    # file list so build output (_out/, _build/, cmake build/, .gen/) stays out
    # of the archive; fall back to a plain recursive zip outside a git checkout.
    # The per-post download link on the index is built from the result.
    post_source_html=""
    if [ -d "$topic_dir/demo" ]; then
        zip_name="${slug}-source.zip"
        if (cd "$topic_dir" && git rev-parse --is-inside-work-tree >/dev/null 2>&1); then
            (cd "$topic_dir" && git ls-files demo | zip -q -@ -) > "$OUT_DIR/$slug/$zip_name"
        else
            (cd "$topic_dir" && zip -qr - demo) > "$OUT_DIR/$slug/$zip_name"
        fi
        zip_size="$(wc -c < "$OUT_DIR/$slug/$zip_name")"
        if [ "$zip_size" -ge 1073741824 ]; then
            zip_display="$(awk "BEGIN{printf \"%.1f GB\", $zip_size/1073741824}")"
        elif [ "$zip_size" -ge 1048576 ]; then
            zip_display="$(awk "BEGIN{printf \"%.1f MB\", $zip_size/1048576}")"
        else
            zip_display="$(awk "BEGIN{printf \"%.0f KB\", $zip_size/1024}")"
        fi
        post_source_html="<a class=\"post-source\" href=\"${slug}/${zip_name}\" download>Source ZIP (${zip_display})</a>"
    fi

    # Collect the index entry. Fields are HTML-escaped, the markup is kept on a
    # single line so the whole entry survives date-sorting as one record (TAB
    # separates the sort key from the markup).
    esc_title="$(html_escape "$title")"
    esc_abstract="$(html_escape "$abstract")"
    esc_category="$(html_escape "$category")"

    if [ -n "$revised_display" ]; then
        post_revised_html="<span class=\"post-revised\">rev. ${revised_display}</span>"
    else
        post_revised_html=""
    fi

    post_html="<article class=\"post\"><div class=\"post-meta\"><span class=\"post-category\">${esc_category}</span><time datetime=\"${date}\">${date_display}</time>${post_revised_html}</div><h2 class=\"post-title\"><a href=\"${slug}/index.html\">${esc_title}</a></h2><p class=\"post-abstract\">${esc_abstract}</p>${post_source_html}</article>"

    POSTS_UNSORTED="${POSTS_UNSORTED}
${date}	${post_html}"

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
# Sort entries by date (newest first)
# --------------------------------------------------------------------------

POSTS_HTML="$(printf '%s\n' "$POSTS_UNSORTED" | grep '	' | sort -t'	' -k1,1r | cut -f2-)"

# --------------------------------------------------------------------------
# Build index page
# --------------------------------------------------------------------------

# Inject the post list into the index template. awk via ENVIRON handles the
# multi-line markup safely (sed cannot carry newlines in a replacement).
_AWK_POSTS="$POSTS_HTML" awk 'BEGIN{posts=ENVIRON["_AWK_POSTS"]} {
    idx = index($0, "{{POSTS}}")
    if (idx > 0) {
        print substr($0, 1, idx-1)
        print posts
        print substr($0, idx+9)
    } else {
        print
    }
}' "$TEMPLATE_DIR/index.html" > "$OUT_DIR/index.html"

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
echo "Entries: $(printf '%s' "$POSTS_HTML" | grep -c '<article class="post">')"
