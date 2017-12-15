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
  if(count == 0) start_heap_address = sbrk(0);
  p_meta index = base;
  p_meta result = base;
  int free_space = 0;
  int current_position = start_heap_address; // start position

  brk_address = sbrk(0);

  switch(fit_flag){
    case FIRST_FIT:
    {
	while(brk_address >= index){
		printf("perfect1\n");
		index = (p_meta *)current_position;
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
		index = index->next;
	}
      //FIRST FIT CODE
    }
    break;

    case BEST_FIT:
    {
      //BEST_FIT CODE
    }
    break;

    case WORST_FIT:
    {
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
  if(free_space){
	//there are free space
  }
  else{
	if(rlim.rlim_max < brk_address + size + META_SIZE) return NULL;
	result = (p_meta *)brk_address;
	return_address = brk_address;
	sbrk(size + META_SIZE);
	if(count == 0){ 
		result->prev = NULL;
		result->next = NULL;
		base = result;
	}
	else{
		result->prev = (p_meta *)prev_address;
		result->prev->next = result;
		result->next = NULL;
	}
	result->free = 0;
	result->size = size;
	
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
	free_list->prev->next = NULL;
	free_list->prev = NULL;
  }

  free_list->free = 1; 
  
  return;
}

void* m_realloc(void* ptr, size_t size)
{

}
