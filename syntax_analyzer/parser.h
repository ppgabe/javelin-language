//
// Created by ppgabe on 11/26/26.
//

#ifndef JVLNC_PARSER_H
#define JVLNC_PARSER_H

#include <stdbool.h>
#include "../lexer/lexer.h"

typedef struct {
    LexerState *lexer;
    Token current;
    Token next;
    bool had_error;
} Parser;

void parser_init(Parser *p, LexerState *l);
bool parse_program(Parser *p);

#endif //JVLNC_PARSER_H
