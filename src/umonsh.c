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

const char *error_message = "An error has occurred\n";

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

char* normalize_line(const char* input) {
    size_t len = strlen(input);
    char* output = malloc(3 * len + 1); // au cas où chaque caractère serait spécial
    if (!output) return NULL;

    int j = 0;
    for (size_t i = 0; i < len; ++i) {
        if (input[i] == '>' || input[i] == '&') {
            output[j++] = ' ';
            output[j++] = input[i];
            output[j++] = ' ';
        } else {
            output[j++] = input[i];
        }
    }
    output[j] = '\0';
    return output;
}

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
    }
}

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

    // 2. Vérifier qu’il n’y a **qu’un seul** ">"
    if (redir_count > 1) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        return -1;
    }

    // 3. Vérifier les erreurs de syntaxe si un ">" est trouvé
    if (redir_count == 1) {
        // (a) ">" ne peut pas être en position 0 (ex: "> file")
        if (redir_index == 0) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            return -1;
        }

        // (b) Il doit y avoir **un seul** argument après ">"
        if (args[redir_index + 1] == NULL || args[redir_index + 2] != NULL) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            return -1;
        }

        // 4. Ouvrir le fichier de redirection
        int fd = open(args[redir_index + 1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd < 0) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            return -1;
        }

        // 5. Rediriger stdout et stderr vers le fichier
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);

        // 6. Couper la commande pour execv (ne garder que la partie avant ">")
        args[redir_index] = NULL;
    }

    return 0; // OK
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

        command[strcspn(command, "\n")] = '\0'; // supression du \n

        char *normalized = normalize_line(command);
        if (!normalized) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            continue;
        }

        if (strchr(normalized, '&') != NULL) {
            char *command_copy = strdup(normalized);
            if (!command_copy) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                free(normalized);
                continue;
            }
            handle_parallel(command_copy);
            free(command_copy);
            free(normalized);
            continue;
        }

        char *args[100];
        int argc = parse_command(normalized, args, 100);
    
        if (VERBOSE) {
            printf("[Verbose mode=1] Command: %s - Allocated %ld octets, Used %ld chars\n", command, command_size, strlen(command));
        }


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
            if (argc != 1) {
               write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            free(command);
            cleanup_paths();
            //printf("Sortie du shell Umonsh\n");
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
         free(normalized);    
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
    } else if (argc == 2) {
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
        
    } else {
        write(STDERR_FILENO, error_message, strlen(error_message));
        cleanup_paths();
        return 1;
    }
}
