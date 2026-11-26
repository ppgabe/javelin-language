//
// Created by ppgabe on 11/26/26.
//

#include "parser.h"

#include <stdio.h>

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
    fprintf(stderr, "[Line %d, Col %d] Syntax Error: %s (got %s)\n",
            p->current.line, p->current.column, msg,
            token_type_name(p->current.type));
    p->had_error = true;
    return false;
}