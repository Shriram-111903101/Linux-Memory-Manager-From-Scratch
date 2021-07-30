#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>       //for getpagesize
#include<memory.h>
#include<stddef.h>
#include<sys/mman.h>    //for mmap and munmap
#include<string.h>
#include "HMM.h"




int main(){

    typedef struct student{
        int a;
        int arr[50];
        char c;
        float f;
        struct student *next;
    }student;




    size_t p = getpagesize();
    printf("page size : %ld", p);
    printf("\nMax no. of struct info in a page : %zu\n", MAX_INFOS);
    printf("size of struct student = %ld\n", sizeof(struct student));

    printf("size of struct node = %ld\n", sizeof(struct node));
    printf("size of struct vm_page = %ld\n", sizeof(struct vm_page));
    printf("size of struct block_info = %ld", sizeof(struct block_info));

    printf("\n");

    reg_struct("student", sizeof(student));
    student *s = mycalloc("student", 1);

    printf("Showing that the structure members are initialized to zero:\n");
    printf(" s->arr[0] = %d ", s->arr[5]);
    printf("\n a = %d",s->a);
    printf("\n f = %.2f", s->f);
    if(!s->next)
        printf("\nnext pointer is initialized to NULL\n");
    else
        printf("NOT NULL");





    return 0;
}
