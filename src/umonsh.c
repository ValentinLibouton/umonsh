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
#include <fcntl.h>
#include <stdbool.h>

#include "string_split_utils.h"

// Active ou désactive le debug (0 = inactif, 1 = actif)
// Pour passer les tests du prof, il faut déactiver le debug!!!
#define DEBUG 0

#if DEBUG
#  define DBG_PRINT(fmt, ...) fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
#  define DBG_PRINT(fmt, ...) 
#endif

#define MAX_PATHS 100


static const char *error_message = "An error has occurred\n";

// Variables globales pour gérer le path (idem qu’avant)
char *path_dirs[MAX_PATHS];
int path_count = 0;

/* === Déclarations des fonctions internes === */
int is_file_empty(FILE *file);
void cleanup_paths(void);
void handle_parallel(char *line);
int handle_redirection(char *args[]);
bool handle_builtin(char **args, int argc, bool *should_exit);
void handle_single_command(char *line);
void shell_mode(FILE *input, bool is_interactive);

/* === Fonctions indépendantes === */

// Test si un fichier est vide
int is_file_empty(FILE *file) {
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    DBG_PRINT("is_file_empty => size=%ld", size);
    return (size == 0);
}

// Libère et réinitialise la liste des répertoires du path
void cleanup_paths() {
    DBG_PRINT("cleanup_paths() appelé, path_count=%d", path_count);
    for (int i = 0; i < path_count; i++) {
        free(path_dirs[i]);
        path_dirs[i] = NULL;
    }
    path_count = 0;
    DBG_PRINT("cleanup_paths => path_count mis à 0");
}

/* === Fonctions liées à la redirection et l’exécution === */

// Gère la redirection si on détecte un symbole ">" dans args
int handle_redirection(char *args[]) {
    DBG_PRINT("handle_redirection() appelé");
    int redir_index = -1;
    int redir_count = 0;

    // 1. Rechercher et compter les ">"
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) {
            redir_index = i;
            redir_count++;
        }
    }
    DBG_PRINT("  -> redir_count=%d, redir_index=%d", redir_count, redir_index);

    // 2. Vérifier qu’il n’y a qu’un seul ">"
    if (redir_count > 1) {
        DBG_PRINT("  -> Plusieurs '>' détectés, erreur");
        write(STDERR_FILENO, error_message, strlen(error_message));
        return -1;
    }
    if (redir_count == 0) {
        DBG_PRINT("  -> Aucune redirection détectée");
        return 0;  // pas de redirection, tout va bien
    }

    // Redirection détectée
    // (a) ">" ne peut être le premier token
    if (redir_index == 0) {
        DBG_PRINT("  -> '>' en premier token, erreur");
        write(STDERR_FILENO, error_message, strlen(error_message));
        return -1;
    }

    // (b) doit y avoir exactement un token après
    if (args[redir_index + 1] == NULL || args[redir_index + 2] != NULL) {
        DBG_PRINT("  -> Mauvaise syntaxe de redirection, erreur");
        write(STDERR_FILENO, error_message, strlen(error_message));
        return -1;
    }

    // Ouvrir le fichier
    DBG_PRINT("  -> Ouverture du fichier pour redirection: %s", args[redir_index + 1]);
    int fd = open(args[redir_index + 1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        DBG_PRINT("  -> Échec open(), erreur");
        write(STDERR_FILENO, error_message, strlen(error_message));
        return -1;
    }

    // Rediriger stdout et stderr
    DBG_PRINT("  -> Dup2 sur STDOUT et STDERR");
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);

    // Tronquer la commande
    args[redir_index] = NULL;
    DBG_PRINT("  -> Redirection OK, suppression token '>' et fichier");

    return 0;
}

/* 
   Lance une commande externe (ni cd, ni exit, ni path).
   Gère redirection, puis parcourt les chemins du path.
*/
static void execute_external(char *args[]) {
    DBG_PRINT("execute_external() => exécution de '%s'", args[0]);
    pid_t pid = fork();
    if (pid == 0) {
        // Processus enfant
        DBG_PRINT("  -> Child lancé (pid=%d), on gère la redirection", getpid());
        if (handle_redirection(args) == -1) {
            DBG_PRINT("  -> Redirection échouée, exit(1)");
            exit(1);
        }

        // Parcourir le path
        char path[256];
        for (int i = 0; i < path_count; i++) {
            snprintf(path, sizeof(path), "%s/%s", path_dirs[i], args[0]);
            DBG_PRINT("  -> Test execv sur %s", path);
            if (access(path, X_OK) == 0) {
                DBG_PRINT("  -> Trouvé exécutable, on exécute");
                execv(path, args);
                // si execv réussit, on ne revient pas
            }
        }
        // Echec total
        DBG_PRINT("  -> Aucune exécution possible, on affiche erreur");
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }
    else if (pid > 0) {
        // parent
        DBG_PRINT("  -> Parent (pid=%d), on wait() l'enfant pid=%d", getpid(), pid);
        wait(NULL);
    } 
    else {
        // fork a échoué
        DBG_PRINT("  -> fork() a échoué, on affiche erreur");
        write(STDERR_FILENO, error_message, strlen(error_message));
    }
}

/* === Fonctions dépendantes (réutilisées) === */

/* 
   handle_builtin : gère cd, exit, path.
   Retourne true si c’était effectivement un builtin (et donc déjà traité).
   - should_exit sera mis à true si la commande "exit" doit terminer le shell tout de suite.
*/
bool handle_builtin(char **args, int argc, bool *should_exit) {
    DBG_PRINT("handle_builtin() => commande='%s', argc=%d", args[0], argc);
    if (strcmp(args[0], "exit") == 0) {
        // exit doit n’avoir aucun argument
        if (argc != 1) {
            DBG_PRINT("  -> exit avec arguments => erreur");
            write(STDERR_FILENO, error_message, strlen(error_message));
            return true; // c’était un builtin, on s’arrête
        }
        // sinon on signale au shell qu’on doit quitter
        DBG_PRINT("  -> commande exit valide => should_exit=true");
        *should_exit = true;
        return true;
    }
    else if (strcmp(args[0], "cd") == 0) {
        // cd doit avoir exactement 1 argument
        DBG_PRINT("  -> commande cd => argc=%d", argc);
        if (argc != 2) {
            DBG_PRINT("    -> Mauvais nb d'arguments => erreur");
            write(STDERR_FILENO, error_message, strlen(error_message));
        } else {
            DBG_PRINT("    -> chdir(%s)", args[1]);
            if (chdir(args[1]) != 0) {
                DBG_PRINT("    -> chdir échec => erreur");
                write(STDERR_FILENO, error_message, strlen(error_message));
            }
        }
        return true;
    }
    else if (strcmp(args[0], "path") == 0) {
        // On réinitialise le path
        DBG_PRINT("  -> commande path => reset path puis ajout arguments");
        cleanup_paths();
        // On ajoute chaque argument dans path_dirs
        for (int i = 1; i < argc; i++) {
            if (path_count < MAX_PATHS) {
                path_dirs[path_count++] = strdup(args[i]);
                DBG_PRINT("    -> path_dirs[%d] = '%s'", path_count-1, args[i]);
            }
        }
        return true;
    }
    // pas un builtin
    DBG_PRINT("  -> pas un builtin");
    return false;
}

/* 
   Gère une commande simple (sans séparateur &).
   1) parse_line_advanced
   2) Si builtin => on traite
   3) Sinon => exécuter external
*/
void handle_single_command(char *line) {
    // Parser
    DBG_PRINT("handle_single_command('%s')", line);
    struct string_split *parsed = parse_line_advanced(line);
    char **args = parsed->parts;
    int argc = parsed->length;
    DBG_PRINT("  -> parse_line_advanced => argc=%d", argc);

    if (argc == 0) {
        DBG_PRINT("  -> commande vide, on free et return");
        free_split(parsed);
        return;
    }

    // Vérifier redirection interdite pour builtin ?
    // -> on détecte s’il y a un ">" et si c’est un builtin
    //    c’est votre choix : on peut refuser la redirection sur cd/exit/path
    //    ou juste l’ignorer
    // ICI, par exemple, on le refuse
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) {
            if (strcmp(args[0], "cd") == 0 ||
                strcmp(args[0], "exit") == 0 ||
                strcmp(args[0], "path") == 0) {
                DBG_PRINT("  -> redirection interdite pour builtin => erreur");
                write(STDERR_FILENO, error_message, strlen(error_message));
                free_split(parsed);
                return;
            }
        }
    }

    bool should_exit = false;
    bool is_builtin = handle_builtin(args, argc, &should_exit);

    // si c’est un builtin
    if (is_builtin) {
        DBG_PRINT("  -> c'était un builtin => free_split et fin");
        free_split(parsed);
        // si c’était "exit", on doit quitter tout de suite
        if (should_exit) {
            // le main shell va s’en charger => on peut lever un flag global
            // ou on peut directement faire exit(0) ici
            // On fait un exit(0) brutal
            DBG_PRINT("  -> on exit(0) direct");
            exit(0);
        }
        return; 
    }

    // Sinon, commande externe
    DBG_PRINT("  -> commande externe => execute_external");
    execute_external(args);

    free_split(parsed);
}

/* 
   Gère la logique "parallel".
   Découpe la ligne en sous-commandes séparées par '&',
   vérifie que ce ne sont pas des builtins, etc.
*/
void handle_parallel(char *line) {
    DBG_PRINT("handle_parallel('%s')", line);
    char *saveptr = NULL;
    char *sub_cmd = strtok_r(line, "&", &saveptr);

    pid_t pids[100];
    int pid_count = 0;

    while (sub_cmd != NULL) {
        // nettoyer
        while (*sub_cmd == ' ') sub_cmd++;
        if (*sub_cmd == '\0') {
            sub_cmd = strtok_r(NULL, "&", &saveptr);
            continue;
        }

        DBG_PRINT("  -> sous-commande = '%s'", sub_cmd);
        // On parse la sous-commande
        struct string_split *parsed = parse_line_advanced(sub_cmd);
        char **args = parsed->parts;
        int argc = parsed->length;
        DBG_PRINT("    -> argc=%d", argc);

        if (argc == 0) {
            free_split(parsed);
            sub_cmd = strtok_r(NULL, "&", &saveptr);
            continue;
        }

        // Interdire builtin en parallèle
        if (strcmp(args[0], "cd") == 0 ||
            strcmp(args[0], "exit") == 0 ||
            strcmp(args[0], "path") == 0) {
            DBG_PRINT("    -> builtin interdit en parallèle => erreur");
            write(STDERR_FILENO, error_message, strlen(error_message));
            free_split(parsed);
            sub_cmd = strtok_r(NULL, "&", &saveptr);
            continue;
        }

        // fork pour exécuter la commande en parallèle
        pid_t pid = fork();
        if (pid == 0) {
            // enfant
            DBG_PRINT("    -> Child parallel => handle_redirection");
            if (handle_redirection(args) == -1) {
                // free_split(parsed); // => le child n’a pas besoin de libérer,
                // il va exit
                DBG_PRINT("      -> échec redirection => exit(1)");
                exit(1);
            }
            // Parcourt le path
            char path[256];
            for (int i = 0; i < path_count; i++) {
                snprintf(path, sizeof(path), "%s/%s", path_dirs[i], args[0]);
                DBG_PRINT("      -> test execv sur %s", path);
                if (access(path, X_OK) == 0) {
                    DBG_PRINT("      -> execv OK");
                    execv(path, args);
                }
            }
            DBG_PRINT("      -> echec total => erreur et exit");
            write(STDERR_FILENO, error_message, strlen(error_message));
            // free_split(parsed);
            exit(1);
        }
        else if (pid > 0) {
            // parent
            DBG_PRINT("    -> Parent parallel => pid=%d", pid);
            pids[pid_count++] = pid;
        }
        else {
            DBG_PRINT("    -> fork échoué => erreur");
            write(STDERR_FILENO, error_message, strlen(error_message));
        }

        free_split(parsed);
        sub_cmd = strtok_r(NULL, "&", &saveptr);
    }

    // attendre tout le monde
    DBG_PRINT("  -> on attend %d enfants", pid_count);
    for (int i = 0; i < pid_count; i++) {
        waitpid(pids[i], NULL, 0);
    }
}

/* 
   Boucle principale du shell (mode interactif ou batch).
   - Lit une ligne
   - Vérifie le parallélisme
   - Sinon appelle handle_single_command
*/
void shell_mode(FILE *input, bool is_interactive) {
    DBG_PRINT("shell_mode(...) => is_interactive=%d", is_interactive);
    char *command = NULL;
    size_t command_size = 0;

    while (1) {
        if (is_interactive) {
            printf("umonsh> ");
        }
        if (getline(&command, &command_size, input) == -1) {
            // fin de fichier ou erreur
            DBG_PRINT("EOF ou erreur => exit(0)");
            free(command);
            cleanup_paths();
            exit(0);
        }
        // enlever le \n
        command[strcspn(command, "\n")] = '\0';
        DBG_PRINT("Lu: '%s'", command);

        // check si on a un "&"
        if (strchr(command, '&') != NULL) {
            DBG_PRINT("Détecté '&' => handle_parallel");
            char *copy = strdup(command);
            if (!copy) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            handle_parallel(copy);
            free(copy);
        }
        else {
            // commande simple
            DBG_PRINT("Commande simple => handle_single_command");
            handle_single_command(command);
        }
    }
}

/* === main === */
int main(int argc, char *argv[]) {
    DBG_PRINT("Démarrage main => argc=%d", argc);
    // chemin par défaut
    path_dirs[0] = strdup("/bin");
    path_count = 1;

    if (argc == 1) {
        // interactif
        DBG_PRINT("Mode interactif => shell_mode(stdin, true)");
        shell_mode(stdin, true);
        cleanup_paths();
        return 0;
    }
    else if (argc == 2) {
        // mode batch
        DBG_PRINT("Mode batch => %s", argv[1]);
        FILE *f = fopen(argv[1], "r");
        if (!f || is_file_empty(f)) {
            DBG_PRINT("Fichier introuvable ou vide => erreur");
            write(STDERR_FILENO, error_message, strlen(error_message));
            if (f) fclose(f);
            cleanup_paths();
            return 1;
        }
        shell_mode(f, false);
        fclose(f);
        cleanup_paths();
        return 0;
    }
    else {
        // arguments invalides
        DBG_PRINT("Arguments invalides => erreur");
        write(STDERR_FILENO, error_message, strlen(error_message));
        cleanup_paths();
        return 1;
    }
}
