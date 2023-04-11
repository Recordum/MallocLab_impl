#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "mingyu",
    /* Second member's email address (leave blank if none) */
    "mingyu.oh15b@gmail.com"
};
/* Basic constants and macros */
#define WSIZE 4 /* Word and header/footer size (bytes) */
#define DSIZE 8 /* Double word size (bytes) */
#define CHUNKSIZE (1<<12) /* Extend heap by this amount (bytes) */
#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define NXRP(bp) ((char *)(bp) - WSIZE)
#define BPRP(bn) ((char *)(bn) - WSIZE)
/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

static unsigned int segregated_free_list[30];


static void init_epilogue(void* start_addr){
    PUT(start_addr + WSIZE, PACK(1,0));
}
static void init_alignment_padding(void* start_addr){
    PUT(start_addr, 0);
}
static void init_header(void* bp, size_t size){
    PUT(HDRP(bp), PACK(size,allocate));
}
static int is_heap_over_flow(void* start_addr){
    return start_addr == (void *)-1;
}
static size_t even_number_words_alignment(size_t words){
    if (words % 2){
        return (words+1)*WSIZE;
    }
    return words*WSIZE;
}

static unsigned int* find_index(size_t adjust_size){
    for (int i = 0; i < 30 ; i++){
        if (adjust_size == 1<<(i+3)){
            return &segregated_free_list[i];
        }
    }
}
static void delete_free_list(unsigned int* index, size_t adjust_size){
    if (*index == NULL){
        return NULL;
    }

    if (**index != NULL){
        init_header(*index);
        *index = **index;
    }
    
}
static void divide_chunk(size_t size, void* bp){

}
static void* extend_heap(size_t words){
    unsigned int *bp;
    size_t size;
    size = even_number_words_alignment(words);
    bp = mem_sbrk(size);
    if((long)bp == -1){
        return NULL;
    }
    PUT(HDRP(bp), PACK(size,0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));
}

int mm_init(void){
    unsigned int* start_addr = mem_sbrk(2*WSIZE);
    if (is_heap_over_flow(start_addr)) {
        return -1;
    }
    for (int i = 0; i < 9 ; i++){
        segregated_free_list[i] = NULL;
    }
    init_alignment_padding(start_addr);
    init_epilogue(start_addr);
    return 0;
}


size_t define_adjust_size(size_t size){
    if (size <= WSIZE){
        return DSIZE;
    }
    return DSIZE * (((size + WSIZE +(DSIZE - 1)) / DSIZE));
}

void *find_with_first_fit(size_t adjust_size){
    unsigned int* start_address = (unsigned int*)(mem_heap_lo() + DSIZE);
    while(1){
        if(start_address == NULL){
            return NULL;
        }
        if(!GET_ALLOC(HDRP(start_address)) && GET_SIZE(HDRP(start_address)) >= adjust_size){
            return (void *) start_address;
        }
        start_address = GET(start_address);
    }
}
void place(unsigned int* bp, size_t adjust_size){
    size_t total_size = GET_SIZE(HDRP(bp));
    if (total_size - adjust_size < 2*DSIZE){
        init_header_and_footer(bp, total_size,1);
        PUT((char *)(GET(PREP(bp)))-WSIZE, GET(bp));
        if(*bp == NULL){
            return;
        }
        PUT(PREP(GET(bp)), GET(PREP(bp)));
        return;
    }
    init_header_and_footer(bp, adjust_size, 1);
    init_header_and_footer(NEXT_BLKP(bp), total_size-adjust_size, 0);

    PUT((char *)(GET(PREP(bp)))-WSIZE, (unsigned int*)NEXT_BLKP(bp));
    
    if(*bp == NULL){
        PUT(PREP(NEXT_BLKP(bp)), GET(PREP(bp)));
        PUT(NEXT_BLKP(bp), 0);
        return;
    }
}
/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size){
    size_t adjust_size;
    size_t extend_size;
    unsigned int *index;
    unsigned int *bp;
    if (size == 0){
        return NULL;
    }
    adjust_size = define_adjust_size(size);
    index = find_index(adjust_size);
    extend_size = MAX(adjust_size, CHUNKSIZE);
    bp = extend_heap((extend_size/WSIZE));
    if (is_heap_over_flow(bp)){
        return NULL;
    }
    divide_chunk(bp, adjust_size);

    return (void*)bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr){
    size_t size = GET_SIZE(HDRP(ptr));
    init_header_and_footer(ptr, size, 0);
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size){
    if(size <= 0){ 
        mm_free(ptr);
        return 0;
    }
    if(ptr == NULL){
        return mm_malloc(size); 
    }
    void *newp = mm_malloc(size); 
    if(newp == NULL){
        return 0;
    }
    size_t oldsize = GET_SIZE(HDRP(ptr));
    if(size < oldsize){
    	oldsize = size; 
	}
    memcpy(newp, ptr, oldsize); 
    mm_free(ptr);
    return newp;
}



