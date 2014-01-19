#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>




/**
 * Show prompt to user, read command and execute them.
 * @return 1 if parent fork, 0 otherwise.
 */
int prompt(){
  char *line = readline("> ");
  char *cmd;
  int fd[2];
  int status;
  pid_t childpid;

  char readbuffer[100];

  cmd = strtok(line,"|");

  while(cmd != NULL && 0){
    pipe(fd);
    // printf("cmd=%s\n",cmd);

    // fork and assign childpid
    if((childpid = fork()) == -1)
    {
      //print error if pid is -1
      perror("fork");
      exit(1);
    }else if(childpid == 0){

      close(fd[0]); // child process closes input side
      dup2(fd[1],1); // child process move stdout to output side
      close(fd[1]);

      // close(fd[1]);
      char str[] = "ok";
      write(fd[1], str, (strlen(str)+1));
      // status = execve(cmd, NULL, NULL);

      exit(status);
      return 0;
    }else{
      close(fd[1]); // parent process closes output side
      // dup2(fd[0],0); //child process dup input side
      // close(fd[0]);

      if(waitpid(childpid, &status, 0) < 0){
        //error
        perror("waitpid()");
        exit(1);
      }
      // close(fd[0]);
      int bytes = read(fd[0], readbuffer, sizeof(readbuffer));
      printf("%d %s\n", bytes, cmd);
    }

    // wait
    // output
    // next pipe
    // printf("%s\n", cmd);
    cmd = strtok(NULL,"|");
  }
  return 1;
}

int main(int argc, char *argv[]){
  while(prompt()); //
  // char * arg[] = { "ls", NULL };
  // execve("ls", arg, NULL);
  return 0;
}

