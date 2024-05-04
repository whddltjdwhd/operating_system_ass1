#define ADDR_SIZE 16

struct pcb *current;
unsigned short *pdbr;
char *pmem, *swaps;
int pfnum, sfnum;
// char **pfArr, **sfArr;
int *pfArr, *sfArr;

void ku_dump_pmem(void);
void ku_dump_swap(void);


struct pcb{
	unsigned short pid;
	FILE *fd;
	unsigned short *pgdir;
	/* Add more fields as needed */
};
void ku_freelist_init(){
	int idx = 0;
	// pfArr에 pfnum 만큼 각 페이지 프레임 시작주소 할당
 	// pfArr = (char**)malloc(pfnum * sizeof(char*));
	// for(idx = 0; idx < pfnum; idx++) {
	// 	pfArr[idx] = pmem + (64 * idx);
	// }
	// for(int i = 0; i < pfnum; i++) {
	// 	printf("pfArr %d : %p\n", i, pfArr[i]);
	// }
	pfArr = (int*)malloc(pfnum * sizeof(int));
	sfArr = (int*)malloc(sfnum * sizeof(int));

	// idx = 0;
	// sfArr에 sfnum 만큼 각 페이지 프레임 시작주소 할당
	// sfArr = (char**)malloc(sfnum * sizeof(char*));
	// for(idx = 0; idx < sfnum; idx++) {
	// 	sfArr[idx] = swaps + (64 * idx);
	// }
}
int ku_proc_init(int argc, char *argv[]){
	FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    if(argc < 2) {
        printf("사용법: 프로그램명 <입력파일명>\n");
        return 1;
    }

    fp = fopen(argv[1], "r");
    if (fp == NULL) {
        perror("파일을 열 수 없습니다.");
        return 1;
    }

    while ((read = getline(&line, &len, fp)) != -1) {
        char pid[10];
        char execfile[256];

        // 줄에서 pid와 실행 파일 이름을 읽음
        if(sscanf(line, "%s %s", pid, execfile) == 2) {
            printf("PID: %s, 실행 파일: %s\n", pid, execfile);

            // 각 실행 파일 내용 읽기
            FILE *execFp = fopen(execfile, "r");
            if(execFp != NULL) {
                char *execLine = NULL;
                size_t execLen = 0;
                while ((read = getline(&execLine, &execLen, execFp)) != -1) {
                    // 실행 파일의 각 줄을 출력
                    printf("실행 파일 내용: %s", execLine);
                }
                if (execLine)
                    free(execLine);
                fclose(execFp);
            } else {
                perror("실행 파일을 열 수 없습니다.");
            }
        }
    }
}
int ku_scheduler(unsigned short arg1){

}
int ku_pgfault_handler(unsigned short arg1){

}
int ku_proc_exit(unsigned short arg1){

}