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

// The <unistd.h> header is your gateway to the OS's process management facilities.
#include <unistd.h>

#include "parse.h"

#define PIPE_READ 0
#define PIPE_WRITE 1

#define STDIN_FD 0
#define STDOUT_FD 1
#define STDERR_FD 2

static void print_cmd(Command *cmd);
static void print_pgm(Pgm *p);
void stripwhite(char *);
void basic_commands(char **argv);
void run_prgm(Pgm *p, bool connect_pipe);
void handle_sigint(int sig);

int main(void)
{
  // Setup functions for signals
  signal(SIGINT, *handle_sigint);

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
        
        pid_t pid = fork();
        if(pid < 0) {
          perror("fork failed");
        }
        else if(pid == 0) {
          run_prgm(cmd.pgm, false);
        }
        else {
          // Check if this is supposed to be a background process or not
          
          int status;
          wait(&status);
        }
      }
      else
      {
        printf("Parse ERROR\n");
      }
    }

    // Clear memory
    free(line);
  }
  
  // Maybe wait for every child process here in case there are ones
  // Or send a signal to terminate all of them

  return 0;
}

/*
 * Run a certain program
 *
 * This function runs in recursion since the order of the programs are in reverse
 */
void run_prgm(Pgm *p, bool connect_pipe) {
  // Run until p becomes null in reverse order like the print function works
  if(p == NULL) {
    return;
  }
  else {
    int fd[2];

    if(connect_pipe && pipe(fd) == -1) {
      perror("pipe failed");
    }

    pid_t pid = fork();

    if(pid < 0) {
      perror("Fork failed!");
    }
    else if(pid == 0) {
      // This is the child
      if(connect_pipe) {
        close(fd[PIPE_READ]);
        if(dup2(fd[PIPE_WRITE], STDOUT_FD) == -1)
          perror("dup2 error in child");
      }
      // Run the program before execvp since the list of programs are in reverse order
      run_prgm(p->next, true);
      
      char** argv = p->pgmlist;
      basic_commands(argv);
      // The above exec should have ended here
    }
    else {
      // This is the parent
      // Wait or do something else if it's supposed to be a background process
      if(connect_pipe) {
        close(fd[PIPE_WRITE]);
        if(dup2(fd[PIPE_READ], STDIN_FD) == -1)
          perror("dup2 error in parent");
      }
      int status;
      wait(&status);
    }
  }
}

void basic_commands(char **argv){
  if(strcmp(argv[0], "cd") == 0) {
    // Do cd stuff
    // Check the standard input for input of directory
    // Check if there is either a standard input or a second argument to this command
    // Change directory or print out an error
    
  }
  else if(strcmp(argv[0], "exit") == 0) {
    // Do exit stuff
    // Propably send SIGINT signal to the shell somehow
    
  }
  else {
    if(execvp(argv[0],argv) == -1){
      perror("execvp failed");
      exit(EXIT_FAILURE);
    }
  }
}

void handle_sigint(int sig) {
  printf("CTRL+C entered!\n");
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
