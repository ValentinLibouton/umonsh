# umonsh
## 1) Présentation du projet
Ce projet consiste à implémenter un mini‐shell Unix nommé umonsh.
Les fonctionnalités principales incluent :
- L’exécution de commandes simples (avec création d’un process enfant à chaque commande).
- La gestion des commandes internes : cd, path, exit.
- La redirection de la sortie via l’opérateur >.
- L’exécution de plusieurs commandes en parallèle, séparées par &.
- La distinction des modes interactif (sans argument) et batch (avec un fichier de script passé en argument).
## 2) Structure de l’archive / organisation du code

- umonsh.c
	Fichier principal, contenant l’implémentation de la boucle du shell, la gestion des commandes internes, la création de processus enfants, etc.
	- Dépend de string_split_utils.h/.c pour la découpe avancée des lignes.
	- Dépend aussi des bibliothèques standard (unistd, stdio, stdlib, sys/wait, fcntl) pour la création de processus, l’IO, etc.

- string_split_utils.h / string_split_utils.c
    Module responsable du découpage (parsing) des lignes de commande.
    - Fonction principale : parse_line_advanced(const char \*input), qui sépare non seulement les tokens par les espaces, mais isole aussi > et & comme des tokens distincts.
    - Fonctions utilitaires pour gérer la structure string_split : make_string_split, add_to_split, free_split.
## 3) Compilation

Pour compiler les deux fichiers :
```bash
gcc umonsh.c string_split_utils.c -o umonsh -Wall
```
- Wall active les avertissements.
- Vous pouvez ajouter -Wextra -Werror si vous voulez un niveau plus strict.

Après compilation, vous obtenez un exécutable umonsh.

## 4) Exécution
1. **Mode interactif** (sans argument) :
```bash
./umonsh
```
Le shell affiche un prompt `umonsh>` et attend vos commandes. Tapez **exit** pour sortir.
2. **Mode batch** (avec un fichier script) :
```bash
./umonsh fichier.in
```
Le shell lira les commandes depuis le fichier `fichier.in` et les exécutera successivement sans afficher de prompt.
3. **Exemples de commandes** :
- `ls` – exécute /bin/ls
- `cd /tmp` – change le répertoire courant à `/tmp`
- `path /usr/bin /bin` – définit deux répertoires de recherche
- `ls > out.txt` – redirige la sortie de ls vers `out.txt` (et la sortie d’erreur standard aussi)
- `cmd1 & cmd2 & cmd3` – exécute trois commandes en parallèle
- `exit` – quitte le shell
## 5) Debug / logs

Le fichier **umonsh.c** possède une macro :
```c
#define DEBUG 0
#if DEBUG
#  define DBG_PRINT(fmt, ...) fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
#  define DBG_PRINT(fmt, ...) 
#endif
```
- Pour **activer** les logs de debug, éditez `umonsh.c` et mettez `#define DEBUG 1`, puis recompilez.
- Les messages de debug apparaîtront sur `stderr` avec un préfixe `[DEBUG]`.
- Quand DEBUG vaut 0, toutes ces impressions sont supprimées à la compilation.

## 6) Remarques et limitations

1. Le shell ne gère pas les redirections plus complexes (`2>`, `<`, `>>`, etc.) — seule la redirection de la sortie standard et de la sortie d’erreur via `>` est implémentée.
2. Le shell **n’autorise pas** la redirection sur les commandes internes `cd`, `exit`, `path`. Si l’utilisateur tente `cd /tmp > out`, un message d’erreur s’affiche.
3. Les commandes internes ne sont pas autorisées en mode **parallèle** (ex. `cd & ls` est refusé).
4. Le projet suppose que l’utilisateur mettra des espaces autour de `>` et `&`. Cependant, le parseur avancé `parse_line_advanced` est capable de traiter un `ls>fichier` sans espace, etc.
5. Le shell se termine en code retour **0** dans la plupart des cas, sauf si un fichier batch est inexistant ou vide (alors il renvoie 1).
