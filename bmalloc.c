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
	printf("	sibling		%p\n", h);
	bm_header_ptr curr_header = (bm_header_ptr) h;
	printf("sibling size: %d\n", curr_header->size);
	size_t size_index = 0;
	if (curr_header == bm_list_head.next) {
		if (curr_header->size == curr_header->next->size && curr_header->size != 12) return curr_header->next;
		return NULL;
	}
	bm_header_ptr itr ;
	bm_header_ptr prv = NULL;
	for (itr = bm_list_head.next ; itr != 0x0 ; itr = itr->next) {
		printf("===sibling===%p\n", itr);
		size_index += expoToNum(itr->size);
		printf("---sibling---%p\n", curr_header);

		if (itr->next == curr_header && curr_header->size != 12) {
			int LR = (size_index / expoToNum(curr_header->size)) % 2;
			/* 0 : h is left node   1 : h is right node */
			if (!LR) { 	// left node
				if (curr_header->size == curr_header->next->size) {
					bm_header_ptr s = curr_header->next;
					printf("##		curr / curr->next / s->next		%p / %p / %p \n", curr_header, curr_header->next, s->next);
					curr_header->next = s->next;
					s->next = prv->next;
					printf("##		curr / curr->next / s->next		%p / %p / %p \n", curr_header, curr_header->next, s->next);
					printf("sibling : return s\n");
					return s;
				}
			} else {	// right node
				if (itr->size == curr_header->size) {
					printf("##		itr / itr->next / curr / curr->next			%p/ %p / %p / %p \n", itr, itr->next, curr_header, curr_header->next);
					itr->next = prv;
					printf("##		itr / itr->next / curr / curr->next			%p/ %p / %p / %p \n", itr, itr->next, curr_header, curr_header->next);
					printf("sibling : return itr\n");
					return itr;
				}
			}
			printf("sibling : return NULL\n");
			return NULL;
		}
		printf("sibling : prv = itr\n");
		prv = itr ;
		if (itr->next == curr_header && curr_header->size == 12) {
			printf("sibling : return prv\n");
			return (void*)prv;
		}
	}
	// printf("why!\n");
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
	// printf("DEBUG: start bmalloc\n");
	////////////////////////////////
	// Put the header of the block to be allocated according to the bm_mode into the fit_header.
	// * If bm_list_head.next == 0x0, fit_header becomes 0x0 because it does not execute the for loop.
	// printf("DEBUG: start fitting\n");
	size_t fit_size = fitting(s);
	// printf("DEBUG: end fitting\n");
	size_t fit_block = 8192;
	bm_header_ptr fit_header = 0x0;						// Variable to find the least size block among allocable blocks when Bestfit.				
	bm_header_ptr prv_header = 0x0;						// Save the previous header for brealloc.
	bm_header_ptr tmp = 0x0;
	bm_header_ptr itr ;
	for (itr = bm_list_head.next ; itr != 0x0 ; itr = itr->next) {
		// printf("forforfro\n");
		// printf("%d >= %ld\n", itr->size, fit_size);
		if (expoToNum(itr->size) >= fit_size && itr->used == 0) {	// If it can be assigned to itr
			// printf("ififififi\n");
			if (bm_mode == BestFit) {					// BestFit
				if (fit_block > fit_size) {
					// printf("?\n");
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
	// printf("DEBUG: start create new page %p\n", fit_header);
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
	// printf("DEBUG: split and allocate %p\n", fit_header);
	// Split up to fitting size and allocate it in
	void* original_next = fit_header->next;
	void* curr_addr = NULL;
	size_t i;
	// printf("%p\n", fit_header);
	// printf("before for! %p\n", fit_header->next);
	// printf("%ld\n", numToExpo(fit_size));
	// printf("fit_size : %ld, expotonum: %ld\n", fit_size, numToExpo(fit_size));
	for (i = fit_header->size; i > numToExpo(fit_size); i--) {
		// printf("start for!\n");
		// Make right side block
		void* next_addr = mmap(NULL, expoToNum(i-1), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		bm_header_ptr next_header = (bm_header_ptr)next_addr;
		next_header->used = 0;
		next_header->size = i-1;
		next_header->next = original_next;
		// printf("DEBUG: %ld / finish right side\n", i);

		// Make left side block
		// printf("%d\n", bm_list_head.next->used);
		curr_addr = brealloc(prv_header != 0x0 ? prv_header->next : bm_list_head.next, expoToNum(i-1));  				// If prv_header is 0x0, we realloc bm_list_head.next
		// printf("finish realloc\n");
		bm_header_ptr curr_header = (bm_header_ptr)curr_addr;
		curr_header->used = 1;
		curr_header->size = i-1;
		curr_header->next = next_addr;
		if (prv_header != 0) {
			// printf("$$%p\n", prv_header);
			prv_header->next = curr_addr;
		} else bm_list_head.next = curr_header;
		original_next = next_addr;
		// printf("DEBUG: %ld / finish left side\n", i);
	}
	// printf("finish for\n");
	// printf("%p\n", prv_header);
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
	
	// printf("we will return this!! %p\n", return_addr);
	return return_addr;
}

void bfree (void * p) 
{
	// free the allocated buffer starting at pointer p.
	
	void* cp = p - sizeof(bm_header);
	// printf("bfree s: %ld\n", sizeof(bm_header));
	// printf("bfree cp: %p\n", cp);
	while (1) {
		printf("bfree: start\n");
		bm_header_ptr curr_header = (bm_header_ptr) cp;
		bm_header_ptr s = (bm_header_ptr) sibling(cp);
		printf("finish sibling : %p\n", s);
		if (curr_header->size == 12) {
			// 여기 munmap 해야 됨!
			bm_header_ptr cp_next = curr_header->next;
			void* clear = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
			bm_header_ptr clear_header = (bm_header_ptr) clear;
			clear_header->used = 0;
			clear_header->size = 12;
			clear_header->next = cp_next;
			curr_header->next = NULL;
			munmap(cp, 4096);
			// 이전거 링크하는 기능
			printf("s->next : %p\n", s);
			if(s != NULL) s->next = clear_header;
			else bm_list_head.next = clear_header;
			printf("12 break\n");
			break;
		}
		if (s == NULL) {
			// printf("null break\n");
			break;
		}
		bm_header_ptr prv = NULL;
		int LR = 0;
		printf("		s->next-next / cp / s		%p / %p / %p\n", s->next->next, cp, s);
		if (s->next->next == cp) { 					// p is left
			LR = 0;
			prv = s->next;
			s->next = curr_header->next;
			curr_header->next = s;
		} else if (s->next->next == s) {			// p is right
			LR = 1;
			prv = s->next;
			s->next = cp;
		} else {
			printf("NO TRICK@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
		}

		printf("bfree: merge\n");

		void* merge = mmap(NULL, expoToNum(curr_header->size) * 2, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		printf("merge pointer : %p\n", merge);
		bm_header_ptr merge_header = (bm_header_ptr)merge;
		merge_header->used = 0;
		merge_header->size = curr_header->size + 1;

		if (!LR) {			// cp is left
			if(prv != NULL) prv->next = merge;
			else bm_list_head.next = merge;
			merge_header->next = s->next;
		} else {			// cp is right
			if(prv != NULL) prv->next = merge;
			else bm_list_head.next = merge;
			merge_header->next = curr_header->next;
		}

		void* old_cp = cp;
		printf("munmap: %p\n", old_cp);
		munmap(old_cp, expoToNum(((bm_header_ptr)old_cp)->size));
		printf("munmap: %p\n", s);
		munmap(s, expoToNum(((bm_header_ptr)s)->size));

		cp = merge;
	}
}

void * brealloc (void * p, size_t s) 
{
	// resize the allocated memory buffer into s bytes.
	// As the result of this operation, the data may be immigrated to a different address, as like realloc possibly does.
	// printf("start realloc--------------------------\n");
	bm_header_ptr curr_header = (bm_header_ptr) p;
	bm_header curr = *curr_header;
	void* addr = mmap(NULL, s, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	// printf("addr in realloc %p\n", addr);
	bm_header_ptr new_header = (bm_header_ptr) addr;
	new_header->used = curr.used;
	new_header->size = numToExpo(s);
	// printf("!!!%p\n", curr.next);
	if (curr.size < numToExpo(s)) {
		new_header->next = curr.next->next;
	} else {
		new_header->next = curr.next;
	}
	// printf("end realloc--------------------------\n");
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
		printf("next: %p\n", itr->next);
	}
	printf("=================================================\n") ;

	//TODO: print out the stat's.
	int total_memory = 0;
	int given_memory = 0;
	int available_memory = 0;
	int inter_frag = 0;
	for (itr = bm_list_head.next, i = 0 ; itr != 0x0 ; itr = itr->next, i++) {
		
	}
}
