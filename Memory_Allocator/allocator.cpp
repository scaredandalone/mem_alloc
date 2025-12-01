#include <iostream>
#include <Windows.h>

#define ALIGNMENT 16
#define MAGIC 144553566
#define MIN_BLOCK_SIZE 32
#define ALIGN(size) (((size) + (ALIGNMENT -1 ) & ~(ALIGNMENT - 1)))
// gets the size requested, + the alignment - 1 and then applies a bit mask to clear the lower bits and make sure that it is multiple of 16


/*/-------------------------------------\\
			BLOCK HEADER STUFF
\\-------------------------------------/*/

struct alignas(16) MemoryBlockHeader
{
	size_t size;
	int magic;
	bool is_free;
	struct MemoryBlockHeader* next;
};

static MemoryBlockHeader* heap_head = NULL;
static void* header_to_payload(MemoryBlockHeader* header) {
	return(void*)((char*)header + sizeof(MemoryBlockHeader));
}
// gets the actual memory location (after the header) of the region allocated



/*/-------------------------------------\\
			ALLOCATOR FUNCTIONS
\\-------------------------------------/*/
static MemoryBlockHeader* find_free_block(size_t size) {
	MemoryBlockHeader* current = heap_head;
	while (current != NULL) {
		if (current->is_free && current->size >= size) {
			return current;
		}
		current = current->next;
	}
	return NULL;
}

static MemoryBlockHeader* request_space(size_t size) {
	MemoryBlockHeader* block;
	size_t total_size = ALIGN(sizeof(MemoryBlockHeader) + size);
	size_t page_size = 65536; // virtualAlloc will give us 64kb anyways, so we just specify it here.
	if (total_size < page_size) {
		total_size = page_size;
	}
	block = (MemoryBlockHeader*)VirtualAlloc(
		NULL,
		total_size,
		MEM_COMMIT | MEM_RESERVE,
		PAGE_READWRITE
	);
	if (block == NULL) {
		// virtual alloc failed 
		return NULL;
	}
	block->size = size;
	block->magic = MAGIC;
	block->is_free = FALSE;
	block->next = NULL;

	return block;
}

void* allocate_mem(size_t size) {
	MemoryBlockHeader* block;
	if (size == 0) {
		return NULL;
	}
	size = ALIGN(size);
	if (size < MIN_BLOCK_SIZE) {
		size = MIN_BLOCK_SIZE;
	}
	if (heap_head == NULL) {
		block = request_space(size);
		if (!block) {
			return NULL;
		}
		heap_head = block;
	}
	else {
		block = find_free_block(size);
		if (block) {
			block->is_free = false;

			// split logic here (later)
		} else {
			block = request_space(size);
			if (!block) {
				return NULL;
			}
			MemoryBlockHeader* current = heap_head;
			while (current->next != NULL) { // while the next node is not null
				current = current->next;	// move through the list
			}
			current->next = block;			// if next node is null, set next to the new block (reached the bottom of the list)
		}
	}
	return header_to_payload(block);
}


int main() {
	void* test = allocate_mem(1000);
	MemoryBlockHeader* header = (MemoryBlockHeader*)((char*)test - sizeof(MemoryBlockHeader));
	std::cout << "Allocated " << header->size << " bytes at " << test << "\n";

}

