#include <string.h>

struct pcb *current;
unsigned short *pdbr;
char *pmem, *swaps; // 16비트만큼 할당
int pfnum, sfnum;
int totalProcessNum;

unsigned short *pfArr, *sfArr;
int allocatedPageNum; // allocatedPageNum은 현재 물리 메모리에 할당된 페이지 수
int pageLoadIndex;   // pageLoadIndex는 몇번째로 물리 메모리에 할당되었는지 기록하는 변수이다.

void ku_dump_pmem(void);

void ku_dump_swap(void);

struct swapFrame {
   int pid; // 스왑공간으로 옮겨진 page가 속한 프로세스의 pid
   int isAllocated;  // 해당 스왑공간이 할당 되어있는지 아닌지 확인하는 변수
   int pageType; // 0: page directory, 1: page table, 2: page
   unsigned short *entry;  // 스왑공간에 할당된 entry
};

struct pageFrame {
   int isAllocated;  // page frame이 할당 되어있는지 아닌지 확인하는 변수
   int loadIndex;    // 몇번째로 물리 메모리에 할당 되었는지 기록하는 변수 이를 통해 FIFO를 구현했다.(loadIndex가 가장 낮은 페이지 eviction)
   int pageType; // 0: page directory, 1: page table, 2: page
   int pid;    // 해당 page가 속한 프로세스의 pid
};

struct allocatedEntry {
   int PFN; // 물리 메모리 몇번 page frame에 올려졌는지 확인하는 변수
   unsigned short *entry;  // 물리 메모리에 할당된 entry
   struct allocatedEntry *next;  // Linked List로 구현됐기에 다음 요소를 참조하는 포인터
};

struct pcb {
   unsigned short pid;  // process의 pid
   FILE *fd;   // process의 file descriptor
   unsigned short *pgdir;  // page directory -> 32개의 pde로 이루어짐
   unsigned short start_vaddr; // 시작 가상 주소
   unsigned short vaddr_size;  // 가상 주소 크기
   struct allocatedEntry *allocEntryArr; // 해당 프로세스의 page들 중 물리 메모리에 올려진 page들의 정보를 관리하는 리스트
};

struct pcb *pcbArr;  // pcb 구조체 배열
struct pageFrame *pageFrameArr;  // pageFrame을 관리하는 배열
struct swapFrame *swapFrameArr;  // swapFrame을 관리하는 배열

void search_alloc_etnryArr(struct allocatedEntry *head) {
   /*
      물리 메모리에 올려진 프로세스의 page들을 모두 탐색
   */
   while (head != NULL ) {
      printf("PFN: %d, entry: %hu\n", head->PFN, *(head->entry));
      head = head->next;
   }
}

unsigned short* get_alloc_entryArr(struct allocatedEntry **headRef, int PFN) {
   /*
      매개변수로 주어진 PFN과 동일한 PFN값을 가지는 entry를 찾고 해당 엔트리를 allocEntryArr에서 삭제하고
      해당 entry를 반환한다.
   */
   struct allocatedEntry *pre = NULL, *head;
   unsigned short* ret = NULL; // 명시적으로 NULL로 초기화
   head = *headRef;
   while (head != NULL) {
      if(PFN == head->PFN) {

         ret = head->entry;

         if (pre == NULL) {
             // pre가 NULL이면, head는 리스트의 첫 번째 노드임
             // 따라서 headRef를 업데이트하여 head의 다음 노드를 새로운 첫 번째 노드로 설정
             *headRef = head->next;
         } else {
             // 아니라면, pre의 next를 head의 다음으로 설정하여 리스트에서 head를 제거
             pre->next = head->next;
         }

         free(head); // head 메모리 해제
         return ret; // 찾은 엔트리 반환
      }
      pre = head; 
      head = head->next; 
   }
   return NULL; 
}

void reap_alloc_entryArr(struct allocatedEntry **head) {
   /*
      매개변수로 주어진 allocatedEntry리스트의 요소들을 모두 삭제한다.
   */
   struct allocatedEntry *it, *nextNode;
   it = *head;
   while (it != NULL) {

      if (pageFrameArr[it->PFN].isAllocated != 1) {
         printf("할당되지 않은 페이지를 삭제하려 합니다!!\n");
         return;
      }

      allocatedPageNum--;
      pageFrameArr[it->PFN].isAllocated = 0;
      
      nextNode = it->next;
      free(it);
      it = nextNode;
   }
   *head = NULL;
}

void ku_freelist_init() {
   /*
      pcb, page frame, swap frame을 관리하는 배열들 동적할당
   */
   // pcb를 관리하는 pcb arr를 10만큼 할당
   pcbArr = (struct pcb *) malloc(10 * sizeof(struct pcb));
   // page frame을 관리하는 pageFrameArr를 pfnum 만큼 할당
   pageFrameArr = (struct pageFrame *) malloc(pfnum * sizeof(struct pageFrame));
   // swap frame을 관리하는 swapFrameArr를 sfnum 만큼 할당
   swapFrameArr = (struct swapFrame *) malloc(sfnum * sizeof(struct swapFrame));

   // pcb array의 초기 pid는 9999로 설정
   for (int i = 0; i < 10; i++) {
      pcbArr[i].pid = 9999;
   }
}

void add_entry_into_entryArr(unsigned short *entry, int PFN, int pnum) {
   /*
      매개변수로 주어진 entry를 entry관리 리스트 allocEntryArr에 추가하는 함수
      pnum이 -1인 경우는 entry가 pte, pde 이고 0인 경우는 page directory이다.
   */
   struct allocatedEntry *newEntry = (struct allocatedEntry *) malloc(sizeof(struct allocatedEntry));
   if (newEntry == NULL) {
      // 메모리 할당 실패 처리
      perror("Memory allocation failed for allocatedEntry.\n");
      return;
   }

   newEntry->entry = entry;
   newEntry->PFN = PFN;

   if(pnum == -1) {
      // pte 및 pde
      newEntry->next = current->allocEntryArr;
      current->allocEntryArr = newEntry;
   } else {
      // page directory
      newEntry->next = pcbArr[pnum].allocEntryArr;
      pcbArr[pnum].allocEntryArr = newEntry;
   }

   pageLoadIndex++;
   allocatedPageNum++;
}

void clear_page_frame(int PFN) {
   /*
      PFN번째 Page Frame의 값을 0으로 초기화 시켜주는 함수이다.
   */
   char *basePageFrame = pmem + (PFN << 6);
   memset(basePageFrame, 0, 64);
}

int check_allocated_page(int PFN) {
   /*
      주어진 PFN을 통해서 page table을 구한다.
      해당 page table의 유효한 pte의 개수를 반환한다.
   */
   int validPageCount = 0;
   unsigned short *ptbr = (unsigned short*)(pmem + (PFN << 6)); // PFN을 이용해 ptbr 계산

   for(int i = 0; i < 32; i++) {
       if(ptbr[i] & 0x1) validPageCount++; // 해당 페이지가 유효하다면 카운트 증가
   }

   return validPageCount;
}

int find_evictionpage() {
   /*
      eviction할 page frame을 찾고 해당 PFN을 반환한다.
   */
   int ret = 1e9, evictPFN = 0; // ret를 매우 큰 값으로 초기화

   for (int i = 0; i < pfnum; i++) {
      if(!pageFrameArr[i].pageType) continue; // page directory라면 건너뛰기

      int pdeValidCount = 0;
      if(pageFrameArr[i].pageType == 1) pdeValidCount = check_allocated_page(i); // 해당 PFN이 page table이라면 유효한 페이지 수 확인

      if (pageFrameArr[i].loadIndex < ret && !pdeValidCount) { 
         // 유효한 페이지가 없고, 현재까지 찾은 것 중 가장 작은 loadIndex라면 값을 갱신
         ret = pageFrameArr[i].loadIndex;
         evictPFN = i;
      }
   }

   return evictPFN; // 유효한 페이지가 없는 페이지 프레임의 PFN 반환
}

struct pcb* get_pcb_byPid(int pid) {
   /*
      매개변수로 주어진 pid를 가지는 pcb 구조체를 반환한다.
   */
   for(int i = 0; i < 10; i++) {
      if(pcbArr[i].pid == pid) {
         return &pcbArr[i];
      }
   }
   return NULL;
}

int swap_out(int PFN) {
   /*
      swap out을 실행하는 함수.
      swap space에서 비어있는 공간을 찾고 eviction할 page frame을 옮겨준다.
   */
   int i = 1, isChecked = 0;  
   for(int i = 1; i <= sfnum; i++) {
      if(swapFrameArr[i].isAllocated) continue;
      // swap page 할당
      swapFrameArr[i].isAllocated = 1;
      // page frame 해제
      pageFrameArr[PFN].isAllocated = 0;

      // eviction할 page의 pid 및 해당 pid값을 가진 pcb를 구함
      int evictPFNpid = pageFrameArr[PFN].pid;
      struct pcb *evictFrameProc = get_pcb_byPid(evictPFNpid);

      // eviction할 page를 PFN을 통해 찾음
      unsigned short *entry = get_alloc_entryArr(&evictFrameProc->allocEntryArr, PFN);
      if(!entry) {
         printf("error! no such entry in memory\n");
         return 0;
      }
      
      swapFrameArr[i].entry = entry;
      swapFrameArr[i].pid = evictPFNpid;
      swapFrameArr[i].pageType = pageFrameArr[PFN].pageType;

      // i의 값을 PFN 부분에 설정하면서 
      //present 비트를 0으로, dirty bit는 유지
      *entry = (i << 2) | (*entry & 0x2);
      isChecked = 1;
      break;
   }

   return (isChecked == 1);
}

int allocate_page_frame(unsigned short *entry, int pageType) {
   /*
      입력으로 주어진 엔트리(pde, pte, page directory)에 대해 page frame을 할당한다.
      pageType 변수는 page directory의 경우 present bit 처리를 해주지 않기 때문에 이를 구별하는 변수이다.
   */

   int isAllocated = 0; // 할당이 됐는지 확인하는 flag 변수

   for (int i = 0; i < pfnum; i++) {
      if (pageFrameArr[i].isAllocated != 0) continue;
      
      pageFrameArr[i].isAllocated = 1; // 페이지 프레임을 사용 중으로 표시
      clear_page_frame(i); // 사용할 page를 청소

      // 해당 page frame의 pageType을 설정(0: page directory, 1: pde, 2: pte)
      pageFrameArr[i].pageType = pageType;

      // pde 및 pde인 경우
      if (pageType > 0) {
         *entry = (i << 4) | (*entry & 0x2) | 0x1; // PFN, present bit, dirty bit 설정
         add_entry_into_entryArr(entry, i, -1); // entry관리 리스트에 entry를 추가한다.
      }

      // 할당된 page frame의 loadIndex 및 pid 설정
      pageFrameArr[i].loadIndex = pageLoadIndex;
      pageFrameArr[i].pid = current->pid;
      isAllocated = 1;
      break;
   }

   if (!isAllocated) {
      // 빈 페이지 프레임을 찾지 못한 경우 swapping 발생
      int evictPFN = find_evictionpage();   //
      int evictPageType = pageFrameArr[evictPFN].pageType;
      int evictPid = pageFrameArr[evictPFN].pid;

      /*
         현재 물리 메모리가 꽉 찬 상태라면 eviction 대상 페이지가 page table이고, 할당될 page가 page일때
         그리고 eviction대상 페이지의 pid와 할당될 page의 pid가 서로 같은 경우 할당할 수 없으므로 1을 반환한다.

         즉, page를 할당하려 하는데 물리 메모리엔 page directory 또는 page table(유효한 pte를 가지고 있는)들 밖에 없는 것이다.
         그렇게 되면 page를 할당할 수 없게 된다!
      */
      if((evictPageType == 1 && pageType == 2) && (evictPid == current->pid) && (allocatedPageNum == pfnum)) {
         return 1;
      }   

      // swap out
      if(swap_out(evictPFN) == 0) return 1;
      
      // swap out 성공시 eviction이 된 page에 page type, 할당 체크 및 clear
      pageFrameArr[evictPFN].isAllocated = 1;
      pageFrameArr[evictPFN].pageType = pageType;
      clear_page_frame(evictPFN);

      // page type이 page table 또는 page일때
      if (pageType > 0) {
          *entry = (evictPFN << 4) | (*entry & 0x2) | 0x1; // present bit, dirty bit 설정
         add_entry_into_entryArr(entry, evictPFN, -1);
         pageFrameArr[evictPFN].loadIndex = pageLoadIndex;
         pageFrameArr[evictPFN].pid = current->pid;   
      }
   }

   return 0;
}

void ku_proc_init(int argc, char *argv[]) {
   /*
      input file을 읽어서 각 프로세스의 정보들을 pcb에 저장한다.
      pcb는 pcb배열 pcbArr에 저장한다.
   */
   if (argc < 2) {
      printf("사용법: 프로그램명 <입력파일명>\n");
      return;
   }

   FILE *fp = fopen(argv[1], "r");
   if (fp == NULL) {
      perror("파일을 열 수 없습니다.");
      return;
   }

   int pnum = 0;
   char execfile[256];
   while (fscanf(fp, "%hu %255s", &(pcbArr[pnum].pid), execfile) == 2) {

      // 각 실행 파일 내용 읽기
      pcbArr[pnum].fd = fopen(execfile, "r");
      if (pcbArr[pnum].fd != NULL) {
         // 해당 pcb의 Pgdir을 초기화 한다.
         pcbArr[pnum].pgdir = (unsigned short *) malloc(32 * sizeof(unsigned short));

         // page directory를 page frame에 할당하는 로직
         for(int i = 0; i < pfnum; i++) {
            if(pageFrameArr[i].isAllocated) continue;

            pageFrameArr[i].isAllocated = 1;
            clear_page_frame(i);

            add_entry_into_entryArr(pcbArr[pnum].pgdir, i, pnum);
            break;
         }

         pageFrameArr[pnum].pid = pcbArr[pnum].pid;

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
}

int ku_scheduler(unsigned short arg1) {
   /*
      스케쥴러 함수로써 Round-Robin형식으로 작동한다.
   */

   // 프로세스가 하나도 없을때, return 1
   if (totalProcessNum == 0) {
      return 1;
   }

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

   // current 프로세스에 할당, pdbr에 current 프로세스의 pgdir 할당
   current = &pcbArr[nextPid];
   pdbr = current->pgdir;

   return 0;
}

int swap_in(unsigned short *entry) {
   /*
      swap in 함수로써 swap sapce에서 해당 entry를 찾고 allocate_page_frame을 통해
      물리 메모리에 할당한다.
   */
   int PFN = (*entry >> 2);
   
   if(swapFrameArr[PFN].isAllocated) {
      swapFrameArr[PFN].isAllocated = 0;

      if(allocate_page_frame(entry, swapFrameArr[PFN].pageType) == 1){
         return 1;
      }
   }

   return 0;
}

int ku_pgfault_handler(unsigned short arg1) {
   /*
      page fault를 발생시 호출되며, 물리 메모리의 빈 page frame에 page directory, page table, page들을 할당한다.
      만약 물리 메모리가 꽉 찼다면 swap out을 진행하고, swap out되어있는 페이지에 접근할시 swap in을 진행한다.
   */
   unsigned short nowVa = arg1;  // 입력으로 들어온 virtual address
   int lowBound = current->start_vaddr;   // 현재 프로세스의 시작 주소
   int highBound = current->start_vaddr + current->vaddr_size - 1; // 현재 프로세스의 bound 주소

   // 입력으로 들어온 가상주소가 유효하지 않을때, 1을 return 한다.
   if (nowVa < lowBound || nowVa > highBound) {
      return 1;
   }

   int directory_index = (nowVa & 0xFFC0) >> 11;
   unsigned short *nowPde = pdbr + directory_index;

   // pde의 present bit이 0이라면 page frame을 할당할지, swap in을 해줄지 확인
   if (!(*nowPde & 0x1)) {
      if(*nowPde == 0) {
         // nowPde가 0이라면 page frame 할당
         if (allocate_page_frame(nowPde, 1) == 1) return 1;
      } else {
         // nowPde의 값이 0이 아니라면 swap in!
         if(swap_in(nowPde) == 1) return 1;
      }
   }

   int PFN = (*nowPde & 0xFFF0) >> 4;
   unsigned short *ptbr = (unsigned short *) (pmem + (PFN << 6));
   int table_index = (nowVa & 0x07C0) >> 6;
   unsigned short *nowPte = ptbr + table_index;

   // pte의 present bit이 0이라면 page frame을 할당할지, swap in을 해줄지 확인
   if (!(*nowPte & 0x1)) {
      if(*nowPte == 0) {
         // nowPte가 0이라면 page frame 할당
         if (allocate_page_frame(nowPte, 2) == 1) return 1;
      } else {
         // nowPte의 값이 0이 아니라면 swap in!
         if(swap_in(nowPte) == 1) return 1;
      }
   } 

   return 0;
}

void reap_swap_entry(int pid) {
   /*
      pid를 통해 swap space에 할당된 page들을 reaping해준다.
   */
   for(int i = 1; i <= sfnum; i++) {
      if(!swapFrameArr[i].isAllocated) continue;

      if(swapFrameArr[i].pid == pid) {
         swapFrameArr[i].pid = 0;
         swapFrameArr[i].entry = NULL;
         swapFrameArr[i].pageType = 0;
         swapFrameArr[i].isAllocated = 0;
      }
   }
}

int ku_proc_exit(unsigned short arg1) {
   /*
      'e' instruction에 대해 exit을 진행할 pid를 입력받고 해당 pid를 찾은 뒤 메모리 해제
   */
   unsigned short exitPid = arg1;
   int validPid = 0;

   for (int i = 0; i < 10; i++) {
      if (pcbArr[i].pid != exitPid) continue;

      // 파일 디스크립터 닫기
      if (pcbArr[i].fd != NULL) {
         fclose(pcbArr[i].fd);
         pcbArr[i].fd = NULL;
      }

      // page directory 메모리 해제
      if (pcbArr[i].pgdir != NULL) {
         free(pcbArr[i].pgdir);
         pcbArr[i].pgdir = NULL;
      }

      // 할당된 page들, swap out된 page들 모두 메모리 해제
      reap_alloc_entryArr(&pcbArr[i].allocEntryArr);
      reap_swap_entry(exitPid);

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
      free(pageFrameArr);
      free(swapFrameArr);
   }

   return 0;
}