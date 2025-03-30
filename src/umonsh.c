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


#define MAX_PATHS 100
#define VERBOSE 0

char *path_dirs[MAX_PATHS];
int path_count = 0;

const char *error_message = "An error has occured\n";

/* === Fonctions indépendantes === */

int parse_command(char *command, char *args[], int max_args) {
    int argc = 0;
    char *token = strtok(command, " \t"); //Commencer à découper la chaîne 'command'
    
    while (token != NULL && argc < max_args -1) {
        args[argc++] = token;
        token = strtok(NULL, " \t"); // Continuer à découper le reste de 'command'
    }
    args[argc] = NULL;
    return argc;
}


/* === Fonctions partiellement indépendantes (usage de var globales) === */

void cleanup_paths() {
    for (int i = 0; i < path_count; i++) {
        free(path_dirs[i]);
    }
}

int handle_redirection(char *args[]) {
    int i = 0;
    while (args[i] != NULL) {
        if (strcmp(args[i], ">") == 0) {
            // Check si un nom de fichier suit
            if (args[i+1] == NULL || args[i+2] != NULL) {
                // Mauvaise syntaxe
                write(STDERR_FILENO, error_message, strlen(error_message));
                return -1;
            }

            // Ouvrir le fichier
            int fd = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd < 0) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                return -1;
            }

            // Rediriger stdout et stderr
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);

            // Couper la commande (pour execv)
            args[i] = NULL;
            return 0;
        }
        i++;
    }

    return 0;  // Pas de redirection
}

/* === Fonctions dépendantes (usage de fonctions du projet) === */

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

        // Parser les arguments
        char *args[100];
        int argc = parse_command(sub_cmd, args, 100);

        if (argc == 0) {
            sub_cmd = strtok_r(NULL, "&", &saveptr);
            continue;
        }

        // Interdiction des commandes internes en parallèle
        if (strcmp(args[0], "cd") == 0 || strcmp(args[0], "exit") == 0 || strcmp(args[0], "path") == 0) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            sub_cmd = strtok_r(NULL, "&", &saveptr);
            continue;
        }

        pid_t pid = fork();
        if (pid == 0) {
            // Enfant
            if (handle_redirection(args) == -1) exit(1);

            char path[256];
            for (int i = 0; i < path_count; i++) {
                snprintf(path, sizeof(path), "%s/%s", path_dirs[i], args[0]);
                if (access(path, X_OK) == 0) {
                    execv(path, args);
                }
            }
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        } else if (pid > 0) {
            // Parent
            pids[pid_count++] = pid;
        } else {
            write(STDERR_FILENO, error_message, strlen(error_message));
        }

        sub_cmd = strtok_r(NULL, "&", &saveptr);
    }

    // Attendre tous les enfants
    for (int i = 0; i < pid_count; i++) {
        waitpid(pids[i], NULL, 0);
    }
}

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


void interactive_mode(FILE *input) {
    // Mode interactif : umonsh>
    char *command = NULL;
    size_t command_size = 0;
    while(1) {
        printf("umonsh> ");
        if (getline(&command, &command_size, stdin) == -1) {
            free(command);
            cleanup_paths();
            exit(0);
        }

        command[strcspn(command, "\n")] = '\0'; // supression du \n
    
        if (VERBOSE) {
            printf("[Verbose mode=1] Command: %s - Allocated %ld octets, Used %ld chars\n", command, command_size, strlen(command));
        }

        if (strchr(command, '&') != NULL) {
            char *command_copy = strdup(command);
            if (command_copy == NULL) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            handle_parallel(command_copy);
            free(command_copy);
            continue;
        }

        char *args[100];
        int argc = parse_command(command, args, 100);

        // Refuser la redirection sur les commandes internes: cd, exit, path
        for (int i = 0; args[i] != NULL; i++) {
            if (strcmp(args[i], ">") == 0 &&
                (strcmp(args[0], "cd") == 0 || strcmp(args[0], "exit") == 0 || strcmp(args[0], "path") == 0)) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
        }

        if (argc == 0) {
            continue;  // Ligne vide, on repart dans la boucle
        }

        if (strcmp(args[0], "exit") == 0) {
            free(command);
            cleanup_paths();
            printf("Sortie du shell Umonsh\n");
            exit(0);
        } else if (strcmp(args[0], "cd") == 0) {
            if (argc != 2) {
                write(STDERR_FILENO, error_message, strlen(error_message));
            } else {
                if (chdir(args[1]) != 0) {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                }
            }
            continue;

        } else if (strcmp(args[0], "path") == 0) {
            // Libérer les anciens chemins
            for (int i = 0; i < path_count; i++) {
                free(path_dirs[i]);
            }

            // Réinitialiser le path
            path_count = 0;

            // Ajouter les nouveaux chemins
            for (int i = 1; i < argc;i++) {
                if (path_count < MAX_PATHS) {
                    path_dirs[path_count++] = strdup(args[i]);
                }
            }
            continue;
        }
        
        if (VERBOSE) {
            //printf("Commande : %s\n", args[0]);
            for (int i = 1; i < argc; i++) {
                printf("Arg %d : %s\n", i, args[i]);
            }    
        }



         execute_command(args);    
    }
}

int main(int argc, char *argv[]) {
    path_dirs[0] = strdup("/bin");
    path_count = 1;
    if (argc == 1) {
        // Mode interactif
        interactive_mode(stdin);
    } else if (argc == 2) {
        // Mode batch
        // A développer plus tard... (pas maintenant)
    } else {
        write(STDERR_FILENO, error_message, strlen(error_message));
        cleanup_paths();
        exit(1);
    }
    cleanup_paths();
    return 0;
}
