#include <lib/bits.h>
#include <lib/stddef.h>

inline int
get_state_bit(state_t state, unsigned int bit)
{
    return (state & (1 << bit)) == 0 ? False : True;
}

inline state_t
set_state_bit(state_t state, unsigned int bit, int value)
{
    if (value) {
        state |= 1 << bit;
    } else {
        state &= ~(1 << bit);
    }
    return state;
}
