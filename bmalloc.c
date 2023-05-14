#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include "bmalloc.h"
#include <sys/mman.h>

bm_option bm_mode = BestFit ;
bm_header bm_list_head = {0, 0, 0x0 } ;

size_t numToExpo(size_t n) {
   size_t res = 0;
   while (n > 1) {
      n >>= 1;
      res++;
   }
   return res;
}

size_t expoToNum(size_t n) {
    size_t result = 1;
	size_t i;
    for (i = 0; i < n; i++) {
        result *= 2;
    }
    return result;
}

void * sibling (void * h)
{
	// returns the header address of the suspected sibling block of h.
	bm_header_ptr curr_header = (bm_header_ptr) h;
	size_t size_index = 0;
	if (curr_header == bm_list_head.next) {
		if (curr_header->size == (curr_header->next != NULL ? curr_header->next->size : curr_header->size) && curr_header->size != 12) return curr_header->next;
		return NULL;
	}
	bm_header_ptr itr ;
	bm_header_ptr prv = NULL;
	for (itr = bm_list_head.next ; itr != 0x0 ; itr = itr->next) {
		size_index += expoToNum(itr->size);

		if (itr->next == curr_header && curr_header->size != 12) {
			int LR = (size_index / expoToNum(curr_header->size)) % 2;
			/* 0 : h is left node   1 : h is right node */
			if (!LR) { 	// left node
				if (curr_header->size == curr_header->next->size) {
					bm_header_ptr s = curr_header->next;
					return s;
				}
			} else {	// right node
				if (itr->size == curr_header->size) {
					return itr;
				}
			}
			return NULL;
		}
		prv = itr ;
		if (itr->next == curr_header && curr_header->size == 12) {
			return NULL;
		}
	}
	printf("sibling : return whyNULL\n");
	return NULL;
}

size_t fitting (size_t s) 
{
	// returns the size field value of a fitting block to accommodate s bytes.
	size_t size;
	for (size = 4096; size >= 16; size >>= 1) {
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
	bm_header_ptr tmp = 0x0;
	bm_header_ptr itr ;
	for (itr = bm_list_head.next ; itr != 0x0 ; itr = itr->next) {
		if (expoToNum(itr->size) >= fit_size && itr->used == 0) {	// If it can be assigned to itr
			if (bm_mode == BestFit) {					// BestFit
				if (fit_block > fit_size) {
					fit_block = fit_size;
					fit_header = itr;
					prv_header = tmp;
				}
				if (itr->size == fit_size) break;
			} else {									// FirstFit
				fit_header = itr;
				break;
			}
		}
		tmp = itr;
	}
	/////////////////////////////////
	// Create new page
	if (fit_header == NULL) {
		void* addr = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		bm_header_ptr curr_header = (bm_header_ptr)addr;
		curr_header->used = 1;
		curr_header->size = 12;
		curr_header->next = NULL;

		if (bm_list_head.next == 0x0) {					// When you first create a page
			bm_list_head.next = addr;
			fit_header = addr;				
		} else {										// When you create the next page
			tmp->next = addr;
			prv_header = tmp;					
			fit_header = addr;
		}
	}
	/////////////////////////////////
	// Split up to fitting size and allocate it in
	void* original_next = fit_header->next;
	void* curr_addr = NULL;
	size_t i;
	for (i = fit_header->size; i > numToExpo(fit_size); i--) {
		// Make right side block
		void* next_addr = mmap(NULL, expoToNum(i-1), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		bm_header_ptr next_header = (bm_header_ptr)next_addr;
		next_header->used = 0;
		next_header->size = i-1;
		next_header->next = original_next;

		// Make left side block
		curr_addr = brealloc(prv_header != 0x0 ? prv_header->next : bm_list_head.next, expoToNum(i-1));  				// If prv_header is 0x0, we realloc bm_list_head.next
		bm_header_ptr curr_header = (bm_header_ptr)curr_addr;
		curr_header->used = 1;
		curr_header->size = i-1;
		curr_header->next = next_addr;
		if (prv_header != 0) {
			prv_header->next = curr_addr;
		} else bm_list_head.next = curr_header;
		original_next = next_addr;
	}

	void* return_addr;
	if (curr_addr == bm_list_head.next) {
		bm_header_ptr c = (bm_header_ptr)curr_addr;
		c->used = 1;
		return_addr = curr_addr + sizeof(bm_header);
	}
	else {
		bm_header_ptr c = (bm_header_ptr)(prv_header != NULL ? prv_header->next : fit_header);
		c->used = 1;
		return_addr = (void *)(prv_header != NULL ? prv_header->next : fit_header) + sizeof(bm_header);
	}
	
	return return_addr;
}

void bfree (void * p) 
{
	// free the allocated buffer starting at pointer p.
	
	void* cp = p - sizeof(bm_header);
	int first = 1;
	while (1) {
		bm_header_ptr curr_header = (bm_header_ptr) cp;
		bm_header_ptr s = (bm_header_ptr) sibling(cp);
		////////////////////////////////////////////////////
		// get prv
		size_t size_index = 0;
		int LR = 0;
		bm_header_ptr itr ;
		bm_header_ptr prv = NULL;
		for (itr = bm_list_head.next ; itr != 0x0 ; itr = itr->next) { // get prv
			size_index += expoToNum(itr->size);

			if (itr->next == curr_header && curr_header->size != 12) {
				LR = (size_index / expoToNum(curr_header->size)) % 2;
				/* 0 : curr is left node   1 : curr is right node */
				if (!LR) {		// left
					prv = itr;
				}
				break;
			}
			if (bm_list_head.next != curr_header) prv = itr ;
		}
		/////////////////////////////////////////////////////
		// sibling == null or sibling->used == 1
		if (s == NULL || (s != NULL && s->used == 1)) {
			// printf("null || used break\n");
			if(first == 1) {
				curr_header->used = 0;
			}
			break;
		}
		/////////////////////////////////////////////////////////
		// max size
		if (curr_header->size == 12) {
			prv->next = curr_header->next;
			munmap(cp, 4096);
			break;
		}

		////////////////////////////////////////////////////////////
		// get prv in sibling without for loop (delete)

		bm_header cp_header = *curr_header;
		bm_header s_header = *(bm_header_ptr)s;

		void* old_cp = cp;
		munmap(old_cp, expoToNum(((bm_header_ptr)old_cp)->size));
		munmap(s, expoToNum(((bm_header_ptr)s)->size));

		void* merge = mmap(cp, expoToNum(cp_header.size) * 2, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		bm_header_ptr merge_header = (bm_header_ptr)merge;
		merge_header->used = 0;
		merge_header->size = cp_header.size + 1;

		if (!LR) {			// cp is left
			if(prv != NULL) {
				prv->next = merge_header;
			}
			else {
				bm_list_head.next = merge_header;
			}
			merge_header->next = s_header.next;
		} else {			// cp is right
			if(prv != NULL) {
				prv->next = merge_header;
			}
			else {
				bm_list_head.next = merge_header;
			}
			merge_header->next = cp_header.next;
		}

		cp = merge;
		first = 0;
	}
}

void * brealloc (void * p, size_t s) 
{
	// resize the allocated memory buffer into s bytes.
	// As the result of this operation, the data may be immigrated to a different address, as like realloc possibly does.
	bm_header_ptr curr_header = (bm_header_ptr) p;
	bm_header curr = *curr_header;
	void* addr = mmap(NULL, s, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	bm_header_ptr new_header = (bm_header_ptr) addr;
	new_header->used = curr.used;
	new_header->size = numToExpo(s);
	if (curr.size < numToExpo(s)) {
		new_header->next = curr.next->next;
	} else {
		new_header->next = curr.next;
	}
	munmap(p, numToExpo(curr_header->size));
	return addr ;
}

void bmconfig (bm_option opt) 
{
	// set the space management scheme as BestFit, or FirstFit.
	bm_mode = opt;
}


void 
bmprint () 
{
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

	size_t total_memory = 0;
	size_t given_memory = 0;
	size_t available_memory = 0;
	for (itr = bm_list_head.next, i = 0 ; itr != 0x0 ; itr = itr->next, i++) {
		total_memory += expoToNum(itr->size);
		if (itr->used == 1) given_memory += expoToNum(itr->size);
		
	}
	available_memory = total_memory - given_memory;

	printf("The total amount of all given memory: %ld\n", total_memory);
	printf("The total amount memory given to the users: %ld\n", given_memory);
	printf("The total amount available memory: %ld\n", available_memory);
	printf("=================================================\n\n") ;
}
