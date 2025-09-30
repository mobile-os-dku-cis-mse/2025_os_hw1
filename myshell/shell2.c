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

int flag = 0;
char *concat(const char *s1, const char *s2) {
    size_t n1 = strlen(s1), n2 = strlen(s2);
    char *out = malloc(n1 + n2 + 1);     
    if (!out) return NULL;
    memcpy(out, s1, n1);
    memcpy(out + n1, s2, n2 + 1);       
    return out;
}


int main(int argc, char *argv[])
{
    pid_t pid;
    char *path = getenv("SISH");
    char *usr = getenv("USER");
    if(!path) path = "> ";
    int fd = open("public.txt", O_TRUNC | O_RDWR);
    if(fd < 0){perror("open"); return 1;}
            
    while(!flag){
        pid = fork();
        if(pid == -1){
            perror("fork error");
			return 0;
        } else if(pid > 0){
            // parent
        
            int status;
            pid_t pid = wait(&status);
            if( WIFEXITED(status)){ // 잘 종료했을 때
                flag = WEXITSTATUS(status);
                if(flag == 1) flag = 0;
                if(flag == 2){
                    flag = 0;

                    lseek(fd, 0, SEEK_SET);
                    char buf[1024];
                    int n = read(fd, buf, sizeof(buf) - 1);
                    buf[n] = '\0';
        
                    chdir(buf);
                    getcwd(path, PATH_MAX);

                }
            }
        } else{
            // child
            // 파일 크기 0으로 잘라서 초기화
            ftruncate(fd, 0);
            lseek(fd, 0, SEEK_SET);     
         
            printf("\n%s:%s >> ", usr, path);

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
            if(!strcmp("quit", cmd)) exit(3);
            if(!strcmp("cd", cmd)) {
                char buffer[1024] = "";
                for (int j = 1; args[j] != NULL; j++) {
                    strcat(buffer, args[j]);
                    if (args[j+1] != NULL) strcat(buffer, " "); // 토큰 사이에 공백 추가
                }
                write(fd, buffer, strlen(buffer));
            
                lseek(fd, 0, SEEK_SET);
                char buf[1024];
                int n = read(fd, buf, sizeof(buf) - 1);
                buf[n] = '\0';

                close(fd);
                exit(2);
            };
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
    if(!flag) printf("종료");

	return 0;
}
