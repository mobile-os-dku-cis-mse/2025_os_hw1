#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>


#define PATH_MAX 4096

int main(int argc, char *argv[])
{
    pid_t pid;
    // 해당 프로그램이 시작한 위치를 가져오기
    char *path = getenv("SISH");
    // 현재 사용자 가져오기
    char *usr = getenv("USER");

    while(1){
        printf("\n%s:%s >> ", usr, path);

        // 문자열을 입력받아 띄어쓰기를 기준으로 토큰화해서 args에 저장
        char all_cmd[2048];
        char *cmd;
        char *save;
        char *args[10];
        fgets(all_cmd, sizeof(all_cmd), stdin);
        args[0] = strtok_r(all_cmd, " \n", &save);
        int i = 1;
        while ((args[i] = strtok_r(NULL, " \n", &save)) != NULL) {
            i++;
        }
        args[i] = NULL;
        cmd = args[0];
    
        // 쉘 종료
        if(!strcmp("quit", cmd)) return 0;
        // 현재 디렉토리 위치 변경
        if(!strcmp("cd", cmd)) {
            chdir(args[1]);
            // 현재 디렉토리에 경로를 업데이트
            getcwd(path, PATH_MAX);
            continue;
        };

        // 자식 프로세스 생성
        pid = fork();
        if(pid == -1){
            perror("fork error");
			return 0;
        } else if(pid > 0){
            // parent
            wait(0);
        } else{
            // child
            // execvp는 해당 명령어를 PATH 환경변수를 통해 찾아서 실행
            if (execvp(cmd, args) == -1) {
                perror("execvp"); 
                printf("없는 명령어입니다.");
                exit(0);
            }
        }
    }

	return 0;
}
