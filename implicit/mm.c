/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
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
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))
/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* 
 * mm_init - initialize the malloc package.
 */


static void init_prologue(void* start_addr) {
    PUT(start_addr + WSIZE, PACK(DSIZE,1));
    PUT(start_addr + 2*WSIZE, PACK(DSIZE,1));
    return;
}
static void init_epilogue(void* start_addr){
    PUT(start_addr + 3*WSIZE, PACK(1,0));
}
static void init_alignment_padding(void* start_addr){
    PUT(start_addr, 0);
}
static void init_header_and_footer(void* bp, size_t size, int allocate){
    PUT(HDRP(bp), PACK(size,allocate));
    PUT(FTRP(bp), PACK(size,allocate));
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


static int is_previous_and_next_allocated(void *previous_footer, void *next_header){
    return GET_ALLOC(previous_footer) && GET_ALLOC(next_header);
}
static int is_next_unallocated(void *previous_footer, void *next_header){
    return GET_ALLOC(previous_footer) && !GET_ALLOC(next_header);
}
static int is_previous_unallocated(void *previous_footer, void *next_header){
    return !GET_ALLOC(previous_footer) && GET_ALLOC(next_header);
}
static int is_previous_and_next_unallocated(void *previous_footer, void *next_header){
    return !GET_ALLOC(previous_footer) && !GET_ALLOC(next_header);
}

static void *coalesce(void *bp){
    char* previous_footer = FTRP(PREV_BLKP(bp));
    char* next_header = HDRP(NEXT_BLKP(bp));
    size_t size = GET_SIZE(HDRP(bp));
    if(is_previous_and_next_allocated(previous_footer, next_header)){
        return bp;
    }
    if(is_next_unallocated(previous_footer, next_header)){
        size += GET_SIZE(next_header);
        PUT(HDRP(bp), PACK(size,0));
        PUT(FTRP(bp), PACK(size,0));
        return bp;
    }
    if(is_previous_unallocated(previous_footer, next_header)){
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size,0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp);
        return bp;
    }
    if(is_previous_and_next_unallocated(previous_footer, next_header)){
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp);
        return bp;
    }
    return bp;
}

static void* extend_heap(size_t words){
    char *bp;
    size_t size;
    size = even_number_words_alignment(words);
    bp = mem_sbrk(size);
    if((long)bp == -1){
        return NULL;
    }
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));

    return coalesce(bp);
}

int mm_init(void){

    char* start_addr = mem_sbrk(4*WSIZE);
    if (is_heap_over_flow(start_addr)) {
        return -1;
    }
    init_alignment_padding(start_addr);
    init_prologue(start_addr);
    init_epilogue(start_addr);
    start_addr += 2*WSIZE;
    if (is_heap_over_flow(extend_heap(CHUNKSIZE/WSIZE))){
        return -1;
    }
    return 0;
}


size_t define_adjust_size(size_t size){
    if (size <= DSIZE){
        return 2*DSIZE;
    }
    return DSIZE * (((size + (DSIZE) + (DSIZE - 1)) / DSIZE));
}

void *find_with_first_fit(size_t adjust_size){
    char* start_address = mem_heap_lo() + DSIZE;
    while(1){
        if(start_address > (char*)mem_heap_hi()){
            
            return NULL;
        }
        if(!GET_ALLOC(HDRP(start_address)) && GET_SIZE(HDRP(start_address)) >= adjust_size){
            return (void *) start_address;
        }
        start_address = NEXT_BLKP(start_address);
    }
}
void place(char* bp, size_t adjust_size){
    size_t total_size = GET_SIZE(HDRP(bp));
    init_header_and_footer(bp, adjust_size, 1);
    if (total_size == adjust_size){
        return;
    }
    init_header_and_footer(NEXT_BLKP(bp), total_size-adjust_size, 0);
}
int find_right_fit(void* bp){
    return bp != NULL;
}
/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size){
    size_t adjust_size;
    size_t extend_size;
    char *bp;
    if (size == 0){
        return NULL;
    }
    adjust_size = define_adjust_size(size);
    bp = find_with_first_fit(adjust_size);
    if (find_right_fit(bp)){
        place(bp, adjust_size);
        return (void *) bp;
    }
    extend_size = MAX(adjust_size, CHUNKSIZE);
    bp = extend_heap((extend_size/WSIZE));
    if (is_heap_over_flow(bp)){
        return NULL;
    }
    place(bp, adjust_size);
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














