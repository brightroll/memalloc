
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <dlfcn.h>

void * memalloc_default_arena = NULL;
char * memalloc_default_type = "INIT";
unsigned char memalloc_debug_mode = 0;

static void* (*real_malloc)(size_t) = NULL;
static void* (*real_free)(void *) = NULL;

static void link_real_funcs(void);

/* memalloc_malloc
 *
 * Storage format
 * |  '|'   1   2   3   4 null A/F null 
 * |  size (little endian unsigned int)
 *
 * size + 12
 */
#pragma pack(push, 1)
struct memalloc_header
{
  char border;
  char type[4];
  char null1;
  char flags;
  char null2;
  unsigned int size;
};
#pragma pack(pop)



/* memalloc_malloc
 *
 */
void *
memalloc_malloc(void * arena, char * type, char clear, size_t size)
{
  if (!real_malloc)
    link_real_funcs();

  void * result = real_malloc(size + sizeof(struct memalloc_header));

  if (result == NULL)
    return result;

  // malloc returns the pointer to the header, we return the result past the header
  void * p = result;
  result += sizeof(struct memalloc_header);

  struct memalloc_header * hdr = (struct memalloc_header *) p; 
  hdr->border = '|';
  strncpy(hdr->type, (type ? type : memalloc_default_type), sizeof(hdr->type));
  hdr->flags = 'A';
  hdr->null1 = hdr->null2 = 0;
  hdr->size = size;

  if (clear)
    memset(result, 0, size);

  return result;
}

/* memalloc_free
 *
 */
void
memalloc_free(void * arena, char * type, void * ptr)
{
  if (ptr == NULL)
    return;

  //TODO: Verify that we weren't already freed
  void * p = ptr - sizeof(struct memalloc_header);
  struct memalloc_header * hdr = (struct memalloc_header *) p; 
  hdr->flags = 'F';
  real_free(p);
}

/* memalloc_size
 *
 */
unsigned int
memalloc_size(void * arena, char * type, void * ptr)
{
  void * p = ptr - sizeof(struct memalloc_header);
  struct memalloc_header * hdr = (struct memalloc_header *) p; 
  return hdr->size;
}

void *malloc(size_t size)
{
  return memalloc_malloc(NULL, memalloc_default_type, 0, size); 
}

void free(void *ptr)
{
  memalloc_free(NULL, memalloc_default_type, ptr); 
}

void *calloc(size_t nmemb, size_t size)
{
  return memalloc_malloc(NULL, memalloc_default_type, 1, (nmemb * size)); 
}

void *realloc(void *ptr, size_t size)
{
  // Error case
  if ((ptr == NULL) && (size == 0))
    return NULL;

  // In this case, it is equivalent to malloc
  if ((ptr == NULL) && (size != 0))
    return memalloc_malloc(NULL, memalloc_default_type, 0, size); 

  // In this case, it is equivalent to free
  if (ptr && (size == 0))
  {
    memalloc_free(NULL, memalloc_default_type, ptr); 
    return NULL;
  }

  // The typical case (ptr && size)
  int actual = memalloc_size(NULL, memalloc_default_type, ptr);
  if (size <= actual)
    return ptr;

  void * new = memalloc_malloc(NULL, memalloc_default_type, 0, size); 
  memcpy(new, ptr, actual);
  memalloc_free(NULL, memalloc_default_type, ptr); 

  return new;
}

static void link_real_funcs(void)
{
  real_malloc = dlsym(RTLD_NEXT, "malloc");
  real_free = dlsym(RTLD_NEXT, "free");
}


