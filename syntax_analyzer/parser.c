//
// Created by ppgabe on 11/26/26.
//

#include "parser.h"

#include <stdio.h>

static void error_at_current(Parser *p, const char *msg);

static Token next_significant_token(LexerState *l) {
    Token t = next_token(l);
    while (t.type == TOKEN_COMMENT || t.type == TOKEN_NOISE) {
        t = next_token(l);
    }
    return t;
}

void parser_init(Parser *p, LexerState *l) {
    p->lexer = l;
    p->had_error = false;
    p->current = next_significant_token(l);
    p->next = next_significant_token(l);
}

static void advance(Parser *p) {
    p->current = p->next;
    p->next = next_significant_token(p->lexer);
}

static bool match(Parser *p, TokenType type) {
    if (p->current.type == type) {
        advance(p);
        return true;
    }
    return false;
}

static bool expect(Parser *p, TokenType type, const char *msg) {
    if (p->current.type == type) {
        advance(p);
        return true;
    }
    error_at_current(p, msg);
    return false;
}

static void error_at_current(Parser *p, const char *msg) {
    fprintf(stderr, "[Line %d, Col %d] Syntax Error: %s (got %s '%.*s')\n",
            p->current.line, p->current.column, msg,
            token_type_name(p->current.type),
            p->current.length, p->current.lexeme ? p->current.lexeme : "");
    p->had_error = true;
}

static void synchronize(Parser *p) {
    advance(p);
    while (p->current.type != TOKEN_EOF) {
        if (p->current.type == TOKEN_SEMICOLON || p->current.type == TOKEN_RBRACE) {
            advance(p);
            return;
        }
        switch (p->current.type) {
            case TOKEN_KW_FUN:
            case TOKEN_RW_RECORD:
            case TOKEN_KW_VAR:
            case TOKEN_KW_VAL:
            case TOKEN_KW_IF:
            case TOKEN_KW_WHILE:
            case TOKEN_KW_DO:
            case TOKEN_KW_RETURN:
                return;
            default:
                advance(p);
        }
    }
}

static void parse_expr(Parser *p);
static void parse_stmt(Parser *p);
static void parse_block(Parser *p);
static void parse_if_stmt(Parser *p);
static void parse_type(Parser *p);