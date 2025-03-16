/***********************************************
 * Project 2: Shell
 * 
 * Group Number : 10
 * Students     : Bryan Pax, Valentin Libouton
 * 
 * Please add details if compilation of your project is not
 * straightforward (for instance, if you use multiple files).
 ***********************************************/
# include <unistd.h>
# include <string.h>

const char *error_message = "An error has occured\n";

int main(int argc, char *argv[]) {
    if (argc == 1) {
        // Mode interactif
    } else if (argc == 2) {
        // Mode batch
    } else {
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }
    return 0;
}
