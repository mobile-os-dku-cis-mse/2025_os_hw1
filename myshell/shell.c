#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>


#define PATH_MAX 4096

extern char **environ;

char *concat(const char *s1, const char *s2) {
    size_t n1 = strlen(s1), n2 = strlen(s2);
    char *out = malloc(n1 + n2 + 1);       // +1 for '\0'
    if (!out) return NULL;
    memcpy(out, s1, n1);
    memcpy(out + n1, s2, n2 + 1);          // 널 문자까지 복사
    return out;
}


int main(int argc, char *argv[])
{
    pid_t pid;
    char *path = getenv("SISH");
    char *usr = getenv("USER");
    if(!path) path = "> ";

    while(1){
        
        printf("\n%s:%s >> ", usr, path);

        // 받아들인 문자열을 cmd 랑 argv 로 구분해서 받기
        char all_cmd[2048];
        char *cmd;
        char *save;
        fgets(all_cmd, sizeof(all_cmd), stdin);

        char *args[10];

        args[0] = strtok_r(all_cmd, " \n", &save);
        int i = 1;
        while ((args[i] = strtok_r(NULL, " \n", &save)) != NULL) {
            i++;
        }
        args[i] = NULL;
        cmd = args[0];

        // 부모 프로세스에서 작동해야할 명령어는 넘기기
        if(!strcmp("quit", cmd)) return 0;
        if(!strcmp("cd", cmd)) {
            chdir(args[1]);
            getcwd(path, PATH_MAX);
            continue;
        };
        pid = fork();
        if(pid == -1){
            perror("fork error");
			return 0;
        } else if(pid > 0){
            // parent
            wait(0);
            
        } else{
            // child
            if(!strncmp("./", cmd, sizeof(char)*2)){
                if (execve(cmd, args, environ) == -1) {
                    perror("execve");   // 실패 시만 여기로 옴
                    printf("없는 명령어입니다.");
                    exit(0);
                }
            }
            if (execve(concat("/bin/", cmd), args, environ) == -1) {
                perror("execve");   // 실패 시만 여기로 옴
                printf("없는 명령어입니다.");
                exit(0);
            }
        }
    }

	return 0;
}