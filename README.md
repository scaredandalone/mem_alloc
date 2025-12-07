### Memory Allocator Interpretation

This is my memory allocator interpretation, based off the contents of "Operating Systems 3 Easy Pieces", in particular the section dedicated to Memory APIs.
Implements a singly linked list to track all allocations and free segments within the heap.
Utilises Windows system calls, ( VirtualAlloc() ) to request additional memory from the operating system when needed.
Appends a struct to memory segments (header block) in order to track the size of the segment, and the locations (ptr) of the next segment.
Includes a next_free ptr within each free block (if the block isnt free, next_free is NULL).



#### SWITCH TO MASTER BRANCH TO VIEW THE SOURCE!
