#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "linenoise.h"

#define MAX_LENGTH 512
#define NUM_INTERNAL_COMMANDS 5

#define clear_terminal() printf("\033[H\033[J")
#define TERMINAL_TITLE "Eggshell Terminal"

//shell variables
char PATH[MAX_LENGTH] = {0};
char *PROMPT = "eggsh> ";
char CWD[MAX_LENGTH] = {0};
char USER[MAX_LENGTH] = {0};
char HOME[MAX_LENGTH] = {0};
char SHELL[MAX_LENGTH] = {0};
char TERMINAL[MAX_LENGTH] = {0};
int EXITCODE = 0;

//user created variables
char USER_VAR_NAMES[MAX_LENGTH][MAX_LENGTH];
char USER_VAR_VALUES[MAX_LENGTH][MAX_LENGTH];

//array that stores the internal commands
char INTERNAL_COMMANDS[NUM_INTERNAL_COMMANDS][MAX_LENGTH];

//tokenised input arguments
char *ARGS[MAX_LENGTH];

//number of input arguments
int INPUT_ARGS_COUNT = 0;

//number of user created variables
int VAR_COUNT = 0;

//number of times source has been called in source
int SOURCE_DEPTH = 0;

void eggsh_init();

void welcome_message();

void print_header();

void get_input_from_terminal();

void get_input_from_file(const char filename[]);

int check_for_char_in_string(const char input[], int input_length, char check_char);

int check_var_name_validity(const char input[], int input_length);

int check_user_variable_names(const char var_name[]);

void set_variable(const char input[], int input_length, int equals_position);

char *get_variable_value(const char var_name[]);

char *get_value_after_dollar(char input[], int input_length);

void clear_string(char input[], int input_length);

void print_standard_variables();

void print_user_variables();

int tokenise_input(char input[]);

void clear_and_null_args();

int check_internal_command(const char input[]);

int execute_internal_command(const char command[], int redirect);

void print_command();

void change_directory(char path[]);

void execute_external_command(const char command[], int redirect);

int main(int argc, char **argv, char **env) {

    //clear any data in the terminal before starting
    clear_terminal();
    linenoiseClearScreen();

    //set the history of linenoise
    linenoiseHistorySetMaxLen(10);

    eggsh_init();

    welcome_message();

    get_input_from_terminal();

    return 0;
}

//function which initialises shell variables
void eggsh_init() {
    //get the HOME environmental variable
    if (getenv("HOME") != NULL) {
        char *temp_home = getenv("HOME");
        strncpy(HOME, temp_home, strlen(temp_home));
    } else {
        perror("Cannot get HOME environment variable");
    }

    //get the USER environmental variable
    if (getenv("USER") != NULL) {
        char *temp_user = getenv("USER");
        strncpy(USER, temp_user, strlen(temp_user));
    } else {
        perror("Cannot get USER environment variable");
    }

    //get the SHELL environmental variable
    if (getenv("SHELL") != NULL) {
        char *temp_shell = getenv("SHELL");
        strncpy(SHELL, temp_shell, strlen(temp_shell));
    } else {
        perror("Cannot get SHELL environment variable");
    }

    //get the name of current terminal
    if (ttyname_r(STDIN_FILENO, TERMINAL, MAX_LENGTH) != 0) {
        perror("Cannot get terminal name.");
    }

    //get the current working directory
    if (getcwd(CWD, sizeof(CWD)) == NULL) {
        perror("Cannot get current working directory");
    }

    //get the search path for external commands
    if (getenv("PATH") != NULL) {
        char *temp_path = getenv("PATH");
        strncpy(PATH, temp_path, strlen(temp_path));
    }

    //fill up the array with all the internal commands to check later on
    //this array is used to allow for more commands in the future
    strncpy(INTERNAL_COMMANDS[0], "exit", strlen("exit"));
    strncpy(INTERNAL_COMMANDS[1], "print", strlen("print"));
    strncpy(INTERNAL_COMMANDS[2], "chdir", strlen("chdir"));
    strncpy(INTERNAL_COMMANDS[3], "all", strlen("all"));
    strncpy(INTERNAL_COMMANDS[4], "source", strlen("source"));

    //set all the input arguments to null
    for (int i = 0; i < MAX_LENGTH; i++) {
        ARGS[i] = NULL;
    }
}

//function that prints the header and a welcome message
void welcome_message() {
    print_header();
    printf("Welcome %s!\n", USER);
}

//function that prints a line a the title of the shell
void print_header() {
    //get the width of the terminal
    struct winsize terminal_size;
    ioctl(STDIN_FILENO, TIOCGWINSZ, &terminal_size);

    //print dashed line
    if (terminal_size.ws_col != 0) {
        for (int i = 0; i < (terminal_size.ws_col - strlen(TERMINAL_TITLE)) / 2; i++) {
            printf("-");
        }
    }

    //print terminal title
    printf("%s", TERMINAL_TITLE);

    //print dashed line
    if (terminal_size.ws_col != 0) {
        for (int i = 0; i < (terminal_size.ws_col - strlen(TERMINAL_TITLE)) / 2; i++) {
            printf("-");
        }
    }

    printf("\n");
}

//function which gets the input from the terminal through linenoise
//checks the input and executes the appropriate set of commands
void get_input_from_terminal() {
    char *input = "";
    int input_length = 0;

    fflush(STDIN_FILENO);

    //get input from terminal
    while ((input = linenoise(PROMPT)) != NULL) {

        //store the input in the linenoise history
        linenoiseHistoryAdd(input);

        input_length = (int) strlen(input);

        //checking for VAR=VALUE
        int equals_position = check_for_char_in_string(input, input_length, '=');

        //if VAR=VALUE, either set an existing variable or create a new one
        if (equals_position != -1 && equals_position != 0) {
            set_variable(input, input_length, equals_position);
        } else if (strcasecmp(input, "") != 0) { //if the input is not VAR=VALUE, tokenise it
            INPUT_ARGS_COUNT = tokenise_input(input);

            int redirect_type = 0;

            //check if the input contains any arguments for redirection
            if (INPUT_ARGS_COUNT > 2) {
                if (strcmp(ARGS[INPUT_ARGS_COUNT - 2], ">") == 0) {
                    redirect_type = 1;
                } else if (strcmp(ARGS[INPUT_ARGS_COUNT - 2], ">>") == 0) {
                    redirect_type = 2;
                } else if (strcmp(ARGS[1], "<") == 0) {
                    redirect_type = 3;
                } else if (strcmp(ARGS[1], "<<<") == 0) {
                    redirect_type = 4;
                }
            }

            char command[10 * MAX_LENGTH] = "";

            //read input from file for input redirection
            if (redirect_type == 3) {
                char filename[MAX_LENGTH] = "";
                strncpy(filename, ARGS[INPUT_ARGS_COUNT - 1], strlen(ARGS[INPUT_ARGS_COUNT - 1]));

                FILE *openFile;
                int file_length = 0;
                char line[MAX_LENGTH] = "";
                char file[10 * MAX_LENGTH] = "";

                //open the file in read only mode
                if ((openFile = fopen(filename, "r")) == NULL) {
                    perror("Cannot open file");
                } else {
                    while (fgets(line, sizeof(line), openFile) != NULL) {
                        strcat(file, line);
                        clear_string(line, (int) strlen(line));
                    }

                    file_length = (int) strlen(file);
                    if (file[file_length - 1] == '\n') {
                        file[file_length - 1] = '\0';
                    }

                    sprintf(command, "%s %s", ARGS[0], file);

                    clear_and_null_args();

                    INPUT_ARGS_COUNT = tokenise_input(command);

                    fclose(openFile);

                    clear_string(filename, (int) strlen(filename));
                    clear_string(file, file_length);
                }
            } else if (redirect_type == 4) { //read input from here string for input redirection
                if (INPUT_ARGS_COUNT == 3) {
                    //shift the arguments over by one and null the last one
                    //remove any ' from the string
                    clear_string(ARGS[1], (int) strlen(ARGS[1]));
                    ARGS[1] = ARGS[2];
                    ARGS[2] = NULL;
                    INPUT_ARGS_COUNT--;
                    strncpy(ARGS[1], &ARGS[1][1], strlen(ARGS[1]));
                    ARGS[1][(int) strlen(ARGS[1])] = '\0';
                    ARGS[1][(int) strlen(ARGS[1]) - 1] = '\0';
                } else if (INPUT_ARGS_COUNT > 3) {
                    //shift the arguments over by one and null the last one
                    //remove any ' from the first string and last string
                    clear_string(ARGS[1], (int) strlen(ARGS[1]));
                    for (int i = 1; i < INPUT_ARGS_COUNT - 1; i++) {
                        ARGS[i] = ARGS[i + 1];
                    }

                    INPUT_ARGS_COUNT--;

                    if (ARGS[1][0] == '\'') {
                        strncpy(ARGS[1], &ARGS[1][1], strlen(ARGS[1]));
                        ARGS[1][(int) strlen(ARGS[1])] = '\0';
                    }

                    if (ARGS[INPUT_ARGS_COUNT - 1][(int) strlen(ARGS[INPUT_ARGS_COUNT - 1]) - 1] == '\'') {
                        ARGS[INPUT_ARGS_COUNT - 1][(int) strlen(ARGS[INPUT_ARGS_COUNT - 1]) - 1] = '\0';
                    }
                }
            }

            //check for internal commands
            int command_position = check_internal_command(ARGS[0]);

            if (command_position != -1) {
                //check if 'exit' is entered
                if (execute_internal_command(ARGS[0], redirect_type) == 1) {
                    break;
                }
            } else { //if it is not an internal command, then it must be an external command
                execute_external_command(ARGS[0], redirect_type);
            }

            //clear and null all the input arguments to that the array can be refilled
            clear_and_null_args();
            clear_string(command, (int) strlen(command));
        }

        //clear the input so that it can be refilled
        clear_string(input, input_length);

        //free the allocated linenoise input
        linenoiseFree(input);
    }
}

//this function is very similar to previous one but instead of getting
//input from linoise, the input is received from a file when 'source' is executed
void get_input_from_file(const char filename[]) {
    FILE *openFile;
    int line_length = 0;
    char line[MAX_LENGTH];

    //open the file in read only mode
    if ((openFile = fopen(filename, "r")) == NULL) {
        perror("Cannot open file");
    } else {
        clear_and_null_args();
        //if a source is run in a source, this will get incremented
        SOURCE_DEPTH++;

        //get inputs from the file line by line
        while (fgets(line, sizeof(line), openFile) != NULL) {
            line_length = (int) strlen(line);

            //remove \n from the end of the line
            if (line[line_length - 1] == '\n') {
                line[line_length - 1] = '\0';
            }

            //checking for VAR=VALUE
            int equals_position = check_for_char_in_string(line, line_length, '=');

            //if VAR=VALUE, either set an existing variable or create a new one
            if (equals_position != -1 && equals_position != 0) {
                set_variable(line, line_length, equals_position);
            } else if (strcasecmp(line, "") != 0) { //if the input is not VAR=VALUE, tokenise it
                INPUT_ARGS_COUNT = tokenise_input(line);

                //code to handle redirection inside source, still buggy
                int redirect_type = 0;

                /*if (INPUT_ARGS_COUNT > 2) {
                    if (strcmp(ARGS[INPUT_ARGS_COUNT - 2], ">") == 0) {
                        redirect_type = 1;
                    } else if (strcmp(ARGS[INPUT_ARGS_COUNT - 2], ">>") == 0) {
                        redirect_type = 2;
                    } else if (strcmp(ARGS[1], "<") == 0) {
                        redirect_type = 3;
                    } else if (strcmp(ARGS[1], "<<<") == 0) {
                        redirect_type = 4;
                    }
                }

                char command[10 * MAX_LENGTH] = "";

                if (redirect_type == 3) {
                    char redirect_filename[MAX_LENGTH] = "";
                    strncpy(redirect_filename, ARGS[INPUT_ARGS_COUNT - 1], strlen(ARGS[INPUT_ARGS_COUNT - 1]));

                    FILE *redirectFile;
                    int file_length = 0;
                    char redirect_line[MAX_LENGTH] = "";
                    char file[10 * MAX_LENGTH] = "";

                    if ((redirectFile = fopen(redirect_filename, "r")) == NULL) {
                        perror("Cannot open file");
                    } else {
                        while (fgets(redirect_line, sizeof(redirect_line), redirectFile) != NULL) {
                            strcat(file, redirect_line);
                            clear_string(redirect_line, (int) strlen(redirect_line));
                        }

                        file_length = (int) strlen(file);
                        if (file[file_length - 1] == '\n') {
                            file[file_length - 1] = '\0';
                        }

                        sprintf(command, "%s %s", ARGS[0], file);

                        clear_and_null_args();

                        INPUT_ARGS_COUNT = tokenise_input(command);

                        fclose(redirectFile);

                        clear_string(redirect_filename, (int) strlen(redirect_filename));
                        clear_string(file, file_length);
                    }
                } else if (redirect_type == 4) {
                    if (INPUT_ARGS_COUNT == 3) {
                        clear_string(ARGS[1], (int) strlen(ARGS[1]));
                        ARGS[1] = ARGS[2];
                        ARGS[2] = NULL;
                        INPUT_ARGS_COUNT--;
                        strncpy(ARGS[1], &ARGS[1][1], strlen(ARGS[1]));
                        ARGS[1][(int) strlen(ARGS[1])] = '\0';
                        ARGS[1][(int) strlen(ARGS[1]) - 1] = '\0';
                    } else if (INPUT_ARGS_COUNT > 3) {
                        clear_string(ARGS[1], (int) strlen(ARGS[1]));
                        for (int i = 1; i < INPUT_ARGS_COUNT - 1; i++) {
                            ARGS[i] = ARGS[i + 1];
                        }

                        INPUT_ARGS_COUNT = INPUT_ARGS_COUNT - 1;

                        if (ARGS[1][0] == '\'') {
                            strncpy(ARGS[1], &ARGS[1][1], strlen(ARGS[1]));
                            ARGS[1][(int) strlen(ARGS[1])] = '\0';
                        }

                        if (ARGS[INPUT_ARGS_COUNT - 1][(int) strlen(ARGS[INPUT_ARGS_COUNT - 1]) - 1] == '\'') {
                            ARGS[INPUT_ARGS_COUNT - 1][(int) strlen(ARGS[INPUT_ARGS_COUNT - 1]) - 1] = '\0';
                        }
                    }
                }*/

                //check for internal commands
                int command_position = check_internal_command(ARGS[0]);

                if (command_position != -1) {
                    //check if 'exit' is entered
                    if (execute_internal_command(ARGS[0], redirect_type) == 1) {
                        break;
                    }
                } else { //if it is not an internal command, then it must be an external command
                    execute_external_command(ARGS[0], redirect_type);
                }

                //this is to check for when a source is run from a source
                if (SOURCE_DEPTH == 2) {
                    clear_and_null_args();
                }

                //clear_string(command, (int) strlen(command));
            }

            clear_string(line, line_length);
        }

        //if a source has finished executing in a source, this will get decremented
        SOURCE_DEPTH--;

        //close the file to avoid any problems
        fclose(openFile);
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

//function to check whether a user created variable is valid
//returns 0 if invalid and 1 if valid
int check_var_name_validity(const char input[], int input_length) {
    int valid = 1;

    for (int i = 0; i < input_length; i++) {
        if (input[i] >= 48 && input[i] <= 57) {
            valid = 1;
        } else if (input[i] >= 65 && input[i] <= 90) {
            valid = 1;
        } else if (input[i] >= 97 && input[i] <= 122) {
            valid = 1;
        } else if (input[i] == '_') {
            valid = 1;
        } else {
            valid = 0;
            break;
        }
    }

    return valid;
}

//function which checks the array of user added variables to see if a variable exists
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

//function which edits a current variable ot adds a user created
//variable if VAR=VALUE is entered
void set_variable(const char input[], int input_length, int equals_position) {
    char temp_var_name[MAX_LENGTH] = {0};
    strncpy(temp_var_name, input, (size_t) equals_position);

    //check if the variable name is valid
    if (check_var_name_validity(temp_var_name, (int) strlen(temp_var_name)) == 0) {
        printf("Invalid variable name.\n");
        return;
    }

    //check if the value of the variable is left empty
    if (equals_position == input_length - 1) {
        printf("Invalid variable value.\n");
        return;
    }

    char temp_var_value[MAX_LENGTH] = {0};
    strncpy(temp_var_value, &input[equals_position + 1], (size_t) (input_length - (equals_position + 1)));

    //check if the $ is used
    int dollar_position = check_for_char_in_string(input, input_length, '$');

    //if the string contains a $ after the =, get the value determined by the variable name after the $
    if (dollar_position == (equals_position + 1)) {
        char var_with_dollar[MAX_LENGTH] = {0};
        strncpy(var_with_dollar, temp_var_value, strlen(temp_var_value));

        clear_string(temp_var_value, MAX_LENGTH);

        strcpy(temp_var_value, get_value_after_dollar(var_with_dollar, (int) strlen(var_with_dollar)));
    }

    //checking whether a variable already exists as a shell variable
    //if the variable already exists as a shell variable, update the the value
    if (strcmp(temp_var_name, "PATH") == 0) {
        if (setenv("PATH", temp_var_value, 1) != 0) {
            perror("Cannot set variable PATH");
        } else {
            clear_string(PATH, (int) strlen(PATH));
            strncpy(PATH, temp_var_value, strlen(temp_var_value));
        }
    } else if (strcmp(temp_var_name, "PROMPT") == 0) {
        clear_string(PROMPT, (int) strlen(PROMPT));
        strncpy(PROMPT, temp_var_value, strlen(temp_var_value));
    } else if (strcmp(temp_var_name, "CWD") == 0) {
        if (chdir(temp_var_value) != 0) {
            perror("Cannot change directory");
        } else {
            clear_string(CWD, (int) strlen(CWD));
            strncpy(CWD, temp_var_value, strlen(temp_var_value));
        }
    } else if (strcmp(temp_var_name, "USER") == 0) {
        if (setenv("USER", temp_var_value, 1) != 0) {
            perror("Cannot set variable USER");
        } else {
            clear_string(USER, (int) strlen(USER));
            strncpy(USER, temp_var_value, strlen(temp_var_value));
        }
    } else if (strcmp(temp_var_name, "HOME") == 0) {
        if (setenv("HOME", temp_var_value, 1) != 0) {
            perror("Cannot set variable HOME");
        } else {
            clear_string(HOME, (int) strlen(HOME));
            strncpy(HOME, temp_var_value, strlen(temp_var_value));
        }
    } else if (strcmp(temp_var_name, "SHELL") == 0) {
        if (setenv("SHELL", temp_var_value, 1) != 0) {
            perror("Cannot set variable SHELL");
        } else {
            clear_string(SHELL, (int) strlen(SHELL));
            strncpy(SHELL, temp_var_value, strlen(temp_var_value));
        }
    } else if (strcmp(temp_var_name, "TERMINAL") == 0) {
        clear_string(TERMINAL, (int) strlen(TERMINAL));
        strncpy(TERMINAL, temp_var_value, strlen(temp_var_value));
    } else {
        //if the variable name is not a shell variable, check if it is a user added variable
        int var_position = check_user_variable_names(temp_var_name);

        if (var_position != -1) { //if the variable already exists as a user added variable, update the the value
            clear_string(USER_VAR_VALUES[var_position], MAX_LENGTH);
            strncpy(USER_VAR_VALUES[var_position], temp_var_value, strlen(temp_var_value));
        } else { //if the variable does not exist, add a new variable
            strncpy(USER_VAR_NAMES[VAR_COUNT], temp_var_name, strlen(temp_var_name));
            strncpy(USER_VAR_VALUES[VAR_COUNT], temp_var_value, strlen(temp_var_value));
            VAR_COUNT++;
        }
        setenv(temp_var_name, temp_var_value, 1);
    }
}

//function which takes a string as an input and checks
//if that string is a variable which has a corresponding value
//if the variable name is found, its value is returned
char *get_variable_value(const char var_name[]) {
    int var_position = -1;

    if (strcmp(var_name, "PATH") == 0) {
        return PATH;
    } else if (strcmp(var_name, "PROMPT") == 0) {
        return PROMPT;
    } else if (strcmp(var_name, "CWD") == 0) {
        return CWD;
    } else if (strcmp(var_name, "USER") == 0) {
        return USER;
    } else if (strcmp(var_name, "HOME") == 0) {
        return HOME;
    } else if (strcmp(var_name, "SHELL") == 0) {
        return SHELL;
    } else if (strcmp(var_name, "TERMINAL") == 0) {
        return TERMINAL;
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
            return "";
        }
    }
}

//function which returns the value of the variable when the input received is '$value'
char *get_value_after_dollar(char input[], int input_length) {
    char var_after_dollar[MAX_LENGTH] = {0};

    strncpy(var_after_dollar, &input[1], (size_t) (input_length - 1));

    if (strcmp(get_variable_value(var_after_dollar), "") == 0) {
        return input;
    } else {
        return get_variable_value(var_after_dollar);
    }
}

//function which clears the given string
void clear_string(char input[], int input_length) {
    if (input_length > 0) {
        for (int i = 0; i < input_length; i++) {
            input[i] = '\0';
        }
    }
}

//function which prints all the standard variables
void print_standard_variables() {
    printf("PATH=%s\n", PATH);
    printf("PROMPT=%s\n", PROMPT);
    printf("CWD=%s\n", CWD);
    printf("USER=%s\n", USER);
    printf("HOME=%s\n", HOME);
    printf("SHELL=%s\n", SHELL);
    printf("TERMINAL=%s\n", TERMINAL);
    printf("EXITCODE=%d\n", EXITCODE);
}

//function which prints all the user created variables
void print_user_variables() {
    if (VAR_COUNT != 0) {
        for (int i = 0; i < VAR_COUNT; i++) {
            printf("%s=%s \n", USER_VAR_NAMES[i], USER_VAR_VALUES[i]);
        }
    }
}

//functions which gets an input string, tokenises it and fills the global ARGS array
//the function returns the number of input arguments
int tokenise_input(char input[]) {
    char *token;
    int token_index = 0;

    //get first token
    token = strtok(input, " ");

    //tokenise the rest of the string
    while (token != NULL && token_index < MAX_LENGTH) {
        ARGS[token_index] = token;
        token_index++;
        token = strtok(NULL, " ");
    }

    return token_index;
}

//function which clears all the input arguments and sets the pointers to null
void clear_and_null_args() {
    if (INPUT_ARGS_COUNT > 0) {
        for (int i = 0; i < INPUT_ARGS_COUNT; i++) {
            clear_string(ARGS[i], (int) strlen(ARGS[i]));
        }

        for (int i = 0; i < MAX_LENGTH; i++) {
            ARGS[i] = NULL;
        }
    }
}

//function which checks whether the first argument in the input is an internal command
//returns -1 if the command is not found, returns a value greater or equal to 0 if it is found
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

//function which executes an internal command depending on the first argument
//the function also take care of output redirection
//returns 1 is 'exit' is entered, returns 0 otherwise
int execute_internal_command(const char command[], int redirect) {
    int exit_terminal = 0;

    //check which internal command is called
    if (strcasecmp(command, "exit") == 0) {
        exit_terminal = 1;
    } else if (strcasecmp(command, "print") == 0) {
        if (INPUT_ARGS_COUNT == 1) {
            printf("Invalid input!");
            return 0;
        }

        //if the '>' redirection argument is used, redirect the output to the file
        if (redirect == 1) {
            //get the input file name and remove the last two input arguments
            char filename[MAX_LENGTH];
            strncpy(filename, ARGS[INPUT_ARGS_COUNT - 1], strlen(ARGS[INPUT_ARGS_COUNT - 1]));

            clear_string(ARGS[INPUT_ARGS_COUNT - 1], (int) strlen(ARGS[INPUT_ARGS_COUNT - 1]));
            clear_string(ARGS[INPUT_ARGS_COUNT - 2], (int) strlen(ARGS[INPUT_ARGS_COUNT - 2]));
            ARGS[INPUT_ARGS_COUNT - 1] = NULL;
            ARGS[INPUT_ARGS_COUNT - 2] = NULL;

            INPUT_ARGS_COUNT = INPUT_ARGS_COUNT - 2;

            //fork the main branch to be able to redirect the output smoothly
            pid_t pid = fork();

            if (pid < 0) {
                perror("Unable to fork");
                exit(EXIT_FAILURE);
            } else if (pid == 0) {
                //open the file in read-write-truncate mode and create it if it is not found
                int fd = open(filename, O_RDWR | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IXUSR);
                if (fd > 0) {
                    //redirect STDOUT to the file
                    dup2(fd, 1);
                    //execute the print command
                    print_command();
                    //close the file
                    close(fd);
                    exit(EXIT_SUCCESS);
                } else {
                    perror("Unable to open file");
                    exit(EXIT_FAILURE);
                }
            }

            wait(NULL);
        } else if (redirect == 2) { //if the '>>' redirection argument is used, redirect the output to the file
            //get the input file name and remove the last two input arguments
            char filename[MAX_LENGTH];
            strncpy(filename, ARGS[INPUT_ARGS_COUNT - 1], strlen(ARGS[INPUT_ARGS_COUNT - 1]));

            clear_string(ARGS[INPUT_ARGS_COUNT - 1], (int) strlen(ARGS[INPUT_ARGS_COUNT - 1]));
            clear_string(ARGS[INPUT_ARGS_COUNT - 2], (int) strlen(ARGS[INPUT_ARGS_COUNT - 2]));
            ARGS[INPUT_ARGS_COUNT - 1] = NULL;
            ARGS[INPUT_ARGS_COUNT - 2] = NULL;

            INPUT_ARGS_COUNT = INPUT_ARGS_COUNT - 2;

            //fork the main branch to be able to redirect the output smoothly
            pid_t pid = fork();

            if (pid < 0) {
                perror("Unable to fork");
                exit(EXIT_FAILURE);
            } else if (pid == 0) {
                //open the file in read-write-append mode
                int fd = open(filename, O_RDWR | O_APPEND, S_IRUSR | S_IWUSR | S_IXUSR);
                if (fd > 0) {
                    //redirect STDOUT to the file
                    dup2(fd, 1);
                    //execute the print command
                    print_command();
                    //close the file
                    close(fd);
                    exit(EXIT_SUCCESS);
                } else {
                    perror("Unable to open file");
                    exit(EXIT_FAILURE);
                }
            }

            wait(NULL);
        } else { //if the output is not redirected, excute print normally
            print_command();
        }
    } else if (strcasecmp(command, "chdir") == 0) {
        //check the input is valid
        if (INPUT_ARGS_COUNT == 1) {
            printf("Invalid input!");
            return 0;
        }
        //execute the chdir command
        change_directory(ARGS[1]);
    } else if (strcasecmp(command, "all") == 0) {
        //if the '>' redirection argument is used, redirect the output to the file
        if (redirect == 1) {
            char filename[MAX_LENGTH];
            strncpy(filename, ARGS[INPUT_ARGS_COUNT - 1], strlen(ARGS[INPUT_ARGS_COUNT - 1]));

            //fork the main branch to be able to redirect the output smoothly
            pid_t pid = fork();

            if (pid < 0) {
                perror("Unable to fork");
                exit(EXIT_FAILURE);
            } else if (pid == 0) {
                //open the file in read-write-truncate mode and create it if it is not found
                int fd = open(filename, O_RDWR | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IXUSR);
                if (fd > 0) {
                    //redirect STDOUT to the file
                    dup2(fd, 1);
                    //print all the standard shell variables
                    print_standard_variables();
                    //print al lthe user created variables
                    print_user_variables();
                    //close the file
                    close(fd);
                    exit(EXIT_SUCCESS);
                } else {
                    perror("Unable to open file");
                    exit(EXIT_FAILURE);
                }
            }

            wait(NULL);
        } else if (redirect == 2) { //if the '>>' redirection argument is used, redirect the output to the file
            char filename[MAX_LENGTH];
            strncpy(filename, ARGS[INPUT_ARGS_COUNT - 1], strlen(ARGS[INPUT_ARGS_COUNT - 1]));

            //fork the main branch to be able to redirect the output smoothly
            pid_t pid = fork();

            if (pid < 0) {
                perror("Unable to fork");
                exit(EXIT_FAILURE);
            } else if (pid == 0) {
                //open the file in read-write-append mode
                int fd = open(filename, O_RDWR | O_APPEND, S_IRUSR | S_IWUSR | S_IXUSR);
                if (fd > 0) {
                    //redirect STDOUT to the file
                    dup2(fd, 1);
                    //print all the standard shell variables
                    print_standard_variables();
                    //print al lthe user created variables
                    print_user_variables();
                    //close the file
                    close(fd);
                    exit(EXIT_SUCCESS);
                } else {
                    perror("Unable to open file");
                    exit(EXIT_FAILURE);
                }
            }

            wait(NULL);
        } else { //if the output is not redirected, output the variables normally to the terminal
            print_standard_variables();
            print_user_variables();
        }
    } else if (strcasecmp(command, "source") == 0) {
        //if the '>' redirection argument is used, redirect the output to the file
        if (redirect == 1) {
            char filename[MAX_LENGTH];
            strncpy(filename, ARGS[INPUT_ARGS_COUNT - 1], strlen(ARGS[INPUT_ARGS_COUNT - 1]));

            //fork the main branch to be able to redirect the output smoothly
            pid_t pid = fork();

            if (pid < 0) {
                perror("Unable to fork");
                exit(EXIT_FAILURE);
            } else if (pid == 0) {
                //open the file in read-write-truncate mode and create it if it is not found
                int fd = open(filename, O_RDWR | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IXUSR);
                if (fd > 0) {
                    //redirect STDOUT to the file
                    dup2(fd, 1);
                    //execute the source command
                    get_input_from_file(ARGS[1]);
                    //close the file
                    close(fd);
                    exit(EXIT_SUCCESS);
                } else {
                    perror("Unable to open file");
                    exit(EXIT_FAILURE);
                }
            }

            wait(NULL);
        } else if (redirect == 2) { //if the '>>' redirection argument is used, redirect the output to the file
            char filename[MAX_LENGTH];
            strncpy(filename, ARGS[INPUT_ARGS_COUNT - 1], strlen(ARGS[INPUT_ARGS_COUNT - 1]));

            //fork the main branch to be able to redirect the output smoothly
            pid_t pid = fork();

            if (pid < 0) {
                perror("Unable to fork");
                exit(EXIT_FAILURE);
            } else if (pid == 0) {
                //open the file in read-write-append
                int fd = open(filename, O_RDWR | O_APPEND, S_IRUSR | S_IWUSR | S_IXUSR);
                if (fd > 0) {
                    //redirect STDOUT to the file
                    dup2(fd, 1);
                    //execute the source command
                    get_input_from_file(ARGS[1]);
                    //close the file
                    close(fd);
                    exit(EXIT_SUCCESS);
                } else {
                    perror("Unable to open file");
                    exit(EXIT_FAILURE);
                }
            }

            wait(NULL);
        } else { //if the output is not redirected, output to the terminal
            get_input_from_file(ARGS[1]);
        }
    }

    return exit_terminal;
}

//function which prints the input, similar to echo
void print_command() {
    int quotes_num = 0;
    int print_literal = 0;

    //check for only one word with two ""
    if (ARGS[1][0] == '"' && ARGS[1][strlen(ARGS[1]) - 1] == '"' && strlen(ARGS[1]) > 1) {
        printf("%.*s\n", (int) (strlen(ARGS[1]) - 2), &ARGS[1][1]);
        return;
    }

    for (int i = 1; i < INPUT_ARGS_COUNT; i++) {
        //check for a single " in the start of each ARG
        if (ARGS[i][0] == '"') {
            quotes_num++;
        }

        //check for a single " in the end of each ARG
        if (strlen(ARGS[i]) > 1) {
            if (ARGS[i][strlen(ARGS[i]) - 1] == '"') {
                quotes_num++;
            }
        }
    }

    //check if the number of quotes in the input is less then 2
    if (quotes_num < 2) {
        for (int i = 1; i < INPUT_ARGS_COUNT; i++) {
            //checking for only "
            if (strcasecmp(ARGS[i], "\"") != 0) {
                //check for " at beginning of word
                if (ARGS[i][0] == '"') {
                    //check for $ after "
                    if (ARGS[i][1] == '$') {
                        //check for a punctuation mark exactly at the of '$VAR'
                        if (ARGS[i][(int) strlen(ARGS[i]) - 1] == ',' ||
                            ARGS[i][(int) strlen(ARGS[i]) - 1] == '.' ||
                            ARGS[i][(int) strlen(ARGS[i]) - 1] == '!' ||
                            ARGS[i][(int) strlen(ARGS[i]) - 1] == '?') {
                            //if the variable is found print the value of the variable with the punctuation mark at the end
                            if (strcasecmp(get_value_after_dollar(&ARGS[i][1], (int) strlen(ARGS[i]) - 2), "") != 0) {
                                printf("%s%c ", get_value_after_dollar(&ARGS[i][1], (int) strlen(ARGS[i]) - 2),
                                       ARGS[i][(int) strlen(ARGS[i]) - 1]);
                            } else { //if the variable is not found, print the word without "
                                printf("%s ", &ARGS[i][1]);
                            }
                        } else { //if there is no punctuation mark after the '$VAR' print the value of the variable normally without "
                            if (strcasecmp(get_value_after_dollar(&ARGS[i][1], (int) strlen(ARGS[i]) - 1), "") != 0) {
                                printf("%s ", get_value_after_dollar(&ARGS[i][1], (int) strlen(ARGS[i]) - 1));
                            } else { //if the variable is not found, print the word without "
                                printf("%s ", &ARGS[i][1]);
                            }
                        }
                    } else { //if there is $, print the word without the "
                        printf("%s ", &ARGS[i][1]);
                    }
                } else if (ARGS[i][(int) strlen(ARGS[i]) - 1] == '"') { //check for " at end of word
                    //check for $ after "
                    if (ARGS[i][0] == '$') {
                        //check for a punctuation mark exactly at the of '$VAR'
                        if (ARGS[i][(int) strlen(ARGS[i]) - 2] == ',' ||
                            ARGS[i][(int) strlen(ARGS[i]) - 2] == '.' ||
                            ARGS[i][(int) strlen(ARGS[i]) - 2] == '!' ||
                            ARGS[i][(int) strlen(ARGS[i]) - 2] == '?') {
                            char temp_arg[MAX_LENGTH] = {0};
                            strncpy(temp_arg, ARGS[i], strlen(ARGS[i]) - 2);
                            //if the variable is found print the value of the variable with the punctuation mark at the end
                            if (strcasecmp(get_value_after_dollar(temp_arg, (int) strlen(ARGS[i]) - 2), "") != 0) {
                                printf("%s%c ", get_value_after_dollar(temp_arg, (int) strlen(ARGS[i]) - 2),
                                       ARGS[i][(int) strlen(ARGS[i]) - 2]);
                            } else { //if the variable is not found, print the word without "
                                printf("%.*s ", (int) (strlen(ARGS[i]) - 1), ARGS[i]);
                            }
                        } else { //if there is no punctuation mark after the '$VAR' print the value of the variable normally
                            char temp_arg[MAX_LENGTH] = {0};
                            strncpy(temp_arg, ARGS[i], strlen(ARGS[i]) - 1);
                            //if the variable is found print the value of the variable without " at the end
                            if (strcasecmp(get_value_after_dollar(temp_arg, (int) strlen(ARGS[i]) - 1), "") != 0) {
                                printf("%s ", get_value_after_dollar(temp_arg, (int) strlen(ARGS[i]) - 1));
                            } else { //if the variable is not found print the word without " at the end
                                printf("%.*s ", (int) (strlen(ARGS[i]) - 1), ARGS[i]);
                            }
                        }
                    } else { //if the word does not have $, print the word without " at the end
                        printf("%.*s ", (int) (strlen(ARGS[i]) - 1), ARGS[i]);
                    }
                } else { //if there are no " in the word, just check for $ and punctuation, otherwise print the word normally
                    if (ARGS[i][0] == '$') {
                        if (ARGS[i][(int) strlen(ARGS[i]) - 1] == ',' ||
                            ARGS[i][(int) strlen(ARGS[i]) - 1] == '.' ||
                            ARGS[i][(int) strlen(ARGS[i]) - 1] == '!' ||
                            ARGS[i][(int) strlen(ARGS[i]) - 1] == '?') {
                            if (strcasecmp(get_value_after_dollar(ARGS[i], (int) strlen(ARGS[i]) - 1), "") != 0) {
                                printf("%s%c ", get_value_after_dollar(ARGS[i], (int) strlen(ARGS[i]) - 1),
                                       ARGS[i][(int) strlen(ARGS[i]) - 1]);
                            } else {
                                printf("%s ", ARGS[i]);
                            }
                        } else {
                            if (strcasecmp(get_value_after_dollar(ARGS[i], (int) strlen(ARGS[i])), "") != 0) {
                                printf("%s ", get_value_after_dollar(ARGS[i], (int) strlen(ARGS[i])));
                            } else {
                                printf("%s ", ARGS[i]);
                            }
                        }
                    } else {
                        printf("%s ", ARGS[i]);
                    }
                }
            }
        }
        printf("\n");
    } else { //if the number of " in the input is greater or equal to 2
        for (int i = 1; i < INPUT_ARGS_COUNT; i++) {
            if (print_literal == 0) { //this checks whether a " has already benn found
                //checking for special case where a word contains a " both at the beginning and end
                if (ARGS[i][0] == '"' && ARGS[i][strlen(ARGS[i]) - 1] == '"' && strlen(ARGS[i]) > 1) {
                    printf("%.*s ", (int) (strlen(ARGS[i]) - 2), &ARGS[i][1]);
                } else {
                    //assuming first quote is the beginning of a string
                    if (ARGS[i][0] == '"') {
                        if (strlen(ARGS[i]) > 1) {
                            printf("%s ", &ARGS[i][1]);
                            print_literal = 1;
                        } else { //this else is to ensure that " on their own also cout
                            print_literal = 1;
                        }
                    } else {
                        //check for $ at the beginning of a string
                        if (ARGS[i][0] == '$') {
                            //check for punctuation marks at the end of a string
                            if (ARGS[i][(int) strlen(ARGS[i]) - 1] == ',' ||
                                ARGS[i][(int) strlen(ARGS[i]) - 1] == '.' ||
                                ARGS[i][(int) strlen(ARGS[i]) - 1] == '!' ||
                                ARGS[i][(int) strlen(ARGS[i]) - 1] == '?') {
                                //check for a variable, if it exists, print the value with the punctuation mark at the end
                                if (strcasecmp(get_value_after_dollar(ARGS[i], (int) strlen(ARGS[i]) - 1), "") != 0) {
                                    printf("%s%c ", get_value_after_dollar(ARGS[i], (int) strlen(ARGS[i]) - 1),
                                           ARGS[i][(int) strlen(ARGS[i]) - 1]);
                                } else { //if the variable does not exist, print the word normally
                                    printf("%s ", ARGS[i]);
                                }
                            } else { //if there is no punctuation mark print the value if the variable is found
                                if (strcasecmp(get_value_after_dollar(ARGS[i], (int) strlen(ARGS[i])), "") != 0) {
                                    printf("%s ", get_value_after_dollar(ARGS[i], (int) strlen(ARGS[i])));
                                } else { //if the variable does not exist, print the word normally
                                    printf("%s ", ARGS[i]);
                                }
                            }
                        } else { //if the string does not have $ at the beginning, print the word normally
                            printf("%s ", ARGS[i]);
                        }
                    }
                }
            } else { //if a " has already been found
                //assuming second quote is in the end of a string
                int second_quote = check_for_char_in_string(ARGS[i], (int) strlen(ARGS[i]), '"');
                if (second_quote != -1) {
                    if (strlen(ARGS[i]) >
                        1) { //if a second " has been found change the value to start printing variable values
                        printf("%.*s ", second_quote, ARGS[i]);
                        print_literal = 0;
                    } else {
                        print_literal = 0;
                    }
                } else { //if no second quote is found, print words as they are
                    printf("%s ", ARGS[i]);
                }
            }

        }
        printf("\n");
    }
}

//function which checks the CWD and the given path and changes it, if it is valid
void change_directory(char path[]) {
    int last_slash_pos = 0;
    char cwd_before_change[MAX_LENGTH];

    //keep a copy of the CWD, in case the new one is invalid
    strncpy(cwd_before_change, CWD, strlen(CWD));

    //check is .. was entered
    if (strcasecmp(path, "..") != 0) {
        //check if the path is valid
        if (chdir(path) != 0) {
            perror("Cannot change directory");
        } else { //if the path is valid, change the variable CWD
            clear_string(CWD, MAX_LENGTH);
            if (getcwd(CWD, sizeof(CWD)) == NULL) {
                perror("Cannot set current working directory");
            }
        }
    } else { //if .. was entered
        //find the last / in the path
        for (int i = (int) strlen(CWD) - 1; i >= 0; i--) {
            if (CWD[i] == '/') {
                last_slash_pos = i;
                break;
            }
        }

        //clear from the position of the last / to the end
        for (int i = last_slash_pos; i < (int) strlen(CWD); i++) {
            CWD[i] = '\0';
        }

        //check if the new path is valid, if it is not valid, revert to the old value
        if (chdir(CWD) != 0) {
            perror("Cannot change directory");
            strncpy(CWD, cwd_before_change, strlen(cwd_before_change));
        } else { //if the path is valid, change the variable CWD
            clear_string(CWD, MAX_LENGTH);
            if (getcwd(CWD, sizeof(CWD)) == NULL) {
                perror("Cannot set current working directory");
            }
        }
    }
}

//function to run a simple fork-plus-exec to execute external commands
void execute_external_command(const char command[], int redirect) {
    //fork the main branch
    pid_t pid = fork();

    //check if the fork was valid
    if (pid < 0) {
        perror("Unable to fork");
        exit(EXIT_FAILURE);
        EXITCODE = 1;
    } else if (pid == 0) { //if the fork is valid, check if it is in the child
        if (redirect == 1) { //check if the output has been redirected using '>'
            char filename[MAX_LENGTH];
            strncpy(filename, ARGS[INPUT_ARGS_COUNT - 1], strlen(ARGS[INPUT_ARGS_COUNT - 1]));

            clear_string(ARGS[INPUT_ARGS_COUNT - 1], (int) strlen(ARGS[INPUT_ARGS_COUNT - 1]));
            clear_string(ARGS[INPUT_ARGS_COUNT - 2], (int) strlen(ARGS[INPUT_ARGS_COUNT - 2]));
            ARGS[INPUT_ARGS_COUNT - 1] = NULL;
            ARGS[INPUT_ARGS_COUNT - 2] = NULL;

            INPUT_ARGS_COUNT = INPUT_ARGS_COUNT - 2;

            //open the file in read-write-truncate mode and create a new file if it is not found
            int fd = open(filename, O_RDWR | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IXUSR);
            if (fd > 0) {
                //redirect STDOUT to file
                dup2(fd, 1);
                //execute external command through execvp which uses the variable PATH
                if (execvp(command, ARGS)) {
                    perror("Exec failed");
                    exit(EXIT_FAILURE);
                    EXITCODE = 1;
                }
                //close the file
                close(fd);
                exit(EXIT_SUCCESS);
                EXITCODE = 0;
            } else {
                perror("Unable to open file");
                exit(EXIT_FAILURE);
                EXITCODE = 1;
            }
        } else if (redirect == 2) { //check if the output has been redirected using '>>'
            char filename[MAX_LENGTH];
            strncpy(filename, ARGS[INPUT_ARGS_COUNT - 1], strlen(ARGS[INPUT_ARGS_COUNT - 1]));

            clear_string(ARGS[INPUT_ARGS_COUNT - 1], (int) strlen(ARGS[INPUT_ARGS_COUNT - 1]));
            clear_string(ARGS[INPUT_ARGS_COUNT - 2], (int) strlen(ARGS[INPUT_ARGS_COUNT - 2]));
            ARGS[INPUT_ARGS_COUNT - 1] = NULL;
            ARGS[INPUT_ARGS_COUNT - 2] = NULL;

            INPUT_ARGS_COUNT = INPUT_ARGS_COUNT - 2;

            //open the file in read-write-append mode
            int fd = open(filename, O_RDWR | O_APPEND, S_IRUSR | S_IWUSR | S_IXUSR);
            if (fd > 0) {
                //redirect STDOUT to file
                dup2(fd, 1);
                //execute external command
                if (execvp(command, ARGS)) {
                    perror("Exec failed");
                    exit(EXIT_FAILURE);
                    EXITCODE = 1;
                }
                //close the file
                close(fd);
                exit(EXIT_SUCCESS);
                EXITCODE = 0;
            } else {
                perror("Unable to open file");
                exit(EXIT_FAILURE);
                EXITCODE = 1;
            }
        } else { //if the output has not been redirected, execute normally
            if (execvp(command, ARGS)) {
                perror("Exec failed");
                exit(EXIT_FAILURE);
                EXITCODE = 1;
            }
        }
    }

    wait(NULL);
}