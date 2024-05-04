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
	unsigned short start_vaddr; // 시작 가상 주소
	unsigned short vaddr_size;  // 가상 주소 크기
	/* Add more fields as needed */
};

struct pcb pcbArr[10];

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

	int pnum = 0;
    while ((read = getline(&line, &len, fp)) != -1) {
        struct pcb process; // pcb 구조체 인스턴스 생성
        char execfile[256];

        // 줄에서 pid와 실행 파일 이름을 읽음
        if(sscanf(line, "%hu %s", &process.pid, execfile) == 2) {
            printf("PID: %hu, 실행 파일: %s\n", process.pid, execfile);

            // 각 실행 파일 내용 읽기
            FILE *execFp = fopen(execfile, "r");
            if(execFp != NULL) {
                char *execLine = NULL;
                size_t execLen = 0;
                ssize_t execRead;
                int lineNum = 0;

                while ((execRead = getline(&execLine, &execLen, execFp)) != -1) {
                    lineNum++;
                    if (lineNum == 2) { // 두 번째 줄 처리
                        sscanf(execLine, "%hu %hu", &process.start_vaddr, &process.vaddr_size);
                        printf("시작 가상 주소: %hu, 가상 주소 크기: %hu\n", process.start_vaddr, process.vaddr_size);
                        break; // 필요한 정보를 읽었으므로 루프 종료
                    }
                }
                if (execLine)
                    free(execLine);
                fclose(execFp);
            } else {
                perror("실행 파일을 열 수 없습니다.");
            }
        }
		
		pcbArr[pnum++] = process;
    }

    if (line)
        free(line);
    fclose(fp);

    return 0;
}

int ku_scheduler(unsigned short arg1){

}
int ku_pgfault_handler(unsigned short arg1){

}
int ku_proc_exit(unsigned short arg1){

}