#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <pwd.h>
#include <sys/types.h>
#include "linenoise.h"

#define MAX_LENGTH 300
#define EDITABLE_SHELL_VARS 7
#define NUM_INTERNAL_COMMANDS 5

#define clear_terminal() printf("\033[H\033[J")
#define TERMINAL_TITLE "Eggshell Terminal"

char PATH[MAX_LENGTH] = {0};
char *PROMPT = "eggsh> ";
char CWD[MAX_LENGTH] = {0};
char USER[MAX_LENGTH] = {0};
char HOME[MAX_LENGTH] = {0};
char SHELL[MAX_LENGTH] = {0};
char TERMINAL[MAX_LENGTH] = {0};
int EXTICODE = 0;

char SHELL_VAR_NAMES[EDITABLE_SHELL_VARS][MAX_LENGTH];

char USER_VAR_NAMES[MAX_LENGTH][MAX_LENGTH];
char USER_VAR_VALUES[MAX_LENGTH][MAX_LENGTH];

char INTERNAL_COMMANDS[NUM_INTERNAL_COMMANDS][MAX_LENGTH];

char ARGS[MAX_LENGTH][MAX_LENGTH];
int INPUT_ARGS_COUNT = 0;

int VAR_COUNT = 0;

void eggsh_init();

void welcome_message();

void print_header();

void get_and_process_input();

int check_for_char_in_string(const char input[], int input_length, char check_char);

int check_shell_variable_names(const char var_name[]);

int check_user_variable_names(const char var_name[]);

void set_variable(const char input[], int input_length, int equals_position);

char *get_variable_value(const char var_name[]);

char *get_value_after_dollar(char input[], int input_length);

void clear_string(char input[], int input_length);

void print_standard_variables();

void print_user_variables();

int tokenize_input(char input[]);

int check_internal_command(const char input[]);

int execute_internal_command(const char command[]);

void print_command();

void change_directory(char path[]);

int main(int argc, char **argv, char **env) {

    //clear any data in the terminal before starting
    clear_terminal();
    linenoiseClearScreen();

    eggsh_init();

    welcome_message();

    get_and_process_input();

    return 0;
}

//initialise shell variables
void eggsh_init() {
    //get the HOME environmental variable
    if (getenv("HOME") != NULL && HOME != "") {
        char *temp_home = getenv("HOME");
        strncpy(HOME, temp_home, strlen(temp_home));
    }

    //get the USER environmental variable
    if (getenv("USER") != NULL && USER != "") {
        char *temp_user = getenv("USER");
        strncpy(USER, temp_user, strlen(temp_user));
    }

    //get the working directory where the shell launched from
    if (getcwd(SHELL, MAX_LENGTH) == NULL) {
        perror("Cannot get shell binary directory.\n");
    }

    //get the name of current terminal
    if (ttyname_r(STDIN_FILENO, TERMINAL, MAX_LENGTH) != 0) {
        perror("Cannot get terminal name.");
    }

    //get the current working directory
    if (getcwd(CWD, MAX_LENGTH) == NULL) {
        perror("Cannot get current working directory.");
    }

    //get the search path for external commands
    if (getenv("PATH") != NULL) {
        char *temp_path = getenv("PATH");
        strncpy(PATH, temp_path, strlen(temp_path));
    }

    strncpy(SHELL_VAR_NAMES[0], "PATH", strlen("PATH"));
    strncpy(SHELL_VAR_NAMES[1], "PROMPT", strlen("PROMPT"));
    strncpy(SHELL_VAR_NAMES[2], "CWD", strlen("CWD"));
    strncpy(SHELL_VAR_NAMES[3], "USER", strlen("USER"));
    strncpy(SHELL_VAR_NAMES[4], "HOME", strlen("HOME"));
    strncpy(SHELL_VAR_NAMES[5], "SHELL", strlen("SHELL"));
    strncpy(SHELL_VAR_NAMES[6], "TERMINAL", strlen("TERMINAL"));

    strncpy(INTERNAL_COMMANDS[0], "exit", strlen("exit"));
    strncpy(INTERNAL_COMMANDS[1], "print", strlen("print"));
    strncpy(INTERNAL_COMMANDS[2], "chdir", strlen("chdir"));
    strncpy(INTERNAL_COMMANDS[3], "all", strlen("all"));
    strncpy(INTERNAL_COMMANDS[4], "source", strlen("source"));
}

//print header and welcome message
void welcome_message() {
    print_header();
    printf("Welcome %s!\n", USER);
}

void print_header() {
    struct winsize terminal_size;
    ioctl(STDIN_FILENO, TIOCGWINSZ, &terminal_size);

    for (int i = 0; i < (terminal_size.ws_col - strlen(TERMINAL_TITLE)) / 2; i++) {
        printf("-");
    }

    printf("%s", TERMINAL_TITLE);

    for (int i = 0; i < (terminal_size.ws_col - strlen(TERMINAL_TITLE)) / 2; i++) {
        printf("-");
    }

    printf("\n");
}

//indefinitely read from the input until 'exit' is entered
void get_and_process_input() {
    char *input;
    int input_length = 0;

    while ((input = linenoise(PROMPT)) != NULL) {
        input_length = (int) strlen(input);

        //checking for var=value
        int equals_position = check_for_char_in_string(input, input_length, '=');

        if (equals_position != -1 && equals_position != 0) {
            set_variable(input, input_length, equals_position);
        } else { //if the input is not var=value, tokenize it

            INPUT_ARGS_COUNT = tokenize_input(input);

            //check for internal commands
            int command_position = check_internal_command(ARGS[0]);

            if (command_position != -1) {
                if (execute_internal_command(ARGS[0]) == 1) {
                    break;
                }
            }
        }

        linenoiseFree(input);
        clear_string(input, input_length);
    }
}

//function to check if the input contains an a certain character
//returns a -1 if no character is found, else returns the position of the character
int check_for_char_in_string(const char input[], int input_length, char check_char) {
    int char_position = -1;

    for (int i = 0; i < input_length; i++) {
        if (input[i] == check_char) {
            char_position = i;
            break;
        }
    }

    return char_position;
}

//check the editable shell variables to see if a variable exists
//if the variable exists return the position of that variable, else return -1
int check_shell_variable_names(const char var_name[]) {
    int var_position = -1;

    for (int i = 0; i < EDITABLE_SHELL_VARS; i++) {
        if (strcasecmp(var_name, SHELL_VAR_NAMES[i]) == 0) {
            var_position = i;
            break;
        }
    }

    return var_position;
}

//check the array of user added variables to see if a variable exists
//if the variable exists return the position of that variable, else return -1
int check_user_variable_names(const char var_name[]) {
    int var_position = -1;

    for (int i = 0; i < VAR_COUNT; i++) {
        if (strcasecmp(var_name, USER_VAR_NAMES[i]) == 0) {
            var_position = i;
            break;
        }
    }

    return var_position;
}

//adding a user created variables if var=value is entered
void set_variable(const char input[], int input_length, int equals_position) {
    char temp_var_name[MAX_LENGTH] = {0};
    strncpy(temp_var_name, input, (size_t) equals_position);

    char temp_var_value[MAX_LENGTH] = {0};
    strncpy(temp_var_value, &input[equals_position + 1], (size_t) (input_length - (equals_position + 1)));

    int dollar_position = check_for_char_in_string(input, input_length, '$');

    if (dollar_position != -1) {
        char var_with_dollar[MAX_LENGTH] = {0};
        strncpy(var_with_dollar, temp_var_value, strlen(temp_var_value));

        clear_string(temp_var_value, MAX_LENGTH);

        strcpy(temp_var_value, get_value_after_dollar(var_with_dollar, (int) strlen(var_with_dollar)));
    }

    //checking whether a variable already exists as a shell variable
    int var_position = check_shell_variable_names(temp_var_name);

    if (var_position != -1) { //if the variable already exists as a shell variable, update the the value
        if (strcmp(SHELL_VAR_NAMES[var_position], "PATH") == 0) {
            clear_string(PATH, (int) strlen(PATH));
            strncpy(PATH, temp_var_value, strlen(temp_var_value));
        } else if (strcmp(SHELL_VAR_NAMES[var_position], "PROMPT") == 0) {
            clear_string(PROMPT, (int) strlen(PROMPT));
            strncpy(PROMPT, temp_var_value, strlen(temp_var_value));
        } else if (strcmp(SHELL_VAR_NAMES[var_position], "CWD") == 0) {
            clear_string(CWD, (int) strlen(CWD));
            strncpy(CWD, temp_var_value, strlen(temp_var_value));
        } else if (strcmp(SHELL_VAR_NAMES[var_position], "USER") == 0) {
            clear_string(USER, (int) strlen(USER));
            strncpy(USER, temp_var_value, strlen(temp_var_value));
        } else if (strcmp(SHELL_VAR_NAMES[var_position], "HOME") == 0) {
            clear_string(HOME, (int) strlen(HOME));
            strncpy(HOME, temp_var_value, strlen(temp_var_value));
        } else if (strcmp(SHELL_VAR_NAMES[var_position], "SHELL") == 0) {
            clear_string(SHELL, (int) strlen(SHELL));
            strncpy(SHELL, temp_var_value, strlen(temp_var_value));
        } else if (strcmp(SHELL_VAR_NAMES[var_position], "TERMINAL") == 0) {
            clear_string(TERMINAL, (int) strlen(TERMINAL));
            strncpy(TERMINAL, temp_var_value, strlen(temp_var_value));
        }
    } else {
        var_position = check_user_variable_names(temp_var_name);

        if (var_position != -1) { //if the variable already exists as a user added variable, update the the value
            clear_string(USER_VAR_VALUES[var_position], MAX_LENGTH);
            strncpy(USER_VAR_VALUES[var_position], temp_var_value, strlen(temp_var_value));
        } else { //if the variable does not exist, add a new variable
            strncpy(USER_VAR_NAMES[VAR_COUNT], temp_var_name, strlen(temp_var_name));
            strncpy(USER_VAR_VALUES[VAR_COUNT], temp_var_value, strlen(temp_var_value));
            VAR_COUNT++;
        }
    }
}

//function which takes a string as an input and checks
//if that string is a variable which has a corresponding value
//if the variable name is found, its name is returned
char *get_variable_value(const char var_name[]) {
    int var_position = -1;

    for (int i = 0; i < EDITABLE_SHELL_VARS; i++) {
        if (strcmp(SHELL_VAR_NAMES[i], var_name) == 0) {
            var_position = i;
            break;
        }
    }

    if (var_position != -1) {
        if (strcmp(SHELL_VAR_NAMES[var_position], "PATH") == 0) {
            return PATH;
        } else if (strcmp(SHELL_VAR_NAMES[var_position], "PROMPT") == 0) {
            return PROMPT;
        } else if (strcmp(SHELL_VAR_NAMES[var_position], "CWD") == 0) {
            return CWD;
        } else if (strcmp(SHELL_VAR_NAMES[var_position], "USER") == 0) {
            return USER;
        } else if (strcmp(SHELL_VAR_NAMES[var_position], "HOME") == 0) {
            return HOME;
        } else if (strcmp(SHELL_VAR_NAMES[var_position], "SHELL") == 0) {
            return SHELL;
        } else if (strcmp(SHELL_VAR_NAMES[var_position], "TERMINAL") == 0) {
            return TERMINAL;
        }
    } else {
        for (int i = 0; i < VAR_COUNT; i++) {
            if (strcmp(USER_VAR_NAMES[i], var_name) == 0) {
                var_position = i;
                break;
            }
        }

        if (var_position != -1) {
            return USER_VAR_VALUES[var_position];
        } else {
            return "VALUE NOT FOUND";
        }
    }
}

//returns the value when the input received is '$value'
char *get_value_after_dollar(char input[], int input_length) {
    char var_after_dollar[MAX_LENGTH] = {0};
    char *var_value = strdup("");

    strncpy(var_after_dollar, &input[1], (size_t) (input_length - 1));

    strcpy(var_value, get_variable_value(var_after_dollar));

    if (strcmp(var_value, "VALUE NOT FOUND") == 0) {
        clear_string(var_value, (int) strlen(var_value));
        strncpy(var_value, input, (size_t) (input_length));
        return input;
    } else {
        return get_variable_value(var_after_dollar);
    }
}

//clear the given string
void clear_string(char input[], int input_length) {
    for (int i = 0; i < input_length; i++) {
        input[i] = 0;
    }
}

//print all the standard variables
void print_standard_variables() {
    //PATH
    //PROMPT
    //CWD
    //USER
    //HOME
    //SHELL
    //TERMINAL
    printf("PATH=%s\n", PATH);
    printf("PROMPT=%s\n", PROMPT);
    printf("CWD=%s\n", CWD);
    printf("USER=%s\n", USER);
    printf("HOME=%s\n", HOME);
    printf("SHELL=%s\n", SHELL);
    printf("TERMINAL=%s\n", TERMINAL);
}

//print all the user variables
void print_user_variables() {
    for (int i = 0; i < VAR_COUNT; i++) {
        printf("%s=%s \n", USER_VAR_NAMES[i], USER_VAR_VALUES[i]);
    }
}

int tokenize_input(char input[]) {
    char *token;
    int token_index = 0;

    for (int i = 0; i < MAX_LENGTH; i++) {
        clear_string(ARGS[i], MAX_LENGTH);
    }

    token = strtok(input, " ");

    while (token != NULL && token_index < MAX_LENGTH) {
        strncpy(ARGS[token_index], token, strlen(token));
        token_index++;
        token = strtok(NULL, " ");
    }

    return token_index;
}

//check whether the first argument in the input is an internal command
int check_internal_command(const char input[]) {
    int command_position = -1;

    for (int i = 0; i < NUM_INTERNAL_COMMANDS; i++) {
        if (strcasecmp(input, INTERNAL_COMMANDS[i]) == 0) {
            command_position = i;
            break;
        }
    }

    return command_position;
}

//execute an internal command depending on the first argument
int execute_internal_command(const char command[]) {
    int exit = 0;

    if (strcasecmp(command, "exit") == 0) {
        exit = 1;
    } else if (strcasecmp(command, "print") == 0) {
        print_command();
    } else if (strcasecmp(command, "chdir") == 0) {
        change_directory(ARGS[1]);
    } else if (strcasecmp(command, "all") == 0) {
        print_standard_variables();
        print_user_variables();
    } else if (strcasecmp(command, "source") == 0) {

    }

    return exit;
}

//functions which prints the input, similar to echo
void print_command() {
    int quotes_num = 0;
    int print_literal = 0;

    //check for only one word with two ""
    if (ARGS[1][0] == '"' && ARGS[1][strlen(ARGS[1]) - 1] == '"' && strlen(ARGS[1]) > 1) {
        printf("%.*s\n", (int) (strlen(ARGS[1]) - 2), &ARGS[1][1]);
        return;
    }

    //check for a single " in the start or end of each ARG
    for (int i = 1; i < INPUT_ARGS_COUNT; i++) {
        if (ARGS[i][0] == '"') {
            quotes_num++;
        }

        if (strlen(ARGS[i]) > 1) {
            if (ARGS[i][strlen(ARGS[i]) - 1] == '"') {
                quotes_num++;
            }
        }
    }

    if (quotes_num < 2) {
        for (int i = 1; i < INPUT_ARGS_COUNT; i++) {
            if (ARGS[i][0] == '$') {
                if (strcasecmp(get_value_after_dollar(ARGS[i], (int) strlen(ARGS[i])), "") != 0) {
                    printf("%s ", get_value_after_dollar(ARGS[i], (int) strlen(ARGS[i])));
                } else {
                    printf("%s ", ARGS[i]);
                }
            } else {
                printf("%s ", ARGS[i]);
            }
        }
        printf("\n");
    } else {
        for (int i = 1; i < INPUT_ARGS_COUNT; i++) {
            if (print_literal == 0) {
                //checking for special case
                if (ARGS[i][0] == '"' && ARGS[i][strlen(ARGS[i]) - 1] == '"' && strlen(ARGS[i]) > 1) {
                    printf("%.*s ", (int) (strlen(ARGS[i]) - 2), &ARGS[i][1]);
                } else {
                    //assuming first quote is the beginning of a string
                    if (ARGS[i][0] == '"') {
                        if (strlen(ARGS[i]) > 1) {
                            printf("%s ", ARGS[i] + 1);
                            print_literal = 1;
                        } else {
                            print_literal = 1;
                        }
                    } else {
                        if (ARGS[i][0] == '$') {
                            if (strcasecmp(get_value_after_dollar(ARGS[i], (int) strlen(ARGS[i])), "") != 0) {
                                printf("%s ", get_value_after_dollar(ARGS[i], (int) strlen(ARGS[i])));
                            } else {
                                printf("%s ", ARGS[i]);
                            }
                        } else {
                            printf("%s ", ARGS[i]);
                        }
                    }
                }
            } else {
                //assuming second quote is in the end of a string
                int second_quote = check_for_char_in_string(ARGS[i], (int) strlen(ARGS[i]), '"');
                if (second_quote != -1) {
                    if (strlen(ARGS[i]) > 1) {
                        printf("%.*s ", second_quote, ARGS[i]);
                        print_literal = 0;
                    } else {
                        print_literal = 0;
                    }
                } else {
                    printf("%s ", ARGS[i]);
                }
            }

        }
        printf("\n");
    }
}


void change_directory(char path[]) {
    if (chdir(path) != 0) {
        perror("Cannot change directory.");
    } else {
        clear_string(CWD, MAX_LENGTH);
        if (getcwd(CWD, MAX_LENGTH) == NULL) {
            perror("Cannot set current working directory.");
        }
    }
}