#include <sys/types.h>
#include <limits.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "run.h"
#include "util.h"

void *base = 0;
void *brk_address;
int return_address = 0;
int prev_address;
int start_heap_address = 0;
int count = 0;
int free_space = 0;
struct rlimit rlim;

p_meta find_meta(p_meta *last, size_t size) {
  int size_diff[100]; //to best fit / worst fit
  int bestfit = 0;
  int worstfit = 0;
  int size_arr_num = 0;
  int min = 100000;
  int max = 0;
  int fit_block_num = 0;

  p_meta index = base;
  p_meta result = base;
  if(base == 0) return result;
  int current_position = start_heap_address; // start position
  brk_address = sbrk(0); //get current brk_address
  free_space = 0;

  switch(fit_flag){
    case FIRST_FIT:
    {
//	printf("fisrt_fit code!!\n");
//	printf("fit_flag = %d\n", fit_flag);
	while(brk_address >= index){
//		index = (p_meta *)current_position;
		if(index->free == 1){
			if(index->size >= size){
				index->free = 0;	
				return_address = current_position;
				result = index;
				free_space = 1;
				break;
			}	
		}
		current_position += index->size + META_SIZE;
		if(index->next == NULL) break;	
		index = index->next;
	}
      //FIRST FIT CODE
    }
    break;

    case BEST_FIT:
    {
	for(int i =0; brk_address >= index; i++, size_arr_num++){
//		index = (p_meta *)current_position;
		if(index->free == 1){
			if(index->size >= size){	
				return_address = current_position;
				size_diff[i] = index->size - size;
				free_space = 1;
			}
			else size_diff[i] = -1;	
		}
		else size_diff[i] = -1;
		current_position += index->size + META_SIZE;
		if(index->next == NULL) break;		
		index = index->next;
	}
	for(int i = 0; i < size_arr_num; i++){
		if(size_diff[i] != -1 && min > size_diff[i]){
			min = size_diff[i];
			fit_block_num = i;
		}
	}
	if(fit_block_num == 0) break;
	index = base;
	for(int i = 0; i < fit_block_num; i++){
		index = index->next;
	}	
	result = index;
      //BEST_FIT CODE
    }
    break;

    case WORST_FIT:
    {
	for(int i =0; brk_address >= index; i++, size_arr_num++){
//		index = (p_meta *)current_position;
		if(index->free == 1){
			if(index->size >= size){	
				return_address = current_position;
				size_diff[i] = index->size - size;
				free_space = 1;
			}
			else size_diff[i] = -1;	
		}
		else size_diff[i] = -1;
		current_position += index->size + META_SIZE;
		if(index->next == NULL) break;		
		index = index->next;
	}
	for(int i = 0; i < size_arr_num; i++){
		if(size_diff[i] != -1 && max < size_diff[i]){
			max = size_diff[i];
			fit_block_num = i;
		}
	}
	index = base;
	for(int i = 0; i < fit_block_num; i++){
		index = index->next;
	}
	result = index;
      //WORST_FIT CODE
    }
    break;
  }
  return result;
}

void *m_malloc(size_t size) {
  getrlimit(RLIMIT_DATA, &rlim);
  brk_address = sbrk(0);

  p_meta last = base;
  p_meta result = find_meta(&last, size);
  p_meta insertion;
  if(free_space){
	//there are free space
	int spare = result->size - size - META_SIZE;
	if(size % 4 != 0) spare -= (4 - size % 4);
	if(spare >= 0){
		insertion = (p_meta *)(result + size + META_SIZE);
		insertion->size = spare;
		result->size = size;
		if(size % 4 != 0) result->size += (4 - size % 4);
		insertion->free = 1;
		result->free = 0;
		insertion->prev = result;
		insertion->next = result->next;
		result->next->prev = insertion;
		result->next = insertion;
	}
	else{
		result->size = size;
		if(size % 4 != 0) result->size += (4 - size % 4);
		result->free = 0;
		printf("there are some unused space: %d\n", spare);
	}
  }
  else{
	if(rlim.rlim_max < brk_address + size + META_SIZE) return NULL;
	result = (p_meta *)brk_address;
	return_address = brk_address;
	sbrk(size + META_SIZE);
	if(size % 4 != 0) sbrk(4 - size % 4);
	if(count == 0){ 
		result->prev = NULL;
		result->next = NULL;
		start_heap_address = brk_address;
		base = result;
	}
	else{
		result->prev = (p_meta *)prev_address;
		result->prev->next = result;
		result->next = NULL;
	}
	result->free = 0;
	result->size = size;
	if(size % 4 != 0) result->size += (4 - size % 4);
	
	count++;
	prev_address = return_address;
	last = (p_meta *)prev_address;
  }
  return_address += META_SIZE;
  return (void *)return_address;
}

void m_free(void *ptr) {
  if(ptr == NULL) return;

  int free_address = (int)ptr - META_SIZE;
  p_meta free_list = (p_meta *)free_address;

  if(free_list->free != 0) return;

  if(free_list->prev != NULL && free_list->prev->free == 1){	
	free_list->prev->size += free_list->size + META_SIZE;
	free_list->prev->next = free_list->next;
	if(free_list->next !=NULL)
		free_list->next->prev = free_list->prev;
  }
  if(free_list->next != NULL && free_list->next->free == 1){
	free_list->next->size += free_list->size + META_SIZE;
	free_list->next->prev = free_list->prev;
	if(free_list->prev !=NULL)
		free_list->prev->next = free_list->next;
	//if head node
	if(free_list->prev == NULL)
		base = free_list->next;
  }

  //if tail node
  if(free_list->next == NULL){
	if(free_list->prev != NULL){
		free_list->prev->next = NULL;
		free_list->prev = NULL;
	}
  }

  free_list->free = 1; 
  return;
}

void* m_realloc(void* ptr, size_t size)
{
  p_meta dest;
  int src_address = (int)ptr - META_SIZE;
  p_meta src = (p_meta *)src_address;
  p_meta insertion;
  int return_address = ptr;
//  printf("src size = %d, size = %d\n",src->size, size);

  if(size < src->size)
  {	
        int spare = src->size - size - META_SIZE;
	if(size % 4 != 0) spare -= (4 - size % 4);
	if(spare >= 0){
		src->size = size;
		if(size % 4 != 0) src->size += (4 - size % 4);
		int new_add = (int) src + size + META_SIZE + 1;
		insertion = (p_meta *)(new_add);
		insertion->size = spare;		
		insertion->free = 1;
		src->free = 0;
		insertion->prev = src;
		insertion->next = src->next;
		src->next->prev = insertion;
		src->next = insertion;
	}
	else memset((void*)(src + size), 0, src->size - size);

  }
  else{
	
	if((src->next != NULL && src->next->free == 1) && src->next->size > (size - src->size)){
	 	int spare = src->next->size - (size - src->size) - META_SIZE;
		if(size % 4 != 0) spare -= (4 - size % 4);
//		printf("spare = %d\n", spare);
		if(spare >= 0){
			memset((void*)(src + src->size), 0, size - src->size);
			int new_add2 = (int)src->next + (size - src->size);
			insertion = (p_meta *)new_add2;
			insertion->size = spare + META_SIZE;
			insertion->free = 1;		
			src->size = size;
			if(size % 4 != 0) src->size += (4 - size % 4);
			insertion->prev = src;
			insertion->next = src->next->next;
			src->next->next->prev = insertion;
			src->next = insertion;
		}		
		else{		
			memset((void*)(src->next), 0, size - src->size);
			src->next = src->next->next;
			if(src->next->next != NULL) src->next->next->prev = src;
			src->size = size;
			printf("there are some unused space: %d\n", spare);
		}
			
	}
	else{
		m_free(ptr);
		dest = m_malloc(size);
		memcpy(dest, ptr, size);
		int return_address = dest;

		/*
		m_free(ptr);
		dest = m_malloc(size);
		memcpy(dest, ptr, src->size);
		memset((void*)(dest+size), 0, size - src->size);
		int return_address = dest;*/
	}
  }
  return_address += META_SIZE;
  return return_address;
}
