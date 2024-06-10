//
// Created by dingjing on 6/10/24.
//

#ifndef sandbox_COMMON_H
#define sandbox_COMMON_H
#include <c/clib.h>

C_BEGIN_EXTERN_C

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#define test_bit(bit, var)            ((var) & (1 << (bit)))
#define set_bit(bit, var)             ((var) |= 1 << (bit))
#define clear_bit(bit, var)           ((var) &= ~(1 << (bit)))

#define test_and_set_bit(bit, var) \
({                                              \
const bool old_state = test_bit(bit, var);      \
set_bit(bit, var);                              \
old_state;                                      \
})

#define test_and_clear_bit(bit, var) \
({                                              \
const bool old_state = test_bit(bit, var);      \
clear_bit(bit, var);                            \
old_state;                                      \
})


C_END_EXTERN_C

#endif // sandbox_COMMON_H
