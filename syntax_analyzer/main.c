#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

typedef struct {
    FILE *file;
    size_t expected_index;
    size_t record_number;
    bool header_checked;
    bool failed;
} SymbolTableReader;

static int append_char(char **buffer, size_t *length, size_t *capacity, int ch) {
    if (*length + 1 >= *capacity) {
        size_t new_capacity = *capacity == 0 ? 128 : *capacity * 2;
        char *grown = realloc(*buffer, new_capacity);
        if (!grown) return 0;
        *buffer = grown;
        *capacity = new_capacity;
    }
    (*buffer)[(*length)++] = (char) ch;
    return 1;
}

static char *read_csv_record(FILE *file, int *malformed) {
    char *record = NULL;
    size_t length = 0;
    size_t capacity = 0;
    int in_quotes = 0;
    int ch;
    *malformed = 0;

    // A quoted field may contain commas or newlines; only an unquoted newline
    // ends a table row.
    while ((ch = fgetc(file)) != EOF) {
        if (ch == '"') {
            if (!append_char(&record, &length, &capacity, ch)) goto fail;
            if (in_quotes) {
                int next = fgetc(file);
                if (next == '"') {
                    if (!append_char(&record, &length, &capacity, next)) goto fail;
                } else {
                    in_quotes = 0;
                    if (next != EOF) ungetc(next, file);
                }
            } else {
                in_quotes = 1;
            }
        } else if (ch == '\n' && !in_quotes) {
            break;
        } else if (!append_char(&record, &length, &capacity, ch)) {
            goto fail;
        }
    }

    if (ferror(file) || (ch == EOF && length == 0)) {
        free(record);
        return NULL;
    }
    if (in_quotes) {
        *malformed = 1;
        goto fail;
    }
    if (length > 0 && record[length - 1] == '\r') length--;
    if (!append_char(&record, &length, &capacity, '\0')) goto fail;
    return record;

fail:
    *malformed = 1;
    free(record);
    return NULL;
}

static void free_fields(char **fields, size_t count) {
    for (size_t i = 0; i < count; i++) free(fields[i]);
}

static int parse_csv_fields(const char *record, char **fields, size_t expected_count) {
    const char *cursor = record;
    size_t count = 0;

    while (count < expected_count) {
        size_t capacity = strlen(cursor) + 1;
        char *field = malloc(capacity);
        if (!field) goto fail;
        size_t length = 0;

        if (*cursor == '"') {
            cursor++;
            for (;;) {
                if (*cursor == '\0') {
                    free(field);
                    goto fail;
                }
                if (*cursor == '"') {
                    if (cursor[1] == '"') {
                        // CSV represents a quote inside a quoted field as "".
                        field[length++] = '"';
                        cursor += 2;
                    } else {
                        cursor++;
                        break;
                    }
                } else {
                    field[length++] = *cursor++;
                }
            }
            if (*cursor != ',' && *cursor != '\0') {
                free(field);
                goto fail;
            }
        } else {
            while (*cursor != ',' && *cursor != '\0') {
                if (*cursor == '"') {
                    free(field);
                    goto fail;
                }
                field[length++] = *cursor++;
            }
        }

        field[length] = '\0';
        fields[count++] = field;
        if (*cursor == ',') {
            cursor++;
        } else if (count != expected_count) {
            goto fail;
        }
    }

    if (*cursor != '\0') goto fail;
    return 1;

fail:
    free_fields(fields, count);
    return 0;
}

static int parse_long_field(const char *text, long *value) {
    char *end;
    errno = 0;
    long parsed = strtol(text, &end, 10);
    if (errno || *text == '\0' || *end != '\0') return 0;
    *value = parsed;
    return 1;
}

static int parse_token_type(const char *name, TokenType *type) {
    for (int i = TOKEN_EOF; i <= TOKEN_RBRACE; i++) {
        if (strcmp(name, token_type_name((TokenType) i)) == 0) {
            *type = (TokenType) i;
            return 1;
        }
    }
    return 0;
}

static Token invalid_table_token(SymbolTableReader *reader) {
    reader->failed = true;
    char *message = malloc(sizeof("malformed symbol table"));
    if (message) strcpy(message, "malformed symbol table");
    return (Token){
        .type = TOKEN_INVALID,
        .line = (int) reader->record_number,
        .column = 1,
        .error = message
    };
}

static int check_header(SymbolTableReader *reader) {
    static const char *expected[] = {
        "index", "line", "column", "type", "lexeme", "value", "error"
    };
    int malformed;
    char *record = read_csv_record(reader->file, &malformed);
    char *fields[7] = {0};
    reader->record_number++;
    if (!record || malformed || !parse_csv_fields(record, fields, 7)) {
        free(record);
        return 0;
    }
    free(record);
    for (size_t i = 0; i < 7; i++) {
        if (strcmp(fields[i], expected[i]) != 0) {
            free_fields(fields, 7);
            return 0;
        }
    }
    free_fields(fields, 7);
    return 1;
}

static Token read_symbol_token(void *context) {
    SymbolTableReader *reader = context;
    if (reader->failed) return (Token){.type = TOKEN_EOF};

    if (!reader->header_checked) {
        reader->header_checked = true;
        if (!check_header(reader)) return invalid_table_token(reader);
    }

    int malformed;
    char *record = read_csv_record(reader->file, &malformed);
    if (!record) {
        if (malformed || ferror(reader->file)) return invalid_table_token(reader);
        return (Token){.type = TOKEN_EOF};
    }

    reader->record_number++;
    char *fields[7] = {0};
    if (!parse_csv_fields(record, fields, 7)) {
        free(record);
        return invalid_table_token(reader);
    }
    free(record);

    long index, line, column;
    TokenType type;
    int valid = parse_long_field(fields[0], &index) &&
                parse_long_field(fields[1], &line) &&
                parse_long_field(fields[2], &column) &&
                index >= 0 && (size_t) index == reader->expected_index &&
                line > 0 && line <= INT_MAX && column > 0 && column <= INT_MAX &&
                parse_token_type(fields[3], &type) && type != TOKEN_EOF;
    if (!valid) {
        free_fields(fields, 7);
        return invalid_table_token(reader);
    }

    Token token = {
        .type = type,
        .lexeme = fields[4],
        .length = (int) strlen(fields[4]),
        .line = (int) line,
        .column = (int) column,
        .error = fields[6]
    };

    if (type == TOKEN_INT_LIT) {
        long value;
        value = 0;
        valid = parse_long_field(fields[5], &value) && value >= INT_MIN && value <= INT_MAX;
        token.int_val = (int) value;
    } else if (type == TOKEN_DOUBLE_LIT) {
        char *end;
        errno = 0;
        token.double_val = strtod(fields[5], &end);
        valid = !errno && *fields[5] != '\0' && *end == '\0';
    } else if (type == TOKEN_BOOL_LIT) {
        valid = strcmp(fields[5], "true") == 0 || strcmp(fields[5], "false") == 0;
        token.bool_val = strcmp(fields[5], "true") == 0;
    } else if (type == TOKEN_CHAR_LIT) {
        long value = 0;
        valid = parse_long_field(fields[5], &value) && value >= 0 && value <= UCHAR_MAX;
        token.char_val = (char) value;
    } else if (type == TOKEN_STRING_LIT) {
        token.string_val = malloc(strlen(fields[5]) + 1);
        if (!token.string_val) valid = 0;
        else strcpy(token.string_val, fields[5]);
    }

    free(fields[0]);
    free(fields[1]);
    free(fields[2]);
    free(fields[3]);
    free(fields[5]);
    if (!valid) {
        free((void *) token.lexeme);
        free((void *) token.error);
        free(token.string_val);
        return invalid_table_token(reader);
    }

    // Keep table order tied to the row index; the parser consumes one row at a time.
    reader->expected_index++;
    return token;
}

static void release_symbol_token(void *context, Token *token) {
    (void) context;
    // The CSV reader allocates each token's text independently.
    free((void *) token->lexeme);
    free((void *) token->error);
    if (token->type == TOKEN_STRING_LIT) free(token->string_val);
}

static int has_symbol_table_extension(const char *path) {
    size_t length = strlen(path);
    return length >= 10 && strcmp(path + length - 10, ".jvln.symb") == 0;
}

int main(int argc, char *argv[]) {
    bool show_parse_tree = false;
    size_t table_count = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--show-parse-tree") == 0) show_parse_tree = true;
        else table_count++;
    }
    if (table_count == 0) {
        fprintf(stderr, "USAGE: jvl_parser [--show-parse-tree] [symbol-table.jvln.symb ...]\n");
        return 1;
    }

    int exit_status = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--show-parse-tree") == 0) continue;
        const char *path = argv[i];
        if (!has_symbol_table_extension(path)) {
            fprintf(stderr, "jvl_parser only accepts symbol tables produced by jvlex (.jvln.symb): '%s'\n", path);
            exit_status = 2;
            continue;
        }

        SymbolTableReader reader = {0};
        reader.file = fopen(path, "rb");
        if (!reader.file) {
            perror(path);
            exit_status = 1;
            continue;
        }

        Parser parser;
        parser_init_token_stream(&parser, read_symbol_token, release_symbol_token,
                                 &reader, NULL, path, show_parse_tree);
        if (parse_program(&parser)) {
            printf("Syntax analysis succeeded: '%s'\n", path);
        } else {
            fprintf(stderr, "Syntax analysis failed: '%s'\n", path);
            exit_status = 1;
        }
        parser_dispose(&parser);
        fclose(reader.file);
    }

    return exit_status;
}
