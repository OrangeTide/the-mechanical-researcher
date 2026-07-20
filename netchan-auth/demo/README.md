# netchan-auth demo: host keys, known_hosts, and an ssh-shaped login

An encrypted echo service where the client checks the server's identity
against a `known_hosts` file, and the server checks the client's identity with
a public key or a password. The socket, the retransmit timer, the keyboard,
the password prompt, and `Ctrl-C` all arrive through one `poll()`, and nothing
blocks.

This is the runnable companion to the article. The code is public domain
(CC0-1.0), vendored so it builds from a bare checkout with no package manager.

## What Is In Here

| File | What it is |
|---|---|
| `nc_crypto.c/.h` | the transport decorator: X25519 handshake, XChaCha20-Poly1305 per packet, and now a long-term identity key plus a `verify_peer` callback |
| `nc_auth.c/.h` | the login conversation, with no idea what carries it |
| `keystore.c/.h` | the five on-disk formats: `known_hosts`, `host_key`, `authorized_keys`, `passwd`, and the client key file |
| `auth_link.c/.h` | the glue that drives netchan, nc_crypto, and nc_auth on the iox loop |
| `auth_server.c` | the echo server, and `--adduser` for enrolling a password |
| `auth_client.c` | the echo client, with the known-hosts decision and the login |
| `nc_keygen.c` | makes a client identity key, optionally passphrase protected |
| `netchan.c/.h`, `nc_udp.c/.h`, `nc_addr.h` | the protocol core and its UDP backend, unchanged |
| `iox/` | the vendored event loop (see `VENDORING.md`) |
| `third_party/` | vendored monocypher (see `VENDORING.md`) |

## Prerequisites

- A C11 compiler (gcc or clang) and GNU make.
- A checkout of modular-make in a sibling directory. The `GNUmakefile` here
  is a three-line stub that includes it. Point it elsewhere with
  `make MODULAR_MAKE=/path/to/modular-make`.
- Linux, or another platform with `getrandom(2)`.

Nothing else. No libsodium, no OpenSSL, no autotools.

## Build

```sh
make            # auth_server, auth_client, nc_keygen, and three tests
make run-tests  # the login state machine, the key files, and a loopback echo
```

Binaries land in `_out/<triplet>/bin/`. The rest of this guide assumes that
directory is on your `PATH`:

```sh
export PATH="$PWD/_out/$(uname -m)-linux-gnu/bin:$PATH"
```

## Run It

Work in a scratch directory, because the programs create key files in the
current directory.

```sh
mkdir /tmp/nc-auth && cd /tmp/nc-auth
```

### 1. Make a Client Key

```sh
nc_keygen -f id_netchan
```

It asks for a passphrase. Press Enter twice to leave the key unencrypted, or
type one to have the secret sealed with XChaCha20-Poly1305 under an Argon2id
key. Either way it prints the line to enrol:

```
add this line to the server's authorized_keys:

    <username> 3d92b22bf90f15d29dc623e1dd2cb76c92f0e9b23690e25f4f529b4c91d2dcc1
```

### 2. Enrol the Key on the Server

```sh
echo "alice 3d92b22b...dcc1" > authorized_keys
```

### 3. Start the Server

```sh
auth_server --port 9000
```

On its first run the server generates `host_key`, its long-term identity, and
prints the public half:

```
listening on udp/9000
host key 21b3da337618c2f037c1cbc164c12d7baa97e7cb9342fe49af0aa938a0b47a49
```

That file is the thing clients will remember. Keep it.

### 4. Connect

In another terminal, in the same directory:

```sh
auth_client --port 9000 --user alice
```

First contact records the host key and logs in with the key pair:

```
connecting to 127.0.0.1:9000 as alice
* unknown host 127.0.0.1, key 21b3da33...7a49
* permanently added to known_hosts
* authenticated as alice, type to echo
netchan>
```

Type a line and it comes back prefixed with `[echo]`, then the prompt is
redrawn. `Ctrl-D` ends the session and `Ctrl-C` shuts either side down
cleanly.

The prompt appears on a terminal only, so piped input produces the same output
it always did. There is no line editor here: the terminal stays in canonical
mode, which is what already gives you backspace and `Ctrl-W`, and each read
delivers a whole line. An echo that arrives while a line is half typed will
land in the middle of it.

`prompt.c` is the one place the demo touches `termios`. A line editor would
not remove it: linenoise and GNU readline both echo what you type, by design,
so a password prompt is exactly the case they do not cover. Clearing
`ECHO` is something you write yourself whichever library you use, and once it
is written the incremental read on top of it is about twenty lines.

Connect a second time and the host-key lines are gone. The key matched what
was on file, so there was nothing to report.

## Things Worth Trying

### A Password Login

Give a user a password and connect without a key:

```sh
auth_server --adduser bob          # prompts twice, appends an Argon2id hash
auth_client --port 9000 --user bob --key /nonexistent
```

The client offers public-key first, finds no key file, and falls back to the
password prompt. `--password` skips the key even when one exists.

A wrong password is refused and the client exits non-zero. The server spends
the same Argon2 work on an account that does not exist as on one that does, so
a stopwatch cannot be used to find out which names are real.

### The Host Key Changing Under You

This is the warning the whole `known_hosts` scheme exists to produce. Move the
server's identity aside and start it again:

```sh
mv host_key host_key.orig
auth_server --port 9000
```

Then connect:

```
  WARNING: THE HOST IDENTIFICATION HAS CHANGED
  Someone could be eavesdropping on you right now.
  The key for 127.0.0.1 in known_hosts is
    21b3da33...7a49
  but the server presented
    55f054d4...643d
  Remove the stale line if you know why it changed.

client: host key not accepted
```

The client aborts before it sends a username, let alone a password. Put the
original key back with `mv host_key.orig host_key`.

### An Unauthorised Key

Generate a second key and try to use it without enrolling it:

```sh
nc_keygen -f id_rogue -N ""
auth_client --port 9000 --user alice --key id_rogue
```

The signature is valid, so the server knows the client really does hold that
secret. It simply is not on the list, so the key is refused and the client
falls back to the password prompt.

## Tests

```sh
make run-tests
```

Three programs, 36 checks:

- **`test_nc_auth`** drives both halves of the login in one process with a
  message queue between them and no sockets involved. It covers the key path,
  the password fallback, a wrong password, an unauthorised name, and the
  important one: a signature captured from a live session is replayed at a
  server with a different session id, and must be rejected.
- **`test_keystore`** exercises the five file formats in a temporary
  directory, including a wrong key-file passphrase being reported as a
  passphrase error rather than as corruption.
- **`test_auth_link`** runs a client and a server on one event loop over real
  loopback sockets, checks that an application round trip completes, then
  repeats it with a mismatched host key and checks that nothing is sent. Its
  last case points a client at a socket that never answers and counts the
  datagrams that come out, which is the regression test for a periodic timer
  that was once scheduled and then quietly retired.

The suite is clean under AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
make clean
make CFLAGS="-fsanitize=address,undefined -g -O1" \
     LDFLAGS="-fsanitize=address,undefined"
make run-tests
```

## Files the Demo Creates

| File | Side | Contents |
|---|---|---|
| `host_key` | server | the server's X25519 identity secret, mode 0600 |
| `authorized_keys` | server | `<user> <hex Ed25519 public key>` per line |
| `passwd` | server | `<user> <hex salt> <hex Argon2id hash>` per line |
| `known_hosts` | client | `<host> <hex X25519 public key>` per line |
| `id_netchan` | client | the client's Ed25519 key pair, optionally sealed |

All of them are plain text with hex fields, so they can be read, diffed, and
edited by hand. That is deliberate. The trust decisions live in files an
operator controls, not inside the protocol.

## Limits

This is a demo, and it is honest about what it is not.

- **One client at a time.** `auth_link` holds a single netchan connection. A
  real server would key sessions by source address and connection id.
- **No rate limiting.** Six failed attempts end a session, but nothing stops a
  client reconnecting immediately. A real deployment needs a backoff.
- **No revocation with a deadline.** Removing a line from `authorized_keys`
  denies the next login, not a session already running.
- **First contact is unauthenticated.** The client records an unknown host key
  rather than prompting, which is ssh's `StrictHostKeyChecking=accept-new`. An
  attacker present for that one connection gets its own key pinned. Checking
  the fingerprint out of band is the only real fix, and it is a policy the
  `verify_peer` callback is free to implement.
