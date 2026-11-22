#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<fcntl.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<unistd.h>
#include<sys/wait.h>
#include<errno.h>

#define FIFO_PERMS (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)


int isPrime(long n) {
	if (n < 2) return 0;
	if (n == 2) return 1;
	if ((n & 1L) == 0L) return 0;
	for (long d = 3; d * d <= n; d += 2) {
		if (n % d == 0) return 0;
	}
	return 1;
}


int main(int argc, char *argv[]){

    // 프로그램 실행하면 파일 이름과 자식 프로세스 수 입력 받기
	// 예외처리
    if(argc != 3){
        printf("인자를 확인해주세요.\n");
        exit(1);
    }

    char *file_name = argv[1];
	int child_count = atoi(argv[2]);
	
    printf("filename: %s, 자식 수:%d\n", file_name, child_count);	
 
    long file_size;

	struct stat file_info;
	
	if(stat(file_name, &file_info)==0){
		file_size = (long)file_info.st_size;
	}

    // 한 번에 읽을 청크 사이즈
    long chunk_size = file_size/child_count;

    // fork하기 전에 pipe 객체를 만들어둬야 자식의 fdt entry에도 pipe의 entry가 저장되기 때문에 fork()하기 전에 pipe객체 만들기
    int pipe_fd[2];
    if(pipe(pipe_fd) == -1){
        // 이미 존재하면 그냥 실행
        perror("Pipe failed\n");
        return 1;
    }

    // file offset을 공유하지 않도록 fork()한 후 생성
    for(int i=0; i<child_count; i++){
        pid_t pid = fork();

        // fork 실패 시 예외 처리
        if(pid == -1){
            perror("자식 프로세스 생성 실패\n");
            exit(errno);
        }

        if(pid >0){
            printf("%d 번째 자식 프로세스 생성됨\n",i+1);
        }else{
        // 자식 프로세스가 실행할 내용
           // 여기 함수로 구현하자.
        /*
            구현 내용./
            1. 파일 열어서 읽기
            2. 범위 나누기(예외처리)
            3. 읽은 범위, 읽은 수 개수, 소수의 개수, 가장 큰 소수 출력
         */
            // 자식 프로세스 read 닫기
            close(pipe_fd[0]);

            int fd = open(file_name,O_RDONLY);

            // 예외 처리
            if(fd == -1){
                perror("파일 열기 에러");
                exit(errno);
            }

            long start = chunk_size*i;
            long end = chunk_size*(i+1);
            if(i+1 == child_count) end = file_size;

            // 범위 나누기
            char c;

            if(start>0){
                    lseek(fd, start-1, SEEK_SET);

                    read(fd,&c, 1);

                    if(c!= ' ' && c!='\n' && c!='\t'){
                        while(read(fd, &c, 1)>0){
                            start++;
                            if (c == ' ' || c == '\n' || c == '\t') {
                                break;
                            }
                        }
                    }
                }

            if(end < file_size){

                lseek(fd, end-1, SEEK_SET);

                read(fd,&c, 1);

                if (c != ' ' && c != '\n' && c != '\t') {
                    while (read(fd, &c, 1) > 0) {
                        end++;
                        if (c == ' ' || c == '\n' || c == '\t') {
                            break;
                        }
                    }
                }
            }

            // 범위 설정하고 file_offset 옮기기
            long read_size = end-start;
            char buf[read_size+1];
            buf[read_size] = '\0';

            lseek(fd, start, SEEK_SET);
          
            // 숫자들 읽기
            read(fd,buf,read_size);

            char* deli = " \n\t";
            char* saveptr = NULL;
            char* token = strtok_r(buf,deli,&saveptr);

            long read_count = 0;
            long prime_count = 0;
            long max = 0;

            while(token != NULL){
                read_count++;
                long temp = atol(token);
                if(isPrime(temp)){
                    prime_count++;
                    if(temp > max) max = temp;
                }

                token = strtok_r(NULL,deli,&saveptr);
            }
            
            // pipe에 자식이 읽은 소수의 개수를 입력
            ssize_t written = write(pipe_fd[1], &prime_count, sizeof(prime_count));

            if (written != sizeof(prime_count)) {
                perror("pipe write failed");
                exit(1);
            }
            // 자식 프로세스 write 닫기
            close(pipe_fd[1]);
            
            printf("[child %d] offset [%ld, %ld) numbers=%ld primes=%ld max=%ld\n", getpid(),start, end, read_count, prime_count, max);

            return 0;
        }
    }
    // 부모 프로세스 write 닫기
    close(pipe_fd[1]);


    // 자식 프로세스가 끝날 때까지 대기
    int success_child = 0;
    long total_prime_count = 0;

    for(int i=0; i<child_count; i++){
        int status;

        // 자식이 pipe에 적은 값 가져오기
        long long_prime;

        ssize_t received = read(pipe_fd[0], &long_prime, sizeof(long_prime));

        if (received != sizeof(long_prime)) {
            fprintf(stderr, "pipe read failed\n");
            exit(1);
        }

        total_prime_count += long_prime;

        printf("[parent %ld] read value is %ld.\n", (long)getpid(), long_prime);

        pid_t child_id = wait(&status);


        if (child_id == -1) {
            perror("Wait 실패.\n");
            break;
        }
        
        // 자식 프로세스가 정상적인 방식으로 종료했는지 검사
        if (WIFEXITED(status)) {

            // 자식 프로세스의 종료 코드 확인
            int exit_code = WEXITSTATUS(status);

            if (exit_code == 0) {
                success_child++;
                printf("[parent %d] child %d terminated (%d/%d)\n",getpid(), child_id, i + 1, child_count);
            }

        } else if (WIFSIGNALED(status)) {
            printf("[parent %d] child %d killed by signal %d\n",getpid(), child_id, WTERMSIG(status));
        }

        if(success_child == child_count){
          printf("[parent %d] Found %ld prime numbers in total.\n", getpid(), total_prime_count);
        }
    }
    // 부모 프로세스 read 닫기
    close(pipe_fd[0]);

	
	return 0;
}
