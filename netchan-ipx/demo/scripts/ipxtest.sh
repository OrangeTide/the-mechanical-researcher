#!/bin/sh
# IPX test harness (development tool).
#
# Builds the DOS binaries, ensures an ipxrelay is running, launches tracked
# DOSBox instances connected through it, and captures their stdout. It only
# ever manages the processes it starts (tracked by PID) and never touches
# other DOSBox or relay processes.
#
# Usage:
#   scripts/ipxtest.sh single <EXE>              one instance, EXE > O1.TXT
#   scripts/ipxtest.sh pair   <EXE> [srvarg]     server (EXE srvarg) + client
#
# Env: TMO=<seconds> per-instance timeout (default 18)
#      RELAY_PORT=<n> relay UDP port (default 19900)
#      IPXRELAY=<path> ipxrelay binary (default ~/Source/ipxrelay/ipxrelay)
set -u
DIR=$(cd "$(dirname "$0")/.." && pwd)
TMO="${TMO:-18}"
RELAY_PORT="${RELAY_PORT:-19900}"
IPXRELAY="${IPXRELAY:-$HOME/Source/ipxrelay/ipxrelay}"
export PATH="$WATCOM/binl:$PATH"
cd "$DIR" || exit 1

if wmake -f makefile 2>&1 | grep -iE 'error|warning'; then
    echo "BUILD FAILED"
    exit 1
fi

# Ensure the relay is up. FRESH=1 bounces it first to clear stale client
# registrations (which otherwise cause broadcast duplication across runs).
if [ "${FRESH:-0}" = "1" ]; then
    sh "$(dirname "$0")/relay.sh" restart >/dev/null
else
    sh "$(dirname "$0")/relay.sh" start >/dev/null
fi

mkconf() {  # $1=conf path  $2=command line
    cat > "$1" <<EOF
[sdl]
output=surface
[ipx]
ipx=true
[autoexec]
mount c $DIR
c:
ipxnet connect 127.0.0.1 $RELAY_PORT
$2
exit
EOF
}

# Launch directly in this shell (backgrounding inside $(...) would detach
# the child so `wait` could not track it).
DB="SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout -k2 $TMO dosbox -noconsole -conf"

case "${1:-}" in
single)
    EXE="$2"
    rm -f O1.TXT
    mkconf /tmp/ipx_a.conf "$EXE > O1.TXT"
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
        timeout -k2 "$TMO" dosbox -noconsole -conf /tmp/ipx_a.conf >/dev/null 2>&1 &
    PA=$!; wait "$PA"
    echo "===== O1.TXT (head) ====="; head -n 40 O1.TXT 2>/dev/null
    ;;
pair)
    EXE="$2"; SRVARG="${3:-s}"
    rm -f O1.TXT O2.TXT
    mkconf /tmp/ipx_a.conf "$EXE $SRVARG > O1.TXT"
    mkconf /tmp/ipx_b.conf "$EXE > O2.TXT"
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
        timeout -k2 "$TMO" dosbox -noconsole -conf /tmp/ipx_a.conf >/dev/null 2>&1 &
    PS=$!
    sleep 3
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
        timeout -k2 "$TMO" dosbox -noconsole -conf /tmp/ipx_b.conf >/dev/null 2>&1 &
    PC=$!
    wait "$PC"; wait "$PS"
    echo "===== O1.TXT (server, head) ====="; head -n 30 O1.TXT 2>/dev/null
    echo "===== O2.TXT (client, head) ====="; head -n 30 O2.TXT 2>/dev/null
    echo "===== summary ====="
    grep -aiE 'pass|fail|done|connected|disconnect' O2.TXT 2>/dev/null | tail -3
    ;;
*)
    echo "usage: $0 single EXE | pair EXE [srvarg]"
    exit 2
    ;;
esac
