#include<stdio.h>
#include <stdbool.h>
#include<limits.h>
#include<stddef.h>
#include<unistd.h>
#include <assert.h>
#include<sys/mman.h>
#include<memory.h>
#include "HMM.h"


static page_of_node *page_head = NULL;
//static void* hsba = NULL;

// Function to request VM pages from kernel
void* req_vm_page(int no_of_pages){

    size_t size = (size_t)no_of_pages * (size_t)sysconf(_SC_PAGE_SIZE);

    char* page = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANON, 0, 0);

    // If mmap fails
    if(page == (void*)(-1))
        return NULL;

    return (void*)page;
}

// Function to return VM pages to the kernel
int return_vm_page(void **page, size_t size){
    int temp = munmap(*page, size);

    // If munmap fails, return 0
    if(temp != 0)
        return 0;
    *page = NULL;
    return 1;
}

bool isEmpty_vm_page(vm_page *v){

    if(v->b.next == NULL && v->b.prev == NULL &&
        v->b.is_free == true)
        return true;
    return false;
}

void mark_vm_page_empty(vm_page *v){

    v->b.next = v->b.prev = NULL;
    v->b.is_free = true;
    return;
}

size_t max_page_allocatable_memory(int units){

    return (size_t)((size_t)sysconf(_SC_PAGE_SIZE) * units - offsetof(vm_page, page_memory));
}

// Function to insert structure info in the vm page
void insert(page_of_node** head, char *name, size_t size){

    page_of_node *new_page = NULL;

    size_t page_size = (size_t)sysconf(_SC_PAGE_SIZE);

    // If size of structure is more than size of VM page
    if(size > page_size){
        printf("Error\n");
        return;
    }

    // If head pointer is NULL
    if(!(*head)){

        (*head) = (page_of_node*)req_vm_page(1);
        (*head)->next = NULL;

        strncpy((*head)->arr[0].name, name, 32);
        (*head)->arr[0].size = size;

        init_list_node(&((*head)->arr[0].list_head));

        return;
    }

    // Traverse in VM page
    size_t count = 0;
    while(count != MAX_INFOS && (*head)->arr[count].size != 0){
            count ++;
    }

    // If VM page is full
    if(count == MAX_INFOS){
        // Request for one more VM page
        new_page = (page_of_node*)req_vm_page(1);
        new_page->next = *head;
        *head = new_page;
        strncpy((*head)->arr[0].name, name, 32);
        (*head)->arr[0].size = size;
        init_list_node(&((*head)->arr[0].list_head));
        return;
    }
    strncpy((*head)->arr[count].name, name, 32);
    (*head)->arr[count].size = size;
    init_list_node(&((*head)->arr[count].list_head));
    return;
}

node* search_for_struct(page_of_node *head, char* name){

    if(!head)
        return NULL;
    int i = 0;
    while(i != MAX_INFOS && (head)->arr[i].size != 0){

        if(strcmp(head->arr[i].name, name) == 0)
            return &(head->arr[i]);
        i++;
    }
    return NULL;
}

vm_page* allocate_vm_page(node *n){

    vm_page* v = (vm_page*)req_vm_page(1);

    mark_vm_page_empty(v);

    //initializing all the components
    v->b.size = max_page_allocatable_memory(1);
    v->b.offset = offsetof(struct vm_page, b);
    v->next = v->prev = NULL;
    init_list_node(&(v->b.n));

    // Set the struct_info pointer
    v->struct_info = n;

    // If head is NULL
    if(!n->vm_page_head)
        n->vm_page_head = v;

    // Insert page at the beginning of linked list
    else{
        v->next = n->vm_page_head;
        n->vm_page_head->prev = v;
    }

    return v;
}


bool allocate_free_block(node *n, block_info *b, int size){

    if(!b->is_free)
        return false;

    if(b->size < size)
        return false;

    int rem_size = b->size - size;

    b->is_free = false;
    b->size = size;
    //block got used so remove from list
    list_remov(&(b->n));

    if(!rem_size)
        return true;

    if(rem_size < (sizeof(block_info) + n->size))
        return true;

    block_info *next = NULL;

    next = get_next_block_by_size(b);

    next->is_free = true;
    next->size = rem_size - sizeof(block_info);

    next->offset = b->offset + sizeof(block_info) + b->size;
    init_list_node(&(next->n));

    bind_blocks_allocation(b, next);
    list_add_free_block(n, next);

    return true;
}

vm_page* add_new_page(node* n){

    vm_page *nn = allocate_vm_page(n);

    if(!nn)
        return NULL;

    list_add_free_block(n, &(nn->b));

    return nn;
}

// getting a free block of required size
vm_page* get_satisfying_req(node *n, int req_size, block_info **b){

    bool status = false;
    vm_page *v = NULL;

    block_info *biggest = get_biggest_free_block(n);

    if(!biggest || biggest->size < req_size){

        v = add_new_page(n);

        //allocate free block from this page
        status = allocate_free_block(n, &(v->b), req_size);

        if(!status){
            *b = NULL;
            return NULL;
        }
        *b = &(v->b);
        return v;
    }
    status = allocate_free_block(n, biggest, req_size);
    if(!status){

        *b = NULL;
        return NULL;
    }

    *b = biggest;

    return ((vm_page*)((char*)biggest - biggest->offset));
}

void reg_struct(char* name, int size){

    insert(&page_head, name, size);
    return;
}

void* mycalloc(char *name, int units){


    node* n = search_for_struct(page_head, name);

    if(!n){

         printf("Error : Structure %s not registered.\n", name);
        return NULL;
    }

    if(units * n->size > max_page_allocatable_memory(1)){
        printf("Error : Memory Requested Exceeds Page Size\n");
        return NULL;
    }

    //there is not a already allocated page
    if(!n->vm_page_head){
        n->vm_page_head = add_new_page(n);

        if(allocate_free_block(n, &(n->vm_page_head->b), units*n->size)){

            memset((char*)n->vm_page_head->page_memory, 0, units*n->size);
            return(void*)n->vm_page_head->page_memory;
        }
    }

    block_info *free_block;

    get_satisfying_req(n, units*n->size, &free_block);

    if(free_block){
        if(free_block->is_free == true){
            memset((char*)(free_block + 1), 0, free_block->size);
            return (void*)(free_block + 1);
        }
    }
    return NULL;
}



