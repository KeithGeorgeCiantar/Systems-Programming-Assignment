#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include "linenoise.h"

#define MAX_LENGTH 1024
#define clear_terminal() printf("\033[H\033[J")

static char* PATH = "/usr/bin";
static char* PROMPT = "eggsh> ";
static char CWD[MAX_LENGTH];
static char* USER;
static char* HOME = "/home/keithgciantar";
static char SHELL[MAX_LENGTH];
static char TERMINAL[MAX_LENGTH];
//static int EXTICODE = 1;

void eggsh_start();
void welcome_message();
void change_directory(char* path);

int main(int argc, char **argv, char **env) {

    clear_terminal();
    linenoiseClearScreen();

    char* input;

    eggsh_start();
    welcome_message();

    change_directory("/home/keithgciantar/Desktop");

    input = linenoise(PROMPT);

    printf("%s is your input...\n\n", input);

    return 0;
}

void eggsh_start(){
    struct winsize terminal_size;
    ioctl(STDIN_FILENO, TIOCGWINSZ, &terminal_size);

    char* title = "Eggshell Terminal";

    for(int i = 0; i < (terminal_size.ws_col - strlen(title))/2; i++){
        printf("-");
    }

    printf("%s", title);

    for(int i = 0; i < (terminal_size.ws_col - strlen(title))/2; i++){
        printf("-");
    }

    printf("\n");

    USER = getenv("USER");

    if(ttyname_r(STDIN_FILENO, TERMINAL, MAX_LENGTH) != 0) {
        perror("Cannot get terminal name");
    }

    if(getcwd(CWD, MAX_LENGTH) == NULL) {
        perror("Cannot get current working directory");
    }

    if(getcwd(SHELL, MAX_LENGTH) == NULL) {
        perror("Cannot get shell binary directory");
    }
}

void welcome_message(){
    printf("Welcome %s!\n", USER);
    printf("Terminal: %s\n", TERMINAL);
    printf("Working Directory: %s\n", CWD);
    printf("Shell Path: %s\n\n", SHELL);
}

void change_directory(char* path){
    chdir(path);

    if(getcwd(CWD, MAX_LENGTH) == NULL) {
        perror("Cannot get current working directory");
    }

    printf("Directory changed to: %s\n\n", CWD);
}