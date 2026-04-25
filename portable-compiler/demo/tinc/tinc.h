/* tinc.h : TinC front-end declarations */

#ifndef TINC_H
#define TINC_H

#include "ir.h"

/****************************************************************
 * Lexer
 ****************************************************************/

enum tok {
	T_EOF = 0,
	T_INT, T_CHAR, T_IF, T_ELSE, T_WHILE,
	T_BREAK, T_CONTINUE, T_RETURN,
	T_IDENT, T_NUMBER, T_STRING, T_CHARLIT,
	T_LPAREN, T_RPAREN, T_LBRACE, T_RBRACE, T_LBRACK, T_RBRACK,
	T_COMMA, T_SEMI, T_ASSIGN,
	T_PLUS, T_MINUS, T_STAR, T_SLASH, T_PERCENT,
	T_AMP, T_PIPE, T_CARET, T_TILDE, T_BANG,
	T_ANDAND, T_OROR,
	T_EQ, T_NE, T_LT, T_LE, T_GT, T_GE,
	T_SHL, T_SHR,
	T_INC, T_DEC,
};

struct token {
	int kind;
	long nval;
	char *sval;
	int slen;
	int line;
};

void lex_init(const char *src, const char *filename);
struct token lex_next(void);
struct token lex_peek(void);

/****************************************************************
 * AST
 ****************************************************************/

enum node_kind {
	N_PROGRAM, N_FUNC, N_GLOBAL,
	N_BLOCK, N_IF, N_WHILE, N_BREAK, N_CONTINUE, N_RETURN, N_EXPR_STMT,
	N_BINOP, N_UNOP, N_ASSIGN, N_INDEX, N_CALL, N_NAME,
	N_NUM, N_STR, N_CHARLIT,
	N_INIT_LIST,
};

struct node {
	int kind;
	int op;
	int line;

	struct node *a, *b, *c;

	struct node *next;

	char *name;
	long ival;
	char *sval;
	int slen;

	int is_ptr;
	int arr_size;
	int base_type;

	int slot;
};

struct node *parse_program(void);

/****************************************************************
 * Lowering
 ****************************************************************/

struct ir_program *lower_program(struct node *ast);

#endif /* TINC_H */
