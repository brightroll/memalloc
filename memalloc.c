
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
//#include <dlfcn.h>

void * memalloc_default_arena = NULL;
char * memalloc_default_type = "INIT";
unsigned char memalloc_debug_mode = 0;

static unsigned char memalloc_init_state = 0; // 0 = uninit, 1 = init
static char memalloc_output_buff[1024];

//static void* (*real_malloc)(size_t) = NULL;
//static void* (*real_free)(void *) = NULL;

//void * __real_malloc(size_t);
//void   __real_free(void *);

/* Variables to save original hooks. */
       static void *(*old_malloc_hook)(size_t, const void *);
       static void  (*old_free_hook)(void *, const void *);
       static void *(*old_realloc_hook)(void *, size_t, const void *);

       static void *__wrap_malloc(size_t, const void *);
       static void  __wrap_free(void *, const void *);
       static void *__wrap_realloc(void *, size_t, const void *);

       static void
       my_init_hook(void)
       {
           old_malloc_hook = __malloc_hook;
           old_free_hook = __free_hook;
           old_realloc_hook = __realloc_hook;

           __malloc_hook = __wrap_malloc;
           __free_hook = __wrap_free;
           __realloc_hook = __wrap_realloc;
       }

       /* Override initializing hook from the C library. */
       void (*__malloc_initialize_hook) (void) = my_init_hook;

void memalloc_init(void)
{
  __malloc_initialize_hook = my_init_hook;
}

//static void link_real_funcs(void);
static void do_init(void);

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

#define LOG(format, ...) do { snprintf(memalloc_output_buff, 1024, format "\n", __VA_ARGS__); write(2, memalloc_output_buff, strlen(memalloc_output_buff)); } while (0)



/* memalloc_malloc
 *
 */
void *
memalloc_malloc(void * arena, char * type, char clear, size_t size)
{
//  if (!real_malloc)
//    link_real_funcs();
  if (memalloc_init_state == 0)
    do_init();

  void * result = malloc(size + sizeof(struct memalloc_header));

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
  if (hdr->border != '|')
  {
    LOG("> memalloc_free %08x - bad/legacy free", ptr);
    free(ptr);
  }
  else
  {
    LOG("> memalloc_free %08x - %s - %d", ptr, hdr->type, hdr->size);
    hdr->flags = 'F';
    free(p);
  }
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

void *
__wrap_malloc(size_t size, const void * caller)
{
  if (memalloc_init_state == 0)
    do_init();
  if (memalloc_debug_mode)
    LOG("> malloc %d", size);
  __malloc_hook = old_malloc_hook;
  void * p = memalloc_malloc(NULL, memalloc_default_type, 0, size); 
  old_malloc_hook = __malloc_hook;
  __malloc_hook = __wrap_malloc;
  if (memalloc_debug_mode)
    LOG("< malloc %08x", p);
  return p;
}

void
__wrap_free(void *ptr, const void * caller)
{
  if (memalloc_debug_mode)
    LOG("> free %08x", ptr);
  __free_hook = old_free_hook;
  memalloc_free(NULL, memalloc_default_type, ptr); 
  old_free_hook = __free_hook;
  __free_hook = __wrap_free;
}

void * __wrap_calloc(size_t nmemb, size_t size)
{
  if (memalloc_init_state == 0)
    do_init();
  if (memalloc_debug_mode)
    LOG("> calloc ", (nmemb * size));
  void * p = memalloc_malloc(NULL, memalloc_default_type, 1, (nmemb * size)); 
  if (memalloc_debug_mode)
    LOG("< calloc %08x", p);
  return p;
}

void * __wrap_realloc(void *ptr, size_t size, const void * caller )
{
  if (memalloc_debug_mode)
    LOG("> realloc %08x %d", ptr, size);
  // Error case
  if ((ptr == NULL) && (size == 0))
    return NULL;

  // In this case, it is equivalent to malloc
  if ((ptr == NULL) && (size != 0))
    return __wrap_malloc(size, NULL); 

  // In this case, it is equivalent to free
  if (ptr && (size == 0))
  {
    __free_hook = old_free_hook;
    memalloc_free(NULL, memalloc_default_type, ptr); 
    __free_hook = __wrap_free;
    return NULL;
  }


  // The typical case (ptr && size)
  __realloc_hook = old_realloc_hook;
  __malloc_hook = old_malloc_hook;
  int actual = memalloc_size(NULL, memalloc_default_type, ptr);
  if (memalloc_debug_mode)
    LOG("| realloc %d", actual);
  __realloc_hook = __wrap_realloc;
  __malloc_hook = __wrap_malloc;
  if (size <= actual)
    return ptr;

  __realloc_hook = old_realloc_hook;
  __malloc_hook = old_malloc_hook;
  __free_hook = old_free_hook;
  void * new = memalloc_malloc(NULL, memalloc_default_type, 0, size); 
  memcpy(new, ptr, actual);
  memalloc_free(NULL, memalloc_default_type, ptr); 
  __realloc_hook = __wrap_realloc;
  __malloc_hook = __wrap_malloc;
  __free_hook = __wrap_free;

  return new;
}

/*
static void link_real_funcs(void)
{
  real_malloc = dlsym(RTLD_NEXT, "malloc");
  real_free = dlsym(RTLD_NEXT, "free");
}
*/


static void do_init(void)
{
  char * p = getenv("MEMALLOC_DEBUG_STDERR");
  if (p)
    memalloc_debug_mode = 1;
  memalloc_init_state = 1;
}
