#include <unistd.h>
#include <stdio.h>
#include "bmalloc.h"
#include <sys/mman.h>

bm_option bm_mode = BestFit ;
bm_header bm_list_head = {0, 0, 0x0 } ;

void * sibling (void * h)
{
	// returns the header address of the suspected sibling block of h.
	bm_header_ptr curr_header = (bm_header_ptr) h;
	size_t size_index = bm_list_head.size;

	bm_header_ptr itr ;
	for (itr = bm_list_head.next ; itr != 0x0 ; itr = itr->next) {
		size_index += itr->size;

		if (itr->next == curr_header) {
			int LR = (size_index / curr_header->size) % 2;
			/* 0 : h is left node   1 : h is right node */
			if (!LR) { 	// left node
				if (curr_header->size == curr_header->next->size) {
					return curr_header->next;
				}
			} else {	// right node
				if (itr->size == curr_header->size) {
					return itr;
				}
			}
			return NULL;
		}
	}
}

int fitting (size_t s) 
{
	// returns the size field value of a fitting block to accommodate s bytes.
	int size;
	for (size = 4096; size >= 16; size >> 1) {
		if (s > (size >> 1) - 9) {
			return size;
		}
	}
	return -1;
}

void * bmalloc (size_t s) 
{
	// allocates a buffer of s-bytes and returns its starting address.

	////////////////////////////////
	// Put the header of the block to be allocated according to the bm_mode into the fit_header.
	// * If bm_list_head.next == 0x0, fit_header becomes 0x0 because it does not execute the for loop.
	size_t fit_size = fitting(s);
	size_t fit_block = 8192;
	bm_header_ptr fit_header = 0x0;						// Variable to find the least size block among allocable blocks when Bestfit.				
	bm_header_ptr prv_header = 0x0;						// Save the previous header for brealloc.
	bm_header_ptr itr ;
	for (itr = bm_list_head.next ; itr != 0x0 ; itr = itr->next) {
		if (itr->size >= fit_size && itr->used == 0) {	// If it can be assigned to itr
			if (bm_mode == BestFit) {					// BestFit
				if (fit_block > fit_size) {
					fit_block = fit_size;
					fit_header = itr;
				}
				if (itr->size == fit_size) break;
			} else {									// FirstFit
				fit_header = itr;
				break;
			}
			prv_header = itr;
		}
	}
	/////////////////////////////////
	// Create new page
	if (fit_header == 0x0) {
		void* addr = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS, -1, 0);
		bm_header_ptr curr_header = (bm_header_ptr)addr;
		curr_header->used = 0;
		curr_header->size = 12;
		curr_header->next = NULL;
		
		void* payload = addr + sizeof(bm_header);

		if (bm_list_head.next == 0x0) {					// When you first create a page
			bm_list_head.next = payload;				
		} else prv_header->next->next = payload;		// When you create the next page (prv_header->next->next = 0x0)
	}
	/////////////////////////////////
	// Split up to fitting size and allocate it in
	void* original_next = fit_header->next;
	size_t i;
	for (i = fit_header->size; i >= fit_size; i >> 1) {
		// Make right side block
		void* next_addr = mmap(NULL, i >> 1, PROT_READ | PROT_WRITE, MAP_ANONYMOUS, -1, 0);
		int x = 0;
		size_t t = i >> 1;
		while (t > 1) {
			t >>= 1;
			x++;
		}
		bm_header_ptr next_header = (bm_header_ptr)next_addr;
		next_header->used = 0;
		next_header->size = x;
		next_header->next = original_next;

		// Make left side block
		void* curr_addr = brealloc(prv_header, i >> 1);
		bm_header_ptr curr_header = (bm_header_ptr)curr_addr;
		curr_header->next = next_addr;

		original_next = next_addr;
	}

	void* return_addr = prv_header->next + sizeof(bm_header);
	
	return return_addr;
}

void bfree (void * p) 
{
	// free the allocated buffer starting at pointer p.
}

void * brealloc (void * p, size_t s) 
{
	// resize the allocated memory buffer into s bytes.
	// As the result of this operation, the data may be immigrated to a different address, as like realloc possibly does.

	return 0x0 ; // erase this 
}

void bmconfig (bm_option opt) 
{
	// set the space management scheme as BestFit, or FirstFit.
	bm_mode = opt;
}


void 
bmprint () 
{
	// print out the internal status of the block list to the standard output.
	bm_header_ptr itr ;
	int i ;

	printf("==================== bm_list ====================\n") ;
	for (itr = bm_list_head.next, i = 0 ; itr != 0x0 ; itr = itr->next, i++) {
		printf("%3d:%p:%1d %8d:", i, ((void *) itr) + sizeof(bm_header), (int)itr->used, (int) itr->size) ;

		int j ;
		char * s = ((char *) itr) + sizeof(bm_header) ;
		for (j = 0 ; j < (itr->size >= 8 ? 8 : itr->size) ; j++) 
			printf("%02x ", s[j]) ;
		printf("\n") ;
	}
	printf("=================================================\n") ;

	//TODO: print out the stat's.

}
