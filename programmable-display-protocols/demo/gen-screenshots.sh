#!/bin/sh
# gen-screenshots.sh : generate terminal and browser screenshots for the demo
# Copyright (c) 2026 — MIT-0 OR Public Domain
# Written with AI assistance (Claude, Anthropic)
#
# Usage: sh gen-screenshots.sh
#
# Generates:
#   screenshot-terminal.svg — terminal session launching server + apps
#   screenshot-browser.png  — browser showing quaoar display with windows
#
# Requirements: playwright (npx playwright), tools/term-screenshot

set -e

DEMO_DIR="$(cd "$(dirname "$0")" && pwd)"
TOOLS_DIR="$(cd "$DEMO_DIR/../tools" && pwd)"
TERM_SCREENSHOT="$TOOLS_DIR/term-screenshot"

# --- Terminal screenshot ---
# Simulated session showing the launch sequence
printf '%s\n' \
    '$ make' \
    'cc -Wall -Wextra -O2 -std=c99 -D_POSIX_C_SOURCE=200809L -o quaoar-server quaoar-server.c' \
    'cc -Wall -Wextra -O2 -std=c99 -D_POSIX_C_SOURCE=200809L   -c -o libquaoar.o libquaoar.c' \
    'ar rv libquaoar.a libquaoar.o' \
    'cc -Wall -Wextra -O2 -std=c99 -D_POSIX_C_SOURCE=200809L -o notepad notepad.c libquaoar.a' \
    'cc -Wall -Wextra -O2 -std=c99 -D_POSIX_C_SOURCE=200809L -o clock clock.c libquaoar.a' \
    '' \
    '$ ./quaoar-server &' \
    '[1] 4821' \
    'quaoar-server: ws://127.0.0.1:9090  socket=/tmp/quaoar-0' \
    '' \
    '$ ./notepad &' \
    '[2] 4822' \
    '' \
    '$ ./clock &' \
    '[3] 4823' \
    | "$TERM_SCREENSHOT" -t "Terminal" -w 720 -o "$DEMO_DIR/screenshot-terminal.svg"

# --- Browser screenshot ---
# Start quaoar-server, launch apps, open browser, capture

# Clean up any previous instances
pkill -f 'quaoar-server' 2>/dev/null || true
rm -f /tmp/quaoar-0
sleep 0.5

cd "$DEMO_DIR"
./quaoar-server &
SERVER_PID=$!
sleep 0.5

./notepad &
NOTEPAD_PID=$!
sleep 0.3

./clock &
CLOCK_PID=$!
sleep 0.5

# Use playwright to open the client and screenshot
CLIENT_URL="file://$DEMO_DIR/quaoar-client.html"

node - "$CLIENT_URL" "$DEMO_DIR/screenshot-browser.png" << 'JSEOF'
const { chromium } = require('playwright');

(async () => {
    const url = process.argv[2];
    const outfile = process.argv[3];

    const browser = await chromium.launch({ headless: true });
    const page = await browser.newPage({
        viewport: { width: 900, height: 600 }
    });
    await page.goto(url);

    // Wait for WebSocket connection and widgets to render
    await page.waitForTimeout(2000);

    await page.screenshot({ path: outfile });
    await browser.close();
})();
JSEOF

# Cleanup
kill $CLOCK_PID $NOTEPAD_PID $SERVER_PID 2>/dev/null || true
wait 2>/dev/null || true
rm -f /tmp/quaoar-0

echo "Generated screenshot-terminal.svg"
echo "Generated screenshot-browser.png"
