#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "string_split_utils.h"

struct string_split* make_string_split(void) {
    struct string_split *res = malloc(sizeof(*res));
    if (!res) return NULL;
    res->length = 0;
    // On alloue un premier bloc pour le tableau (1 slot = le pointeur NULL final)
    res->parts = malloc(sizeof(char*));
    if (!res->parts) {
        free(res);
        return NULL;
    }
    res->parts[0] = NULL;
    return res;
}

void free_split(struct string_split *split) {
    if (!split) return;
    for (unsigned int i = 0; i < split->length; i++) {
        free(split->parts[i]);
    }
    free(split->parts);
    free(split);
}

// add_to_split alloue la place pour un string supplémentaire + le pointeur NULL final
void add_to_split(struct string_split *split, const char *string) {
    // Redimensionner le tableau parts
    // On agrandit de +2 : 
    //   1 pour le nouveau string,
    //   1 pour le pointeur NULL final.
    char **tmp = realloc(split->parts, (split->length + 2) * sizeof(char*));
    if (!tmp) {
        // En cas d’échec, on pourrait gérer l’erreur,
        // mais dans un simple projet, on peut se contenter d’ignorer ou d’exit.
        return;
    }
    split->parts = tmp;

    // Dupliquer la chaîne
    split->parts[split->length] = strdup(string);
    if (!split->parts[split->length]) {
        return;
    }
    // On incrémente le nombre de morceaux
    split->length++;

    // On place un pointeur NULL après le dernier string
    split->parts[split->length] = NULL;
}

/* 
   parse_line_advanced : parse une ligne pour générer des tokens.
   - Délimite sur les espaces
   - Sépare aussi '>' et '&' en tokens individuels
   - Exemple:
        "ls>fichier"    -> ["ls", ">", "fichier"]
        "ls & pwd"      -> ["ls", "&", "pwd"]
*/
struct string_split* parse_line_advanced(const char *input) {
    struct string_split *res = make_string_split();
    if (!res) return NULL;

    // On duplique la chaîne pour pouvoir la manipuler si besoin
    // (ici on pourrait travailler en lecture seule, car on ne modifie pas *copy,
    //  mais c'est plus sûr de travailler sur une copie si on veut la charcuter)
    char *copy = strdup(input);
    if (!copy) {
        free_split(res);
        return NULL;
    }

    char *p = copy;
    while (*p != '\0') {
        // 1) Sauter les espaces initiaux
        while (isspace((unsigned char)*p)) {
            p++;
        }
        if (*p == '\0') break; // fin de chaîne

        // 2) Si on tombe sur '>' ou '&', on fait directement un token
        if (*p == '>' || *p == '&') {
            char special[2];
            special[0] = *p;
            special[1] = '\0';
            add_to_split(res, special);
            p++;
            continue;
        }

        // 3) Sinon, on lit un “morceau” jusqu’au prochain espace
        //    ou symbole spécial '>' ou '&'
        char *start = p;
        while (*p != '\0' 
               && !isspace((unsigned char)*p) 
               && *p != '>' 
               && *p != '&') 
        {
            p++;
        }

        // p pointe maintenant sur l’un de:
        // - un '\0' (fin de ligne)
        // - un espace
        // - un '>'
        // - un '&'

        // On extrait la sous-chaîne [start..p-1]
        // On la duplique dans add_to_split
        // Comme on ne veut pas altérer 'copy', on peut faire ceci :
        char old = *p;  // on sauvegarde le caractère de fin
        *p = '\0';      // on coupe la chaîne
        add_to_split(res, start);
        *p = old;       // on restaure pour la suite
        // On ne fait pas p++ ici, car on veut réexaminer s’il s’agit d’un '>' ou '&'
        // => la boucle while(*p != '\0') continue, et retombe dans le if (*p == '>')
    }

    free(copy);
    return res;
}
