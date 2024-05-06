#define ADDR_SIZE 16
#define PFN_NUM 1024
#define DIR_SIZE 32   // 2^5
#define TABLE_SIZE 32 // 2^5
#define PAGE_SIZE 64  // 2^6
#define PAGE_MASK 0x3F // 6비트 마스크

struct pcb *current;
unsigned short *pdbr;
char *pmem, *swaps; // 16비트만큼 할당
int pfnum, sfnum;
int totalProcessNum;

unsigned short *pfArr, *sfArr;
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
	unsigned short start_vaddr; // 시작 가상 주소
	unsigned short vaddr_size;  // 가상 주소 크기
	/* Add more fields as needed */
};

struct pcb pcbArr[10];

void ku_freelist_init(){
	int idx = 0;
	// pfArr에 pfnum 만큼 각 페이지 프레임 시작주소 할당
 	pfArr = (unsigned short *)malloc(pfnum * sizeof(unsigned short));
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
	printf("물리메모리 시작 주소: %hu, 페이지 디렉토리 시작주소: %p\n", pmem, pdbr);
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
		pcbArr[i].pid = 9999;
		pcbArr[i].pgdir = (unsigned short *)malloc(DIR_SIZE * sizeof(unsigned short));
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
		totalProcessNum++;
    }

    if (line)
        free(line);
    fclose(fp);

    return 0;
}

int ku_scheduler(unsigned short arg1){
	unsigned short nowPid = arg1;
	if(nowPid > 9) {
		current = &pcbArr[0];
		pdbr = current->pgdir;
		return 0;
	}
	unsigned short nextPid = (nowPid + 1) % totalProcessNum;
	if(pcbArr[nextPid].pid == 9999  || !totalProcessNum) {
		printf("nxt: %d, total: %d", nextPid, totalProcessNum);
		printf("해당 pid를 가진 프로세스는 존재하지 않습니다.\n");
		return 1;
	}

	current = &pcbArr[nextPid];
	pdbr = current->pgdir;
	printf("\ncontext switch 발생!from %d -> to %d\n\n", arg1, current->pid);
	return 0;
}
int ku_pgfault_handler(unsigned short arg1){
    unsigned short nowVa = arg1;
	int lowBound = current->start_vaddr;
	int highBound = current->start_vaddr + current->vaddr_size - 1;
	if(nowVa < lowBound || nowVa > highBound) {
		printf("범위를 넘어서는 가상 주소입니다!\n");
		return 1;
	}
    int dir_index =  (nowVa & 0xFFC0) >> 11; // 상위 5비트
	int tbl_index = (nowVa & 0x07C0) >> 6;
    unsigned short *nowPde = current->pgdir + dir_index;
	printf("pgdir index: %hu\n", dir_index);
	int PFN;
	// printf("now PDE VAL: %hu\n", *nowPde);
    if(!(*nowPde & 0x1)){ // PDE가 비어있는 경우
        // printf("너가 문제였구나! 너에게 할당해주마!!!! %p\n", nowPde);
        int i = 0;
        for(i = 0; i < PFN_NUM; i++) {
            if(!pArr[i]) { // 빈 페이지 프레임을 찾음
                pArr[i] = 1; // 페이지 프레임을 사용 중으로 표시
				printf("page frame number: %d\n", i);
                *nowPde = (i << 4) | 0x1; // PDE에 PFN 및 present bit 설정
                break;
            }
        }
        if(i == PFN_NUM) { // 빈 페이지 프레임을 찾지 못한 경우
            printf("할당 가능한 페이지 프레임이 없습니다.\n");
            return 1; // 오류 반환
        }
    }

	PFN = (*nowPde & 0xFFF0) >> 4;
	unsigned short *ptbr = (unsigned short*)(pmem + (PFN << 6));

	tbl_index = (nowVa & 0x07C0) >> 6;
	unsigned short *pte = ptbr + tbl_index;
	if(!(*pte & 0x1)){
		int i = 0;
		for(i = 0; i < PFN_NUM; i++) {
            if(!pArr[i]) { // 빈 페이지 프레임을 찾음
                pArr[i] = 1; // 페이지 프레임을 사용 중으로 표시
				printf("page frame number: %d\n", i);
                *pte = (i << 4) | 0x1; // PTE에 PFN 및 present bit 설정
                break;
            }
        }
		// printf("pdeval: %hu PFN: %d, ptbr: %p, table index: %d, pte: %p, pteVal: %hu\n", *nowPde, PFN, ptbr, tbl_index, pte, *pte);
		
	}
	
    return 0;
}


int ku_proc_exit(unsigned short arg1){
	unsigned short exitPid = arg1;
	int validPid = 0;
	for(int i = 0; i < 10; i++) {
		if(pcbArr[i].pid == exitPid) {
			if(pcbArr[i].fd != NULL) {
                fclose(pcbArr[i].fd);
                pcbArr[i].fd = NULL;
            }

			for(int j = i; j < totalProcessNum; j++) {
				pcbArr[j] = pcbArr[j + 1];
			}
			free(pcbArr[i].pgdir);
			totalProcessNum--;
			validPid = 1;
			break;
		} 
	}
	if(!validPid) {
		printf("Invalid Pid!!\n");
		return -1;
	}
	// printf("proccess %d exit! totalPNum: %d \n", exitPid, totalProcessNum);
	return 0;
}