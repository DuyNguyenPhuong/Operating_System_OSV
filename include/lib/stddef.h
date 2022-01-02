#ifndef _STDDEF_H_
#define _STDDEF_H_

/*
 * Some standard definitions
 */

#ifndef False
#define False 0
#endif /* False */
#ifndef True
#define True !False
#endif /* True */

#ifndef NULL
#define NULL 0
#endif /* NULL */

#define PADDR_NONE 0
#define SWAPID_NONE 0

/**
 * Get the struct from a member
 * @member_addr: address of this member.
 * @struct:	the type of the struct this is embedded in.
 * @member:	the name of the member within the struct.
 */
#define retrieve_struct(member_addr, struct, member) \
	((struct *) ((char *) (member_addr) - (size_t)&(((struct *)0)->member)))

#define offset_of(struct, member) \
	((size_t)&(((struct *)0)->member))

/* pass in an array to get number of elements */
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))

#define min(a, b) ((a < b) ? a : b)

#endif /* _STDDEF_H_ */
