#ifndef _SYSLINUX_MOVEBITS_H
#define _SYSLINUX_MOVEBITS_H

#include <inttypes.h>
#include <stdio.h>

typedef uint32_t addr_t;

/*
 * A syslinux_movelist is a linked list of move operations.  The ordering
 * is important, so no sorting requirement can be imposed.
 */
struct syslinux_movelist {
  addr_t dst;
  addr_t src;
  addr_t len;
  struct syslinux_movelist *next;
};

/*
 * A syslinux_memmap is a sorted, linked list of memory regions,
 * guaranteed to satisfy the constraint that no adjacent zones have
 * the same type.  Undefined memory ranges are represented with entries;
 * they have type SMT_UNDEFINED.
 *
 * Note that there is no length field.  The length of a region is obtained
 * by looking at the start of the next entry in the chain.
 */
enum syslinux_memmap_types {
  SMT_ERROR = -2,		/* Internal error token */
  SMT_END = -1,			/* End of list */
  SMT_UNDEFINED = 0,		/* Unknown range */
  SMT_FREE = 1,			/* Available memory */
  SMT_RESERVED,			/* Unusable memory */
  SMT_ALLOC,			/* Memory allocated by user */
  SMT_ZERO,			/* Memory that should be zeroed */
};

struct syslinux_memmap {
  addr_t start;
  enum syslinux_memmap_types type;
  struct syslinux_memmap *next;
};


struct syslinux_pm_regs {
  uint32_t eax, ecx, edx, ebx;
  uint32_t esp, ebp, esi, edi;
  uint32_t eip;
};

/*
 * moves is computed from "fraglist" and "memmap".  Areas that are
 * to be zeroed should be marked as such in the memmap, not in the
 * fraglist.
 */

int syslinux_compute_movelist(struct syslinux_movelist **movelist,
			      struct syslinux_movelist *fraglist,
			      struct syslinux_memmap *memmap);

struct syslinux_memmap *syslinux_memory_map(void);
void syslinux_free_movelist(struct syslinux_movelist *);
int syslinux_add_movelist(struct syslinux_movelist **,
			  addr_t dst, addr_t src, addr_t len);
int syslinux_allocate_from_list(struct syslinux_movelist **freelist,
				addr_t dst, addr_t len);
int syslinux_prepare_shuffle(struct syslinux_movelist *fraglist,
			     struct syslinux_memmap *memmap);
int syslinux_shuffle_boot_rm(struct syslinux_movelist *fraglist,
			     struct syslinux_memmap *memmap,
			     uint16_t bootflags,
			     uint32_t edx, uint32_t esi, uint16_t ds,
			     uint16_t cs, uint16_t ip);
int syslinux_shuffle_boot_pm(struct syslinux_movelist *fraglist,
			     struct syslinux_memmap *memmap,
			     uint16_t bootflags,
			     struct syslinux_pm_regs *regs);

/* Operatons on struct syslinux_memmap */
struct syslinux_memmap *syslinux_init_memmap(void);
int syslinux_add_memmap(struct syslinux_memmap **list,
			addr_t start, addr_t len,
			enum syslinux_memmap_types type);
enum syslinux_memmap_types syslinux_memmap_type(struct syslinux_memmap *list,
						addr_t start, addr_t len);
int syslinux_memmap_largest(struct syslinux_memmap *list,
			    enum syslinux_memmap_types type,
			    addr_t *start, addr_t *len);
void syslinux_free_memmap(struct syslinux_memmap *list);
struct syslinux_memmap *syslinux_dup_memmap(struct syslinux_memmap *list);

/* Debugging functions */
void syslinux_dump_movelist(FILE *file, struct syslinux_movelist *ml);
void syslinux_dump_memmap(FILE *file, struct syslinux_memmap *memmap);

#endif /* _SYSLINUX_MOVEBITS_H */