// test implementing ls | wc

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define STDIN_FILENO 0
#define STDOUT_FILENO 1

/**
 * Fork and execute a command
 * @param in file descriptor for input
 * @param out file descripfor for output
 * @param cmd command
 */
pid_t fork_exec(int in, int out, char *cmd){
  pid_t pid;
  if((pid=fork())<0){
    //print error from fork if error
    perror("fork_exec");
    exit(-1);
  }else if (pid==0){
    // run the exec command in the child process
    if(in!= STDIN_FILENO){ // redirect if in is not stdin
      dup2(in,STDIN_FILENO);
      close(in);
    }
    if(out!= STDOUT_FILENO){//redirect if out is not stdout
      dup2(out, STDOUT_FILENO);
      close(out);
    }
    char *cmdArgv[80] = {cmd, NULL};
    execvp(cmdArgv[0], cmdArgv);
    perror(cmd); //it will reach here only if execvp fails
    exit(-1);
  }
  printf("parent fork %s %d\n",cmd, pid);
  return pid;
}

/**
 * Run a serie of piped commands
 * @param n    number of commands in the pipe
 * @param cmds arrays of commands
 */
void fork_runline(int n, char **cmds){
  int in = STDIN_FILENO; // file descriptor for in -- initially set as stdin
  int out;
  int status;
  pid_t pid;
  int fd[2]; // file descriptor for piping

  //for each command, create a fork to execute except the last one.
  for(int i=0 ; i<n-1 ; i++){
    pipe(fd);
    out = fd[1]; // fd is write end of the pipe
    pid = fork_exec(in, out, cmds[i]);


    //only parent process of the fork_exec will reach here
    waitpid(pid, &status, 0); // wait for its child to finish first!

    // close(fd[0]); //TODOwhere to put this

    close(out); // no need for writing since the child will write
    in = fd[1]; // now the in side of the pipe became in for the next command

    // printf("yo! %s\n", cmds[i]);
  }

  //Last stage of the pipeline -- redirect in to be stdin
  if(in!=STDIN_FILENO)
    dup2(in,STDIN_FILENO);

  char *cmd = cmds[n-1];
  char *cmdArgv[80] = {cmd, NULL};
  printf("yo! %s", cmd);
  fflush(stdout);
  execvp(cmdArgv[0], cmdArgv);
  perror("last");
  exit(-1);
}


int prompt(){
  pid_t pid;
  int status, fd[2];

  char *cmds[80] = {"ls","head","wc", NULL};
  int cmd_count = 3;

  // char *cmds[80] = {"ls","wc", NULL};
  // int cmd_count = 2;

  // for(int i=0 ; i<cmd_count ; i++){
  //   printf("%s\n", cmds[i]);
  // }

  // printf("hello world");
  if((pid = fork())==0){
    fork_runline(cmd_count, cmds);
  }
  waitpid(pid, &status, 0);
  return 0;
}

int main(){
  prompt();
  return 0;
}