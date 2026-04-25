/* lex.c : hand-written lexer for TinC */

#include "tinc.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char *src_buf;
static const char *src_file;
static const char *p;
static int line;
static int peeked;
static struct token peek_tok;

struct kw {
	const char *s;
	int k;
};

static const struct kw keywords[] = {
	{ "int",      T_INT },
	{ "char",     T_CHAR },
	{ "if",       T_IF },
	{ "else",     T_ELSE },
	{ "while",    T_WHILE },
	{ "break",    T_BREAK },
	{ "continue", T_CONTINUE },
	{ "return",   T_RETURN },
	{ NULL, 0 },
};

void
lex_init(const char *src, const char *filename)
{
	src_buf = src;
	src_file = filename ? filename : "<input>";
	p = src;
	line = 1;
	peeked = 0;
}

static void
skip_ws(void)
{
	for (;;) {
		while (*p == ' ' || *p == '\t' || *p == '\r')
			p++;
		if (*p == '\n') {
			line++;
			p++;
			continue;
		}
		if (p[0] == '/' && p[1] == '/') {
			while (*p && *p != '\n')
				p++;
			continue;
		}
		if (p[0] == '/' && p[1] == '*') {
			p += 2;
			while (*p && !(p[0] == '*' && p[1] == '/')) {
				if (*p == '\n')
					line++;
				p++;
			}
			if (*p)
				p += 2;
			continue;
		}
		break;
	}
}

static int
hex_digit(int c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return 10 + c - 'a';
	if (c >= 'A' && c <= 'F') return 10 + c - 'A';
	return -1;
}

static int
read_escape(void)
{
	int c, h1, h2;

	c = *p++;
	switch (c) {
	case 'n':  return '\n';
	case 't':  return '\t';
	case 'r':  return '\r';
	case '0':  return '\0';
	case '\\': return '\\';
	case '\'': return '\'';
	case '"':  return '"';
	case 'x':
		h1 = hex_digit((unsigned char)*p);
		if (h1 < 0)
			die("%s:%d: bad \\x escape", src_file, line);
		p++;
		h2 = hex_digit((unsigned char)*p);
		if (h2 < 0)
			return h1;
		p++;
		return (h1 << 4) | h2;
	default:
		die("%s:%d: unknown escape \\%c", src_file, line, c);
	}
	return 0;
}

static struct token
make(int k)
{
	struct token t;

	t.kind = k;
	t.nval = 0;
	t.sval = NULL;
	t.slen = 0;
	t.line = line;
	return t;
}

static struct token
lex_one(void)
{
	struct token t;
	const char *start;
	int c;
	long n;
	char *buf;
	size_t cap, len;

	skip_ws();
	t = make(T_EOF);
	if (*p == '\0')
		return t;

	c = (unsigned char)*p;

	if (isalpha(c) || c == '_') {
		const struct kw *kw;
		size_t sz;
		start = p;
		while (isalnum((unsigned char)*p) || *p == '_')
			p++;
		sz = (size_t)(p - start);
		for (kw = keywords; kw->s; kw++) {
			if (strlen(kw->s) == sz &&
			    memcmp(kw->s, start, sz) == 0)
				return make(kw->k);
		}
		t = make(T_IDENT);
		t.sval = xstrndup(start, sz);
		return t;
	}

	if (isdigit(c)) {
		n = 0;
		if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
			int d;
			p += 2;
			if (hex_digit((unsigned char)*p) < 0)
				die("%s:%d: bad hex literal",
				    src_file, line);
			while ((d = hex_digit((unsigned char)*p)) >= 0) {
				n = (n << 4) | d;
				p++;
			}
		} else {
			while (isdigit((unsigned char)*p)) {
				n = n * 10 + (*p - '0');
				p++;
			}
		}
		t = make(T_NUMBER);
		t.nval = n;
		return t;
	}

	if (c == '\'') {
		p++;
		if (*p == '\\') {
			p++;
			n = read_escape();
		} else {
			n = (unsigned char)*p++;
		}
		if (*p != '\'')
			die("%s:%d: unterminated char literal",
			    src_file, line);
		p++;
		t = make(T_CHARLIT);
		t.nval = n;
		return t;
	}

	if (c == '"') {
		p++;
		cap = 16;
		len = 0;
		buf = xmalloc(cap);
		while (*p && *p != '"') {
			int ch = 0;
			if (*p == '\\') {
				p++;
				ch = read_escape();
			} else if (*p == '\n') {
				die("%s:%d: newline in string",
				    src_file, line);
			} else {
				ch = (unsigned char)*p++;
			}
			if (len + 1 >= cap) {
				cap *= 2;
				buf = realloc(buf, cap);
				if (!buf)
					die("oom");
			}
			buf[len++] = (char)ch;
		}
		if (*p != '"')
			die("%s:%d: unterminated string", src_file, line);
		p++;
		buf[len] = '\0';
		t = make(T_STRING);
		t.sval = buf;
		t.slen = (int)len;
		return t;
	}

	switch (c) {
	case '(': p++; return make(T_LPAREN);
	case ')': p++; return make(T_RPAREN);
	case '{': p++; return make(T_LBRACE);
	case '}': p++; return make(T_RBRACE);
	case '[': p++; return make(T_LBRACK);
	case ']': p++; return make(T_RBRACK);
	case ',': p++; return make(T_COMMA);
	case ';': p++; return make(T_SEMI);
	case '~': p++; return make(T_TILDE);
	case '*': p++; return make(T_STAR);
	case '/': p++; return make(T_SLASH);
	case '%': p++; return make(T_PERCENT);
	case '^': p++; return make(T_CARET);
	case '=':
		p++;
		if (*p == '=') { p++; return make(T_EQ); }
		return make(T_ASSIGN);
	case '!':
		p++;
		if (*p == '=') { p++; return make(T_NE); }
		return make(T_BANG);
	case '<':
		p++;
		if (*p == '=') { p++; return make(T_LE); }
		if (*p == '<') { p++; return make(T_SHL); }
		return make(T_LT);
	case '>':
		p++;
		if (*p == '=') { p++; return make(T_GE); }
		if (*p == '>') { p++; return make(T_SHR); }
		return make(T_GT);
	case '&':
		p++;
		if (*p == '&') { p++; return make(T_ANDAND); }
		return make(T_AMP);
	case '|':
		p++;
		if (*p == '|') { p++; return make(T_OROR); }
		return make(T_PIPE);
	case '+':
		p++;
		return make(T_PLUS);
	case '-':
		p++;
		return make(T_MINUS);
	}

	die("%s:%d: unexpected character 0x%02x", src_file, line, c);
	return t;
}

struct token
lex_next(void)
{
	if (peeked) {
		peeked = 0;
		return peek_tok;
	}
	return lex_one();
}

struct token
lex_peek(void)
{
	if (!peeked) {
		peek_tok = lex_one();
		peeked = 1;
	}
	return peek_tok;
}
