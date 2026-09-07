I'll create a progressive learning series where we build ONE memory allocator and gradually enhance it. This way you'll understand each concept deeply.

## Example 1: Basic Memory Pool (Fixed Size Blocks)

```c
#include <stdio.h>

#define POOL_SIZE 4096
#define BLOCK_SIZE 32
#define MAX_BLOCKS 128

static char memory_pool[POOL_SIZE];
static int block_used[MAX_BLOCKS] = {0};

void* my_malloc() {
    for (int i = 0; i < MAX_BLOCKS; i++) {
        if (!block_used[i]) {
            block_used[i] = 1;
            return &memory_pool[i * BLOCK_SIZE];
        }
    }
    return NULL;
}

void my_free(void* ptr) {
    if (ptr == NULL) return;
    int index = ((char*)ptr - memory_pool) / BLOCK_SIZE;
    block_used[index] = 0;
}

int main() {
    void* ptr1 = my_malloc();
    void* ptr2 = my_malloc();
    
    printf("Allocated block 1 at: %p\n", ptr1);
    printf("Allocated block 2 at: %p\n", ptr2);
    
    my_free(ptr1);
    void* ptr3 = my_malloc();
    printf("Reused freed block at: %p\n", ptr3);
    
    return 0;
}
```

## Example 2: Adding Block Metadata (Header)

```c
#include <stdio.h>

#define POOL_SIZE 4096
#define BLOCK_SIZE 64
#define MAX_BLOCKS 64

typedef struct Block {
    int size;
    int is_free;
    struct Block* next;
} Block;

static char memory_pool[POOL_SIZE];
static Block* free_list = NULL;

void init_allocator() {
    free_list = (Block*)memory_pool;
    free_list->size = POOL_SIZE - sizeof(Block);
    free_list->is_free = 1;
    free_list->next = NULL;
}

void* my_malloc(int size) {
    Block* current = free_list;
    
    while (current != NULL) {
        if (current->is_free && current->size >= size) {
            // Found suitable block
            if (current->size > size + sizeof(Block) + 8) {
                // Split the block
                Block* new_block = (Block*)((char*)current + sizeof(Block) + size);
                new_block->size = current->size - size - sizeof(Block);
                new_block->is_free = 1;
                new_block->next = current->next;
                
                current->size = size;
                current->next = new_block;
            }
            current->is_free = 0;
            return (void*)((char*)current + sizeof(Block));
        }
        current = current->next;
    }
    return NULL;
}

void my_free(void* ptr) {
    if (ptr == NULL) return;
    Block* block = (Block*)((char*)ptr - sizeof(Block));
    block->is_free = 1;
}

int main() {
    init_allocator();
    
    void* p1 = my_malloc(32);
    void* p2 = my_malloc(16);
    
    printf("Allocated p1: %p\n", p1);
    printf("Allocated p2: %p\n", p2);
    
    my_free(p1);
    void* p3 = my_malloc(10);
    printf("Allocated p3 (should reuse p1's space): %p\n", p3);
    
    return 0;
}
```

## Example 3: Coalescing Free Blocks

```c
#include <stdio.h>

#define POOL_SIZE 4096

typedef struct Block {
    int size;
    int is_free;
    struct Block* next;
    struct Block* prev;
} Block;

static char memory_pool[POOL_SIZE];
static Block* free_list = NULL;

void init_allocator() {
    free_list = (Block*)memory_pool;
    free_list->size = POOL_SIZE - sizeof(Block);
    free_list->is_free = 1;
    free_list->next = NULL;
    free_list->prev = NULL;
}

void coalesce_blocks() {
    Block* current = free_list;
    
    while (current != NULL && current->next != NULL) {
        if (current->is_free && current->next->is_free) {
            // Merge current and next block
            current->size += sizeof(Block) + current->next->size;
            current->next = current->next->next;
            if (current->next != NULL) {
                current->next->prev = current;
            }
        } else {
            current = current->next;
        }
    }
}

void* my_malloc(int size) {
    Block* current = free_list;
    
    while (current != NULL) {
        if (current->is_free && current->size >= size) {
            if (current->size > size + sizeof(Block) + 8) {
                Block* new_block = (Block*)((char*)current + sizeof(Block) + size);
                new_block->size = current->size - size - sizeof(Block);
                new_block->is_free = 1;
                new_block->next = current->next;
                new_block->prev = current;
                
                if (current->next != NULL) {
                    current->next->prev = new_block;
                }
                
                current->size = size;
                current->next = new_block;
            }
            current->is_free = 0;
            return (void*)((char*)current + sizeof(Block));
        }
        current = current->next;
    }
    return NULL;
}

void my_free(void* ptr) {
    if (ptr == NULL) return;
    Block* block = (Block*)((char*)ptr - sizeof(Block));
    block->is_free = 1;
    coalesce_blocks();
}

int main() {
    init_allocator();
    
    void* p1 = my_malloc(100);
    void* p2 = my_malloc(50);
    void* p3 = my_malloc(75);
    
    printf("Before free - p1: %p, p2: %p, p3: %p\n", p1, p2, p3);
    
    my_free(p2);
    void* p4 = my_malloc(120);
    printf("After coalescing - p4: %p\n", p4);
    
    return 0;
}
```

## Example 4: Best-Fit Allocation Strategy

```c
#include <stdio.h>

#define POOL_SIZE 8192

typedef struct Block {
    int size;
    int is_free;
    struct Block* next;
} Block;

static char memory_pool[POOL_SIZE];
static Block* free_list = NULL;

void init_allocator() {
    free_list = (Block*)memory_pool;
    free_list->size = POOL_SIZE - sizeof(Block);
    free_list->is_free = 1;
    free_list->next = NULL;
}

void* my_malloc_best_fit(int size) {
    Block* current = free_list;
    Block* best_fit = NULL;
    int min_size_diff = 999999;
    
    // Find best fit block
    while (current != NULL) {
        if (current->is_free && current->size >= size) {
            int size_diff = current->size - size;
            if (size_diff < min_size_diff) {
                min_size_diff = size_diff;
                best_fit = current;
            }
        }
        current = current->next;
    }
    
    if (best_fit != NULL) {
        if (best_fit->size > size + sizeof(Block) + 8) {
            Block* new_block = (Block*)((char*)best_fit + sizeof(Block) + size);
            new_block->size = best_fit->size - size - sizeof(Block);
            new_block->is_free = 1;
            new_block->next = best_fit->next;
            
            best_fit->size = size;
            best_fit->next = new_block;
        }
        best_fit->is_free = 0;
        return (void*)((char*)best_fit + sizeof(Block));
    }
    return NULL;
}

void my_free(void* ptr) {
    if (ptr == NULL) return;
    Block* block = (Block*)((char*)ptr - sizeof(Block));
    block->is_free = 1;
}

int main() {
    init_allocator();
    
    void* p1 = my_malloc_best_fit(100);
    void* p2 = my_malloc_best_fit(200);
    void* p3 = my_malloc_best_fit(50);
    
    printf("Best-fit allocations:\n");
    printf("p1 (100 bytes): %p\n", p1);
    printf("p2 (200 bytes): %p\n", p2);
    printf("p3 (50 bytes): %p\n", p3);
    
    return 0;
}
```

## Example 5: Adding Memory Alignment

```c
#include <stdio.h>

#define POOL_SIZE 8192
#define ALIGNMENT 8

typedef struct Block {
    int size;
    int is_free;
    struct Block* next;
} Block;

static char memory_pool[POOL_SIZE];
static Block* free_list = NULL;

int align_size(int size) {
    return (size + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);
}

void init_allocator() {
    free_list = (Block*)memory_pool;
    free_list->size = POOL_SIZE - sizeof(Block);
    free_list->is_free = 1;
    free_list->next = NULL;
}

void* my_malloc_aligned(int size) {
    int aligned_size = align_size(size);
    Block* current = free_list;
    
    while (current != NULL) {
        if (current->is_free && current->size >= aligned_size) {
            if (current->size > aligned_size + sizeof(Block) + 8) {
                Block* new_block = (Block*)((char*)current + sizeof(Block) + aligned_size);
                new_block->size = current->size - aligned_size - sizeof(Block);
                new_block->is_free = 1;
                new_block->next = current->next;
                
                current->size = aligned_size;
                current->next = new_block;
            }
            current->is_free = 0;
            return (void*)((char*)current + sizeof(Block));
        }
        current = current->next;
    }
    return NULL;
}

void my_free(void* ptr) {
    if (ptr == NULL) return;
    Block* block = (Block*)((char*)ptr - sizeof(Block));
    block->is_free = 1;
}

int main() {
    init_allocator();
    
    void* p1 = my_malloc_aligned(17);
    void* p2 = my_malloc_aligned(33);
    void* p3 = my_malloc_aligned(65);
    
    printf("Aligned allocations (all should be 8-byte aligned):\n");
    printf("p1: %p (aligned to %d)\n", p1, ALIGNMENT);
    printf("p2: %p (aligned to %d)\n", p2, ALIGNMENT);
    printf("p3: %p (aligned to %d)\n", p3, ALIGNMENT);
    
    return 0;
}
```

## Example 6: Multiple Memory Pools (Segregated Lists)

```c
#include <stdio.h>

#define NUM_POOLS 3
#define SMALL_POOL_SIZE 1024
#define MEDIUM_POOL_SIZE 4096
#define LARGE_POOL_SIZE 8192
#define SMALL_BLOCK_MAX 64
#define MEDIUM_BLOCK_MAX 256

typedef struct Block {
    int size;
    int is_free;
    struct Block* next;
} Block;

typedef struct MemoryPool {
    char* pool;
    int pool_size;
    Block* free_list;
} MemoryPool;

static MemoryPool pools[NUM_POOLS];

void init_pool(MemoryPool* mp, int size) {
    mp->pool_size = size;
    mp->free_list = (Block*)mp->pool;
    mp->free_list->size = size - sizeof(Block);
    mp->free_list->is_free = 1;
    mp->free_list->next = NULL;
}

void init_allocator() {
    static char small_pool[SMALL_POOL_SIZE];
    static char medium_pool[MEDIUM_POOL_SIZE];
    static char large_pool[LARGE_POOL_SIZE];
    
    pools[0].pool = small_pool;
    pools[1].pool = medium_pool;
    pools[2].pool = large_pool;
    
    init_pool(&pools[0], SMALL_POOL_SIZE);
    init_pool(&pools[1], MEDIUM_POOL_SIZE);
    init_pool(&pools[2], LARGE_POOL_SIZE);
}

MemoryPool* get_pool_for_size(int size) {
    if (size <= SMALL_BLOCK_MAX) return &pools[0];
    if (size <= MEDIUM_BLOCK_MAX) return &pools[1];
    return &pools[2];
}

void* my_malloc(int size) {
    MemoryPool* mp = get_pool_for_size(size);
    Block* current = mp->free_list;
    
    while (current != NULL) {
        if (current->is_free && current->size >= size) {
            if (current->size > size + sizeof(Block) + 8) {
                Block* new_block = (Block*)((char*)current + sizeof(Block) + size);
                new_block->size = current->size - size - sizeof(Block);
                new_block->is_free = 1;
                new_block->next = current->next;
                
                current->size = size;
                current->next = new_block;
            }
            current->is_free = 0;
            return (void*)((char*)current + sizeof(Block));
        }
        current = current->next;
    }
    return NULL;
}

void my_free(void* ptr) {
    if (ptr == NULL) return;
    Block* block = (Block*)((char*)ptr - sizeof(Block));
    block->is_free = 1;
}

int main() {
    init_allocator();
    
    void* p1 = my_malloc(32);   // Small pool
    void* p2 = my_malloc(128);  // Medium pool
    void* p3 = my_malloc(512);  // Large pool
    
    printf("Segregated pools allocation:\n");
    printf("Small allocation: %p\n", p1);
    printf("Medium allocation: %p\n", p2);
    printf("Large allocation: %p\n", p3);
    
    return 0;
}
```

## Example 7: Adding Realloc Support

```c
#include <stdio.h>
#include <string.h>

#define POOL_SIZE 16384

typedef struct Block {
    int size;
    int is_free;
    struct Block* next;
} Block;

static char memory_pool[POOL_SIZE];
static Block* free_list = NULL;

void init_allocator() {
    free_list = (Block*)memory_pool;
    free_list->size = POOL_SIZE - sizeof(Block);
    free_list->is_free = 1;
    free_list->next = NULL;
}

void* my_malloc(int size) {
    Block* current = free_list;
    
    while (current != NULL) {
        if (current->is_free && current->size >= size) {
            if (current->size > size + sizeof(Block) + 8) {
                Block* new_block = (Block*)((char*)current + sizeof(Block) + size);
                new_block->size = current->size - size - sizeof(Block);
                new_block->is_free = 1;
                new_block->next = current->next;
                
                current->size = size;
                current->next = new_block;
            }
            current->is_free = 0;
            return (void*)((char*)current + sizeof(Block));
        }
        current = current->next;
    }
    return NULL;
}

void my_free(void* ptr) {
    if (ptr == NULL) return;
    Block* block = (Block*)((char*)ptr - sizeof(Block));
    block->is_free = 1;
}

void* my_realloc(void* ptr, int new_size) {
    if (ptr == NULL) return my_malloc(new_size);
    if (new_size == 0) {
        my_free(ptr);
        return NULL;
    }
    
    Block* block = (Block*)((char*)ptr - sizeof(Block));
    
    // Check if next block is free and we can expand
    Block* next = block->next;
    if (next != NULL && next->is_free && 
        block->size + next->size + sizeof(Block) >= new_size) {
        // Expand current block
        block->size += next->size + sizeof(Block);
        block->next = next->next;
        return ptr;
    }
    
    // Allocate new block and copy data
    void* new_ptr = my_malloc(new_size);
    if (new_ptr != NULL) {
        int copy_size = block->size < new_size ? block->size : new_size;
        for (int i = 0; i < copy_size; i++) {
            ((char*)new_ptr)[i] = ((char*)ptr)[i];
        }
        my_free(ptr);
    }
    return new_ptr;
}

int main() {
    init_allocator();
    
    char* str = (char*)my_malloc(10);
    for (int i = 0; i < 9; i++) str[i] = 'A' + i;
    str[9] = '\0';
    
    printf("Original string: %s\n", str);
    
    str = (char*)my_realloc(str, 20);
    str[9] = 'X';
    str[10] = 'Y';
    str[11] = 'Z';
    str[12] = '\0';
    
    printf("Reallocated string: %s\n", str);
    
    my_free(str);
    return 0;
}
```

## Example 8: Memory Leak Detection

```c
#include <stdio.h>

#define POOL_SIZE 8192
#define MAX_ALLOCATIONS 100

typedef struct Block {
    int size;
    int is_free;
    struct Block* next;
    int alloc_id;
} Block;

typedef struct AllocationRecord {
    void* ptr;
    int size;
    int active;
} AllocationRecord;

static char memory_pool[POOL_SIZE];
static Block* free_list = NULL;
static AllocationRecord alloc_records[MAX_ALLOCATIONS];
static int total_allocations = 0;
static int active_allocations = 0;

void init_allocator() {
    free_list = (Block*)memory_pool;
    free_list->size = POOL_SIZE - sizeof(Block);
    free_list->is_free = 1;
    free_list->next = NULL;
    free_list->alloc_id = -1;
    
    for (int i = 0; i < MAX_ALLOCATIONS; i++) {
        alloc_records[i].active = 0;
    }
}

void* my_malloc_tracked(int size) {
    Block* current = free_list;
    
    while (current != NULL) {
        if (current->is_free && current->size >= size) {
            if (current->size > size + sizeof(Block) + 8) {
                Block* new_block = (Block*)((char*)current + sizeof(Block) + size);
                new_block->size = current->size - size - sizeof(Block);
                new_block->is_free = 1;
                new_block->next = current->next;
                new_block->alloc_id = -1;
                
                current->size = size;
                current->next = new_block;
            }
            current->is_free = 0;
            current->alloc_id = total_allocations;
            
            // Record allocation
            alloc_records[total_allocations].ptr = (void*)((char*)current + sizeof(Block));
            alloc_records[total_allocations].size = size;
            alloc_records[total_allocations].active = 1;
            
            total_allocations++;
            active_allocations++;
            
            return alloc_records[current->alloc_id].ptr;
        }
        current = current->next;
    }
    return NULL;
}

void my_free_tracked(void* ptr) {
    if (ptr == NULL) return;
    Block* block = (Block*)((char*)ptr - sizeof(Block));
    
    if (block->alloc_id >= 0 && block->alloc_id < total_allocations) {
        alloc_records[block->alloc_id].active = 0;
        active_allocations--;
    }
    
    block->is_free = 1;
}

void report_memory_leaks() {
    printf("\n=== Memory Leak Report ===\n");
    printf("Total allocations: %d\n", total_allocations);
    printf("Active allocations: %d\n", active_allocations);
    
    if (active_allocations > 0) {
        printf("\nLeaked allocations:\n");
        for (int i = 0; i < total_allocations; i++) {
            if (alloc_records[i].active) {
                printf("  Leak #%d: %p (%d bytes)\n", 
                       i, alloc_records[i].ptr, alloc_records[i].size);
            }
        }
    } else {
        printf("No memory leaks detected!\n");
    }
}

int main() {
    init_allocator();
    
    void* p1 = my_malloc_tracked(100);
    void* p2 = my_malloc_tracked(200);
    void* p3 = my_malloc_tracked(300);
    
    printf("Allocated 3 blocks\n");
    
    my_free_tracked(p2);
    printf("Freed p2\n");
    
    report_memory_leaks();
    
    my_free_tracked(p1);
    my_free_tracked(p3);
    
    report_memory_leaks();
    
    return 0;
}
```

## Example 9: Memory Statistics and Debugging

```c
#include <stdio.h>

#define POOL_SIZE 16384

typedef struct Block {
    int size;
    int is_free;
    struct Block* next;
} Block;

typedef struct MemoryStats {
    int total_memory;
    int free_memory;
    int used_memory;
    int total_blocks;
    int free_blocks;
    int used_blocks;
    int fragmentation_count;
} MemoryStats;

static char memory_pool[POOL_SIZE];
static Block* free_list = NULL;

void init_allocator() {
    free_list = (Block*)memory_pool;
    free_list->size = POOL_SIZE - sizeof(Block);
    free_list->is_free = 1;
    free_list->next = NULL;
}

void* my_malloc(int size) {
    Block* current = free_list;
    
    while (current != NULL) {
        if (current->is_free && current->size >= size) {
            if (current->size > size + sizeof(Block) + 8) {
                Block* new_block = (Block*)((char*)current + sizeof(Block) + size);
                new_block->size = current->size - size - sizeof(Block);
                new_block->is_free = 1;
                new_block->next = current->next;
                
                current->size = size;
                current->next = new_block;
            }
            current->is_free = 0;
            return (void*)((char*)current + sizeof(Block));
        }
        current = current->next;
    }
    return NULL;
}

void my_free(void* ptr) {
    if (ptr == NULL) return;
    Block* block = (Block*)((char*)ptr - sizeof(Block));
    block->is_free = 1;
}

void get_memory_stats(MemoryStats* stats) {
    stats->total_memory = POOL_SIZE;
    stats->free_memory = 0;
    stats->used_memory = 0;
    stats->total_blocks = 0;
    stats->free_blocks = 0;
    stats->used_blocks = 0;
    stats->fragmentation_count = 0;
    
    Block* current = free_list;
    int prev_free = 0;
    
    while (current != NULL) {
        stats->total_blocks++;
        
        if (current->is_free) {
            stats->free_blocks++;
            stats->free_memory += current->size + sizeof(Block);
            
            if (prev_free) {
                stats->fragmentation_count++;
            }
            prev_free = 1;
        } else {
            stats->used_blocks++;
            stats->used_memory += current->size + sizeof(Block);
            prev_free = 0;
        }
        
        current = current->next;
    }
}

void print_memory_stats() {
    MemoryStats stats;
    get_memory_stats(&stats);
    
    printf("\n=== Memory Statistics ===\n");
    printf("Total Memory: %d bytes\n", stats.total_memory);
    printf("Free Memory: %d bytes (%.1f%%)\n", 
           stats.free_memory, (float)stats.free_memory / stats.total_memory * 100);
    printf("Used Memory: %d bytes (%.1f%%)\n", 
           stats.used_memory, (float)stats.used_memory / stats.total_memory * 100);
    printf("Total Blocks: %d\n", stats.total_blocks);
    printf("Free Blocks: %d\n", stats.free_blocks);
    printf("Used Blocks: %d\n", stats.used_blocks);
    printf("Fragmentation Count: %d\n", stats.fragmentation_count);
    printf("===========================\n");
}

int main() {
    init_allocator();
    
    print_memory_stats();
    
    void* p1 = my_malloc(100);
    void* p2 = my_malloc(200);
    void* p3 = my_malloc(50);
    void* p4 = my_malloc(500);
    
    printf("\nAfter allocations:");
    print_memory_stats();
    
    my_free(p2);
    my_free(p4);
    
    printf("\nAfter freeing some blocks:");
    print_memory_stats();
    
    void* p5 = my_malloc(75);
    void* p6 = my_malloc(300);
    
    printf("\nAfter more allocations:");
    print_memory_stats();
    
    return 0;
}
```

## Example 10: Complete Memory Allocator with All Features

```c
#include <stdio.h>

#define POOL_SIZE 32768
#define ALIGNMENT 8
#define MAX_ALLOCATIONS 500

typedef struct Block {
    int size;
    int is_free;
    struct Block* next;
    struct Block* prev;
    int alloc_id;
    int magic;  // Magic number for corruption detection
} Block;

#define MAGIC_NUMBER 0x12345678

typedef struct AllocationRecord {
    void* ptr;
    int size;
    int active;
} AllocationRecord;

static char memory_pool[POOL_SIZE];
static Block* free_list = NULL;
static AllocationRecord alloc_records[MAX_ALLOCATIONS];
static int total_allocations = 0;
static int active_allocations = 0;
static int total_memory_allocated = 0;

// Utility functions
int align_size(int size) {
    return (size + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);
}

void init_allocator() {
    free_list = (Block*)memory_pool;
    free_list->size = POOL_SIZE - sizeof(Block);
    free_list->is_free = 1;
    free_list->next = NULL;
    free_list->prev = NULL;
    free_list->alloc_id = -1;
    free_list->magic = MAGIC_NUMBER;
    
    for (int i = 0; i < MAX_ALLOCATIONS; i++) {
        alloc_records[i].active = 0;
    }
}

// Coalesce adjacent free blocks
void coalesce_blocks() {
    Block* current = free_list;
    
    while (current != NULL && current->next != NULL) {
        if (current->is_free && current->next->is_free) {
            current->size += sizeof(Block) + current->next->size;
            current->next = current->next->next;
            if (current->next != NULL) {
                current->next->prev = current;
            }
        } else {
            current = current->next;
        }
    }
}

// Best-fit allocation with alignment
void* my_malloc(int size) {
    int aligned_size = align_size(size);
    Block* current = free_list;
    Block* best_fit = NULL;
    int min_size_diff = 999999;
    
    // Find best fit block
    while (current != NULL) {
        if (current->is_free && current->size >= aligned_size) {
            int size_diff = current->size - aligned_size;
            if (size_diff < min_size_diff) {
                min_size_diff = size_diff;
                best_fit = current;
            }
        }
        current = current->next;
    }
    
    if (best_fit != NULL) {
        // Split block if it's large enough
        if (best_fit->size > aligned_size + sizeof(Block) + 8) {
            Block* new_block = (Block*)((char*)best_fit + sizeof(Block) + aligned_size);
            new_block->size = best_fit->size - aligned_size - sizeof(Block);
            new_block->is_free = 1;
            new_block->next = best_fit->next;
            new_block->prev = best_fit;
            new_block->alloc_id = -1;
            new_block->magic = MAGIC_NUMBER;
            
            if (best_fit->next != NULL) {
                best_fit->next->prev = new_block;
            }
            
            best_fit->size = aligned_size;
            best_fit->next = new_block;
        }
        
        best_fit->is_free = 0;
        best_fit->alloc_id = total_allocations;
        best_fit->magic = MAGIC_NUMBER;
        
        // Record allocation
        alloc_records[total_allocations].ptr = (void*)((char*)best_fit + sizeof(Block));
        alloc_records[total_allocations].size = aligned_size;
        alloc_records[total_allocations].active = 1;
        
        total_allocations++;
        active_allocations++;
        total_memory_allocated += aligned_size;
        
        return alloc_records[best_fit->alloc_id].ptr;
    }
    return NULL;
}

void my_free(void* ptr) {
    if (ptr == NULL) return;
    Block* block = (Block*)((char*)ptr - sizeof(Block));
    
    // Check for corruption
    if (block->magic != MAGIC_NUMBER) {
        printf("Memory corruption detected at block %p!\n", ptr);
        return;
    }
    
    if (block->alloc_id >= 0 && block->alloc_id < total_allocations) {
        alloc_records[block->alloc_id].active = 0;
        active_allocations--;
    }
    
    block->is_free = 1;
    coalesce_blocks();
}

void* my_realloc(void* ptr, int new_size) {
    if (ptr == NULL) return my_malloc(new_size);
    if (new_size == 0) {
        my_free(ptr);
        return NULL;
    }
    
    Block* block = (Block*)((char*)ptr - sizeof(Block));
    int aligned_new_size = align_size(new_size);
    
    // Try to expand in place
    Block* next = block->next;
    if (next != NULL && next->is_free && 
        block->size + next->size + sizeof(Block) >= aligned_new_size) {
        block->size += next->size + sizeof(Block);
        block->next = next->next;
        if (next->next != NULL) {
            next->next->prev = block;
        }
        return ptr;
    }
    
    // Allocate new block and copy
    void* new_ptr = my_malloc(aligned_new_size);
    if (new_ptr != NULL) {
        int copy_size = block->size < aligned_new_size ? block->size : aligned_new_size;
        for (int i = 0; i < copy_size; i++) {
            ((char*)new_ptr)[i] = ((char*)ptr)[i];
        }
        my_free(ptr);
    }
    return new_ptr;
}

void print_memory_stats() {
    int free_memory = 0;
    int free_blocks = 0;
    int used_blocks = 0;
    int total_blocks = 0;
    Block* current = free_list;
    
    while (current != NULL) {
        total_blocks++;
        if (current->is_free) {
            free_blocks++;
            free_memory += current->size + sizeof(Block);
        } else {
            used_blocks++;
        }
        current = current->next;
    }
    
    printf("\n=== Memory Allocator Statistics ===\n");
    printf("Total Pool Size: %d bytes\n", POOL_SIZE);
    printf("Total Allocations: %d\n", total_allocations);
    printf("Active Allocations: %d\n", active_allocations);
    printf("Free Memory: %d bytes\n", free_memory);
    printf("Used Memory: %d bytes\n", POOL_SIZE - free_memory);
    printf("Total Blocks: %d\n", total_blocks);
    printf("Free Blocks: %d\n", free_blocks);
    printf("Used Blocks: %d\n", used_blocks);
    printf("===================================\n");
}

void report_leaks() {
    int leak_count = 0;
    
    printf("\n=== Memory Leak Report ===\n");
    for (int i = 0; i < total_allocations; i++) {
        if (alloc_records[i].active) {
            printf("Leak #%d: %p (%d bytes)\n", 
                   i, alloc_records[i].ptr, alloc_records[i].size);
            leak_count++;
        }
    }
    
    if (leak_count == 0) {
        printf("No memory leaks detected!\n");
    } else {
        printf("Total leaks: %d\n", leak_count);
    }
    printf("===========================\n");
}

int main() {
    init_allocator();
    
    printf("Testing complete memory allocator...\n");
    
    // Test basic allocation
    void* p1 = my_malloc(100);
    void* p2 = my_malloc(50);
    void* p3 = my_malloc(200);
    
    print_memory_stats();
    
    // Test realloc
    p2 = my_realloc(p2, 150);
    printf("\nAfter realloc of p2 to 150 bytes:");
    print_memory_stats();
    
    // Test alignment
    void* p4 = my_malloc(17);
    void* p5 = my_malloc(33);
    printf("\nAligned allocations at addresses: %p, %p\n", p4, p5);
    
    // Test freeing and coalescing
    my_free(p1);
    my_free(p3);
    printf("\nAfter freeing p1 and p3 (coalescing should occur):");
    print_memory_stats();
    
    // Test fragmentation
    void* p6 = my_malloc(250);
    void* p7 = my_malloc(75);
    void* p8 = my_malloc(500);
    
    my_free(p6);
    my_free(p8);
    printf("\nAfter creating fragmentation:");
    print_memory_stats();
    
    // Test leak detection
    printf("\nBefore cleanup:");
    report_leaks();
    
    my_free(p2);
    my_free(p4);
    my_free(p5);
    my_free(p7);
    
    printf("\nAfter cleanup:");
    print_memory_stats();
    report_leaks();
    
    return 0;
}
```

This progressive series builds your understanding step by step:

1. **Example 1**: Basic fixed-size block allocation
2. **Example 2**: Variable-size blocks with metadata
3. **Example 3**: Coalescing adjacent free blocks
4. **Example 4**: Best-fit allocation strategy
5. **Example 5**: Memory alignment
6. **Example 6**: Multiple pools for different sizes
7. **Example 7**: Realloc support
8. **Example 8**: Memory leak detection
9. **Example 9**: Statistics and debugging
10. **Example 10**: Complete allocator with all features

Each example builds on the previous one, so you can see how the allocator evolves from simple to complex. The final version incorporates all concepts learned throughout the series.
