#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <errno.h>

extern char **environ;

int execute_builtin(char *command, char *args[]){
	static char last_dir[1024];
	char current_dir[1024];
	if(getcwd(current_dir, sizeof(current_dir)) == NULL){
		perror("getcwd");
		return 1;
	}
	if(strcmp(command, "quit") == 0){
		printf("SiSH를 종료합니다.\n");
		fflush(stdout);
		exit(0);
	}
	if(strcmp(command, "cd") == 0){
		char *target_dir = args[1];
		if(target_dir == NULL || strcmp(target_dir, "~") == 0){ // cd 이후 인자가 없거나 home을 뜻하는 ~ 입력시 홈 디렉토리로 이동
			target_dir = getenv("HOME");
			if(target_dir == NULL){
				fprintf(stderr, "SiSH: cd: HOME 환경 변수를 찾을 수 없습니다.\n");
				return 1;
			}
		}else if(target_dir != NULL && strcmp(target_dir, "-") == 0){ // 이전 디렉토리로 이동
			if(last_dir[0] == '\0'){
				fprintf(stderr,"SiSH: cd: 이전 디렉토리 기록이 없습니다.\n");
				return 1;
			}
			target_dir = last_dir;
			printf("%s\n", target_dir);
		}
		if(chdir(target_dir) == 0){
			strcpy(last_dir, current_dir);
		}else{
			perror("SiSH: cd");
		}
		return 1;
	}
	return 0;
}

int main(){
	char input[256];
	char input_copy[256];
	char cwd[1024];
	while(1){
		if(getcwd(cwd, sizeof(cwd)) == NULL){
			perror("getcwd failed");
			printf("SiSH>> ");
		}else{
			printf("SiSH >> %s >> ",cwd);
		}

		fgets(input, sizeof(input), stdin);
		input[strcspn(input,"\n")] = 0;
	
		if(strlen(input) == 0){
			continue;
		}

		strcpy(input_copy,input);
		int i = 0;
		char *token = strtok(input_copy," ");
		char *args[128];
		while(token != NULL){
			args[i++] = token;
			token = strtok(NULL, " ");
		}
		args[i] = NULL;
		char *command = args[0];

		if(execute_builtin(command,args) == 1){
			continue;
		}

		char *path_env = getenv("PATH");
		char full_path[1024];
		int command_found = 0;

		if(path_env != NULL){
			char path_copy[1024];
			strcpy(path_copy, path_env);
			char *dir = strtok(path_copy, ":");

			while(dir != NULL){
				sprintf(full_path, "%s/%s",dir,command);
				if(access(full_path,F_OK) == 0){
					command_found = 1;
					break;
				}
				dir = strtok(NULL,":");
			}
			if(command_found){
				pid_t pid = fork();
				if(pid == 0){ // 자식프로세스 실행
					execve(full_path, args, environ);
					perror("execve failed");
					exit(EXIT_FAILURE);
				}else if(pid > 0){ // 부모프로세스 실행
					int status;
					waitpid(pid, &status, 0);
				}else{
					fprintf(stderr,"SiSH: fork failed %s\n",strerror(errno));
				}
			}else{
				fprintf(stderr, "SiSH: command not found: %s\n",command);
			}
		}
	}
	return 0;
}
