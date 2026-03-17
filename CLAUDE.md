# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository Purpose

This is a collection of research topics, primarily gathered through agentic AI sessions, published as a blog on GitHub Pages.

## Research Topic Structure

Each topic gets its own directory, named in lower-case kebab-case (1–5 words), e.g. `quantum-error-correction/`.

Each research topic should include these sections:
- Introduction
- Abstract
- Findings
- Conclusion

Topics may also include supplementary files such as formal reports with larger datasets.

## Static Site Generator

The blog is built with a custom shell script pipeline using **lowdown** for markdown-to-HTML conversion. The site is published to a `gh-pages` branch.

### Markdown and HTML capabilities
- **lowdown** supports GFM tables natively.
- Inline SVG and raw HTML pass through with `--html-no-escapehtml --html-no-skiphtml` flags (already configured in `build.sh`).
- External SVG files can be referenced with `<img>` tags.
- **graphviz** (`dot`, `neato`, etc.) and **mscgen** are available for generating diagrams as SVG, which can be inlined into articles.

### Build
```sh
./build.sh            # builds to _build/
./build.sh /tmp/out   # builds to custom output directory
```

### Deploy
```sh
./deploy.sh           # builds the site and pushes to gh-pages branch
```

### Project layout
- `build.sh` — main build script
- `site/templates/` — HTML templates (`index.html`, `article.html`) with `{{KEY}}` placeholders
- `site/static/css/` — stylesheets (`index.css` for desk scene, `article.css` for articles)
- `site/static/js/card-stack.js` — scroll-hijacked card stack interaction
- `_build/` — generated output (gitignored)

### Topic frontmatter
Each topic's `index.md` requires YAML frontmatter:
```yaml
---
title: Topic Title
date: YYYY-MM-DD
abstract: One-line summary shown on the index card
category: tag-name
---
```

## Site Design

### Permalinks
Articles have permalinks — each article is at `/<slug>/index.html`. Cards and article pages show permalink icons.

### Index page
A scroll-hijacked card stack — index cards piled on a wooden desk with a pen and mug of tea. Cards lift off the deck, zoom to fill the viewport, and are flicked off the top of the screen on scroll. Scrolling back reverses everything like rewinding a movie. Clicking a card flips it right-to-left and transitions to the article. Each card shows title, abstract, date, and category. Cards must fit on-screen (no normal scrolling). The visual style is cartoon-like, not photorealistic.

### Typography
- Card titles: IBM Plex Sans
- Card body: IBM Plex Sans Condensed
- Card date/category: IBM Plex Mono
- Articles: TeX Gyre family (TBD)

### Colors
Warm palette derived from the desk scene — wood tones, cream/white cards, dark ink.

## Supplementary Files

When a research topic links to a `.md` file in a subdirectory (e.g. `demo/README.md`, `demo/PROTOCOL.md`), the build should produce an `.html` version alongside it. Link to both formats from the article so readers can choose.

## Screenshots

Use `tools/term-screenshot` to generate inline SVG terminal screenshots for demonstrating command-line utilities. The SVG can be inlined directly into article markdown (lowdown passes raw HTML/SVG through). Prefer screenshots over code blocks when showing actual program output.

## Website Design

Use the `frontend-design` and `web-design-guidelines` skills when designing or modifying the website's HTML/CSS/JS (index page, article layout, card stack interaction).

The website must support a mobile-friendly layout and interface for both the index page card stack and article pages.

## Demo Setup Documentation

When a research topic includes a demo with runnable code, the demo's README.md must include a clear step-by-step setup guide that a reader can follow to run it themselves. Include prerequisites, build instructions, launch order, and what to expect.

## Conventions

- The README.md maintains a list of topics with links (currently a TODO).
- When adding a new research topic, update the Topics section in README.md with a link to it.
- Each git commit should update a single topic only.
- A research topic is generally consolidated into a single commit (using fixup/amend/history rewrite as needed) before pushing.
