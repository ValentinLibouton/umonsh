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
#include <sys/wait.h>
#include <fcntl.h>  // pour open()
#include <stdbool.h>

#include "string_split_utils.h"

#define MAX_PATHS 100
#define VERBOSE 0

char *path_dirs[MAX_PATHS];
int path_count = 0;

const char *error_message = "An error has occurred\n";

/* === Fonctions indépendantes === */

int is_file_empty(FILE *file) {
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);  // Important pour permettre la lecture après
    return size == 0;
}


/* === Fonctions partiellement indépendantes (usage de var globales) === */

void cleanup_paths() {
    for (int i = 0; i < path_count; i++) {
        free(path_dirs[i]);
        path_dirs[i] = NULL; 
    }
    path_count = 0;
}

/* 
    Vérifie s’il y a une redirection ">" unique dans args.
    Si oui, on redirige stdout et stderr vers le fichier, puis on tronque args à ">".
    Renvoie -1 en cas d’erreur; 0 sinon.
*/
int handle_redirection(char *args[]) {
    int redir_index = -1;
    int redir_count = 0;

    // 1. Rechercher et compter les ">"
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) {
            redir_index = i;
            redir_count++;
        }
    }

    // 2. Vérifier qu’il n’y a qu’un seul ">"
    if (redir_count > 1) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        return -1;
    }

    // 3. S’il n’y a pas de ">", rien à faire
    if (redir_count == 0) {
        return 0;
    }

    // S’il y en a un seul:
    // (a) Ne peut pas être le premier token
    if (redir_index == 0) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        return -1;
    }

    // (b) Doit y avoir exactement un token après
    if (args[redir_index + 1] == NULL || args[redir_index + 2] != NULL) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        return -1;
    }

    // Ouvrir le fichier de redirection
    int fd = open(args[redir_index + 1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        return -1;
    }

    // Rediriger stdout et stderr
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);

    // Tronquer la commande pour execv (on coupe au ">"…)
    args[redir_index] = NULL;

    return 0;
}



/* === Fonctions dépendantes (usage de fonctions du projet) === */

/* 
    Exécute une suite de commandes séparées par `&` en parallèle.
    Ensuite, attend que tous les enfants terminent.
*/
void handle_parallel(char *line) {
    char *saveptr;
    char *sub_cmd = strtok_r(line, "&", &saveptr);
    pid_t pids[100];
    int pid_count = 0;

    while (sub_cmd != NULL) {
        // Supprimer espaces inutiles
        while (*sub_cmd == ' ') sub_cmd++;

        // Ignorer les lignes vides
        if (*sub_cmd == '\0') {
            sub_cmd = strtok_r(NULL, "&", &saveptr);
            continue;
        }

        // Découpage via string_split_utils
        struct string_split *parsed = parse_line_advanced(sub_cmd);
        char **args = parsed->parts;
        int argc = parsed->length;

        if (argc == 0) {
            free_split(parsed);
            sub_cmd = strtok_r(NULL, "&", &saveptr);
            continue;
        }

        // Interdiction des commandes internes en parallèle
        if (strcmp(args[0], "cd") == 0 ||
            strcmp(args[0], "exit") == 0 ||
            strcmp(args[0], "path") == 0) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            free_split(parsed);
            sub_cmd = strtok_r(NULL, "&", &saveptr);
            continue;
        }

        pid_t pid = fork();
        if (pid == 0) {
            // Enfant
            if (handle_redirection(args) == -1) {
                //free_split(parsed);
                exit(1);
            }

            // Tester chaque répertoire du path
            char path[256];
            for (int i = 0; i < path_count; i++) {
                snprintf(path, sizeof(path), "%s/%s", path_dirs[i], args[0]);
                if (access(path, X_OK) == 0) {
                    execv(path, args);
                    // Si execv réussit, on ne revient pas ici
                }
            }
            // Si on arrive là, échec...
            write(STDERR_FILENO, error_message, strlen(error_message));
            //free_split(parsed);
            exit(1);
        } else if (pid > 0) {
            // Parent
            pids[pid_count++] = pid;
        } else {
            // Erreur fork
            write(STDERR_FILENO, error_message, strlen(error_message));
        }
        free_split(parsed);
        sub_cmd = strtok_r(NULL, "&", &saveptr);
    }

    // Attendre tous les enfants
    for (int i = 0; i < pid_count; i++) {
        waitpid(pids[i], NULL, 0);
    }
}

/* 
    Exécute une commande externe (pas cd/path/exit).
    Gère la redirection avant d’appeler execv.
*/
void execute_command(char *args[]) {
    pid_t pid = fork();
    if (pid == 0) {
        // Processus enfant

        // Gérer la redirection
        if (handle_redirection(args) == -1) { // pour les ... > ...
            exit(1);  // Erreur de redirection
        }

        // Parcours les chemins du path
        char path[256];
        for (int i =0; i <path_count; i++) {
            snprintf(path, sizeof(path), "%s/%s", path_dirs[i], args[0]);
            // Vérifie si le fichier est exécutable
            if (access(path, X_OK) == 0) {
                execv(path, args);
            }
        }

        // Si on arrive ici, execv a échoué
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);

    } else if (pid > 0) {
        // Processus parent
        wait(NULL);
    } else {
        // fork a échoué
        write(STDERR_FILENO, error_message, strlen(error_message));
    }
}


void shell_mode(FILE *input, int is_interactive) {
    char *command = NULL;
    size_t command_size = 0;
    while(1) {
        if (is_interactive) {
            printf("umonsh> ");
        }
        
        if (getline(&command, &command_size, input) == -1) {
            free(command);
            cleanup_paths();
            exit(0);
        }

        // Supprimer le \n final
        command[strcspn(command, "\n")] = '\0'; // supression du \n

        
        // Vérifier si on a du parallélisme :
        if (VERBOSE) printf("[Debug] Vérifier si on a du parallélisme. Usage de:'&'\n");
        if (strchr(command, '&') != NULL) {
            // On gère TOUT en parallèle
            char *command_copy = strdup(command);
            if (!command_copy) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            handle_parallel(command_copy);
            free(command_copy);
            continue;
        }

        // Sinon, on gère une commande simple

        // Découper la ligne en tokens
        if (VERBOSE) printf("[Debug] Découper la ligne en tokens\n");
        struct string_split *parsed = parse_line_advanced(command);
        char **args = parsed->parts;
        int argc = parsed->length;
    
        if (VERBOSE) printf("[Debug] Command: %s - Allocated %ld octets, Used %ld chars\n", command, command_size, strlen(command));

        if (argc == 0) {
            if (VERBOSE) printf("[Debug] Ligne vide, on repart dans la boucle\n");
            free_split(parsed);
            continue;  // Ligne vide, on repart dans la boucle
        }

        /*TODO les quelques ligne ci-dessous, je veux essayer de les supprimer...*/
        // Vérifier si la commande interne n’a pas de redirection
        // On peut faire un mini-scan ici, ou plus simplement:
        // Si on détecte ">" ET c’est un builtin => erreur
        if (VERBOSE) printf("[Debug] Vérifier si la commande interne n’a pas de redirection. Usage de:'>'\n");
        bool invalid_cmd = false;
        for (int i = 0; args[i] != NULL; i++) {
            if (VERBOSE) printf("[Debug] %s ?= > \n",args[i]);
            if (strcmp(args[i], ">") == 0 &&
                (strcmp(args[0], "cd") == 0 ||
                    strcmp(args[0], "exit") == 0 ||
                    strcmp(args[0], "path") == 0)) {
                if (VERBOSE) printf("[Debug] L'usage de la redirection '>' a été détectée avec une commande interdite %s\n", args[0]);
                write(STDERR_FILENO, error_message, strlen(error_message));
                invalid_cmd = true;
                break;
            }
        }
        if (invalid_cmd) {
            if (VERBOSE) printf("[Debug] Commande invalide\n");
            free_split(parsed);
            continue;
        }


        

        // Gestion des commandes internes

        // 1) exit
        if (strcmp(args[0], "exit") == 0) {
            if (argc != 1) {
               write(STDERR_FILENO, error_message, strlen(error_message));
               free_split(parsed);
                continue;
            }
            free_split(parsed);
            free(command);
            cleanup_paths();
            //printf("Sortie du shell Umonsh\n");
            exit(0);
        }

        // 2) cd
        else if (strcmp(args[0], "cd") == 0) {
            if (VERBOSE) printf("[Debug] Commande cd\n");
            if (argc != 2) {
                write(STDERR_FILENO, error_message, strlen(error_message));
            } else {
                if (chdir(args[1]) != 0) {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                }
            }
            free_split(parsed);
            continue;

        }

        // 3) path
        else if (strcmp(args[0], "path") == 0) {
            // Nettoyer l’ancien path
            cleanup_paths();
            // Ajouter les nouveaux chemins
            for (int i = 1; i < argc; i++) {
                if (path_count < MAX_PATHS) {
                    path_dirs[path_count++] = strdup(args[i]);
                }
            }
            free_split(parsed);
            continue;
        }

        if (VERBOSE) {
            //printf("Commande : %s\n", args[0]);
            for (int i = 1; i < argc; i++) {
                printf("Arg %d : %s\n", i, args[i]);
            }    
        }

        // Si on arrive ici, c’est une commande externe
         execute_command(args);
         free_split(parsed);
    }
}

int main(int argc, char *argv[]) {
    path_dirs[0] = strdup("/bin");
    path_count = 1;

    if (argc == 1) {
        // Mode interactif
        shell_mode(stdin, 1);
        cleanup_paths();
        return 0;
    }

    else if (argc == 2) {
        // Mode batch
        FILE *file = fopen(argv[1], "r");
        if (file == NULL || is_file_empty(file)) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            if (file) fclose(file);
            cleanup_paths();
            return 1;
        }
        shell_mode(file, 0);
        fclose(file);
        cleanup_paths();
        return 0;
        
    }
    else {
        write(STDERR_FILENO, error_message, strlen(error_message));
        cleanup_paths();
        return 1;
    }
}
