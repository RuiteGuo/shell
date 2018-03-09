#include <stdio.h>
#include <unistd.h>
#include <memory.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define ERROR 0;
#define PARENT 2;

int detect_valid_sign(int argc, char **argv);

void clear_buffer(int *pid_buffer);

void get_line(char *buffer, size_t bufferSize) {
    buffer[bufferSize - 1] = '\0';
    if ((fgets(buffer, 4096, stdin)) == 0) {
        perror("Cannot read from stdin");
        exit(1);
    }
    return;
}

int count_num(int argc, char *buffer) {
    int i;
    for (i = 0; buffer[i] != '\0'; i++) {
        if (buffer[i] == ' ') {
            argc++;
        }
    }
    return argc;
}

int set_argv(int argc, char **argv, char *buffer) {
    char *split = " \t";
    char *line;
    line = strtok(buffer, "\n");
    if (line == NULL) {
        argv[0] = NULL;
        return 0;
    } else {
        line = strtok(line, split);
        int arg_num = 0;
        while (line != NULL) {
            argv[arg_num] = line;
            line = strtok(NULL, split);
            arg_num++;
        }
        argv[argc] = NULL;
        for (int i = 0; i < argc; i++) {
            if (argv[i] == NULL) {
                argc = i;
                break;
            }
        }
    }
    return argc;
}

void print_error() {
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
}

int detect_exit_cd_pwd_newline(char **argv, int *pid_buffer) {
    int flag = 0;
    const char *new_line_str = "";
    const char *cdstr = "cd";
    const char *exitstr = "exit";
    const char *pwdstr = "pwd";
    if (strcmp(argv[0], new_line_str) == 0) {
        flag = 1;
    }
    if (strcmp(argv[0], cdstr) == 0) {
        if (argv[1] == NULL) {
            chdir(getenv("HOME"));
        } else {
            DIR *dir = opendir(argv[1]);
            if (dir) {
                /* Directory exists. */
                chdir(argv[1]);
                closedir(dir);
            } else if (ENOENT == errno) {
                /* Directory does not exist. */
                print_error();
            } else {
                /* opendir() failed for some other reason. */
                perror("Cannot open the directory");
            }

        }
        flag = 1;
    }
    if (strcmp(argv[0], exitstr) == 0) {
        for (int i = 0; i < 22; i++) {
            if (pid_buffer[i] != 0) {
                pid_t my_pid = pid_buffer[i];
                kill(my_pid, SIGKILL);
            }
        }
        free(pid_buffer);
        exit(0);
    }
    if (strcmp(argv[0], pwdstr) == 0) {
        if (argv[1] != NULL) {
            print_error();
            flag = 1;
        } else {
            char cwd[1024];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                fprintf(stdout, "%s\n", cwd);
                fflush(stdout);
                flag = 1;
            } else {
                printf("getcwd() error");
            }
        }
    }

    return flag;
}

void print_empty() {
//int empty_argc = 0;
    char *empty_argv[128];
    empty_argv[0] = NULL;
    empty_argv[1] = NULL;
    execvp(empty_argv[0], empty_argv);
    print_error();
}

void execute_larger(char **argv, int sign_pos) {
    struct stat path_stat;
    stat(argv[sign_pos + 1], &path_stat);
    int status = S_ISDIR(path_stat.st_mode);

    argv[sign_pos] = NULL;
    int file = open(argv[sign_pos + 1], O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);

    if (status != 0) {
        print_error();
        print_empty();
    }
    //stdout output
    dup2(file, STDOUT_FILENO);
    close(file);
}

void execute_less(char **argv, int sign_pos) {
    argv[sign_pos] = NULL;
    int file = open(argv[sign_pos + 1], O_RDONLY);
    if (file == -1) {
        print_error();
        print_empty();
    }
    //put to stdin
    dup2(file, STDIN_FILENO);
    close(file);
}

int detect_sign(int argc, char **argv) {
    int has_larger_sign = 0;
    int has_less_sign = 0;
    int pos;
    for (pos = 0; pos < argc; pos++) {
        if (strcmp(argv[pos], ">") == 0) {
            has_larger_sign = 1;
        }
        if (strcmp(argv[pos], "<") == 0) {
            has_less_sign = 1;
        }
    }
    //set up the flag for the sign.
    if (has_larger_sign == 1 && has_less_sign == 1) { //has both signs
        return 3;//has <> at the same time
    }
    if (has_larger_sign == 1) {
        return 1; // has >
    } else if (has_less_sign == 1) {
        return 2; // has <
    }

    return 0;// no sign involved
}

int find_larger_pos(int argc, char **argv) {//actual pos or -1 if there is no
    int pos;
    for (pos = 0; pos < argc; pos++) {
        if (strcmp(argv[pos], ">") == 0) {
            return pos;
        }
    }
    return -1;
}

int find_less_pos(int argc, char **argv) {//actual pos or -1 if there is no
    int pos;
    for (pos = 0; pos < argc; pos++) {
        if (strcmp(argv[pos], "<") == 0) {
            return pos;
        }
    }
    return -1;
}

void execute_both_signs(char **argv, int larger_pos, int less_pos) {
    execute_larger(argv, larger_pos);
    execute_less(argv, less_pos);
}

int execute_sign(int sign, int argc, char **argv) { // 0 is normal, -1 means too many files involved
    if (sign == 1) { //>
        int larger_sign_pos = find_larger_pos(argc, argv);

        if (argv[larger_sign_pos + 2] != NULL && strcmp(argv[larger_sign_pos + 2], "<") != 0) {
            return -1;
        }
        execute_larger(argv, larger_sign_pos);
    } else if (sign == 2) {//<
        int less_sign_pos = find_less_pos(argc, argv);
        if (argv[less_sign_pos + 2] != NULL && strcmp(argv[less_sign_pos + 2], ">") != 0) {
            return -1;
        }
        execute_less(argv, less_sign_pos);
    } else {//<> exist at the same time
        int larger_sign_pos = find_larger_pos(argc, argv);
        int less_sign_pos = find_less_pos(argc, argv);
        if (argv[less_sign_pos + 2] != NULL && strcmp(argv[less_sign_pos + 2], ">") != 0) {
            return -1;
        }
        if (argv[larger_sign_pos + 2] != NULL && strcmp(argv[larger_sign_pos + 2], "<") != 0) {
            return -1;
        }
        execute_both_signs(argv, larger_sign_pos, less_sign_pos);
    }
    return 0; // normal, just one file name
}

void execute_instruction(int argc, char **argv) {
    //execvp operation
    int has_sign = detect_sign(argc, argv);// 1 is >, 2 is <, 3 is <>, 0 means there is no sign involved
    //if has sign
    if (has_sign == 1 || has_sign == 2 || has_sign == 3) {
        int valid_sign = detect_valid_sign(argc, argv); // 1 is normal, 0 means lack of filename(invalid)
        //if has sign and valid
        if (valid_sign == 1) {
            int multiple_arguments = execute_sign(has_sign, argc,
                                                  argv);//0 is normal, -1 is error meaning too many arguements
            if (multiple_arguments == -1) {
                //too many arguments
                print_error();
                print_empty();
            }
        } else {
            //not enough arguments
            print_error();
            print_empty();
        }
    }
    //actual execution
    execvp(argv[0], argv);
    print_error();
    print_empty();
}

int detect_pipeline(int argc, char **argv) {// 0 means there is no pipeline, 1 means there is a pipeline
    const char *separator = "|";
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], separator) == 0) {
            return 1;
        }
    }
    return 0;
}

int find_pipeline_pos(int argc, char **argv) { // 0 means there is no pipeline, i is the pipeline_pos
    const char *separator = "|";
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], separator) == 0) {
            return i;
        }
    }
    return 0;
}

int separate_argv_first_half(int pos, char **argv, char **to_return) {
    for (int i = 0; i < pos; i++) {
        to_return[i] = argv[i];
    }
    to_return[pos] = NULL;
    return pos;
}

int separate_argv_second_half(int pos, int argc, char **argv, char **to_return) {
    for (int i = 0; i < argc - pos - 1; i++) {
        to_return[i] = argv[pos + i + 1];
    }
    to_return[argc - 1 - pos + 1] = NULL;
    return argc - 1 - pos;
}

void free_argv(char *argv[128]) {
    for (int i = 0; argv[i] != NULL; i++) {
        argv[i] = NULL;
    }
}

void add_pid(int pid, int *pid_buffer) {
    for (int i = 0; i < 30; i++) {
        if (pid_buffer[i] == 0 && pid != 0) {
            pid_buffer[i] = pid;
            break;
        }
    }
}

int execute_double_fork(char *instruction_1[128], int instruction_1_count, char *instruction_2[128],
                        int instruction_2_count) {
    //clear_pid_buffer(pid_buffer);
    fflush(stdout);
    //save the stdin and stdout
    int save_in, save_out;
    save_in = dup(STDIN_FILENO);
    save_out = dup(STDOUT_FILENO);
    int pipefd1[2];
    pipe(pipefd1);
    //the first fork
    int pid = fork();
    if (pid == 0) {
        //child
        //connect the write end of the pipe to stdout
        close(pipefd1[0]);
        dup2(pipefd1[1], STDOUT_FILENO);
        close(pipefd1[1]);
        execute_instruction(instruction_1_count, instruction_1);
    } else if (pid > 0) {
        //connect the read end of the pipe to stdin
        close(pipefd1[1]);
        dup2(pipefd1[0], STDIN_FILENO);
        close(pipefd1[0]);
    } else {
        perror("Error\n");
        return ERROR;
    }
    close(pipefd1[0]);
    close(pipefd1[1]);
    //the second fork
    int pid_second = fork();
    if (pid_second == 0) {
        //child
        //since stdin already has content, we can just call execute function call here.
        execute_instruction(instruction_2_count, instruction_2);
    } else if (pid_second > 0) {
        close(STDIN_FILENO);
        waitpid(0, NULL, 0);
        wait(NULL);
        fflush(stdout);
    } else {
        perror("Error\n");
        return ERROR;
    }
    //restore the stdin and stdout
    dup2(save_in, STDIN_FILENO);
    dup2(save_out, STDOUT_FILENO);
    fflush(stdout);
    return PARENT;
}

int execute_single_fork(int argc, char **argv) {
    //clear_pid_buffer(pid_buffer);
    int pid = fork();
    if (pid == 0) {
        //child
        execute_instruction(argc, argv);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
        fflush(stdout);
        return PARENT;
    } else {
        perror("Error\n");
        return ERROR;
    }
    return PARENT;
}

int detect_background(int argc, char **argv) {// return 1 means the instruction has background execution command
    // 0 means the instruction does not have back ground execution
    int final_pos = argc - 1;
    if (strcmp(argv[final_pos], "&") == 0) {
        return 1;
    }
    return 0;
}

int execute_background_single_fork(int argc, char **argv, int *pid_buffer) {
    int pid = fork();
    add_pid(pid, pid_buffer);
    if (pid == 0) {
        //child
        execute_instruction(argc, argv);
    } else if (pid > 0) {
        //without waiting for the child process to finish.
        return PARENT;
    } else {
        perror("Error\n");
        return ERROR;
    }
    return PARENT;
}

int create_child(int argc, char **argv, int *pid_buffer) {
    char *instruction_1[128];
    int instruction_1_count = 0;
    char *instruction_2[128];
    int instruction_2_count = 0;
    free_argv(instruction_1);
    free_argv(instruction_2);

    int background_flag = 0;
    background_flag = detect_background(argc, argv);
    if (background_flag == 1) {//if has background execution, then set up the flag and then remove the final "&"
        int final_pos = argc - 1;
        argv[final_pos] = NULL;
        argc--;
    }
    int double_fork = 0;
    int already_dealt_with = detect_exit_cd_pwd_newline(argv, pid_buffer);
    if (already_dealt_with == 0) {
        int has_pipeline = detect_pipeline(argc, argv);
        if (has_pipeline == 1) {//has pipeline
            //normal(has pipeline, valid pipeline)
            int pipeline_pos = find_pipeline_pos(argc, argv);
            instruction_1_count = separate_argv_first_half(pipeline_pos, argv, instruction_1);
            instruction_2_count = separate_argv_second_half(pipeline_pos, argc, argv, instruction_2);
            double_fork = 1;

        }
        // forking process.
        clear_buffer(pid_buffer);
        if (double_fork == 1) {
            return execute_double_fork(instruction_1, instruction_1_count, instruction_2, instruction_2_count);
        } else {
            //single fork
            if (background_flag == 1) {//there is background execution flag
                return execute_background_single_fork(argc, argv, pid_buffer);
            } else {//normal operation without background execution
                return execute_single_fork(argc, argv);
            }
        }
    }
    return PARENT;
}

void clear_buffer(int *pid_buffer) {
    for (int i = 0; i < 22; i++) {
        if (pid_buffer[i] != 0) {
            while (waitpid(pid_buffer[i], NULL, WNOHANG) == -1) {
                pid_buffer[i] = 0;
                break;
            }
        }
    }

}

int detect_valid_sign(int argc, char **argv) { //already know that one of the signs exists.
    int larger_pos = find_larger_pos(argc, argv);
    int less_pos = find_less_pos(argc, argv);
    if (larger_pos != -1) { //">" actually exists
        if (argv[larger_pos + 1] == NULL) {
            return 0;
        }
    }
    if (less_pos != -1) { //"<" actually exists
        if (argv[less_pos + 1] == NULL) {
            return 0;
        }
    }
    //return true if there is ONE valid file name after the signs
    return 1;
}

void truncate_pid_buffer(int *pid_buffer) {
    for (int i = 0; i < 22; i++) {
        pid_buffer[i] = 0;
    }
}

int main(int argc, char *argv[]) {
    int lines_num = 1;
    int *pid_buffer = malloc(22 * sizeof(pid_t));
    truncate_pid_buffer(pid_buffer);
    if (argc > 1) {
        print_error();
        exit(1);
    }
    while (1) {
        char inputBuffer[4096];
        char *my_argv[128];
        free_argv(my_argv);
        int my_argc = 1;
        //print the console
        char *output = malloc(30 * sizeof(char));
        sprintf(output, "mysh (%d)> ", lines_num);
        fputs(output, stdout);
        free(output);
        //read from user input
        fflush(stdout);
        get_line(inputBuffer, 129);
        int too_long = 0;
        if (strlen(inputBuffer) > 129) {
            print_error();
            lines_num++;
            too_long = 1;
        }
        if (too_long == 0) {
            my_argc = count_num(my_argc, inputBuffer);
            //refactor the my_argc
            my_argc = set_argv(my_argc, my_argv, inputBuffer);

            const char *separator = "|";
            int split_valid_flag = 1;
            for (int i = 0; i < my_argc; i++) {
                if (strcmp(my_argv[i], separator) == 0 && my_argv[i + 1] == NULL) {
                    print_error();
                    split_valid_flag = 0;
                    lines_num++;
                }
            }
            if (my_argc != 0 && split_valid_flag == 1) {
                lines_num++;
                int result = 0;
                result = create_child(my_argc, my_argv, pid_buffer);
                fflush(stdout);
                if (result == -1) {
                    exit(-1);
                }
            }
        }
    }
}
