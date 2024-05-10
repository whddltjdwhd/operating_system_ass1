#include <string.h>

#define ADDR_SIZE 16
#define PFN_NUM 4096
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
int allocatedPageNum;

void ku_dump_pmem(void);

void ku_dump_swap(void);

struct swapFrame
{
   int isAllocated;
};


struct pageFrame {
    int isAllocated;
    int loadIndex;
    int pageType; // 0: page directory, 1: page table, 2: page
    int validPteNum; // 만약 해당 프레임에 page table이 할당되어 있다면 유효한 pte 의 개수를 관리한다.
};

struct allocatedEntry {
    int PFN;
    unsigned short *entry;
    struct allocatedEntry *next;
};


struct pcb {
    unsigned short pid;
    FILE *fd;
    unsigned short *pgdir;
    unsigned short start_vaddr; // 시작 가상 주소
    unsigned short vaddr_size;  // 가상 주소 크기
    int procsAllocatedPageNum;
    unsigned short *pfnArr; // 할당된 pde, pte의 pfn
    struct allocatedEntry *allocEntryArr; // 할당된 entry를 관리하는 배열
};

struct pcb *pcbArr;
struct pageFrame *pageFrameArr;
struct swapFrame *swapFrameArr;

void searchAllocEntryArr(struct allocatedEntry *head) {
    while (head != NULL) {
        // printf("PFN: %d, entry: %hu\n", head->PFN, *(head->entry));
        head = head->next;
    }
}
void getAllocEntryArr(struct allocatedEntry *head, int PFN) {
    while (head != NULL) {
        if(PFN == head->PFN) {
        printf("PFN: %d, entry: %p entry val: %hu\n", head->PFN, head->entry, *(head->entry));
         return;
        }
        head = head->next;
    }
}

void reapAllocEntryArr(struct allocatedEntry **head) {
    struct allocatedEntry *it, *nextNode;
    it = *head;
    while (it != NULL) {
        // printf("Delete PFN: %d, entry: %hu\n", it->PFN, *(it->entry));
        if (pageFrameArr[it->PFN].isAllocated != 1) {
            printf("something error!\n");
            return;
        }

        printf("reaped PFN: %d\n", it->PFN);
        pageFrameArr[it->PFN].isAllocated = 0;
        nextNode = it->next;
        free(it);
        it = nextNode;
    }
    *head = NULL;
}

void ku_freelist_init() {
    pcbArr = (struct pcb *) malloc(10 * sizeof(struct pcb));
    // page frame을 관리하는 pageFrameArr를 PFN_NUM(1024) 만큼 할당
    pageFrameArr = (struct pageFrame *) malloc(pfnum * sizeof(struct pageFrame));
    swapFrameArr = (struct swapFrame *) malloc(sfnum * sizeof(struct swapFrame));

    // pcb array의 초기 pid는 9999로 설정
    for (int i = 0; i < 10; i++) {
        pcbArr[i].pid = 9999;
    }
}

void addEntryIntoEntryArr(unsigned short *entry, int PFN) {
    struct allocatedEntry *newEntry = (struct allocatedEntry *) malloc(sizeof(struct allocatedEntry));
    if (newEntry == NULL) {
        // 메모리 할당 실패 처리
        perror("Memory allocation failed for allocatedEntry.\n");
        return;
    }

    newEntry->entry = entry;
    newEntry->PFN = PFN;

    if (current->procsAllocatedPageNum == 0) {
        current->allocEntryArr = newEntry;
        current->allocEntryArr->next = NULL;
    } else {
        newEntry->next = current->allocEntryArr;
        current->allocEntryArr = newEntry;
    }
}

void clearPageFram(int PFN) {
    /*
       PFN번째 Page Frame의 값을 0으로 초기화 시켜주는 함수이다.
    */
    char *basePageFrame = pmem + (PFN << 6);
    memset(basePageFrame, 0, 64);
}

int find_page() {
    int ret = 1e9;
    for (int i = 0; i < pfnum; i++) {
        if (!pageFrameArr[i].pageType || pageFrameArr[i].validPteNum) continue;
        if (pageFrameArr[i].loadIndex < ret) ret = pageFrameArr[i].loadIndex;
    }
    return ret;
}

int check_pagetable(int evictPFN) {
    unsigned short allocCount = 0;
    // pmem에서 evictPFN에 해당하는 위치로 pte를 설정합니다.
    unsigned short *pte = (unsigned short *) (pmem + (evictPFN << 6));

    for (int i = 0; i < 32; i++) {
        if (pte[i] & 0x1) allocCount++;
    }
    printf("pte count: %d\n", allocCount);
    // 모든 엔트리가 할당되지 않았다면 1을, 그렇지 않다면 0을 반환합니다.
    return allocCount == 0 ? 1 : 0;
}

void swap_out(int PFN) {
 
   int i = 1;
   for(int i = 1; i <= sfnum; i++) {
      if(swapFrameArr[i].isAllocated) continue;
      swapFrameArr[i].isAllocated = 1;
      getAllocEntryArr(current->allocEntryArr, PFN);
      // printf("entry: %p, entry val: %hu\n", nowPte, *nowPte);
      // i의 값을 PFN 부분에 설정하면서 present 비트를 0으로, dirty bit는 유지
      // *nowPte = (i << 2) | (*nowPte & 0x2);
      // printf("AFTER entry: %p, entry val: %hu\n", nowPte, *nowPte);

      break;
   }
}

int allocate_page_frame(unsigned short *entry, int pageType) {
    /*
       입력으로 주어진 엔트리(pde, pte, page directory)에 대해 page frame을 할당한다.
       isPgdir 변수는 page directory의 경우 present bit 처리를 해주지 않기 때문에 이를 구별하는 변수이다.
    */

    int isAllocated = 0;
    for (int i = 0; i < 4; i++) {
        if (pageFrameArr[i].isAllocated != 0) continue;

        pageFrameArr[i].isAllocated = 1; // 페이지 프레임을 사용 중으로 표시
        clearPageFram(i);

        // pde, pte의 경우 isPageDirectoory를 0으로 설정
        pageFrameArr[i].pageType = pageType;

        if (pageType > 0) {
            *entry = (i << 4) | 0x1; // present bit 설정
            // 현재 pcb에서 page entry 관리하는 로직
            current->procsAllocatedPageNum++;
            addEntryIntoEntryArr(entry, i);
        }
        pageFrameArr[i].loadIndex = allocatedPageNum++;
        isAllocated = 1;
        break;
    }

    if (!isAllocated) {
        // 빈 페이지 프레임을 찾지 못한 경우 swapping!
        printf("hi\n");

        int oldestPFN = find_page();
        int pageType = pageFrameArr[oldestPFN].pageType;
        printf("valid pte num: %d\n", pageFrameArr[oldestPFN].validPteNum);
        
         // swapping!
         printf("okay you deserve to be evicted! %d\n", oldestPFN);
         swap_out(oldestPFN);
        
        return 1;
    }

    return 0;
}

int ku_proc_init(int argc, char *argv[]) {
    /*
       input file을 읽어서 각 프로세스의 정보들을 pcb에 저장합니다.
       pcb는 pcb배열 pcbArr에 저장합니다.
    */
    if (argc < 2) {
        printf("사용법: 프로그램명 <입력파일명>\n");
        return 1;
    }

    FILE *fp = fopen(argv[1], "r");
    if (fp == NULL) {
        perror("파일을 열 수 없습니다.");
        return 1;
    }

    int pnum = 0;
    char execfile[256];
    while (fscanf(fp, "%hu %255s", &(pcbArr[pnum].pid), execfile) == 2) {

        // 각 실행 파일 내용 읽기
        pcbArr[pnum].fd = fopen(execfile, "r");

        if (pcbArr[pnum].fd != NULL) {
            // 해당 pcb의 Pgdir을 초기화 한다.
            pcbArr[pnum].pgdir = (unsigned short *) malloc(DIR_SIZE * sizeof(unsigned short));
            allocate_page_frame(pcbArr[pnum].pgdir, 0);

            // 두 번째 줄에서 시작 가상 주소와 가상 주소 크기 읽기
            if (fscanf(pcbArr[pnum].fd, "%*s %hu %hu", &(pcbArr[pnum].start_vaddr), &(pcbArr[pnum].vaddr_size)) != 2) {
                // 오류 처리
                perror("Invalid File Format!\n");
            }
        } else {
            perror("실행 파일을 열 수 없습니다.");
        }
        pnum++;
        totalProcessNum++;
    }

    fclose(fp);
    return 0;
}

int ku_scheduler(unsigned short arg1) {
    // 프로세스가 하나도 없을때, return 1
    if (totalProcessNum == 0) return 1;

    // pid가 10이상일 때, current에 pid 0인 프로세스 할당
    unsigned short nowPid = arg1;
    if (nowPid >= 10) {
        current = &pcbArr[0];
        pdbr = current->pgdir;
        return 0;
    }

    // 다음 pid를 구할때, page directory가 할당 되어있고, pid가 유효한 프로세스의 pid할당.
    unsigned short nextPid = (nowPid + 1) % 10;
    while (pcbArr[nextPid].pgdir == NULL || (pcbArr[nextPid].pid == 9999)) {
        nextPid = (nextPid + 1) % 10;
    }

    if (pcbArr[nextPid].pid == 9999) {
        printf("nxt: %d, total: %d", nextPid, totalProcessNum);
        printf("해당 pid를 가진 프로세스는 존재하지 않습니다.\n");
        return 1;
    }

    // current 프로세스에 할당, pdbr에 current 프로세스의 pgdir 할당
    current = &pcbArr[nextPid];
    pdbr = current->pgdir;
    printf("\ncontext switch from pid: %d -> to pid: %d\n", nowPid, current->pid);
    return 0;
}

int ku_pgfault_handler(unsigned short arg1) {
    unsigned short nowVa = arg1;  // 입력으로 들어온 virtual address
    int lowBound = current->start_vaddr;   // 현재 프로세스의 시작 주소
    int highBound = current->start_vaddr + current->vaddr_size - 1; // 현재 프로세스의 bound 주소

    // 입력으로 들어온 가상주소가 유효하지 않을때, 1을 return 한다.
    if (nowVa < lowBound || nowVa > highBound) {
        // printf("범위를 넘어서는 가상 주소입니다!\n");
        return 1;
    }

    int directory_index = (nowVa & 0xFFC0) >> 11;
    unsigned short *nowPde = pdbr + directory_index;

    // pde의 present bit이 0이라면 pde에 page frame을 할당해준다.
    if (!(*nowPde & 0x1)) {
        if (allocate_page_frame(nowPde, 1) == -1) return 1;
        printf("allocate page table! pde: %p, pde val: %hu\n", nowPde, *nowPde);
    }

    int PFN = (*nowPde & 0xFFF0) >> 4;
    unsigned short *ptbr = (unsigned short *) (pmem + (PFN << 6));
    int table_index = (nowVa & 0x07C0) >> 6;
    unsigned short *nowPte = ptbr + table_index;

    // pde의 present bit이 0이라면 pde에 page frame을 할당해준다.
    if (!(*nowPte & 0x1)) {
        if((*nowPde) & 0x1) {
         if(pageFrameArr[PFN].loadIndex == 1) {
            // printf("valid pte num +++!!!!!%d,\n", PFN);
            pageFrameArr[PFN].validPteNum++;
         }
        }
        if (allocate_page_frame(nowPte, 2) == -1) return 1;
        printf("allocate page! pte: %p, pte val: %hu\n", nowPte, *nowPte);
    }

    return 0;
}

int ku_proc_exit(unsigned short arg1) {
    /*
       'e' instruction에 대해 exit을 진행할 pid를 입력받고 해당 pid를 찾은 뒤 메모리 해제
    */
    unsigned short exitPid = arg1;
    int validPid = 0;

    for (int i = 0; i < 10; i++) {
        if (pcbArr[i].pid != exitPid) continue;

        if (pcbArr[i].fd != NULL) {
            fclose(pcbArr[i].fd);
            pcbArr[i].fd = NULL;
        }

        if (pcbArr[i].pgdir != NULL) {
            free(pcbArr[i].pgdir);
            pcbArr[i].pgdir = NULL;
        }

        reapAllocEntryArr(&pcbArr[i].allocEntryArr);

        printf("free pid: %d\n", exitPid);
        totalProcessNum--;
        validPid = 1;
        break;
    }

    if (!validPid) {
        // exit이 일어나지 않음
        printf("Invalid Pid!!\n");
        return 1;
    }

    if (totalProcessNum == 0) {
        // 모든 프로세스 종료!
        free(pcbArr);
        printf("모든 프로세스 종료!\n");
        return -1;
    }

    return 0;
}