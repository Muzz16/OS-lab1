/*
 * Main source code file for lsh shell program
 *
 * You are free to add functions to this file.
 * If you want to add functions in a separate file(s)
 * you will need to modify the CMakeLists.txt to compile
 * your additional file(s).
 *
 * Add appropriate comments in your code to make it
 * easier for us while grading your assignment.
 *
 * Using assert statements in your code is a great way to catch errors early and make debugging easier.
 * Think of them as mini self-checks that ensure your program behaves as expected.
 * By setting up these guardrails, you're creating a more robust and maintainable solution.
 * So go ahead, sprinkle some asserts in your code; they're your friends in disguise!
 *
 * All the best!
 */
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>

// The <unistd.h> header is your gateway to the OS's process management facilities.
#include <unistd.h>

#include <fcntl.h> // I assume we need this to actually open files

#include "parse.h"

#define PIPE_READ 0
#define PIPE_WRITE 1

#define STDIN_FD 0
#define STDOUT_FD 1
#define STDERR_FD 2

/* FLAGS FOR RUNNING THE PROGRAM */
#define FLAG_NONE 0
#define FLAG_BACKGROUND 1
#define FLAG_IS_PARENT_SHELL 2

static void print_cmd(Command *cmd);
static void print_pgm(Pgm *p);
void stripwhite(char *);
void basic_commands(char **argv);
void run_prgm(Pgm *p, unsigned char flags, char* rstdout, char*rstdin);
void handle_sigint(int sig);
void handle_sigchld(int sig);

pid_t foreground_pgid = -1;

int main(void)
{
  signal(SIGINT, *handle_sigint); // Handle SIGINT (CTRL+C)
  signal(SIGCHLD, *handle_sigchld); // Handle SIGCHLD to get rid of zombie processes

  for (;;)
  {
    char *line;
    line = readline("> ");

    if(line == NULL){
      printf("EOF\n");
      break;
    }

    // Remove leading and trailing whitespace from the line
    stripwhite(line);

    // If stripped line not blank
    if (*line)
    {
      add_history(line);

      Command cmd;
      if (parse(line, &cmd) == 1)
      {
        // Just prints cmd
        print_cmd(&cmd);

        run_prgm(cmd.pgm, (cmd.background ? FLAG_BACKGROUND : FLAG_NONE) | FLAG_IS_PARENT_SHELL, cmd.rstdout, cmd.rstdin);
      }
      else
      {
        printf("Parse ERROR\n");
      }
    }

    // Clear memory
    free(line);
  }
  
  exit(0);

  return 0;
}

/*
 * Run a certain program
 *
 * This function runs in recursion since the order of the programs are in reverse
 */
void run_prgm(Pgm *p, unsigned char flags, char* rstdout, char*rstdin) {
  // Run until p becomes null in reverse order like the print function works
  if(p == NULL) {
    return;
  }
  char** argv = p->pgmlist;
  
  // Call exit on parent
  if(strcmp(argv[0], "exit") == 0) {
    // We should also check in case the argv[1] argument isn't null if they want to exit with a certain exit code
    // Also exit shouldn't exit in case this command is called "sleep 1 | exit", this should only exit the "sleep 1" child process
    if(argv[1] == NULL) {
      exit(0);
    }
    else {
      exit(atoi(argv[1]));
    }
  }
  // Call cd on parent
  else if(strcmp(argv[0], "cd") == 0){
    // Check the argument after cd
    if(argv[1] == NULL){
      // if no argument has been given
      fprintf(stderr, "cd: expected an argument");
    }
    else{
      // otherwise we change directory
      if(chdir(argv[1]) != 0){
        // if chdir return -1 there is an error
        perror("cd failed");
      }
    }
    return;
  }

  int fd[2];
  
  bool is_background = flags & FLAG_BACKGROUND;
  bool is_parent_shell = flags & FLAG_IS_PARENT_SHELL;
  bool connect_pipe = !is_parent_shell; // We want to connect everything except the lsh shell
  if(connect_pipe && pipe(fd) == -1) {
    perror("pipe failed");
  }

  pid_t pid = fork();

  if(pid < 0) { /* ERROR HAPPENED WITH FORK */
    perror("Fork failed!");
  }
  else if(pid == 0) { /* THIS IS THE CHILD PROCESS */
    signal(SIGINT, SIG_DFL); // Reset SIGINT to default behavior in child process

    if(connect_pipe) {
      close(fd[PIPE_READ]);
      if(dup2(fd[PIPE_WRITE], STDOUT_FD) == -1)
        perror("dup2 error in child");
    }
    
    // Check if we have gotten an output redirect
    if(rstdout){ 
      int descriptor = open(rstdout, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR); // create or open new file
      if(descriptor == -1){
        perror("file opening error");
      } else {
        if(dup2(descriptor, STDOUT_FD) == -1){
          perror("dup2 redirection error");
        }
      }
      close(descriptor); // close file
    }
    
    // Check if we have gotten an input redirect and apply it only on the last item which in this case will be the first item written in the program list
    if(p->next == NULL && rstdin){ 
      int descriptor = open(rstdin, O_RDONLY); // create or open new file
      if(descriptor == -1){
        perror("file opening error");
      } else {
        if(dup2(descriptor, STDIN_FD) == -1){
          perror("dup2 redirection error");
        }
      }
      close(descriptor); // close file
    }

    // Run the program before execvp since the list of programs are in reverse order
    run_prgm(p->next, FLAG_NONE, NULL, rstdin);

    // Handles any other command
    if(execvp(argv[0],argv) == -1){
      perror("execvp failed");
      exit(EXIT_FAILURE);
    }
  }
  else { /* THIS IS THE PARENT PROCESS */
    if(is_parent_shell) // Only do this when its the parent shell
      setpgid(pid, pid); // Set the pgid of the child to its own pid
    

    // Wait or do something else if it's supposed to be a background process
    if(connect_pipe) {
      close(fd[PIPE_WRITE]);
      if(dup2(fd[PIPE_READ], STDIN_FD) == -1)
        perror("dup2 error in parent");
    }

    // if not background process
    if(!is_background) {
      foreground_pgid = pid; // This is a foreground process, set the pgid
      int status;
      wait(&status);
      foreground_pgid = -1; // Reset the foreground pgid
    }
  }
}

void handle_sigint(int sig) {
  if(sig == SIGINT) {
    printf("\nCTRL+C entered!\n");
    if(foreground_pgid > 0){
      kill(-foreground_pgid, SIGINT); // Send SIGINT to the entire foreground process group
    } 
  }
}

void handle_sigchld(int sig){
  if(sig == SIGCHLD){
    waitpid(-1, NULL, WNOHANG); //clean up all child processes that have finished
  }
}

/*
 * Print a Command structure as returned by parse on stdout.
 *
 * Helper function, no need to change. Might be useful to study as inspiration.
 */
static void print_cmd(Command *cmd_list)
{
  printf("------------------------------\n");
  printf("Parse OK\n");
  printf("stdin:      %s\n", cmd_list->rstdin ? cmd_list->rstdin : "<none>");
  printf("stdout:     %s\n", cmd_list->rstdout ? cmd_list->rstdout : "<none>");
  printf("background: %s\n", cmd_list->background ? "true" : "false");
  printf("Pgms:\n");
  print_pgm(cmd_list->pgm);
  printf("------------------------------\n");
}

/* Print a (linked) list of Pgm:s.
 *
 * Helper function, no need to change. Might be useful to study as inpsiration.
 */
static void print_pgm(Pgm *p)
{
  if (p == NULL)
  {
    return;
  }
  else
  {
    char **pl = p->pgmlist;

    /* The list is in reversed order so print
     * it reversed to get right
     */
    print_pgm(p->next);
    printf("            * [ ");
    while (*pl)
    {
      printf("%s ", *pl++);
    }
    printf("]\n");
  }
}


/* Strip whitespace from the start and end of a string.
 *
 * Helper function, no need to change.
 */
void stripwhite(char *string)
{
  size_t i = 0;

  while (isspace(string[i]))
  {
    i++;
  }

  if (i)
  {
    memmove(string, string + i, strlen(string + i) + 1);
  }

  i = strlen(string) - 1;
  while (i > 0 && isspace(string[i]))
  {
    i--;
  }

  string[++i] = '\0';
}
