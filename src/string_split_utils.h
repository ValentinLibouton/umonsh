#ifndef STRING_SPLIT_UTILS_H
#define STRING_SPLIT_UTILS_H

struct string_split {
    unsigned int length;
    char **parts;
};

struct string_split* make_string_split(void);
void free_split(struct string_split *split);
void add_to_split(struct string_split *split, const char *string);

// Ancienne fonction (optionnelle) :
// struct string_split* split_on_char(char *string, const char *delim);

// NOUVELLE FONCTION : parseur avancé
struct string_split* parse_line_advanced(const char *input);

#endif