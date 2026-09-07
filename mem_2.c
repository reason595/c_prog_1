Excellent! You've mastered the fundamentals. Now let's go DEEPER into advanced memory allocator concepts. We'll continue building on the same allocator with more sophisticated features.

## Example 11: Thread-Safe Allocator with Simple Locking

```c
#include <stdio.h>

#define POOL_SIZE 65536
#define ALIGNMENT 8
#define MAX_ALLOCATIONS 1000

typedef struct Block {
    int size;
    int is_free;
    struct Block* next;
    struct Block* prev;
    int alloc_id;
    int magic;
    int thread_id;  // Track which thread owns this block
} Block;

#define MAGIC_NUMBER 0x12345678

typedef struct AllocationRecord {
    void* ptr;
    int size;
    int active;
    int thread_id;
} AllocationRecord;

static char memory_pool[POOL_SIZE];
static Block* free_list = NULL;
static AllocationRecord alloc_records[MAX_ALLOCATIONS];
static int total_allocations = 0;
static int active_allocations = 0;

// Simple spinlock for thread safety
static int lock_flag = 0;

void spinlock_lock() {
    while (lock_flag) {
        // Busy wait (in real systems, use atomic operations)
    }
    lock_flag = 1;
}

void spinlock_unlock() {
    lock_flag = 0;
}

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
    free_list->thread_id = -1;
    
    for (int i = 0; i < MAX_ALLOCATIONS; i++) {
        alloc_records[i].active = 0;
    }
}

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

void* my_malloc_thread_safe(int size, int thread_id) {
    spinlock_lock();  // Enter critical section
    
    int aligned_size = align_size(size);
    Block* current = free_list;
    Block* best_fit = NULL;
    int min_size_diff = 999999;
    
    // Best-fit search
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
        if (best_fit->size > aligned_size + sizeof(Block) + 8) {
            Block* new_block = (Block*)((char*)best_fit + sizeof(Block) + aligned_size);
            new_block->size = best_fit->size - aligned_size - sizeof(Block);
            new_block->is_free = 1;
            new_block->next = best_fit->next;
            new_block->prev = best_fit;
            new_block->alloc_id = -1;
            new_block->magic = MAGIC_NUMBER;
            new_block->thread_id = -1;
            
            if (best_fit->next != NULL) {
                best_fit->next->prev = new_block;
            }
            
            best_fit->size = aligned_size;
            best_fit->next = new_block;
        }
        
        best_fit->is_free = 0;
        best_fit->alloc_id = total_allocations;
        best_fit->magic = MAGIC_NUMBER;
        best_fit->thread_id = thread_id;
        
        alloc_records[total_allocations].ptr = (void*)((char*)best_fit + sizeof(Block));
        alloc_records[total_allocations].size = aligned_size;
        alloc_records[total_allocations].active = 1;
        alloc_records[total_allocations].thread_id = thread_id;
        
        void* result = alloc_records[total_allocations].ptr;
        
        total_allocations++;
        active_allocations++;
        
        spinlock_unlock();  // Exit critical section
        return result;
    }
    
    spinlock_unlock();
    return NULL;
}

void my_free_thread_safe(void* ptr) {
    if (ptr == NULL) return;
    
    spinlock_lock();  // Enter critical section
    
    Block* block = (Block*)((char*)ptr - sizeof(Block));
    
    if (block->magic != MAGIC_NUMBER) {
        printf("Memory corruption detected at block %p!\n", ptr);
        spinlock_unlock();
        return;
    }
    
    if (block->alloc_id >= 0 && block->alloc_id < total_allocations) {
        alloc_records[block->alloc_id].active = 0;
        active_allocations--;
    }
    
    block->is_free = 1;
    block->thread_id = -1;
    coalesce_blocks();
    
    spinlock_unlock();  // Exit critical section
}

void print_thread_stats() {
    printf("\n=== Thread-Safe Allocator Stats ===\n");
    printf("Total allocations: %d\n", total_allocations);
    printf("Active allocations: %d\n", active_allocations);
    
    int thread_allocations[4] = {0};
    for (int i = 0; i < total_allocations; i++) {
        if (alloc_records[i].active && alloc_records[i].thread_id >= 0 && alloc_records[i].thread_id < 4) {
            thread_allocations[alloc_records[i].thread_id]++;
        }
    }
    
    for (int i = 0; i < 4; i++) {
        printf("Thread %d active allocations: %d\n", i, thread_allocations[i]);
    }
    printf("===================================\n");
}

int main() {
    init_allocator();
    
    printf("Testing thread-safe allocator (simulating 4 threads)...\n");
    
    // Simulate multi-threaded allocation
    void* p1 = my_malloc_thread_safe(100, 0);
    void* p2 = my_malloc_thread_safe(200, 1);
    void* p3 = my_malloc_thread_safe(150, 0);
    void* p4 = my_malloc_thread_safe(300, 2);
    void* p5 = my_malloc_thread_safe(50, 3);
    void* p6 = my_malloc_thread_safe(250, 1);
    
    print_thread_stats();
    
    // Simulate thread-specific frees
    my_free_thread_safe(p1);
    my_free_thread_safe(p3);
    my_free_thread_safe(p5);
    
    printf("\nAfter freeing some thread allocations:");
    print_thread_stats();
    
    return 0;
}
```

## Example 12: Memory Pool with Garbage Collection (Reference Counting)

```c
#include <stdio.h>

#define POOL_SIZE 65536
#define ALIGNMENT 8
#define MAX_ALLOCATIONS 1000

typedef struct Block {
    int size;
    int is_free;
    struct Block* next;
    struct Block* prev;
    int alloc_id;
    int magic;
    int ref_count;  // Reference count for garbage collection
    int is_marked;  // Mark bit for GC
} Block;

#define MAGIC_NUMBER 0x12345678

typedef struct AllocationRecord {
    void* ptr;
    int size;
    int active;
    int ref_count;
} AllocationRecord;

static char memory_pool[POOL_SIZE];
static Block* free_list = NULL;
static AllocationRecord alloc_records[MAX_ALLOCATIONS];
static int total_allocations = 0;
static int active_allocations = 0;
static int gc_cycles = 0;

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
    free_list->ref_count = 0;
    free_list->is_marked = 0;
    
    for (int i = 0; i < MAX_ALLOCATIONS; i++) {
        alloc_records[i].active = 0;
        alloc_records[i].ref_count = 0;
    }
}

void* my_malloc_gc(int size) {
    int aligned_size = align_size(size);
    Block* current = free_list;
    Block* best_fit = NULL;
    int min_size_diff = 999999;
    
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
        if (best_fit->size > aligned_size + sizeof(Block) + 8) {
            Block* new_block = (Block*)((char*)best_fit + sizeof(Block) + aligned_size);
            new_block->size = best_fit->size - aligned_size - sizeof(Block);
            new_block->is_free = 1;
            new_block->next = best_fit->next;
            new_block->prev = best_fit;
            new_block->alloc_id = -1;
            new_block->magic = MAGIC_NUMBER;
            new_block->ref_count = 0;
            new_block->is_marked = 0;
            
            if (best_fit->next != NULL) {
                best_fit->next->prev = new_block;
            }
            
            best_fit->size = aligned_size;
            best_fit->next = new_block;
        }
        
        best_fit->is_free = 0;
        best_fit->alloc_id = total_allocations;
        best_fit->magic = MAGIC_NUMBER;
        best_fit->ref_count = 1;  // Initial reference
        best_fit->is_marked = 0;
        
        alloc_records[total_allocations].ptr = (void*)((char*)best_fit + sizeof(Block));
        alloc_records[total_allocations].size = aligned_size;
        alloc_records[total_allocations].active = 1;
        alloc_records[total_allocations].ref_count = 1;
        
        void* result = alloc_records[total_allocations].ptr;
        total_allocations++;
        active_allocations++;
        
        return result;
    }
    return NULL;
}

void increment_ref(void* ptr) {
    if (ptr == NULL) return;
    Block* block = (Block*)((char*)ptr - sizeof(Block));
    block->ref_count++;
    alloc_records[block->alloc_id].ref_count++;
}

void decrement_ref(void* ptr) {
    if (ptr == NULL) return;
    Block* block = (Block*)((char*)ptr - sizeof(Block));
    block->ref_count--;
    alloc_records[block->alloc_id].ref_count--;
    
    // If ref count drops to zero, collect immediately
    if (block->ref_count == 0) {
        block->is_free = 1;
        alloc_records[block->alloc_id].active = 0;
        active_allocations--;
    }
}

void run_garbage_collector() {
    gc_cycles++;
    int collected = 0;
    
    printf("\nRunning garbage collection cycle #%d...\n", gc_cycles);
    
    Block* current = free_list;
    while (current != NULL) {
        if (!current->is_free && current->ref_count == 0) {
            // Collect this block
            current->is_free = 1;
            if (current->alloc_id >= 0) {
                alloc_records[current->alloc_id].active = 0;
                active_allocations--;
                collected++;
            }
        }
        current = current->next;
    }
    
    printf("Collected %d blocks\n", collected);
}

void print_gc_stats() {
    printf("\n=== Garbage Collection Stats ===\n");
    printf("Total allocations: %d\n", total_allocations);
    printf("Active allocations: %d\n", active_allocations);
    printf("GC cycles run: %d\n", gc_cycles);
    
    int total_refs = 0;
    for (int i = 0; i < total_allocations; i++) {
        if (alloc_records[i].active) {
            total_refs += alloc_records[i].ref_count;
            printf("Alloc #%d: %d bytes, %d references\n", 
                   i, alloc_records[i].size, alloc_records[i].ref_count);
        }
    }
    printf("Total references: %d\n", total_refs);
    printf("================================\n");
}

int main() {
    init_allocator();
    
    printf("Testing garbage collection with reference counting...\n");
    
    // Create objects with different lifetimes
    void* obj1 = my_malloc_gc(100);
    void* obj2 = my_malloc_gc(200);
    void* obj3 = my_malloc_gc(150);
    
    // Add references
    increment_ref(obj1);  // obj1 has 2 refs
    increment_ref(obj2);  // obj2 has 2 refs
    increment_ref(obj2);  // obj2 has 3 refs
    
    print_gc_stats();
    
    // Remove some references
    decrement_ref(obj1);
    decrement_ref(obj2);
    decrement_ref(obj2);
    
    printf("\nAfter decrementing references:");
    print_gc_stats();
    
    // Force garbage collection
    run_garbage_collector();
    
    printf("\nAfter GC:");
    print_gc_stats();
    
    return 0;
}
```

## Example 13: Memory Defragmentation (Compaction)

```c
#include <stdio.h>
#include <string.h>

#define POOL_SIZE 65536
#define ALIGNMENT 8
#define MAX_ALLOCATIONS 1000

typedef struct Block {
    int size;
    int is_free;
    struct Block* next;
    struct Block* prev;
    int alloc_id;
    int magic;
    void* old_address;  // For tracking moved blocks
} Block;

#define MAGIC_NUMBER 0x12345678

typedef struct AllocationRecord {
    void* ptr;
    int size;
    int active;
    void* new_ptr;  // Updated pointer after compaction
} AllocationRecord;

static char memory_pool[POOL_SIZE];
static Block* free_list = NULL;
static AllocationRecord alloc_records[MAX_ALLOCATIONS];
static int total_allocations = 0;
static int active_allocations = 0;
static int compaction_count = 0;

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
    free_list->old_address = NULL;
    
    for (int i = 0; i < MAX_ALLOCATIONS; i++) {
        alloc_records[i].active = 0;
        alloc_records[i].new_ptr = NULL;
    }
}

void* my_malloc(int size) {
    int aligned_size = align_size(size);
    Block* current = free_list;
    Block* best_fit = NULL;
    int min_size_diff = 999999;
    
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
        if (best_fit->size > aligned_size + sizeof(Block) + 8) {
            Block* new_block = (Block*)((char*)best_fit + sizeof(Block) + aligned_size);
            new_block->size = best_fit->size - aligned_size - sizeof(Block);
            new_block->is_free = 1;
            new_block->next = best_fit->next;
            new_block->prev = best_fit;
            new_block->alloc_id = -1;
            new_block->magic = MAGIC_NUMBER;
            new_block->old_address = NULL;
            
            if (best_fit->next != NULL) {
                best_fit->next->prev = new_block;
            }
            
            best_fit->size = aligned_size;
            best_fit->next = new_block;
        }
        
        best_fit->is_free = 0;
        best_fit->alloc_id = total_allocations;
        best_fit->magic = MAGIC_NUMBER;
        best_fit->old_address = (void*)((char*)best_fit + sizeof(Block));
        
        alloc_records[total_allocations].ptr = (void*)((char*)best_fit + sizeof(Block));
        alloc_records[total_allocations].size = aligned_size;
        alloc_records[total_allocations].active = 1;
        
        void* result = alloc_records[total_allocations].ptr;
        total_allocations++;
        active_allocations++;
        
        return result;
    }
    return NULL;
}

void my_free(void* ptr) {
    if (ptr == NULL) return;
    Block* block = (Block*)((char*)ptr - sizeof(Block));
    
    if (block->magic != MAGIC_NUMBER) {
        printf("Memory corruption detected!\n");
        return;
    }
    
    if (block->alloc_id >= 0 && block->alloc_id < total_allocations) {
        alloc_records[block->alloc_id].active = 0;
        active_allocations--;
    }
    
    block->is_free = 1;
}

void compact_memory() {
    compaction_count++;
    printf("\nCompacting memory (cycle #%d)...\n", compaction_count);
    
    Block* current = free_list;
    Block* last_used = NULL;
    
    while (current != NULL) {
        if (!current->is_free) {
            if (last_used != NULL) {
                // There's free space before this block
                Block* free_block = last_used->next;
                if (free_block != current) {
                    // Move this block to after last_used
                    int move_size = current->size + sizeof(Block);
                    void* old_ptr = (void*)((char*)current + sizeof(Block));
                    void* new_location = (void*)((char*)last_used + sizeof(Block) + last_used->size);
                    
                    // Copy block data
                    Block* new_block = (Block*)new_location;
                    new_block->size = current->size;
                    new_block->is_free = 0;
                    new_block->next = current->next;
                    new_block->prev = last_used;
                    new_block->alloc_id = current->alloc_id;
                    new_block->magic = MAGIC_NUMBER;
                    
                    // Copy user data
                    for (int i = 0; i < current->size; i++) {
                        ((char*)new_location + sizeof(Block) + i) = 
                        ((char*)current + sizeof(Block) + i);
                    }
                    
                    // Update allocation record
                    if (current->alloc_id >= 0) {
                        alloc_records[current->alloc_id].ptr = 
                            (void*)((char*)new_location + sizeof(Block));
                    }
                    
                    // Update links
                    last_used->next = new_block;
                    if (current->next != NULL) {
                        current->next->prev = new_block;
                    }
                    
                    current = new_block;
                }
            }
            last_used = current;
        }
        current = current->next;
    }
    
    // Consolidate all free space at the end
    if (last_used != NULL) {
        int free_size = 0;
        Block* free_block = last_used->next;
        
        while (free_block != NULL) {
            free_size += free_block->size + sizeof(Block);
            free_block = free_block->next;
        }
        
        if (free_size > sizeof(Block)) {
            Block* consolidated = (Block*)((char*)last_used + sizeof(Block) + last_used->size);
            consolidated->size = free_size - sizeof(Block);
            consolidated->is_free = 1;
            consolidated->next = NULL;
            consolidated->prev = last_used;
            consolidated->alloc_id = -1;
            consolidated->magic = MAGIC_NUMBER;
            
            last_used->next = consolidated;
        }
    }
}

void print_fragmentation() {
    printf("\n=== Fragmentation Analysis ===\n");
    Block* current = free_list;
    int free_blocks = 0;
    int total_free = 0;
    int largest_free = 0;
    int total_blocks = 0;
    
    while (current != NULL) {
        total_blocks++;
        if (current->is_free) {
            free_blocks++;
            total_free += current->size;
            if (current->size > largest_free) {
                largest_free = current->size;
            }
        }
        current = current->next;
    }
    
    printf("Total blocks: %d\n", total_blocks);
    printf("Free blocks: %d\n", free_blocks);
    printf("Total free memory: %d bytes\n", total_free);
    printf("Largest free block: %d bytes\n", largest_free);
    printf("Fragmentation ratio: %.2f%%\n", 
           (float)(free_blocks - 1) / free_blocks * 100);
    printf("================================\n");
}

int main() {
    init_allocator();
    
    printf("Testing memory defragmentation...\n");
    
    // Create fragmentation
    void* p1 = my_malloc(100);
    void* p2 = my_malloc(200);
    void* p3 = my_malloc(150);
    void* p4 = my_malloc(300);
    void* p5 = my_malloc(50);
    
    print_fragmentation();
    
    // Free every other block to create holes
    my_free(p2);
    my_free(p4);
    
    printf("\nAfter freeing p2 and p4 (creating holes):");
    print_fragmentation();
    
    // Compact memory
    compact_memory();
    
    printf("\nAfter compaction:");
    print_fragmentation();
    
    // Try to allocate a large block
    void* p6 = my_malloc(500);
    if (p6 != NULL) {
        printf("Successfully allocated 500 bytes after compaction!\n");
    }
    
    return 0;
}
```

## Example 14: Memory Pool with Buddy System

```c
#include <stdio.h>

#define POOL_SIZE 65536
#define MIN_BLOCK_SIZE 64
#define MAX_ORDER 10  // 2^10 = 1024 blocks of minimum size

typedef struct BuddyBlock {
    int order;
    int is_free;
    struct BuddyBlock* next_free;
    struct BuddyBlock* prev_free;
    void* buddy;
} BuddyBlock;

typedef struct BuddyAllocator {
    char* pool;
    int pool_size;
    BuddyBlock* free_lists[MAX_ORDER + 1];
    int total_allocations;
    int active_allocations;
} BuddyAllocator;

static char memory_pool[POOL_SIZE];
static BuddyAllocator allocator;

int get_order(int size) {
    int order = 0;
    int block_size = MIN_BLOCK_SIZE;
    
    while (block_size < size + sizeof(BuddyBlock)) {
        block_size *= 2;
        order++;
        if (order > MAX_ORDER) {
            return -1;  // Too large
        }
    }
    return order;
}

void init_buddy_allocator() {
    allocator.pool = memory_pool;
    allocator.pool_size = POOL_SIZE;
    allocator.total_allocations = 0;
    allocator.active_allocations = 0;
    
    // Initialize free lists
    for (int i = 0; i <= MAX_ORDER; i++) {
        allocator.free_lists[i] = NULL;
    }
    
    // Create initial block of maximum order
    BuddyBlock* initial = (BuddyBlock*)allocator.pool;
    initial->order = MAX_ORDER;
    initial->is_free = 1;
    initial->next_free = NULL;
    initial->prev_free = NULL;
    initial->buddy = NULL;
    
    allocator.free_lists[MAX_ORDER] = initial;
}

void remove_from_free_list(BuddyBlock* block) {
    if (block->prev_free != NULL) {
        block->prev_free->next_free = block->next_free;
    } else {
        allocator.free_lists[block->order] = block->next_free;
    }
    
    if (block->next_free != NULL) {
        block->next_free->prev_free = block->prev_free;
    }
    
    block->next_free = NULL;
    block->prev_free = NULL;
}

void add_to_free_list(BuddyBlock* block) {
    block->next_free = allocator.free_lists[block->order];
    block->prev_free = NULL;
    
    if (allocator.free_lists[block->order] != NULL) {
        allocator.free_lists[block->order]->prev_free = block;
    }
    
    allocator.free_lists[block->order] = block;
}

BuddyBlock* split_block(int order) {
    if (order > MAX_ORDER) return NULL;
    
    BuddyBlock* block = allocator.free_lists[order];
    if (block == NULL) {
        // Try to get a larger block and split it
        block = split_block(order + 1);
        if (block == NULL) return NULL;
    }
    
    remove_from_free_list(block);
    
    // Split the block
    int new_order = order - 1;
    int half_size = MIN_BLOCK_SIZE << new_order;
    
    BuddyBlock* buddy = (BuddyBlock*)((char*)block + half_size);
    buddy->order = new_order;
    buddy->is_free = 1;
    buddy->next_free = NULL;
    buddy->prev_free = NULL;
    buddy->buddy = block;
    
    block->order = new_order;
    block->buddy = buddy;
    
    add_to_free_list(block);
    add_to_free_list(buddy);
    
    return allocator.free_lists[new_order];
}

BuddyBlock* find_buddy(BuddyBlock* block) {
    int block_size = MIN_BLOCK_SIZE << block->order;
    char* block_addr = (char*)block - allocator.pool;
    char* buddy_addr = block_addr ^ block_size;
    return (BuddyBlock*)(allocator.pool + buddy_addr);
}

void* my_malloc_buddy(int size) {
    int order = get_order(size);
    if (order < 0) return NULL;
    
    BuddyBlock* block = allocator.free_lists[order];
    if (block == NULL) {
        block = split_block(order + 1);
    }
    
    if (block != NULL) {
        remove_from_free_list(block);
        block->is_free = 0;
        allocator.total_allocations++;
        allocator.active_allocations++;
        return (void*)((char*)block + sizeof(BuddyBlock));
    }
    
    return NULL;
}

void my_free_buddy(void* ptr) {
    if (ptr == NULL) return;
    
    BuddyBlock* block = (BuddyBlock*)((char*)ptr - sizeof(BuddyBlock));
    block->is_free = 1;
    allocator.active_allocations--;
    
    // Try to merge with buddy
    while (block->order < MAX_ORDER) {
        BuddyBlock* buddy = find_buddy(block);
        
        if (buddy->is_free && buddy->order == block->order) {
            // Remove buddy from free list
            remove_from_free_list(buddy);
            
            // Merge blocks
            if (buddy < block) {
                block = buddy;
            }
            
            block->order++;
            block->buddy = find_buddy(block);
        } else {
            break;
        }
    }
    
    add_to_free_list(block);
}

void print_buddy_stats() {
    printf("\n=== Buddy System Stats ===\n");
    printf("Pool size: %d bytes\n", allocator.pool_size);
    printf("Total allocations: %d\n", allocator.total_allocations);
    printf("Active allocations: %d\n", allocator.active_allocations);
    
    printf("\nFree lists by order:\n");
    for (int i = 0; i <= MAX_ORDER; i++) {
        int count = 0;
        BuddyBlock* block = allocator.free_lists[i];
        while (block != NULL) {
            count++;
            block = block->next_free;
        }
        
        if (count > 0) {
            printf("  Order %d (%d bytes): %d blocks\n", 
                   i, MIN_BLOCK_SIZE << i, count);
        }
    }
    printf("===========================\n");
}

int main() {
    init_buddy_allocator();
    
    printf("Testing buddy system allocator...\n");
    
    void* p1 = my_malloc_buddy(100);
    void* p2 = my_malloc_buddy(200);
    void* p3 = my_malloc_buddy(50);
    void* p4 = my_malloc_buddy(400);
    void* p5 = my_malloc_buddy(75);
    
    print_buddy_stats();
    
    my_free_buddy(p2);
    my_free_buddy(p4);
    
    printf("\nAfter freeing p2 and p4:");
    print_buddy_stats();
    
    void* p6 = my_malloc_buddy(350);
    printf("\nAllocated 350 bytes after freeing (should reuse merged blocks)");
    
    return 0;
}
```

## Example 15: Memory Allocator with Memory Mapping Simulation

```c
#include <stdio.h>

#define VIRTUAL_MEMORY_SIZE 262144  // 256KB virtual memory
#define PAGE_SIZE 4096
#define NUM_PAGES 64
#define MAX_ALLOCATIONS 1000

typedef struct Page {
    int page_num;
    int is_mapped;
    int is_free;
    int allocation_id;
    struct Page* next_free;
} Page;

typedef struct Block {
    int size;
    int is_free;
    struct Block* next;
    struct Block* prev;
    int alloc_id;
    int magic;
    int start_page;
    int num_pages;
} Block;

#define MAGIC_NUMBER 0x12345678

typedef struct AllocationRecord {
    void* ptr;
    int size;
    int active;
    int start_page;
    int num_pages;
} AllocationRecord;

static char virtual_memory[VIRTUAL_MEMORY_SIZE];
static Page pages[NUM_PAGES];
static Block* free_list = NULL;
static AllocationRecord alloc_records[MAX_ALLOCATIONS];
static int total_allocations = 0;
static int active_allocations = 0;
static int mapped_pages = 0;

void init_page_table() {
    for (int i = 0; i < NUM_PAGES; i++) {
        pages[i].page_num = i;
        pages[i].is_mapped = 0;
        pages[i].is_free = 1;
        pages[i].allocation_id = -1;
        pages[i].next_free = NULL;
    }
}

void init_allocator() {
    init_page_table();
    
    free_list = (Block*)virtual_memory;
    free_list->size = VIRTUAL_MEMORY_SIZE - sizeof(Block);
    free_list->is_free = 1;
    free_list->next = NULL;
    free_list->prev = NULL;
    free_list->alloc_id = -1;
    free_list->magic = MAGIC_NUMBER;
    free_list->start_page = 0;
    free_list->num_pages = NUM_PAGES;
    
    for (int i = 0; i < MAX_ALLOCATIONS; i++) {
        alloc_records[i].active = 0;
    }
}

int map_pages(int start_page, int num_pages, int alloc_id) {
    int pages_mapped = 0;
    
    for (int i = start_page; i < start_page + num_pages && i < NUM_PAGES; i++) {
        if (!pages[i].is_mapped) {
            pages[i].is_mapped = 1;
            pages[i].is_free = 0;
            pages[i].allocation_id = alloc_id;
            mapped_pages++;
            pages_mapped++;
        }
    }
    
    return pages_mapped;
}

void unmap_pages(int alloc_id) {
    for (int i = 0; i < NUM_PAGES; i++) {
        if (pages[i].allocation_id == alloc_id) {
            pages[i].is_mapped = 0;
            pages[i].is_free = 1;
            pages[i].allocation_id = -1;
            mapped_pages--;
        }
    }
}

int get_num_pages(int size) {
    return (size + PAGE_SIZE - 1) / PAGE_SIZE;
}

void* my_malloc_virtual(int size) {
    int aligned_size = size;
    int num_pages = get_num_pages(size + sizeof(Block));
    
    // Find contiguous free pages
    int start_page = -1;
    int consecutive_free = 0;
    
    for (int i = 0; i < NUM_PAGES; i++) {
        if (pages[i].is_free) {
            if (consecutive_free == 0) {
                start_page = i;
            }
            consecutive_free++;
            
            if (consecutive_free >= num_pages) {
                break;
            }
        } else {
            consecutive_free = 0;
            start_page = -1;
        }
    }
    
    if (start_page == -1 || consecutive_free < num_pages) {
        printf("Not enough contiguous virtual memory\n");
        return NULL;
    }
    
    // Find a block in free list
    Block* current = free_list;
    Block* best_fit = NULL;
    int min_size_diff = 999999;
    
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
        if (best_fit->size > aligned_size + sizeof(Block) + PAGE_SIZE) {
            Block* new_block = (Block*)((char*)best_fit + sizeof(Block) + aligned_size);
            new_block->size = best_fit->size - aligned_size - sizeof(Block);
            new_block->is_free = 1;
            new_block->next = best_fit->next;
            new_block->prev = best_fit;
            new_block->alloc_id = -1;
            new_block->magic = MAGIC_NUMBER;
            new_block->start_page = start_page + num_pages;
            new_block->num_pages = best_fit->num_pages - num_pages;
            
            if (best_fit->next != NULL) {
                best_fit->next->prev = new_block;
            }
            
            best_fit->size = aligned_size;
            best_fit->next = new_block;
            best_fit->num_pages = num_pages;
        }
        
        best_fit->is_free = 0;
        best_fit->alloc_id = total_allocations;
        best_fit->magic = MAGIC_NUMBER;
        best_fit->start_page = start_page;
        best_fit->num_pages = num_pages;
        
        // Map pages
        map_pages(start_page, num_pages, total_allocations);
        
        alloc_records[total_allocations].ptr = (void*)((char*)best_fit + sizeof(Block));
        alloc_records[total_allocations].size = aligned_size;
        alloc_records[total_allocations].active = 1;
        alloc_records[total_allocations].start_page = start_page;
        alloc_records[total_allocations].num_pages = num_pages;
        
        void* result = alloc_records[total_allocations].ptr;
        total_allocations++;
        active_allocations++;
        
        return result;
    }
    
    return NULL;
}

void my_free_virtual(void* ptr) {
    if (ptr == NULL) return;
    Block* block = (Block*)((char*)ptr - sizeof(Block));
    
    if (block->magic != MAGIC_NUMBER) {
        printf("Memory corruption detected!\n");
        return;
    }
    
    // Unmap pages
    unmap_pages(block->alloc_id);
    
    if (block->alloc_id >= 0 && block->alloc_id < total_allocations) {
        alloc_records[block->alloc_id].active = 0;
        active_allocations--;
    }
    
    block->is_free = 1;
}

void print_virtual_memory_map() {
    printf("\n=== Virtual Memory Map ===\n");
    printf("Total pages: %d\n", NUM_PAGES);
    printf("Mapped pages: %d\n", mapped_pages);
    printf("Free pages: %d\n", NUM_PAGES - mapped_pages);
    
    printf("\nPage map (M=mapped, F=free):\n");
    for (int i = 0; i < NUM_PAGES; i++) {
        if (i % 16 == 0) printf("\nPage %2d-%2d: ", i, i + 15);
        
        if (pages[i].is_mapped) {
            printf("M");
        } else {
            printf("F");
        }
        
        if ((i + 1) % 16 != 0 && i != NUM_PAGES - 1) {
            printf(" ");
        }
    }
    printf("\n\n");
    
    printf("Active allocations:\n");
    for (int i = 0; i < total_allocations; i++) {
        if (alloc_records[i].active) {
            printf("  Alloc #%d: %d bytes, pages %d-%d\n", 
                   i, alloc_records[i].size,
                   alloc_records[i].start_page,
                   alloc_records[i].start_page + alloc_records[i].num_pages - 1);
        }
    }
    printf("===========================\n");
}

int main() {
    init_allocator();
    
    printf("Testing virtual memory allocator...\n");
    
    void* p1 = my_malloc_virtual(5000);
    void* p2 = my_malloc_virtual(10000);
    void* p3 = my_malloc_virtual(2000);
    
    print_virtual_memory_map();
    
    my_free_virtual(p2);
    
    printf("\nAfter freeing p2 (10KB):");
    print_virtual_memory_map();
    
    void* p4 = my_malloc_virtual(8000);
    
    printf("\nAfter allocating 8KB (should reuse freed pages):");
    print_virtual_memory_map();
    
    return 0;
}
```

These 5 advanced examples cover:

1. **Thread Safety**: Implementing locks for concurrent access
2. **Garbage Collection**: Reference counting for automatic memory management
3. **Defragmentation**: Compacting memory to reduce fragmentation
4. **Buddy System**: Efficient power-of-2 block allocation
5. **Virtual Memory**: Simulating page-based memory mapping

Each example introduces a completely different aspect of memory management that real-world allocators use. You now understand how professional memory allocators work under the hood!
