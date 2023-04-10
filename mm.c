

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
#define PREP(bp) ((char *)(bp) + WSIZE)
#define SUCC(bp) PREP(bp) - WS

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))
/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

static unsigned int* root_pointer;
static unsigned int* last_pointer;

static void init_prologue(void* start_addr) {
    PUT(start_addr + WSIZE, PACK(2*DSIZE,1));
    PUT(start_addr + 2* WSIZE, start_addr + 6*WSIZE);
    PUT(start_addr + 3*WSIZE, 0);
    PUT(start_addr + 4*WSIZE, PACK(2*DSIZE,1));
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
static void  place_free_pointer_link(void* bp){
    unsigned int* start_address = mem_heap_lo() + DSIZE;
    while(1){
        if (*start_address == bp){
            PUT(PREP(bp), (void*)start_address + WSIZE);
            return;
        }
        if (*start_address > (unsigned int)bp){
            PUT(PREP(*start_address), (unsigned int*)PREP(bp));
            PUT(bp, GET(start_address));
            PUT(PREP(bp), (unsigned int*)(PREP(start_address)));
            PUT(start_address, (unsigned int*)bp);
            return;
        }
        start_address = GET(start_address);
    }
}

static void *coalesce(void *bp){
    unsigned int* successor_address;
    unsigned int* predecessor_address;
    char* previous_footer = FTRP(PREV_BLKP(bp));
    char* next_header = HDRP(NEXT_BLKP(bp));
    size_t size = GET_SIZE(HDRP(bp));
    if(is_previous_and_next_allocated(previous_footer, next_header)){
        place_free_pointer_link(bp);
        return bp;
    }
    if(is_next_unallocated(previous_footer, next_header)){
        successor_address = GET(NEXT_BLKP(bp));
        predecessor_address = GET(PREP(NEXT_BLKP(bp)));
        size += GET_SIZE(next_header);
        PUT(bp, successor_address);
        PUT(PREP(bp), predecessor_address);
        PUT((char*)predecessor_address - WSIZE, bp);
        PUT(PREP(successor_address), PREP(bp));
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
        successor_address = GET(NEXT_BLKP(bp));
        PUT(HDRP(PREV_BLKP(bp)), successor_address);
        PUT(PREP(successor_address), PREP(HDRP(PREV_BLKP(bp))));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp);
        return bp;
    }
    return bp;
}

static void* extend_heap(size_t words){
    unsigned int *bp;
    size_t size;ls
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
    unsigned int* start_addr = mem_sbrk(6*WSIZE);
    if (is_heap_over_flow(start_addr)) {
        return -1;
    }
    init_alignment_padding(start_addr);
    init_prologue(start_addr);
    init_epilogue(start_addr);

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
    unsigned int* start_address = (unsigned int*)(mem_heap_lo() + DSIZE);
    while(1){
        if(start_address == NULL){
            *start_address = mem_heap_hi;
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
        init_header_and_footer(bp, adjust_size+DSIZE,1);
        PUT((void*)GET(PREP(bp)) - WSIZE, *bp);
        PUT(PREP(*bp), GET(PREP(bp)));
        place_free_pointer_link(bp);
        return;
    }
    init_header_and_footer(bp, adjust_size, 1);
    if (total_size - adjust_size == 0){
        PUT((void*)GET(PREP(bp)) - WSIZE, *bp);
        if ((unsigned int*)(*bp) == 0){
            return;
        }
        PUT(PREP(*bp), GET(PREP(bp)));
        return;
    }   
    init_header_and_footer(NEXT_BLKP(bp), total_size-adjust_size, 0);
    place_free_pointer_link(NEXT_BLKP(bp));

    // PUT(NEXT_BLKP(bp), GET(bp));
    // PUT(PREP(NEXT_BLKP(bp)), GET(PREP(bp)));
    // PUT((void*)GET(PREP(NEXT_BLKP(bp)))-WSIZE, (unsigned int*)NEXT_BLKP(bp));
    // if (*bp == NULL){
    //     return;
    // }
    // PUT(PREP(GET(NEXT_BLKP(bp))), (unsigned int*)PREP(NEXT_BLKP(bp)));
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
    unsigned int *bp;
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














