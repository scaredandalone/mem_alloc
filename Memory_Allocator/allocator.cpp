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
	MemoryBlockHeader* next;
	MemoryBlockHeader* next_free;

};

static MemoryBlockHeader* heap_head = NULL;
static MemoryBlockHeader* free_list_head = NULL;




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

static void remove_from_free_list(MemoryBlockHeader* target) {
	if (!target) return;

	MemoryBlockHeader* prev = nullptr;
	MemoryBlockHeader* cur = free_list_head;

	while (cur && cur != target) {
		prev = cur;
		cur = cur->next_free;
	}

	if (cur == target) {
		if (prev) {
			prev->next_free = cur->next_free;
		}
		else {
			free_list_head = cur->next_free;
		}
		target->next_free = NULL; // Detach the block
	}
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
	block->next_free = NULL;
	block->next = NULL;

	return block;
}

void* allocate_mem(size_t size) {
	MemoryBlockHeader* block = NULL;

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
		// Try to find a suitable free block
		MemoryBlockHeader* prev_free = NULL;
		MemoryBlockHeader* current_free = free_list_head;
		while (current_free && current_free->size < size) {
			prev_free = current_free;
			current_free = current_free->next_free;
		}

		if (current_free) {
			block = current_free;
			if (prev_free) {
				prev_free->next_free = current_free->next_free;
			}
			else {
				free_list_head = block->next_free;
			}
			block->next_free = NULL;
			block->is_free = false;

			// splitting
			size_t remaining_size = block->size - size;
			if (remaining_size >= ALIGN(sizeof(MemoryBlockHeader) + MIN_BLOCK_SIZE)) {
				MemoryBlockHeader* new_header = (MemoryBlockHeader*)((char*)header_to_payload(block) + size);
				block->size = size;
				new_header->size = remaining_size - sizeof(MemoryBlockHeader);
				new_header->magic = MAGIC;
				new_header->is_free = true;
				new_header->next = block->next;
				new_header->next_free = free_list_head;
				free_list_head = new_header;
				block->next = new_header;
			}
			block->is_free = false;
		}
	}

	// If no suitable free block was found, request new space and append to heap list
	if (!block) {
		block = request_space(size);
		if (!block) {
			return NULL;
		}
		// Append to the end of the heap list
		MemoryBlockHeader* current = heap_head;
		while (current->next != NULL) {
			current = current->next;
		}
		current->next = block;
	}

	std::cout << "allocated " << block->size << " bytes ";
	return header_to_payload(block);
}

void free_mem(void* ptr) {
	if (!ptr) return;

	MemoryBlockHeader* header =
		(MemoryBlockHeader*)((char*)ptr - sizeof(MemoryBlockHeader));

	if (header->is_free) {
		std::cout << "DOUBLE FREE DETECTED\n";
		return;
	}

	header->is_free = true;



	// forward coalesce
	MemoryBlockHeader* next_header = header->next;

	if (next_header && next_header->is_free &&
		(char*)next_header == (char*)header_to_payload(header) + header->size)
	{
		remove_from_free_list(next_header);

		header->size += sizeof(MemoryBlockHeader) + next_header->size;
		header->next = next_header->next;
		next_header->next = NULL;
	}

	// backwards coalesce
	MemoryBlockHeader* before_header = find_prev_block(header);


	if (before_header && before_header->is_free &&
		(char*)header == (char*)header_to_payload(before_header) + before_header->size)
	{

		remove_from_free_list(before_header);
		before_header->size += sizeof(MemoryBlockHeader) + header->size;
		before_header->next = header->next;

		header->next = NULL;
		header = before_header;
	}

	header->next_free = free_list_head;
	free_list_head = header;

	std::cout << "freed " << header->size << " bytes at location " << ptr << "\n";
}


void dump_heap() {
	std::cout << "\n======= HEAP LIST =======\n";
	MemoryBlockHeader* cur = heap_head;
	int i = 0;
	if (!cur) {
		std::cout << "Heap is empty.\n";
	}

	while (cur) {
		std::cout << "Block " << i++
			<< " | header: " << cur
			<< " | payload: " << header_to_payload(cur)
			<< " | size: " << cur->size
			<< " | is_free: " << cur->is_free
			<< " | next: " << cur->next
			<< " | next_free: " << cur->next_free
			<< "\n";
		cur = cur->next;
	}

	std::cout << "\n======= FREE LIST  =======\n";
	MemoryBlockHeader* cur_free = free_list_head;
	int j = 0;

	if (!cur_free) {
		std::cout << "Free list is empty.\n";
	}

	while (cur_free) {
		std::cout << "Free Block " << j++
			<< " | address: " << cur_free
			<< " | size: " << cur_free->size
			<< " | **IS_FREE**: " << (cur_free->is_free ? "YES" : "NO") // Should always be YES
			<< " | next_free: " << cur_free->next_free
			<< "\n";
		cur_free = cur_free->next_free;
	}
	std::cout << "========================================\n";
}



int main() {
	std::cout << "sizeof(MemoryBlockHeader) = " << sizeof(MemoryBlockHeader) << "\n\n";
	dump_heap();
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

