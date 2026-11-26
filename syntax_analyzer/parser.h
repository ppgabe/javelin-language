#ifndef JVLNC_PARSER_H
#define JVLNC_PARSER_H

#include <stdbool.h>
#include "../lexer/lexer.h"

typedef Token (*ParserTokenReader)(void *context);
typedef void (*ParserTokenReleaser)(void *context, Token *token);

typedef struct {
    ParserTokenReader read_token;
    ParserTokenReleaser release_token;
    void *token_context;
    const char *source;
    const char *filename;
    Token current;
    Token next;
    bool had_error;
    bool recovering;
    bool show_parse_tree;
    int depth;
} Parser;

void parser_init(Parser *p, LexerState *l, const char *filename, bool show_parse_tree);
void parser_init_token_stream(Parser *p, ParserTokenReader read_token,
                              ParserTokenReleaser release_token, void *context,
                              const char *source, const char *filename,
                              bool show_parse_tree);
void parser_dispose(Parser *p);
bool parse_program(Parser *p);

#endif //JVLNC_PARSER_H
