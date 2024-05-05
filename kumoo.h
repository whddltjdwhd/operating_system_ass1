#define ADDR_SIZE 16
#define PFN_NUM 1024
#define DIR_SIZE 32   // 2^5
#define TABLE_SIZE 32 // 2^5
#define PAGE_SIZE 64  // 2^6
#define PAGE_MASK 0x3F // 6비트 마스크

struct pcb *current;
unsigned short *pdbr;
char *pmem, *swaps;
int pfnum, sfnum;


char **pfArr, **sfArr;
int assignedPfNum;
int pArr[PFN_NUM];

void ku_dump_pmem(void);
void ku_dump_swap(void);

struct pte {
	unsigned short *pgtb;
};

struct pcb {
	unsigned short pid;
	FILE *fd;
	unsigned short *pgdir;
	struct pte *pteArr;
	unsigned short start_vaddr; // 시작 가상 주소
	unsigned short vaddr_size;  // 가상 주소 크기
	/* Add more fields as needed */
};

struct pcb pcbArr[10];

void ku_freelist_init(){
	int idx = 0;
	// pfArr에 pfnum 만큼 각 페이지 프레임 시작주소 할당
 	pfArr = (char**)malloc(pfnum * sizeof(char*));
	for(idx = 0; idx < pfnum; idx++) {
		pfArr[idx] = pmem + (64 * idx);
	}
	// for(int i = 0; i < pfnum; i++) {
	// 	printf("pfArr %d : %p\n", i, pfArr[i]);
	// }
	assignedPfNum = 0;
	// pfArr = (int*)malloc(pfnum * sizeof(int));
	// pfArr = (unsigned short*)malloc(PFN_NUM * sizeof(unsigned short));
	sfArr = (unsigned short*)malloc(sfnum * sizeof(int));
	// pdbr = (unsigned short *)(pmem);
	printf("물리메모리 시작 주소: %p, 페이지 디렉토리 시작주소: %p\n", pmem, pdbr);
	// idx = 0;
	// sfArr에 sfnum 만큼 각 페이지 프레임 시작주소 할당
	// sfArr = (char**)malloc(sfnum * sizeof(char*));
	// for(idx = 0; idx < sfnum; idx++) {
	// 	sfArr[idx] = swaps + (64 * idx); q
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
	for(int i = 0; i < 10; i++) {
		pcbArr[i].pid = -1;
		pcbArr[i].pgdir = (unsigned short *)malloc(DIR_SIZE * sizeof(unsigned short));
		pcbArr[i].pteArr = (unsigned short *)malloc(TABLE_SIZE * sizeof(unsigned short));
	}

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
	unsigned short nowPid = arg1;
	if(nowPid > 9) {
		nowPid = 0;
	}

	if(pcbArr[nowPid].pid == -1) {
		printf("해당 pid를 가진 프로세스는 존재하지 않습니다.\n");
		return 1;
	}

	current = &pcbArr[nowPid];
	pdbr = current->pgdir;
	return 0;
}
int ku_pgfault_handler(unsigned short arg1){
    unsigned short nowVa = arg1;
    int dir_index =  (nowVa & 0xFFC0) >> 11; // 상위 5비트
	int tbl_index = (nowVa & 0x07C0) >> 6;
    unsigned short *nowPde = current->pgdir + dir_index;
	int PFN;

    if(!*nowPde) { // PDE가 비어있는 경우
        printf("너가 문제였구나! 너에게 할당해주마!!!! %p\n", nowPde);
        int i = 0;
        for(i = 0; i < PFN_NUM; i++) {
            if(!pArr[i]) { // 빈 페이지 프레임을 찾음
                pArr[i] = 1; // 페이지 프레임을 사용 중으로 표시
                // PDE에 프레임 번호 설정 + present bit 설정
                // 여기서는 간단하게 프레임 번호를 PDE 값으로 사용합니다.
                *nowPde = (i << 4) | 0x1; // PDE에 PFN 및 present bit 설정
                break;
            }
        }
        if(i == PFN_NUM) { // 빈 페이지 프레임을 찾지 못한 경우
            printf("할당 가능한 페이지 프레임이 없습니다.\n");
            return -1; // 오류 반환
        }
        PFN = (*nowPde & 0xFFF0) >> 4;
    	unsigned short *ptbr = (unsigned short*)(pmem + (PFN << 6));

		tbl_index = (nowVa & 0x07C0) >> 6;
    	unsigned short *pte = ptbr + tbl_index;
		printf("pdeval: %hu PFN: %d, ptbr: %p, table index: %d, pte: %p, pteVal: %hu\n", *nowPde, PFN, ptbr, tbl_index, pte, *pte);
		*pte |= 0x1;
    }
    // 이미 PDE에 할당된 경우 혹은 할당 후의 처리는 여기서 추가할 수 있습니다.
    return 0;
}


int ku_proc_exit(unsigned short arg1){

}