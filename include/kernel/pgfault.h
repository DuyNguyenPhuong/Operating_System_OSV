#ifndef _PGFAULT_H_
#define _PGFAULT_H_

#include <kernel/types.h>

/*
 * Page fault handler.
 */
 
void handle_page_fault(vaddr_t fault_addr, int present, int write, int user);

#endif /* _PGFAULT_H_ */
