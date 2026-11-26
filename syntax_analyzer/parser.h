//
// Created by ppgabe on 11/26/26.
//

#ifndef JVLNC_PARSER_H
#define JVLNC_PARSER_H

#include <stdbool.h>
#include "../lexer/lexer.h"

typedef struct {
    LexerState *lexer;
    const char *filename;
    Token current;
    Token next;
    bool had_error;
    bool recovering;
    bool show_parse_tree;
    int depth;
} Parser;

void parser_init(Parser *p, LexerState *l, const char *filename, bool show_parse_tree);
bool parse_program(Parser *p);

#endif //JVLNC_PARSER_H
