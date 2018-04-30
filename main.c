#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include "linenoise.h"

#define MAX_LENGTH 1024
#define clear_terminal() printf("\033[H\033[J")
#define TERMINAL_TITLE "Eggshell Terminal"

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
void print_header();

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
    print_header();

    //get USER environmental variable
    USER = getenv("USER");

    //get name of current terminal
    if(ttyname_r(STDIN_FILENO, TERMINAL, MAX_LENGTH) != 0) {
        perror("Cannot get terminal name");
    }

    //get the current working directory
    if(getcwd(CWD, MAX_LENGTH) == NULL) {
        perror("Cannot get current working directory");
    }

    //get the working directory where the shell launched from
    if(getcwd(SHELL, MAX_LENGTH) == NULL) {
        perror("Cannot get shell binary directory");
    }

    //get the search path for external commands
    PATH = getenv("PATH");
}

void welcome_message(){
    printf("Welcome %s!\n", USER);
    printf("Terminal: %s\n", TERMINAL);
    printf("Working Directory: %s\n", CWD);
    printf("Shell Path: %s\n\n", SHELL);
    printf("Search Path: %s\n", PATH);
}

void change_directory(char* path){
    chdir(path);

    if(getcwd(CWD, MAX_LENGTH) == NULL) {
        perror("Cannot get current working directory");
    }

    printf("Directory changed to: %s\n\n", CWD);
}

void print_header(){
    struct winsize terminal_size;
    ioctl(STDIN_FILENO, TIOCGWINSZ, &terminal_size);

    for(int i = 0; i < (terminal_size.ws_col - strlen(TERMINAL_TITLE))/2; i++){
        printf("-");
    }

    printf("%s", TERMINAL_TITLE);

    for(int i = 0; i < (terminal_size.ws_col - strlen(TERMINAL_TITLE))/2; i++){
        printf("-");
    }

    printf("\n");
}