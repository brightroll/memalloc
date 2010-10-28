
/* memalloc_malloc
 *
 */
void *
memalloc_malloc(void * arena, char * type, char clear, size_t size);


/* memalloc_free
 *
 */
void
memalloc_free(void * arena, char * type, void * ptr);


/* memalloc_size
 *
 */
unsigned int
memalloc_size(void * arena, char * type, void * ptr);



extern void * memalloc_default_arena;
extern char * memalloc_default_type;
extern unsigned char memalloc_debug_mode;


