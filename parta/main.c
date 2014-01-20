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

#define INPUT_SIZE 100
#define STDIN_FILENO 0
#define STDOUT_FILENO 1

void prompt(){

  char *input = (char*)malloc(sizeof(char)* INPUT_SIZE);
  char *cmd1, *cmd2;
  int status1, status2;
  pid_t pid;
  input = readline("> "); //show a simple prompt with "> " and save input to input

  if(strcmp(input,"exit\n")==0) //exit command
    exit(0);

  cmd1 = strtok(input ,"|");

  if(cmd1 != NULL){
    // printf("%s", cmd1);
    pid = fork();
    while((cmd2 = strtok(NULL, "|"))!=NULL){

      cmd1 = cmd2;
    }


    if(pid<0){
      perror("fork");
      exit(-1);
    }else if (pid==0){
      //child process
      //TODO add case for having pipeline
      char *argv[] = {cmd1, NULL};
      printf("%s %s", cmd1, *argv);
      execv(cmd1 ,argv);
      perror("cmd1");
      exit(-1);
    }else {
      //parent process
      waitpid(pid, &status1, 0);


    }
  }
}

int main(int argc, char *argv[]){
  while(1)
    prompt(); //keep prompting
  return 0;
}


