#ifndef _BITS_H_
#define _BITS_H_

/*
 * Utility functions for bit manipulation.
 */

#include <kernel/types.h>

/*
 * Get the value (True or False) of the bit position in a state.
 */
int get_state_bit(state_t state, unsigned int bit);

/*
 * Set the value of the bit position in a state. Return the new state value.
 */
state_t set_state_bit(state_t state, unsigned int bit, int value);

#endif /* _BITS_H_ */
