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
#include <ctype.h>

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define INPUT_SIZE 80

/**
 * Fork and execute a command
 * @param in file descriptor for input
 * @param out file descripfor for output
 * @param cmd command
 */
pid_t fork_exec(int in, int out, char *cmd){
  while(*cmd==' ') cmd++;
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
    char *cmdArgv[INPUT_SIZE] = {cmd, NULL};
    execvp(cmdArgv[0], cmdArgv);
    perror(cmd); //it will reach here only if execvp fails
    exit(-1);
  }
  // printf("parent fork %s %d\n",cmd, pid);
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
    out = fd[1]; // fd[1] is write end of the pipe
    pid = fork_exec(in, out, cmds[i]);

    //only parent process of the fork_exec will reach here
    waitpid(pid, &status, 0); // wait for its child to finish first!

    // close(fd[0]); //TODO where to put this

    close(out); // no noweed for writing since the child will write
    in = fd[0]; // now the in side of the pipe became in for the next command

  }

  //Last stage of the pipeline -- redirect in to be stdin
  if(in!=STDIN_FILENO)
    dup2(in,STDIN_FILENO);

  char *cmd = cmds[n-1];
  char * const cmdArgv[INPUT_SIZE] = {cmd, NULL};
  // fflush(stdout);
  execvp(cmdArgv[0], cmdArgv);
  perror("last");
  exit(-1);
}



// char* trim(char *a){
//   while(isspace(*a)) a++;
//   for(int i=strlen(a)-1; i>=0 && isspace(a[i]); i--){
//     a[i] = 0;
//   }
//   return a;
// }

/**
 * Trim (code from http://stackoverflow.com/questions/122616/how-do-i-trim-leading-trailing-whitespace-in-a-standard-way)
 * @param  a str to be trim
 * @return   trimmed string.
 */
char* trim(char * str) {
   char *end;

   // Trim leading space
   while(str != NULL && *str != 0 && isspace(*str)) str++;

   if(*str == 0)  // All spaces?
     return str;

   // Trim trailing space
   end = str + strlen(str) - 1;
   while(end > str && isspace(*end)) end--;

   // Write new null terminator
   *(end+1) = 0;

   return str;
}

/**
 * Show prompt to user and execute command that user enters.
 */
void prompt(){
  pid_t pid;
  int status, fd[2];

  char *input = (char*)malloc(sizeof(char)* INPUT_SIZE);
  char **cmds = (char**)malloc(sizeof(char*)* 30);
  char *cmd;
  input = readline("> "); //show a simple prompt with "> " and save input to input
  int cmd_count = 1;

  cmd = strtok(input, "|");
  if(cmd==NULL) return; //if cmd is null, end this prompt loop.
  *cmds = trim(cmd);

  if(cmd!=NULL){ //if non-empty command

    //save tokens to cmds
    while((cmd = strtok(NULL, "|"))!=NULL){
      // printf("%s\n", cmd);
      cmds[cmd_count++] = trim(cmd);
    }

    // print read command for checking
    // for(int i=0; i<cmd_count ;i++){
    //   printf("%s", cmds[i]);
    // }
    // fflush(stdout);

    if((pid = fork())==0){
      fork_runline(cmd_count, cmds);
    }
    waitpid(pid, &status, 0);
  }else{
    printf("cmds==NULL");
  }

}

void test(){
  // Original Test sets
  pid_t pid;

  // char *cmds[80] = {"ls","head","wc", NULL};
  // char *cmds[80] = {"w","head","wc", NULL};
  // int cmd_count = 3;

  char *cmds[80] = {"ls","wc", NULL};
  // char *cmds[80] = {"w","wc", NULL};
  // char *cmds[80] = {"ifconfig","wc", NULL};
  int cmd_count = 2;

  // for(int i=0 ; i<cmd_count ; i++){
  //   printf("%s\n", cmds[i]);
  // }
  if((pid = fork())==0){
    fork_runline(cmd_count, cmds);
  }
}

size_t trimwhitespace(char *out, size_t len, const char *str)
{
  if(len == 0)
    return 0;

  const char *end;
  size_t out_size;

  // Trim leading space
  while(isspace(*str)) str++;

  if(*str == 0)  // All spaces?
  {
    *out = 0;
    return 1;
  }

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace(*end)) end--;
  end++;

  // Set output size to minimum of trimmed string length and buffer size minus 1
  out_size = (end - str) < len-1 ? (end - str) : len-1;

  // Copy trimmed string and add null terminator
  memcpy(out, str, out_size);
  out[out_size] = 0;

  return out_size;
}

int main(){
  // char a[] = "  a  ";
  // char *b = trim("");
  // printf("%s",b);

  // char *a = strtok(" a ","|"), *b = malloc(sizeof(char)*80);
  // strcpy(b,a);
  // b= trim(b);
  // printf("%s\n",b);
  while(1) prompt();
  // test();
  return 0;
}