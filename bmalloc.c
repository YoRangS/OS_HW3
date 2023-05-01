#include <unistd.h>
#include <stdio.h>
#include "bmalloc.h" 

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
			int LR = (size_index / itr->size) % 2;
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
	for (size = 4096; size >= 16; size >> 2) {
		if (s >= size) {
			return size;
		}
	}
	return -1;
}

void * bmalloc (size_t s) 
{
	// allocates a buffer of s-bytes and returns its starting address.

	return 0x0 ; // erase this
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
