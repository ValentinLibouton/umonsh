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

#define VERBOSE 1

const char *error_message = "An error has occured\n";

int parse_command(char *command, char *args[], int max_args) {
    int argc = 0;
    char *token = strtok(command, " "); //Commencer à découper la chaîne 'command'
    
    while (token != NULL && argc < max_args -1) {
        args[argc++] = token;
        token = strtok(NULL, " "); // Continuer à découper le reste de 'command'
    }
    args[argc] = NULL;
    return argc;
}

void interactive_mode(FILE *input) {
    // Mode interactif : umonsh>
    char *command = NULL;
    size_t command_size = 0;
    while(1) {
        printf("umonsh> ");
        if (getline(&command, &command_size, stdin) == -1) {
            free(command);
            exit(0);
        }

        command[strcspn(command, "\n")] = '\0'; // supression du \n
    
        if (VERBOSE) {
            printf("[Verbose mode=1] Command: %s - Allocated %ld octets, Used %ld chars\n", command, command_size, strlen(command));
        }

        char *args[100];
        int argc = parse_command(command, args, 100);

        if (argc == 0) {
            continue;  // Ligne vide, on repart dans la boucle
        }

        if (strcmp(args[0], "exit") == 0) {
            free(command);
            printf("Sortie du shell Umonsh\n");
            exit(0);
        }

        //printf("Commande : %s\n", args[0]);
        for (int i = 1; i < argc; i++) {
            printf("Arg %d : %s\n", i, args[i]);
        }    
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
