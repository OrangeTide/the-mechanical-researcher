/* quaoar-server.c : websocket-to-unix-socket bridge for quaoar display protocol */
/* Copyright (c) 2026 — MIT-0 OR Public Domain */
/* Written with AI assistance (Claude, Anthropic) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <poll.h>

#define MAX_APPS      16
#define MAX_MSG       65536
#define BUF_SIZE      (MAX_MSG * 2)
#define WS_GUID       "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/****************************************************************
 * SHA-1 — single-shot implementation for WebSocket handshake
 * Based on sha1tiny.c by Jon Mayo (PUBLIC DOMAIN, June 2010)
 ****************************************************************/

#define ROL32(v, b) (((v) << (b)) | ((v) >> (32 - (b))))

static void
sha1(const void *data, size_t len, uint8_t md[20])
{
    uint32_t h[5] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0 };
    uint32_t v[5], w[80];
    uint64_t cnt = 0;
    unsigned i, tail = 0, length = 0, done = 0;

    while (!done) {
        for (i = 0; i < 64; i++) {
            uint32_t n = w[i / 4], d = 0;
            if (len > i) {
                d = *(const uint8_t *)data;
                data = (const uint8_t *)data + 1;
                cnt += 8;
            } else if (!tail && i == len) {
                d = 0x80;
                tail = 1;
            } else if (tail && i >= 56) {
                if (i == 56) length = 1;
                if (length) {
                    d = (cnt >> (64 - (i - 56 + 1) * 8)) & 255;
                    if (i == 63) done = 1;
                }
            }
            switch (i % 4) {
            case 0: n = (n & 0x00ffffff) | (d << 24); break;
            case 1: n = (n & 0xff00ffff) | (d << 16); break;
            case 2: n = (n & 0xffff00ff) | (d << 8); break;
            case 3: n = (n & 0xffffff00) | d; break;
            }
            w[i / 4] = n;
        }
        len = len > 64 ? len - 64 : 0;
        for (i = 16; i < 80; i++)
            w[i] = ROL32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        for (i = 0; i < 5; i++) v[i] = h[i];
        for (i = 0; i < 80; i++) {
            uint32_t f, k, t;
            if      (i < 20) { f = (v[1] & v[2]) | (~v[1] & v[3]); k = 0x5a827999; }
            else if (i < 40) { f = v[1] ^ v[2] ^ v[3];             k = 0x6ed9eba1; }
            else if (i < 60) { f = (v[1] & v[2]) | (v[1] & v[3]) | (v[2] & v[3]); k = 0x8f1bbcdc; }
            else              { f = v[1] ^ v[2] ^ v[3];             k = 0xca62c1d6; }
            t = ROL32(v[0], 5) + f + v[4] + k + w[i];
            v[4] = v[3]; v[3] = v[2]; v[2] = ROL32(v[1], 30); v[1] = v[0]; v[0] = t;
        }
        for (i = 0; i < 5; i++) h[i] += v[i];
    }
    for (i = 0; i < 5; i++) {
        *md++ = h[i] >> 24; *md++ = h[i] >> 16;
        *md++ = h[i] >> 8;  *md++ = h[i];
    }
}

/****************************************************************
 * Base64
 ****************************************************************/

static const char b64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int
base64_encode(const uint8_t *in, int len, char *out)
{
    int i, j = 0;

    for (i = 0; i + 2 < len; i += 3) {
        out[j++] = b64[in[i] >> 2];
        out[j++] = b64[((in[i] & 3) << 4) | (in[i + 1] >> 4)];
        out[j++] = b64[((in[i + 1] & 0xf) << 2) | (in[i + 2] >> 6)];
        out[j++] = b64[in[i + 2] & 0x3f];
    }
    if (i < len) {
        out[j++] = b64[in[i] >> 2];
        if (i + 1 < len) {
            out[j++] = b64[((in[i] & 3) << 4) | (in[i + 1] >> 4)];
            out[j++] = b64[(in[i + 1] & 0xf) << 2];
        } else {
            out[j++] = b64[(in[i] & 3) << 4];
            out[j++] = '=';
        }
        out[j++] = '=';
    }
    out[j] = 0;
    return j;
}

/****************************************************************
 * Netstring framing (djb)
 ****************************************************************/

/** Extract one netstring from buf. Returns payload length or -1. */
static int
ns_decode(const uint8_t *buf, int blen, uint8_t *msg, int max,
      int *consumed)
{
    int colon = -1;

    for (int i = 0; i < blen && i < 10; i++) {
        if (buf[i] == ':') { colon = i; break; }
        if (buf[i] < '0' || buf[i] > '9') return -1;
    }
    if (colon < 1) return -1;
    if (colon > 1 && buf[0] == '0') return -1; /* leading zero */

    int len = 0;
    for (int i = 0; i < colon; i++)
        len = len * 10 + (buf[i] - '0');

    int total = colon + 1 + len + 1;
    if (blen < total) return -1; /* incomplete */
    if (buf[total - 1] != ',') return -1;
    if (len > max) return -1;

    memcpy(msg, buf + colon + 1, len);
    *consumed = total;
    return len;
}

/****************************************************************
 * WebSocket protocol
 ****************************************************************/

static int
read_all(int fd, void *buf, int n)
{
    int got = 0;

    while (got < n) {
        int r = read(fd, (uint8_t *)buf + got, n - got);
        if (r <= 0) return -1;
        got += r;
    }
    return got;
}

static int
write_all(int fd, const void *buf, int n)
{
    int sent = 0;

    while (sent < n) {
        int w = write(fd, (const uint8_t *)buf + sent, n - sent);
        if (w <= 0) return -1;
        sent += w;
    }
    return sent;
}

/** Read HTTP upgrade request, extract Sec-WebSocket-Key, complete handshake. */
static int
ws_handshake(int fd)
{
    char req[4096], key[128] = {0};
    int pos = 0;

    while (pos < (int)sizeof(req) - 1) {
        int r = read(fd, req + pos, 1);
        if (r <= 0) return -1;
        pos++;
        if (pos >= 4 && memcmp(req + pos - 4, "\r\n\r\n", 4) == 0)
            break;
    }
    req[pos] = 0;

    /* extract Sec-WebSocket-Key */
    char *kp = strstr(req, "Sec-WebSocket-Key:");
    if (!kp) kp = strstr(req, "sec-websocket-key:");
    if (!kp) {
        fprintf(stderr, "no websocket key\n");
        return -1;
    }
    kp += 18;
    while (*kp == ' ') kp++;
    int kl = 0;
    while (kp[kl] && kp[kl] != '\r' && kp[kl] != '\n')
        key[kl] = kp[kl], kl++;
    key[kl] = 0;

    /* compute accept key: SHA-1(key + GUID) -> base64 */
    char combined[128];
    int clen = snprintf(combined, sizeof(combined), "%s%s", key, WS_GUID);
    uint8_t digest[20];
    sha1(combined, clen, digest);
    char accept[32];
    base64_encode(digest, 20, accept);

    char resp[512];
    int rlen = snprintf(resp, sizeof(resp),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n\r\n", accept);
    return write_all(fd, resp, rlen);
}

/** Read one WebSocket frame. Returns payload length or -1. */
static int
ws_read(int fd, uint8_t *payload, int max, int *opcode)
{
    uint8_t hdr[2];

    if (read_all(fd, hdr, 2) < 0) return -1;
    *opcode = hdr[0] & 0x0f;
    int masked = (hdr[1] & 0x80) != 0;
    uint64_t len = hdr[1] & 0x7f;
    if (len == 126) {
        uint8_t ext[2];
        if (read_all(fd, ext, 2) < 0) return -1;
        len = (uint16_t)ext[0] << 8 | ext[1];
    } else if (len == 127) {
        uint8_t ext[8];
        if (read_all(fd, ext, 8) < 0) return -1;
        len = 0;
        for (int i = 0; i < 8; i++)
            len = (len << 8) | ext[i];
    }
    if (len > (uint64_t)max) return -1;
    uint8_t mask[4] = {0};
    if (masked && read_all(fd, mask, 4) < 0) return -1;
    if (read_all(fd, payload, (int)len) < 0) return -1;
    if (masked)
        for (uint64_t i = 0; i < len; i++)
            payload[i] ^= mask[i % 4];
    return (int)len;
}

/** Write a WebSocket text frame (unmasked, server-to-client). */
static int
ws_write(int fd, const uint8_t *payload, int len)
{
    uint8_t hdr[10];
    int hlen = 0;

    hdr[hlen++] = 0x81; /* FIN + text opcode */
    if (len < 126) {
        hdr[hlen++] = (uint8_t)len;
    } else if (len < 65536) {
        hdr[hlen++] = 126;
        hdr[hlen++] = (len >> 8) & 0xff;
        hdr[hlen++] = len & 0xff;
    } else {
        hdr[hlen++] = 127;
        for (int i = 7; i >= 0; i--)
            hdr[hlen++] = ((uint64_t)len >> (i * 8)) & 0xff;
    }
    if (write_all(fd, hdr, hlen) < 0) return -1;
    return write_all(fd, payload, len);
}

static int
ws_pong(int fd, const uint8_t *data, int len)
{
    uint8_t hdr[2] = { 0x8A, (uint8_t)len }; /* FIN + pong */

    if (write_all(fd, hdr, 2) < 0) return -1;
    if (len > 0) return write_all(fd, data, len);
    return 0;
}

/****************************************************************
 * App connection management
 ****************************************************************/

struct app {
    int fd;
    int id;
    uint8_t rbuf[BUF_SIZE];
    int rlen;
};

static struct app apps[MAX_APPS];
static int ws_fd = -1;
static int next_app_id = 1;
static char sock_path[108] = "/tmp/quaoar-0";
static volatile int running = 1;

static void
on_signal(int sig)
{
    (void)sig;
    running = 0;
}

static struct app *
app_add(int fd)
{
    for (int i = 0; i < MAX_APPS; i++) {
        if (apps[i].fd <= 0) {
            apps[i].fd = fd;
            apps[i].id = next_app_id++;
            apps[i].rlen = 0;
            fprintf(stderr, "app %d connected\n", apps[i].id);
            return &apps[i];
        }
    }
    return NULL;
}

static void
app_remove(int idx)
{
    if (apps[idx].fd <= 0)
        return;
    fprintf(stderr, "app %d disconnected\n", apps[idx].id);
    if (ws_fd > 0) {
        char msg[128];
        int n = snprintf(msg, sizeof(msg),
            "-1 disconnect %d", apps[idx].id);
        ws_write(ws_fd, (uint8_t *)msg, n);
    }
    close(apps[idx].fd);
    apps[idx].fd = 0;
}

/****************************************************************
 * Message routing
 ****************************************************************/

/** Forward a message to the browser, prepending an ID. */
static void
forward_to_ws(int id, const uint8_t *msg, int len)
{
    char out[MAX_MSG];
    int n;

    if (ws_fd <= 0 || len == 0) return;
    n = snprintf(out, sizeof(out), "%d %.*s",
             id, len, (const char *)msg);
    if (n > 0 && n < (int)sizeof(out))
        ws_write(ws_fd, (uint8_t *)out, n);
}

/** Send a netstring to an app socket. */
static void
send_to_app(int fd, const uint8_t *data, int len)
{
    char frame[MAX_MSG + 16];
    int hlen = snprintf(frame, 16, "%d:", len);

    memcpy(frame + hlen, data, len);
    frame[hlen + len] = ',';
    write_all(fd, frame, hlen + len + 1);
}

/** Forward a message from browser to the appropriate app. */
static void
forward_to_app(const uint8_t *msg, int len)
{
    /* first token is app ID (may be negative for system channels) */
    int app_id = 0, pos = 0, neg = 0;

    if (pos < len && msg[pos] == '-') {
        neg = 1;
        pos++;
    }
    while (pos < len && msg[pos] >= '0' && msg[pos] <= '9')
        app_id = app_id * 10 + (msg[pos++] - '0');
    if (neg) app_id = -app_id;
    if (pos >= len || msg[pos] != ' ') return;
    pos++; /* skip space */

    int plen = len - pos;

    /* negative ID = system channel, broadcast to all apps */
    if (app_id < 0) {
        for (int i = 0; i < MAX_APPS; i++)
            if (apps[i].fd > 0)
                send_to_app(apps[i].fd,
                        msg + pos, plen);
        return;
    }

    /* find the app */
    int idx = -1;
    for (int i = 0; i < MAX_APPS; i++)
        if (apps[i].fd > 0 && apps[i].id == app_id) {
            idx = i;
            break;
        }
    if (idx < 0) return;

    send_to_app(apps[idx].fd, msg + pos, plen);
}

/** Process buffered data from an app socket, extract netstrings. */
static void
app_process(int idx)
{
    while (apps[idx].rlen > 0) {
        uint8_t msg[MAX_MSG];
        int consumed;
        int mlen = ns_decode(apps[idx].rbuf, apps[idx].rlen,
                     msg, sizeof(msg), &consumed);
        if (mlen < 0) break;
        if (mlen > 0)
            forward_to_ws(apps[idx].id, msg, mlen);
        if (consumed > 0) {
            memmove(apps[idx].rbuf,
                apps[idx].rbuf + consumed,
                apps[idx].rlen - consumed);
            apps[idx].rlen -= consumed;
        }
    }
}

/****************************************************************
 * Main
 ****************************************************************/

int
main(int argc, char **argv)
{
    int port = 9090;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc)
            port = atoi(argv[++i]);
        else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc)
            snprintf(sock_path, sizeof(sock_path), "%s",
                 argv[++i]);
        else {
            fprintf(stderr,
                "usage: quaoar-server [-p port]"
                " [-s socket_path]\n");
            return 1;
        }
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGPIPE, SIG_IGN);

    /* TCP listener for WebSocket */
    int tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(tcp_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
    };
    if (bind(tcp_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind tcp");
        return 1;
    }
    listen(tcp_fd, 1);

    /* Unix socket listener for apps */
    unlink(sock_path);
    int unix_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un uaddr = { .sun_family = AF_UNIX };
    snprintf(uaddr.sun_path, sizeof(uaddr.sun_path), "%s", sock_path);
    if (bind(unix_fd, (struct sockaddr *)&uaddr, sizeof(uaddr)) < 0) {
        perror("bind unix");
        return 1;
    }
    listen(unix_fd, MAX_APPS);

    fprintf(stderr, "quaoar-server: ws://127.0.0.1:%d  socket=%s\n",
        port, sock_path);
    memset(apps, 0, sizeof(apps));

    while (running) {
        struct pollfd fds[2 + MAX_APPS + 1];
        int nfds = 0;

        fds[nfds++] = (struct pollfd){ .fd = tcp_fd, .events = POLLIN };
        fds[nfds++] = (struct pollfd){ .fd = unix_fd, .events = POLLIN };
        for (int i = 0; i < MAX_APPS; i++)
            if (apps[i].fd > 0)
                fds[nfds++] = (struct pollfd){
                    .fd = apps[i].fd, .events = POLLIN,
                };
        if (ws_fd > 0)
            fds[nfds++] = (struct pollfd){
                .fd = ws_fd, .events = POLLIN,
            };

        if (poll(fds, nfds, 1000) < 0) {
            if (errno == EINTR) continue;
            break;
        }

        for (int fi = 0; fi < nfds; fi++) {
            if (!(fds[fi].revents & (POLLIN | POLLHUP | POLLERR)))
                continue;

            /* new WebSocket connection */
            if (fds[fi].fd == tcp_fd) {
                int cfd = accept(tcp_fd, NULL, NULL);
                if (cfd < 0) continue;
                if (ws_fd > 0) {
                    fprintf(stderr,
                        "replacing existing browser"
                        " connection\n");
                    close(ws_fd);
                    for (int i = 0; i < MAX_APPS; i++)
                        app_remove(i);
                }
                if (ws_handshake(cfd) < 0) {
                    fprintf(stderr,
                        "websocket handshake"
                        " failed\n");
                    close(cfd);
                } else {
                    ws_fd = cfd;
                    fprintf(stderr, "browser connected\n");
                    char hello[256];
                    int hlen = snprintf(hello,
                        sizeof(hello),
                        "-1 hello %s",
                        sock_path);
                    ws_write(ws_fd,
                         (uint8_t *)hello,
                         hlen);
                }
                continue;
            }

            /* new app connection */
            if (fds[fi].fd == unix_fd) {
                int afd = accept(unix_fd, NULL, NULL);
                if (afd < 0) continue;
                if (!app_add(afd)) {
                    fprintf(stderr, "too many apps\n");
                    close(afd);
                }
                continue;
            }

            /* data from browser */
            if (fds[fi].fd == ws_fd) {
                uint8_t payload[MAX_MSG];
                int opcode;
                int plen = ws_read(ws_fd, payload,
                           sizeof(payload), &opcode);
                if (plen < 0) {
                    fprintf(stderr,
                        "browser disconnected\n");
                    close(ws_fd);
                    ws_fd = -1;
                    for (int i = 0; i < MAX_APPS; i++)
                        app_remove(i);
                    continue;
                }
                if (opcode == 0x8) { /* close */
                    fprintf(stderr,
                        "browser sent close\n");
                    close(ws_fd);
                    ws_fd = -1;
                    for (int i = 0; i < MAX_APPS; i++)
                        app_remove(i);
                } else if (opcode == 0x9) { /* ping */
                    ws_pong(ws_fd, payload, plen);
                } else if (opcode == 0x1 && plen > 0) {
                    payload[plen] = 0;
                    forward_to_app(payload, plen);
                }
                continue;
            }

            /* data from an app */
            for (int i = 0; i < MAX_APPS; i++) {
                if (apps[i].fd <= 0 ||
                    fds[fi].fd != apps[i].fd)
                    continue;
                int space = BUF_SIZE - apps[i].rlen;
                if (space <= 0) {
                    app_remove(i);
                    break;
                }
                int r = read(apps[i].fd,
                         apps[i].rbuf + apps[i].rlen,
                         space);
                if (r <= 0) {
                    app_remove(i);
                    break;
                }
                apps[i].rlen += r;
                app_process(i);
                break;
            }
        }
    }

    /* cleanup */
    if (ws_fd > 0) close(ws_fd);
    for (int i = 0; i < MAX_APPS; i++)
        if (apps[i].fd > 0) close(apps[i].fd);
    close(tcp_fd);
    close(unix_fd);
    unlink(sock_path);
    fprintf(stderr, "quaoar-server: shutdown\n");
    return 0;
}
