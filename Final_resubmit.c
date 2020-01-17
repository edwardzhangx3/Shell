#include <stdio.h>
#include <sys/types.h> /* fstat wait */
#include <sys/wait.h>  /* wait */
#include <unistd.h>    /* getcwd fstat pipe close fork execve chdir */
#include <stdlib.h>    /* getenv */
#include <string.h>    /* memset rindex */
#include <ctype.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_LINE_SIZE 256

char pwd[MAX_LINE_SIZE];         // current working directory
char * home;        // HOME
pid_t pid;
int command_pipe[2];
char ** paths;
int path_num = 0;

#define READ  0
#define WRITE 1

struct cmd_struct {
	char * name;
	char ** argv;
	char * line;
	int argc;
};
struct cmd_struct * read_line(char * line) {
	struct cmd_struct * cmd;
	char *ch;
	int argc;

	cmd = (struct cmd_struct *)malloc(sizeof(struct cmd_struct));
	cmd->line = line;
	cmd->name = NULL;
	cmd->argv = NULL;

	for (ch=line; *ch; ++ch)
		if (*ch != ' ' && *ch != '\t') break;
    	cmd->name = ch;
	
	for ( ; *ch; ++ch)
		if (*ch == ' ' || *ch == '\t') break;
	*ch = 0;
	++ch;

	argc = 0;
	line = ch;
	while (*ch) {
		for ( ; *ch; ++ch) 
			if (*ch != ' ' && *ch != '\t') break;
		if (*ch) argc ++;
		++ch;
		for ( ; *ch; ++ch)
			if (*ch == ' ' || *ch == '\t') break;
		++ch;
	}
	cmd->argv = (char **)malloc( (argc+2)*sizeof(char *) );
	
	cmd->argv[0] = cmd->name;
	argc = 1;
	ch = line;
	while (*ch) {
		for ( ; *ch; ++ch) 
			if (*ch != ' ' && *ch != '\t') break;
		if (*ch) cmd->argv[argc ++] = ch;
		++ch;
		for ( ; *ch; ++ch)
			if (*ch == ' ' || *ch == '\t') break;
		*ch = 0;
		++ch;
	}
	cmd->argv[argc] = 0;
	cmd->argc = argc;

	return cmd;
}
char ** split(char * str, char delimiter){
	int loc;
	int cnt = 1;
	int len = strlen(str);
//Get a size of the pointers array
	for (loc = 0; loc < len; loc++){
		if (str[loc] == delimiter){
			cnt++;
		}
	}
	int i = 0;
	int flag = 0;
	int index = 0;
//allocate cnt char * bytes to res
	char ** res = malloc((cnt+1)*sizeof(char *));
//loop the string and add to an array res
	while(str[i] != '\0'){
		if (str[i] == delimiter){
			int new_len = i - flag;
			char *  new_str = malloc((new_len + 1)*sizeof(char));
			strncpy(new_str, str + flag, new_len);
			new_str[new_len] = '\0';
			res[index] = new_str;
//            printf("%s Length %d \n", new_str, new_len);
			flag += (new_len + 1);
			index++;
		}
		i++;
	}
	if (i - flag > 0){
			int new_len = i - flag;
			char *  new_str = malloc((new_len + 1)*sizeof(char));
			strncpy(new_str, str + flag, new_len);
			new_str[new_len] = '\0';
			res[index] = new_str;
  //          printf("%s Length %d \n", new_str, new_len);
	}
	res[index + 1] = NULL;
	return res;
}

//multiple pipes handler
static int command(struct cmd_struct * cmd, int input, int first, int last, bool out, char * out_file)
{
	int thePipe[2];
    	char command[64];
	/* Invoke pipe */
	pipe( thePipe );	
	pid = fork();
 	int i; 	
    
	if (pid == 0) {
//		printf("%d", 1);
		if (first == 1 && last == 0 && input == 0) {
			// First command
			dup2( thePipe[WRITE], STDOUT_FILENO );
		} else if (first == 0 && last == 0 && input != 0) {
			// Middle command
			dup2(input, STDIN_FILENO);
			dup2(thePipe[WRITE], STDOUT_FILENO);
		} else {
			//Last
			//output redirection
			if (out){
				int fdout = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
				dup2(fdout, STDOUT_FILENO);
				close(fdout);
			}
			dup2( input, STDIN_FILENO );
		}
		for (i = 0; i<path_num; ++ i) {
			sprintf( command, "%s/%s", paths[i], cmd->name);
			if ( access(command, F_OK) != -1) {
				execv( command, cmd->argv);
				exit(1);
			}
		}
	}
	if (input != 0) 
		close(input);
	// Nothing more needs to be written
	close(thePipe[WRITE]);
	// If it's the last command, nothing more needs to be read
	if (last == 1)
		close(thePipe[READ]);
	return thePipe[READ];
}

void get_paths() {
	paths = split(getenv("PATH"), ':');
	int i = 0;
	while(paths[i] != NULL){
		printf("Path %d: %s\n", i, paths[i]);
		i++;
	}
	path_num = i + 1;
}

static char line[200];
static int n = 0; /* number of calls to 'command' */

static int run(struct cmd_struct * cmd, int input, int first, int last, bool out, char * out_file)
{
	if (cmd -> name != NULL) {
		n += 1;
		return command(cmd, input, first, last, out, out_file);
	}
	return 0;
}

static void cleanup(int n)
{
	int i;
	for (i = 0; i < n; ++i) 
		wait(NULL); 
}
static char* skipwhite(char* s)
 {
     while (isspace(*s)) ++s;
     return s;
 }
int main( int argc, char ** argv, char ** ENVS )
{
	 char * prompt = "$ ";
	 struct cmd_struct * cmd;
	 char ** arg;
	 int i;
	 getcwd( pwd, MAX_LINE_SIZE);
	 home = getenv("HOME");
	 get_paths();
	 
	 printf("%s%s", pwd, prompt);
	 while (1) { 
		 scanf("%200[^\n]s",line);
		 getchar();		
		 if ( strcmp( line, "exit" ) == 0) {
			 break;
		 }
		
		 char ** pipes = split(line, '|');
		 int i = 0;
		 while(pipes[i] != NULL){
			i++;
		 }
		
		 int pIndex = 0;
		 int input = 0;
		 int first = 1;
		 struct cmd_struct * temp_cmd;
		 char * out_file;
		 bool outb = false;
		 while(pipes[pIndex + 1] != NULL){
			temp_cmd = read_line(pipes[pIndex]);
			input = run(temp_cmd, input, first, 0, outb, out_file);
			first = 0;
			pIndex++;
		 }

		 char * to_be_parse = pipes[pIndex];
		 //check if the last command contains '>', output redirection
		 if (strstr(pipes[pIndex], ">")){
			outb = true;
			char ** out = split(pipes[pIndex], '>');
			out_file = skipwhite(out[1]);
			to_be_parse = out[0];
		 }
		
		 temp_cmd = read_line(to_be_parse);	
		 input = run(temp_cmd, input, first, 1, outb, out_file);
		 cleanup(n);
		 n = 0;
		 printf("%s%s", pwd, prompt);
	 }
	 return 0;
}
