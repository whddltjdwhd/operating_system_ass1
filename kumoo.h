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

	// pcbArr의 pid값을 -1로 초기화
	for(int i = 0; i < 10; i++) pcbArr[i].pid = -1;

	int pnum = 0;
    while ((read = getline(&line, &len, fp)) != -1) {
        char execfile[256];

        // 줄에서 pid와 실행 파일 이름을 읽음
        if(sscanf(line, "%hu %s", &(pcbArr[pnum].pid), execfile) == 2) {
            printf("PID: %hu, 실행 파일: %s\n", pcbArr[pnum].pid, execfile);

            // 각 실행 파일 내용 읽기
            pcbArr[pnum].fd = fopen(execfile, "r");
            if(pcbArr[pnum].fd != NULL) {
                char *execLine = NULL;
                size_t execLen = 0;
                ssize_t execRead;
                int lineNum = 0;

                while ((execRead = getline(&execLine, &execLen, pcbArr[pnum].fd)) != -1) {
                    lineNum++;
                    if (lineNum == 2) { // 두 번째 줄 처리
                        sscanf(execLine, "%hu %hu", &(pcbArr[pnum].start_vaddr),&(pcbArr[pnum].vaddr_size));
                        printf("시작 가상 주소: %hu, 가상 주소 크기: %hu\n", pcbArr[pnum].start_vaddr, pcbArr[pnum].vaddr_size);
                        break; // 필요한 정보를 읽었으므로 루프 종료
                    }
                }
                if (execLine) free(execLine);
            } else {
                perror("실행 파일을 열 수 없습니다.");
            }
        }
		pnum++;
    }

    if (line)
        free(line);
    fclose(fp);

    return 0;
}

int ku_scheduler(unsigned short arg1){
	if(arg1 > 9) {
		arg1 = 0;
	}

	if(pcbArr[arg1].pid == -1) {
		printf("해당 pid를 가진 프로세스는 존재하지 않습니다.\n");
		return 1;
	}

	current = &pcbArr[arg1];

	return 0;
}
int ku_pgfault_handler(unsigned short arg1){

}
int ku_proc_exit(unsigned short arg1){

}