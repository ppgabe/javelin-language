#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"

static bool has_jvln_extension(const char *path) {
    size_t len = strlen(path);
    return len >= 5 && strcmp(path + (len - 5), ".jvln") == 0;
}

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        perror(path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);

    char *buffer = malloc((size_t)size + 1);
    if (!buffer) {
        fprintf(stderr, "Error: Out of memory reading '%s'\n", path);
        fclose(file);
        return NULL;
    }

    size_t read_bytes = fread(buffer, 1, (size_t)size, file);
    buffer[read_bytes] = '\0';
    fclose(file);
    return buffer;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "USAGE: jvlnc [--show-parse-tree] [filename ...]\n");
        return 1;
    }

    bool show_parse_tree = false;
    int source_count = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--show-parse-tree") == 0) {
            show_parse_tree = true;
        } else {
            source_count++;
        }
    }
    if (source_count == 0) {
        fprintf(stderr, "USAGE: jvlnc [--show-parse-tree] [filename ...]\n");
        return 1;
    }

    FILE *null_stream = fopen("/dev/null", "w");
    if (null_stream) {
        debug_output_stream = null_stream;
    }

    int exit_status = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--show-parse-tree") == 0) {
            continue;
        }
        const char *path = argv[i];

        if (!has_jvln_extension(path)) {
            fprintf(stderr, "jvlnc only recognizes Javelin source code (.jvln): '%s'\n", path);
            if (null_stream) fclose(null_stream);
            return 2;
        }

        char *source = read_file(path);
        if (!source) {
            exit_status = 1;
            continue;
        }

        LexerState lexer;
        lexer_init(&lexer, source);

        Parser parser;
        parser_init(&parser, &lexer, path, show_parse_tree);

        if (parse_program(&parser)) {
            printf("Syntax analysis succeeded: '%s'\n", path);
        } else {
            fprintf(stderr, "Syntax analysis failed: '%s'\n", path);
            exit_status = 1;
        }

        free(source);
    }

    if (null_stream) {
        fclose(null_stream);
    }

    return exit_status;
}