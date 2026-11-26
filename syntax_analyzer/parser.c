//
// Created by ppgabe on 11/26/26.
//

#include "parser.h"

#include <stdio.h>
#include <stdlib.h>

static void error_at_current(Parser *p, const char *msg);

static void print_error_context(const char *source, const Token *t) {
    if (!source || t->line <= 0) return;

    const char *line_start = source;
    int current_line = 1;
    while (*line_start != '\0' && current_line < t->line) {
        if (*line_start == '\n') {
            current_line++;
        }
        line_start++;
    }

    const char *line_end = line_start;
    while (*line_end != '\0' && *line_end != '\n' && *line_end != '\r') {
        line_end++;
    }

    int line_len = (int) (line_end - line_start);

    fprintf(stderr, " %4d | %.*s\n", t->line, line_len, line_start);

    fprintf(stderr, "      | ");
    int col = (t->column > 1) ? t->column - 1 : 0;
    for (int i = 0; i < col; i++) {
        if (i < line_len && line_start[i] == '\t') {
            fputc('\t', stderr);
        } else {
            fputc(' ', stderr);
        }
    }

    int underline_len = (t->length > 1) ? t->length : 1;
    fputc('^', stderr);
    for (int i = 1; i < underline_len; i++) {
        fputc('~', stderr);
    }
    fputc('\n', stderr);
}

static Token next_significant_token(Parser *p) {
    for (;;) {
        Token t = next_token(p->lexer);
        if (t.type == TOKEN_COMMENT || t.type == TOKEN_NOISE) {
            continue;
        }
        if (t.type == TOKEN_INVALID) {
            fprintf(stderr, "%s:%d:%d: Lexical error: %s: '%.*s'\n",
                    p->filename, t.line, t.column,
                    t.error ? t.error : "invalid token",
                    t.length, t.lexeme ? t.lexeme : "");
            print_error_context(p->lexer->source, &t);
            p->had_error = true;
            return t;
        }
        return t;
    }
}

static bool is_type_token(TokenType type) {
    return type == TOKEN_RW_INT ||
           type == TOKEN_RW_DOUBLE ||
           type == TOKEN_RW_CHAR ||
           type == TOKEN_RW_BOOL;
}

void parser_init(Parser *p, LexerState *l, const char *filename, bool show_parse_tree) {
    p->lexer = l;
    p->filename = filename ? filename : "<input>";
    p->had_error = false;
    p->recovering = false;
    p->show_parse_tree = show_parse_tree;
    p->depth = 0;
    p->current = next_significant_token(p);
    p->next = next_significant_token(p);
}

static void print_indent(int depth) {
    for (int i = 0; i < depth; i++) {
        printf("  ");
    }
}

static void trace_enter(Parser *p, const char *rule) {
    if (p->show_parse_tree && !p->recovering) {
        print_indent(p->depth);
        printf("%s\n", rule);
    }
    p->depth++;
}

static void trace_exit(Parser *p) {
    if (p->depth > 0) {
        p->depth--;
    }
}

static void advance(Parser *p) {
    if (p->show_parse_tree && !p->recovering && p->current.type != TOKEN_EOF) {
        print_indent(p->depth);
        printf("%s ('%.*s')\n",
               token_type_name(p->current.type),
               p->current.length,
               p->current.lexeme ? p->current.lexeme : "");
    }
    if (p->current.type == TOKEN_STRING_LIT) {
        free(p->current.string_val);
    }
    p->current = p->next;
    p->next = next_significant_token(p);
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
    fprintf(stderr, "%s:%d:%d: Syntax error: %s (got %s '%.*s')\n",
            p->filename, p->current.line, p->current.column, msg,
            token_type_name(p->current.type),
            p->current.length, p->current.lexeme ? p->current.lexeme : "");
    print_error_context(p->lexer->source, &p->current);
    p->had_error = true;
}

static void synchronize(Parser *p) {
    p->recovering = true;

    while (p->current.type != TOKEN_EOF) {
        if (p->current.type == TOKEN_SEMICOLON) {
            advance(p);
            p->recovering = false;
            return;
        }
        if (p->current.type == TOKEN_RBRACE) {
            p->recovering = false;
            return; // Stop before '}' so parse_block() consumes it
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
                p->recovering = false;
                return;
            default:
                advance(p);
        }

        p->recovering = false;
    }
}

static void parse_expr(Parser *p);

static void parse_stmt(Parser *p);

static void parse_block(Parser *p);

static void parse_if_stmt(Parser *p);

static void parse_type(Parser *p);

static void parse_type(Parser *p) {
    trace_enter(p, "<type>");
    switch (p->current.type) {
        case TOKEN_RW_INT:
        case TOKEN_RW_DOUBLE:
        case TOKEN_RW_CHAR:
        case TOKEN_RW_BOOL:
            advance(p);
            break;
        default:
            error_at_current(p, "Expected a type ('int', 'double', 'char', or 'bool')");
            break;
    }
    trace_exit(p);
}

// <args_list> -> <expr> { "," <expr> }
static void parse_args_list(Parser *p) {
    trace_enter(p, "<args_list>");
    parse_expr(p);
    while (match(p, TOKEN_COMMA)) {
        parse_expr(p);
    }
    trace_exit(p);
}

// <call> -> <id> "(" [ <args_list> ] ")"
static void parse_call(Parser *p) {
    trace_enter(p, "<call>");
    expect(p, TOKEN_IDENT, "Expected function name");
    expect(p, TOKEN_LPAREN, "Expected '(' after function name");
    if (p->current.type != TOKEN_RPAREN && p->current.type != TOKEN_EOF) {
        parse_args_list(p);
    }
    expect(p, TOKEN_RPAREN, "Expected ')' after arguments");
    trace_exit(p);
}

static void parse_when_case(Parser *p) {
    trace_enter(p, "<when_case>");
    if (match(p, TOKEN_KW_CASE)) {
        if (is_type_token(p->current.type)) {
            parse_type(p);
            expect(p, TOKEN_IDENT, "Expected identifier after type in 'case'");
        } else {
            parse_expr(p);
        }
    } else if (match(p, TOKEN_KW_DEFAULT)) {
    } else {
        error_at_current(p, "Expected 'case' or 'default' in when-block");
    }
    trace_exit(p);
}

static void parse_when_rule(Parser *p) {
    trace_enter(p, "<when_rule>");
    parse_when_case(p);
    expect(p, TOKEN_ARROW, "Expected '->' after when case");
    if (p->current.type == TOKEN_LBRACE) {
        parse_block(p);
        match(p, TOKEN_SEMICOLON);
    } else {
        parse_expr(p);
        expect(p, TOKEN_SEMICOLON, "Expected ';' after when rule expression");
    }
    trace_exit(p);
}

// <when_block> -> "{" <when_rule> { <when_rule> } "}"
static void parse_when_block(Parser *p) {
    trace_enter(p, "<when_block>");
    expect(p, TOKEN_LBRACE, "Expected '{' to start when-block");

    if (p->current.type == TOKEN_RBRACE) {
        error_at_current(p, "when-block must contain at least one rule");
    } else {
        parse_when_rule(p);
        while (p->current.type == TOKEN_KW_CASE || p->current.type == TOKEN_KW_DEFAULT) {
            parse_when_rule(p);
        }
    }

    expect(p, TOKEN_RBRACE, "Expected '}' to close when-block");
    trace_exit(p);
}

static void parse_when_expr(Parser *p) {
    trace_enter(p, "<when_expr>");
    expect(p, TOKEN_KW_WHEN, "Expected 'when'");
    expect(p, TOKEN_LPAREN, "Expected '(' after 'when'");
    parse_expr(p);
    expect(p, TOKEN_RPAREN, "Expected ')' after when condition");
    parse_when_block(p);
    trace_exit(p);
}

static void parse_term(Parser *p) {
    trace_enter(p, "<term>");
    switch (p->current.type) {
        case TOKEN_INT_LIT:
        case TOKEN_DOUBLE_LIT:
        case TOKEN_BOOL_LIT:
        case TOKEN_CHAR_LIT:
        case TOKEN_STRING_LIT:
            advance(p);
            break;

        case TOKEN_IDENT:
            if (p->next.type == TOKEN_LPAREN) {
                parse_call(p);
            } else {
                advance(p);
            }
            break;

        case TOKEN_KW_WHEN:
            parse_when_expr(p);
            break;

        // "(" <expr> ")"
        case TOKEN_LPAREN:
            advance(p);
            parse_expr(p);
            expect(p, TOKEN_RPAREN, "Expected ')' after parenthesized expression");
            break;

        case TOKEN_INVALID:
            advance(p);
            break;

        default:
            error_at_current(p, "Expected an expression");
            break;
    }
    trace_exit(p);
}

static void parse_as(Parser *p) {
    trace_enter(p, "<as_expr>");
    parse_term(p);
    while (match(p, TOKEN_KW_AS) || match(p, TOKEN_KW_SAFE_AS)) {
        parse_type(p);
    }
    trace_exit(p);
}

// <unary_expr> -> ("-" | "+") <unary_expr> | <term>
static void parse_unary_expr(Parser *p) {
    trace_enter(p, "<unary_expr>");
    if (match(p, TOKEN_OP_SUB) || match(p, TOKEN_OP_ADD)) {
        parse_unary_expr(p);
    } else {
        parse_as(p);
    }
    trace_exit(p);
}

// <not_expr> -> ("not" | "!") <not_expr> | <unary_expr>
static void parse_not_expr(Parser *p) {
    trace_enter(p, "<not_expr>");
    if (match(p, TOKEN_OP_NOT) || match(p, TOKEN_OP_BANG)) {
        parse_not_expr(p);
    } else {
        parse_unary_expr(p);
    }
    trace_exit(p);
}

// <pow_expr> -> <not_expr> [ "^" <pow_expr> ]  (right-associative)
static void parse_pow_expr(Parser *p) {
    trace_enter(p, "<pow_expr>");
    parse_not_expr(p);
    if (match(p, TOKEN_OP_EXPONENT)) {
        parse_pow_expr(p);
    }
    trace_exit(p);
}

// <mul_expr> -> <pow_expr> { ("*" | "/" | "%" | "div") <pow_expr> }
static void parse_mul_expr(Parser *p) {
    trace_enter(p, "<mul_expr>");
    parse_pow_expr(p);
    while (match(p, TOKEN_OP_MUL) ||
           match(p, TOKEN_OP_DIV) ||
           match(p, TOKEN_OP_MOD) ||
           match(p, TOKEN_OP_INT_DIV)) {
        parse_pow_expr(p);
    }
    trace_exit(p);
}

// <add_expr> -> <mul_expr> { ("+" | "-") <mul_expr> }
static void parse_add_expr(Parser *p) {
    trace_enter(p, "<add_expr>");
    parse_mul_expr(p);
    while (match(p, TOKEN_OP_ADD) || match(p, TOKEN_OP_SUB)) {
        parse_mul_expr(p);
    }
    trace_exit(p);
}

// <rel_expr> -> <add_expr> { ("<" | ">" | "<=" | ">=") <add_expr> }
static void parse_rel_expr(Parser *p) {
    trace_enter(p, "<rel_expr>");
    parse_add_expr(p);
    while (match(p, TOKEN_OP_LT) ||
           match(p, TOKEN_OP_GT) ||
           match(p, TOKEN_OP_LTE) ||
           match(p, TOKEN_OP_GTE)) {
        parse_add_expr(p);
    }
    trace_exit(p);
}

// <eq_expr> -> <rel_expr> { ("==" | "!=") <rel_expr> }
static void parse_eq_expr(Parser *p) {
    trace_enter(p, "<eq_expr>");
    parse_rel_expr(p);
    while (match(p, TOKEN_OP_EQUALS) || match(p, TOKEN_OP_NOT_EQ)) {
        parse_rel_expr(p);
    }
    trace_exit(p);
}

// <and_expr> -> <eq_expr> { "and" <eq_expr> }
static void parse_and_expr(Parser *p) {
    trace_enter(p, "<and_expr>");
    parse_eq_expr(p);
    while (match(p, TOKEN_OP_AND)) {
        parse_eq_expr(p);
    }
    trace_exit(p);
}

// <or_expr> -> <and_expr> { "or" <and_expr> }
static void parse_or_expr(Parser *p) {
    trace_enter(p, "<or_expr>");
    parse_and_expr(p);
    while (match(p, TOKEN_OP_OR)) {
        parse_and_expr(p);
    }
    trace_exit(p);
}

// <assign_expr> -> <assign_stmt> | <or_expr>
// <assign_stmt> -> <id> ("=" | "+=" | "-=") <expr>
static void parse_assign_expr(Parser *p) {
    trace_enter(p, "<assign_expr>");
    if (p->current.type == TOKEN_IDENT &&
        (p->next.type == TOKEN_OP_ASSIGN ||
         p->next.type == TOKEN_OP_ADD_ASSIGN ||
         p->next.type == TOKEN_OP_SUB_ASSIGN)) {
        advance(p); // consume <id>
        advance(p); // consume <assign_ops>
        parse_expr(p);
    } else {
        parse_or_expr(p);
    }
    trace_exit(p);
}

// <expr> -> <assign_expr>
static void parse_expr(Parser *p) {
    trace_enter(p, "<expr>");
    parse_assign_expr(p);
    trace_exit(p);
}

// <modifier> -> "var" | "val"
static void parse_modifier(Parser *p) {
    trace_enter(p, "<modifier>");
    if (!match(p, TOKEN_KW_VAR) && !match(p, TOKEN_KW_VAL)) {
        error_at_current(p, "Expected 'var' or 'val'");
    }
    trace_exit(p);
}

// <decl_explicit> -> <modifier> <id> ":" <type>
static void parse_decl_explicit(Parser *p) {
    trace_enter(p, "<decl_explicit>");
    parse_modifier(p);
    expect(p, TOKEN_IDENT, "Expected parameter name");
    expect(p, TOKEN_COLON, "Expected ':' after parameter name");
    parse_type(p);
    trace_exit(p);
}

// <params_list> -> <decl_explicit> { "," <decl_explicit> }
static void parse_params_list(Parser *p) {
    trace_enter(p, "<params_list>");
    parse_decl_explicit(p);
    while (match(p, TOKEN_COMMA)) {
        parse_decl_explicit(p);
    }
    trace_exit(p);
}

static void parse_decl_stmt(Parser *p) {
    trace_enter(p, "<decl_stmt>");
    parse_modifier(p);
    expect(p, TOKEN_IDENT, "Expected variable name");
    if (match(p, TOKEN_COLON)) {
        parse_type(p);
    }
    expect(p, TOKEN_OP_ASSIGN, "Expected '=' in variable declaration");
    parse_expr(p);
    expect(p, TOKEN_SEMICOLON, "Expected ';' after variable declaration");
    trace_exit(p);
}

// <fun_decl> -> "fun" <id> "(" [ <params_list> ] ")" [ ":" <type> ] <block>
static void parse_fun_decl(Parser *p) {
    trace_enter(p, "<fun_decl>");
    expect(p, TOKEN_KW_FUN, "Expected 'fun'");
    expect(p, TOKEN_IDENT, "Expected function name");
    expect(p, TOKEN_LPAREN, "Expected '(' after function name");
    if (p->current.type != TOKEN_RPAREN && p->current.type != TOKEN_EOF) {
        parse_params_list(p);
    }
    expect(p, TOKEN_RPAREN, "Expected ')' after parameter list");
    if (match(p, TOKEN_COLON)) {
        parse_type(p);
    }
    parse_block(p);
    trace_exit(p);
}

// <record_decl> -> "record" <id> "(" [ <params_list> ] ")" <block>
static void parse_record_decl(Parser *p) {
    trace_enter(p, "<record_decl>");
    expect(p, TOKEN_RW_RECORD, "Expected 'record'");
    expect(p, TOKEN_IDENT, "Expected record name");
    expect(p, TOKEN_LPAREN, "Expected '(' after record name");
    if (p->current.type != TOKEN_RPAREN && p->current.type != TOKEN_EOF) {
        parse_params_list(p);
    }
    expect(p, TOKEN_RPAREN, "Expected ')' after record fields");
    parse_block(p);
    trace_exit(p);
}

// <return_stmt> -> "return" [ <expr> ] ";"
static void parse_return_stmt(Parser *p) {
    trace_enter(p, "<return_stmt>");
    expect(p, TOKEN_KW_RETURN, "Expected 'return'");
    if (p->current.type != TOKEN_SEMICOLON && p->current.type != TOKEN_EOF) {
        parse_expr(p);
    }
    expect(p, TOKEN_SEMICOLON, "Expected ';' after return statement");
    trace_exit(p);
}

// <while_clause> -> "while" "(" <expr> ")"
static void parse_while_clause(Parser *p) {
    trace_enter(p, "<while_clause>");

    expect(p, TOKEN_KW_WHILE, "Expected 'while'");
    expect(p, TOKEN_LPAREN, "Expected '(' after 'while'");
    parse_expr(p);
    expect(p, TOKEN_RPAREN, "Expected ')' after while condition");

    trace_exit(p);
}

// <while_stmt> -> <while_clause> <block>
static void parse_while_stmt(Parser *p) {
    trace_enter(p, "<while_stmt>");

    parse_while_clause(p);
    parse_block(p);

    trace_exit(p);
}

// <do_while_stmt> -> "do" <block> <while_clause> ";"
static void parse_do_while_stmt(Parser *p) {
    trace_enter(p, "<do_while_stmt>");

    expect(p, TOKEN_KW_DO, "Expected 'do'");
    parse_block(p);
    parse_while_clause(p);
    expect(p, TOKEN_SEMICOLON, "Expected ';' after do-while statement");

    trace_exit(p);
}

// <else_stmt> -> <block> | <if_stmt>
static void parse_else_stmt(Parser *p) {
    trace_enter(p, "<else_stmt>");

    if (p->current.type == TOKEN_KW_IF) {
        parse_if_stmt(p);
    } else {
        parse_block(p);
    }

    trace_exit(p);
}

// <if_stmt> -> "if" "(" <expr> ")" [ "then" ] <block> [ "else" <else_stmt> ]
static void parse_if_stmt(Parser *p) {
    trace_enter(p, "<if_stmt>");

    expect(p, TOKEN_KW_IF, "Expected 'if'");
    expect(p, TOKEN_LPAREN, "Expected '(' after 'if'");
    parse_expr(p);
    expect(p, TOKEN_RPAREN, "Expected ')' after if condition");
    parse_block(p);
    if (match(p, TOKEN_KW_ELSE)) {
        parse_else_stmt(p);
    }

    trace_exit(p);
}

// <standalone_stmt> -> <decl_stmt> | <return_stmt> | <expr> ";" | "break" ";" | "continue" ";"
static void parse_standalone_stmt(Parser *p) {
    trace_enter(p, "<standalone_stmt>");

    if (p->current.type == TOKEN_KW_VAR || p->current.type == TOKEN_KW_VAL) {
        parse_decl_stmt(p);
    } else if (p->current.type == TOKEN_KW_RETURN) {
        parse_return_stmt(p);
    } else if (match(p, TOKEN_KW_BREAK)) {
        expect(p, TOKEN_SEMICOLON, "Expected ';' after 'break'");
    } else if (match(p, TOKEN_KW_CONTINUE)) {
        expect(p, TOKEN_SEMICOLON, "Expected ';' after 'continue'");
    } else {
        parse_expr(p);
        expect(p, TOKEN_SEMICOLON, "Expected ';' after expression");
    }

    trace_exit(p);
}

// <stmt> -> <standalone_stmt> | <if_stmt> | <while_stmt> | <do_while_stmt>
static void parse_stmt(Parser *p) {
    trace_enter(p, "<stmt>");

    bool prev_error = p->had_error;
    p->had_error = false;

    if (p->current.type == TOKEN_KW_IF) {
        parse_if_stmt(p);
    } else if (p->current.type == TOKEN_KW_WHILE) {
        parse_while_stmt(p);
    } else if (p->current.type == TOKEN_KW_DO) {
        parse_do_while_stmt(p);
    } else {
        parse_standalone_stmt(p);
    }

    // Panic! Move to the boundary.
    if (p->had_error) {
        synchronize(p);
    }
    p->had_error = prev_error || p->had_error;

    trace_exit(p);
}

// <block> -> "{" { <stmt> } "}"
static void parse_block(Parser *p) {
    trace_enter(p, "<block>");
    expect(p, TOKEN_LBRACE, "Expected '{' to start block");
    while (p->current.type != TOKEN_RBRACE &&
           p->current.type != TOKEN_EOF &&
           p->current.type != TOKEN_KW_FUN &&
           p->current.type != TOKEN_RW_RECORD) {
        parse_stmt(p);
    }
    expect(p, TOKEN_RBRACE, "Expected '}' to close block");
    trace_exit(p);
}

// <program> -> (<fun_decl> | <record_decl> | <decl_stmt>) { <fun_decl> | <record_decl> | <decl_stmt> }
bool parse_program(Parser *p) {
    trace_enter(p, "<program>");
    if (p->current.type == TOKEN_EOF) {
        error_at_current(p, "Program cannot be empty");
        trace_exit(p);
        return false;
    }

    while (p->current.type != TOKEN_EOF) {
        bool prev_error = p->had_error;
        p->had_error = false;
        p->depth = 1;

        if (p->current.type == TOKEN_KW_FUN) {
            parse_fun_decl(p);
        } else if (p->current.type == TOKEN_RW_RECORD) {
            parse_record_decl(p);
        } else if (p->current.type == TOKEN_KW_VAR || p->current.type == TOKEN_KW_VAL) {
            parse_decl_stmt(p);
        } else {
            error_at_current(p, "Expected 'fun', 'record', 'var', or 'val' at top level");
            p->recovering = true;
            advance(p);
            synchronize(p);
        }

        if (p->had_error) {
            synchronize(p);
        }
        p->had_error = prev_error || p->had_error;
    }

    printf("Done\n");
    trace_exit(p);
    return !p->had_error;
}
