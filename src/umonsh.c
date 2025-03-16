/***********************************************
 * Project 2: Shell
 * 
 * Group Number : 18
 * Students     : Bryan Pax, Valentin Libouton
 * 
 * Please add details if compilation of your project is not
 * straightforward (for instance, if you use multiple files).
 ***********************************************/
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

const char *error_message = "An error has occured\n";

void interactive_mode(FILE *input) {
    // Mode interactif : umonsh>
    char *buffer = NULL;
    size_t size = 0;
    // Ce qui suit devra être dans une boucle infinie ...
    printf("umonsh> ");
    if (getline(&buffer, &size, stdin) == -1) {
        //break; // EOF ou erreur ...
    }
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        // Mode interactif
        interactive_mode(stdin);
    } else if (argc == 2) {
        // Mode batch
        // A développer plus tard... (pas maintenant)
    } else {
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }
    return 0;
}
