
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>

void * memalloc_default_arena = NULL;
char * memalloc_default_type = "INIT";
unsigned char memalloc_debug_mode = 0;

void * memalloc_lo_mem;
void * memalloc_hi_mem;
unsigned long long memalloc_stats_alloc = 0;
unsigned long long memalloc_stats_free = 0;

static unsigned char memalloc_init_state = 0; // 0 = uninit, 1 = init
static char memalloc_output_buff[1024];

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

static void do_init(void);

/* memalloc_header
 *
 * Storage format
 * |  '|'   1   2   3   4   5   6   7   8 null A/F null 
 * |  size (little endian unsigned int)
 *
 * size + 16
 */
#pragma pack(push, 1)
struct memalloc_header
{
  char wilderness[16];
  char border;
  char type[8];
  char null1;
  char flags;
  char null2;
  unsigned int size;
};
#pragma pack(pop)

#define DBGLOG(format, ...) if (memalloc_debug_mode) do { snprintf(memalloc_output_buff, 1024, format "\n", __VA_ARGS__); write(2, memalloc_output_buff, strlen(memalloc_output_buff)); } while (0)


#define ERRLOG(format, ...) do { snprintf(memalloc_output_buff, 1024, format "\n", __VA_ARGS__); write(2, memalloc_output_buff, strlen(memalloc_output_buff)); } while (0)

/* memalloc_alloc
 *
 */
void *
memalloc_alloc(void * arena, char * type, char clear, size_t size)
{
  if (memalloc_init_state == 0)
    do_init();

  memalloc_stats_alloc++;

  DBGLOG("> memalloc_alloc %s %d %d", type, clear, size);

  if (size == 0)
  {
    DBGLOG("< memalloc_alloc 0x%08x", size);
    return NULL;
  }

  __malloc_hook = old_malloc_hook;
  void * result = malloc(size + sizeof(struct memalloc_header));
  old_malloc_hook = __malloc_hook;
  __malloc_hook = __wrap_malloc;

  if (result == NULL)
  {
    DBGLOG("< memalloc_alloc *FAIL* 0x%08x", 0);
    return NULL;
  }

  // malloc returns the pointer to the header, we return the result past the header
  void * p = result;
  result += sizeof(struct memalloc_header);

  struct memalloc_header * hdr = (struct memalloc_header *) p; 
  memset(hdr->wilderness, 0, sizeof(hdr->wilderness));
  hdr->border = '|';
  strncpy(hdr->type, (type ? type : memalloc_default_type), sizeof(hdr->type));
  hdr->flags = 'A';
  hdr->null1 = hdr->null2 = 0;
  hdr->size = size;

  if (clear)
    memset(result, 0, size);

  if (!memalloc_lo_mem)
    memalloc_lo_mem = result;
  if (result > memalloc_hi_mem)
    memalloc_hi_mem = result;

  DBGLOG("< memalloc_alloc 0x%08x", (uint) result);
  return result;
}

/* memalloc_free
 *
 */
void
memalloc_free(void * arena, char * type, void * ptr)
{
  memalloc_stats_free++;

  if (ptr == NULL)
    return;

  //TODO: Verify that we weren't already freed
  void * p = ptr - sizeof(struct memalloc_header);
  struct memalloc_header * hdr = (struct memalloc_header *) p; 
  if (hdr->border != '|')
  {
    DBGLOG("> memalloc_free 0x%08x - BAD/LEGACY FREE", (uint) ptr);
    __free_hook = old_free_hook;
    free(ptr);
    old_free_hook = __free_hook;
    __free_hook = __wrap_free;
  }
  else
  {
    DBGLOG("> memalloc_free 0x%08x - %s - %d", (uint) ptr, hdr->type, hdr->size);
    if (hdr->flags == 'F')
      ERRLOG("* memalloc_free DOUBLE 0x%08x - %s - %d", (uint) ptr, hdr->type, hdr->size);
    hdr->flags = 'F';
    __free_hook = old_free_hook;
    free(p);
    old_free_hook = __free_hook;
    __free_hook = __wrap_free;

    //hdr->flags = 'F'; // taking a little risk, but free will blow up

    // 2010/10/30 - Yep, sure as ****. My mutation messed up
    // malloc's internal structure. Essentially, once you call
    // free you are not to touch the memory. This puts into
    // question this project. What can I safely leave in memory
    // that will survive. My flag was a solid 12 bytes away!
    // I don't want to implement my own allocators
    // yet.
    // The lack of a standard way of changing allocators easily is 
    // a problem with C.
    // LATER - giving myself another 16 byte margin seems to work
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
  DBGLOG("> malloc %d", size);

  void * p = memalloc_alloc(NULL, memalloc_default_type, 0, size); 

  DBGLOG("< malloc 0x%08x", (uint) p);
  return p;
}

void
__wrap_free(void *ptr, const void * caller)
{
  DBGLOG("> free 0x%08x", (uint) ptr);
  memalloc_free(NULL, memalloc_default_type, ptr); 
}

void * __wrap_realloc(void *ptr, size_t size, const void * caller )
{
  DBGLOG("> realloc 0x%08x %d", (uint) ptr, size);
  // Error case
  if ((ptr == NULL) && (size == 0))
    return NULL;

  // In this case, it is equivalent to malloc
  if ((ptr == NULL) && (size != 0))
    return __wrap_malloc(size, NULL); 

  // In this case, it is equivalent to free
  if (ptr && (size == 0))
  {
    memalloc_free(NULL, memalloc_default_type, ptr); 
    return NULL;
  }

  // The typical case (ptr && size)
  int actual = memalloc_size(NULL, memalloc_default_type, ptr);
  DBGLOG("| realloc %d -> %d", actual, size);
  if (size <= actual)
  {
    DBGLOG("< realloc 0x%08x - same", (uint) ptr);
    return ptr;
  }

  __realloc_hook = old_realloc_hook;
  void * new_ptr = memalloc_alloc(NULL, memalloc_default_type, 0, size); 
  memcpy(new_ptr, ptr, actual);
  memalloc_free(NULL, memalloc_default_type, ptr); 
  __realloc_hook = __wrap_realloc;

  DBGLOG("< realloc 0x%08x", (uint) new_ptr);
  return new_ptr;
}

static void do_init(void)
{
  char * p = getenv("MEMALLOC_DEBUG_STDERR");
  if (p)
    memalloc_debug_mode = 1;
  memalloc_init_state = 1;
}
