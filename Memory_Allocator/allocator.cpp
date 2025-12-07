#include <iostream>
#include <Windows.h>

#define ALIGNMENT 16
#define MAGIC 144553566
#define MIN_BLOCK_SIZE 32
// gets the size requested, + the alignment - 1 and then applies a bit mask to clear the lower bits and make sure that it is multiple of 16
#define ALIGN(size) (((size) + (ALIGNMENT -1 ) & ~(ALIGNMENT - 1)))


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



/*/-------------------------------------\\
			HELPER FUNCTIONS
\\-------------------------------------/*/
// gets the actual memory location (after the header) of the region allocated
static void* header_to_payload(MemoryBlockHeader* header) {
	return(void*)((char*)header + sizeof(MemoryBlockHeader));
}

struct MemoryBlockHeader* find_prev_block(MemoryBlockHeader* target) {
	MemoryBlockHeader* current = heap_head;
	while (current != NULL && current->next != target) {
		current = current->next;
	}
	return current;
}

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



/*/-------------------------------------\\
			ALLOCATOR FUNCTIONS
\\-------------------------------------/*/



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
	block->size = total_size - sizeof(MemoryBlockHeader);
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
			// split logic
			size_t remaining_size = block->size - size;														  
			if (remaining_size >= ALIGN(sizeof(MemoryBlockHeader) + MIN_BLOCK_SIZE)){						  // splitting is worthwhile (if remaining size is > than minimum size)
				MemoryBlockHeader* new_header = (MemoryBlockHeader*)((char*)header_to_payload(block) + size); // create a new header for new block
				block->size = size;																			  
				new_header->size = remaining_size - sizeof(MemoryBlockHeader);								  
				new_header->magic = MAGIC;																	  
				new_header->is_free = true;																	  
				new_header->next = block->next;																  
				block->next = new_header;																	  
			}																								 
			block->is_free = false;																			 
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
	std::cout << "allocated " << block->size << " bytes ";

	return header_to_payload(block);
}

void free_mem(void* ptr) {
	if (ptr == NULL) {
		return;
	}

	struct MemoryBlockHeader* header = (struct MemoryBlockHeader*)((char*)ptr - sizeof(MemoryBlockHeader));
	if (header->is_free) {
		std::cout << "DOUBLE FREE DETECTED\n";
		return;
	}
	header->is_free = true;

	// coalesce blocks after the free
	MemoryBlockHeader* next_header = header->next;
	if (next_header != NULL && next_header->is_free) {								   // if next header is valid, and is free
		if ((char*)next_header == (char*)header_to_payload(header) + header->size) {   // check the memory is contigious
			header->size += sizeof(MemoryBlockHeader) + next_header->size;			   // merge
			header->next = next_header->next;										   // merge
		}
	}

	// coalesce blocks before the free
	MemoryBlockHeader* before_header = find_prev_block(header);
	if (before_header != NULL && before_header->is_free) {
		if ((char*)header == (char*)header_to_payload(before_header) + before_header->size) {   // check the memory is contigious
			before_header->size += sizeof(MemoryBlockHeader) + header->size;			   // merge
			before_header->next = before_header->next;										   // merge
			header = before_header;
		}
	}


	std::cout << "freed " << header->size << " bytes at location " << ptr;
}

void dump_heap() {
	std::cout << "\n--- HEAP DUMP ---\n";
	MemoryBlockHeader* cur = heap_head;
	int i = 0;
	while (cur) {
		std::cout << "Block " << i++
			<< " | header: " << cur
			<< " | payload: " << (void*)((char*)cur + sizeof(MemoryBlockHeader))
			<< " | size: " << cur->size
			<< " | is_free: " << cur->is_free
			<< " | next: " << cur->next
			<< "\n";
		cur = cur->next;
	}
	std::cout << "-----------------\n";
}

int main() {
	std::cout << "sizeof(MemoryBlockHeader) = " << sizeof(MemoryBlockHeader) << "\n\n";

	std::cout << "\n===== TEST 1: Basic Allocation =====\n";
	void* t1 = allocate_mem(1000);
	dump_heap();
	free_mem(t1);
	dump_heap();

	std::cout << "\n===== TEST 2: Reuse Freed Block =====\n";
	void* t2 = allocate_mem(512);
	dump_heap(); 
	free_mem(t2);
	dump_heap(); 
	void* t3 = allocate_mem(256);   // should reuse and split
	dump_heap(); 
	free_mem(t3);
	dump_heap(); 

	std::cout << "\n===== TEST 3: Multiple Alloc =====\n";
	void* a = allocate_mem(128);
	void* b = allocate_mem(256);
	void* c = allocate_mem(512);
	dump_heap(); 

	free_mem(b);   // free middle
	dump_heap(); 
	free_mem(a);   // backward coalesce test
	dump_heap(); 
	free_mem(c);   // forward coalesce test
	dump_heap(); 

	std::cout << "\n===== TEST 4: Large After Coalesce =====\n";
	void* big = allocate_mem(900);
	dump_heap(); 
	free_mem(big);
	dump_heap(); 

	std::cout << "\n===== TEST 5: Fragmentation Pattern =====\n";
	void* f1 = allocate_mem(64);
	void* f2 = allocate_mem(64);
	void* f3 = allocate_mem(64);
	void* f4 = allocate_mem(64);
	dump_heap(); 

	free_mem(f2);
	free_mem(f4);
	dump_heap(); 

	void* f5 = allocate_mem(60);  // should reuse f2
	void* f6 = allocate_mem(60);  // should reuse f4
	dump_heap();

	free_mem(f1);
	free_mem(f3);
	free_mem(f5);
	free_mem(f6);
	dump_heap();

	std::cout << "\n===== TEST 6: Double Free Detection Test =====\n";
	void* d1 = allocate_mem(100);
	dump_heap(); 
	free_mem(d1);
	dump_heap();
	free_mem(d1);  // should print DOUBLE FREE DETECTED
	dump_heap();

	std::cout << "\n===== ALL TESTS COMPLETED =====\n";
	return 0;
}

