#include <stdio.h>
#include <stdlib.h>
#include "kumoo.h"

#define SCHED	0
#define PGFAULT	1
#define EXIT	2
#define TSLICE	5

struct handlers{
       int (*sched)(unsigned short);
       int (*pgfault)(unsigned short);
       int (*exit)(unsigned short);
}kuos;

void ku_dump_pmem(void){
    for(int i = 0; i < (64 << 12); i++){
        printf("%x ", pmem[i]);
    }
    printf("\n");
}
void ku_dump_swap(void){
    for(int i = 0; i < (64 << 14); i++){
        printf("%x ", swaps[i]);
    }
    printf("\n");
}

void ku_reg_handler(int flag, int (*func)(unsigned short)){
	switch(flag){
		case SCHED:
			kuos.sched = func;
			break;
		case PGFAULT:
			kuos.pgfault = func;
			break;
		case EXIT:
			kuos.exit = func;
			break;
		default:
			exit(0);
	}
}

int ku_traverse(unsigned short va){
	int pd_index, pt_index, pa;
    unsigned short *ptbr;
	unsigned short *pte, *pde;
    int PFN;

	pd_index = (va & 0xFFC0) >> 11;
    printf("pdeIndex: %d, pdbr: %p, pmem: %p\n", pd_index, pdbr, pmem);
	pde = pdbr + pd_index;
    printf("pde: %p\n", pde);
    printf("pde val: %hu\n", *pde);

	if(!*pde) return -1;
    printf("PDE 검사 통과!~ pde: %hu\n", *pde);

    PFN = (*pde & 0xFFF0) >> 4;
    ptbr = (unsigned short*)(pmem + (PFN << 6));

    pt_index = (va & 0x07C0) >> 6;
    pte = ptbr + pt_index;
    printf("PFN: %d, pte: %p\n", PFN, pte);
     printf("ptbr: %p pte val: %hu\n",ptbr, *pte);
    if(!*pte)
        return -1;

    PFN = (*pte & 0xFFF0) >> 4;

    pa = (PFN << 6)+(va & 0x3F);
    printf("pa: %d\n", pa);


	return pa;
}


void ku_os_init(void){
    /* Initialize physical memory*/
    pfnum = 1 << 12;
    sfnum = 1 << 14;
    pmem = (char*)malloc(64 << 12);
    swaps = (char*)malloc(64 << 14);
    /* Init free list*/
    ku_freelist_init();
    /*Register handler*/
	ku_reg_handler(SCHED, ku_scheduler);
	ku_reg_handler(PGFAULT, ku_pgfault_handler);
	ku_reg_handler(EXIT, ku_proc_exit);
}

void op_read(){
    unsigned short va;
    int addr, pa, ret = 0;
    char sorf = 'S';
    /* get Address from the line */
    if(fscanf(current->fd, "%d", &addr) == EOF){
        /* Invaild file format */
        return;
    }
    va = addr & 0xFFFF;
    printf("va: %hu\n", va);
    pa = ku_traverse(va);

    if (pa < 0){
        /* page fault!*/
        printf("page fault!\n");
        ret = kuos.pgfault(va);
    	if (ret > 0){
		/* No free page frames or SEGFAULT */
		sorf = 'E';
		ret = kuos.exit(current->pid);
            if (ret > 0){
                /* invalid PID */
                printf("invalid PID\n");
                return;
            }
        }
        else {
            printf("retry!\n");
            pa = ku_traverse(va);
            sorf = 'F';
        }
    } 

    if (pa < 0){
        printf("not exists pa %d: %d -> (%c)\n", current->pid, va, sorf);
    }
    else {
        printf("%d: %d -> %d (%c)\n", current->pid, va, pa, sorf);
    }
  
}

void op_write(){
    unsigned short va;
    int addr, pa, ret = 0;
    char input ,sorf = 'S';
    /* get Address from the line */
    if(fscanf(current->fd, "%d %c", &addr, &input) == EOF){
        /* Invaild file format */
        return;
    }
    va = addr & 0xFFFF;
    pa = ku_traverse(va);

    if (pa < 0){
        /* page fault!*/
        ret = kuos.pgfault(va);
    } 
    if (ret > 0){
        /* No free page frames or SEGFAULT */
        sorf = 'E';
        ret = kuos.exit(current->pid);
        if (ret > 0){
            /* invalid PID */
            return;
        }
    }
    else {
        pa = ku_traverse(va);
        sorf = 'F';
    }

    if (pa < 0){
        printf("not exists pa %d: %d -> (%c)\n", current->pid, va, sorf);
    }
    else {
        *(pmem + pa) = input;
        printf("%d: %d -> %d (%c)\n", current->pid, va, pa, sorf);
    }

}

void do_ops(char op){
    char sorf;
    int ret;
    switch(op){
        case 'r':
            op_read();
        break;

        case 'w':
            op_write();
        break;

        case 'e':
            ret = kuos.exit(current->pid);
            if (ret > 0){
                /* invalid PID */
                return;
            }
        break;
    }

}

void ku_run_procs(void){
	unsigned char va;
    char sorf;
	int addr, pa, i;
    char op;
    int ret;

    ret = kuos.sched(10);
    printf("ret: %d\n", ret);
    /* No processes */
    if (ret > 0)
        return;
    
	do{
		if(!current)
			exit(0);

        printf("now proccess pid: %d\n", current->pid);
		for( i=0 ; i<TSLICE ; i++){
            /* Get operation from the line */
			if(fscanf(current->fd, "%c", &op) == EOF){
                /* Invaild file format */
                printf("Invalid File Format!\n");
                return;
			}
            printf("now op: %c\n", op);
            do_ops(op);
		}

		ret = kuos.sched(current->pid);
        /* No processes */
        if (ret > 0)
            return;

	}while(1);
}

int main(int argc, char *argv[]){
	/* System initialization */
	ku_os_init();
	/* Per-process initialization */
	ku_proc_init(argc, argv);
    for(int i =0; i < 3; i++){
        printf("pid: %d, start add: %hu, add length: %hu\n", pcbArr[i].pid, pcbArr[i].start_vaddr, pcbArr[i].vaddr_size);
    }
	/* Process execution */
	ku_run_procs();

	return 0;
}
