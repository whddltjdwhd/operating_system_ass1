#include <string.h>
#define ADDR_SIZE 16
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

struct pageFrame
{
   int isPageDirectory;
   int isAllocated;
   int index;
};

struct swapFrame
{
	int isAllocated;
   int index;
};


struct allocatedEntry
{
   int PFN;
   int index;
   int status; // 1: in phisycal memory, 0: in swap space
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
      printf("PFN: %d, entry: %hu index: %d\n", head->PFN, *(head->entry), head->index);
      head = head->next;
   }
}

void changeAllocEntryStatus(struct allocatedEntry *head, unsigned short *entry) {
   while (head != NULL) {
      printf("PFN: %d, entry: %hu index: %d\n", head->PFN, *(head->entry), head->index);

      head = head->next;
   }
}

void reapAllocEntryArr(struct allocatedEntry **head) {
   struct allocatedEntry *it, *nextNode;
   it = *head;
   while (it != NULL) {
      // printf("Delete PFN: %d, entry: %hu\n", it->PFN, *(it->entry));
      if(pageFrameArr[it->PFN].isAllocated == 1) {
        //  printf("reaped PFN: %d\n", it->PFN);
         pageFrameArr[it->PFN].isAllocated = 0;
      } else {
         printf("something error!\n");
         return;
      }
      nextNode = it->next;
      free(it);
	//   allocatedPageNum--;
      it = nextNode;
   }
   *head = NULL;
}

void ku_freelist_init(){
   pcbArr = (struct pcb*) malloc(10 * sizeof(struct pcb));
   // page frame을 관리하는 pageFrameArr를 pfnum(1 << 12) 만큼 할당
   pageFrameArr = (struct pageFrame *)malloc(pfnum * sizeof(struct pageFrame));

   // swap frame을 관리하는 swapFrameArr를 sfnum(1 << 14) 만큼 할당
   swapFrameArr = (struct swapFrame *)malloc(sfnum * sizeof(struct swapFrame));

   // pcb array의 초기 pid는 9999로 설정
   for(int i = 0; i < 10; i++) {
      pcbArr[i].pid = 9999;
   }
}

void addEntryIntoEntryArr(unsigned short* entry, int PFN) {
   struct allocatedEntry *newEntry = (struct allocatedEntry *)malloc(sizeof(struct allocatedEntry));
   if (newEntry == NULL) {
        // 메모리 할당 실패 처리
      perror("Memory allocation failed for allocatedEntry.\n");
      return;
   }

   newEntry->entry = entry;
   newEntry->PFN = PFN;
   newEntry->index = allocatedPageNum;
   newEntry->status = 1;

   if(current->procsAllocatedPageNum == 0) {
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


int swap_in(unsigned short *entry) {
	int i = 0;
	for(int i = 0; i < sfnum; i++) {
		
	}
}

int swap_out(unsigned short *entry) {
	int i = 0;
	for(int i = 0; i < sfnum; i++) {
		if(swapFrameArr[i].isAllocated) continue;

		// 스왑 공간 꽉차서 할당 못하는 경우도 생각해야함, free도 올바르게 하는지 확인 필요
		swapFrameArr[i].isAllocated = 1;
		*entry &= 0x0; // present bit -> 0

		// 해당하는 entry 관리 배열에서 entry를 찾고 status를 0으로 만들어줌
	}
}

void evict_page(int evictPFN) {
	pageFrameArr[evictPFN].isAllocated = 0;
	unsigned short *evictEntry = (unsigned short*)(pmem + (evictPFN << 6));

	swap_out(evictEntry);
}

int find_oldest_page() {
	int oldestPageIndex = 1e9;
	for(int i = 0; i < pfnum; i++) {
		if(pageFrameArr[i].isPageDirectory) continue;
		
		if(pageFrameArr[i].index < oldestPageIndex) oldestPageIndex = pageFrameArr[i].index ;
	}
	return oldestPageIndex;
}

int allocate_page_frame(unsigned short *entry, int isPgdir) {
   /* 
      입력으로 주어진 엔트리(pde, pte, page directory)에 대해 page frame을 할당한다.
      isPgdir 변수는 page directory의 경우 present bit 처리를 해주지 않기 때문에 이를 구별하는 변수이다.
   */

   int i = 0, isAllocated = 0;
   for(i = 0; i < pfnum; i++) {
      if(!pageFrameArr[i].isAllocated) { // 빈 페이지 프레임을 찾음
	//   printf("allocated number: %d, is page dir?: %d\n", i, isPgdir);
         pageFrameArr[i].isAllocated = 1; // 페이지 프레임을 사용 중으로 표시
         clearPageFram(i);

         if(isPgdir == 0) {
            *entry = (i << 4) | 0x1; // present bit 설정
         } 

         // pde, pte의 경우 isPageDirectoory를 0으로 설정
         pageFrameArr[i].isPageDirectory = isPgdir;

         if(!isPgdir) {
            // 현재 pcb에서 page entry 관리하는 로직
            current->procsAllocatedPageNum++;
            addEntryIntoEntryArr(entry, i);
         }
			pageFrameArr[i].index = allocatedPageNum++;

         isAllocated = 1;
         break;
      }
   }

   if(!isAllocated) { 
      // 빈 페이지 프레임을 찾지 못한 경우 swapping!
    //   printf("할당 가능한 페이지 프레임이 없습니다.\n");
	int swapOutPFN = find_oldest_page();
	// unsigned short *evictEntry = (unsigned short*)(pmem + (swapOutPFN << 6));
	// printf("swap out page frame number: %d\n", swapOutPFN);
		evict_page(swapOutPFN);
	// swap_in(entry);
      return 1;
   }
   
   return 0;
}

int ku_proc_init(int argc, char *argv[]){
   /*
      input file을 읽어서 각 프로세스의 정보들을 pcb에 저장합니다.
      pcb는 pcb배열 pcbArr에 저장합니다.
   */
   if(argc < 2) {
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

      if(pcbArr[pnum].fd != NULL) {
         // 해당 pcb의 Pgdir을 초기화 한다.
         pcbArr[pnum].pgdir = (unsigned short *)malloc(DIR_SIZE * sizeof(unsigned short));
         allocate_page_frame(pcbArr[pnum].pgdir, 1);

         // 두 번째 줄에서 시작 가상 주소와 가상 주소 크기 읽기
         if(fscanf(pcbArr[pnum].fd, "%*s %hu %hu", &(pcbArr[pnum].start_vaddr),&(pcbArr[pnum].vaddr_size)) != 2) {
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

int ku_scheduler(unsigned short arg1){
   // 프로세스가 하나도 없을때, return 1
   if(totalProcessNum == 0) return 1;

   // pid가 10이상일 때, current에 pid 0인 프로세스 할당
   unsigned short nowPid = arg1;
   if(nowPid > 9) {
      current = &pcbArr[0];
      pdbr = current->pgdir;
      return 0;
   }

   // 다음 pid를 구할때, page directory가 할당 되어있고, pid가 유효한 프로세스의 pid할당.
   unsigned short nextPid = (nowPid + 1) % 10;
   while(pcbArr[nextPid].pgdir == NULL || (pcbArr[nextPid].pid == 9999)) {
      nextPid = (nextPid + 1) % 10;
   }

   if(pcbArr[nextPid].pid == 9999  || !totalProcessNum) {
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

int ku_pgfault_handler(unsigned short arg1){
   unsigned short nowVa = arg1;  // 입력으로 들어온 virtual address
   int lowBound = current->start_vaddr;   // 현재 프로세스의 시작 주소
   int highBound = current->start_vaddr + current->vaddr_size - 1; // 현재 프로세스의 bound 주소

   // 입력으로 들어온 가상주소가 유효하지 않을때, 1을 return 한다.
   if(nowVa < lowBound || nowVa > highBound) {
      // printf("범위를 넘어서는 가상 주소입니다!\n");
      return 1;
   }

   int PFN;
   int directory_index =  (nowVa & 0xFFC0) >> 11;
   int table_index = (nowVa & 0x07C0) >> 6;
   unsigned short *nowPde = pdbr + directory_index;

   // pde의 present bit이 0이라면 pde에 page frame을 할당해준다.
   if(!(*nowPde & 0x1)){
      printf("allocate page table!\n");
      if(allocate_page_frame(nowPde, 0) == -1) return 1;
   }

   PFN = (*nowPde & 0xFFF0) >> 4;
   unsigned short *ptbr = (unsigned short*)(pmem + (PFN << 6));
   table_index = (nowVa & 0x07C0) >> 6;
   unsigned short *nowPte = ptbr + table_index;

   // pde의 present bit이 0이라면 pde에 page frame을 할당해준다.
   if(!(*nowPte & 0x1)){
      printf("allocate page!\n");
      if(allocate_page_frame(nowPte, 0) == -1) return 1;
   }
   
    return 0;
}

int ku_proc_exit(unsigned short arg1){
   /*
      'e' instruction에 대해 exit을 진행할 pid를 입력받고 해당 pid를 찾은 뒤 메모리 해제
   */
   unsigned short exitPid = arg1;
   int validPid = 0;

   for(int i = 0; i < 10; i++) {
      if(pcbArr[i].pid == exitPid) {

         if(pcbArr[i].fd != NULL) {
                fclose(pcbArr[i].fd);
                pcbArr[i].fd = NULL;
            }

         if(pcbArr[i].pgdir != NULL) {
            free(pcbArr[i].pgdir);
            pcbArr[i].pgdir = NULL;
         }
		
        //  searchAllocEntryArr(pcbArr[i].allocEntryArr);
         reapAllocEntryArr(&pcbArr[i].allocEntryArr);

         printf("free pid: %d\n", exitPid);
         totalProcessNum--;
         validPid = 1;
         break;
      } 
   }

   if(!validPid) {
      // exit이 일어나지 않음
      printf("Invalid Pid!!\n");
      return 1;
   }

   if(totalProcessNum == 0) {
      // 모든 프로세스 종료!
      free(pcbArr);
      printf("모든 프로세스 종료!\n");
      return -1;
   }
 
   return 0;
}