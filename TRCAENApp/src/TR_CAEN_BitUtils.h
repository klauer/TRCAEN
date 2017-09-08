/* This file is part of the CAEN Digitizer Driver.
 * It is subject to the license terms in the LICENSE.txt file found in the
 * top-level directory of this distribution and at
 * https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. No part
 * of the CAEN Digitizer Driver, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms
 * contained in the LICENSE.txt file.
 */

#ifndef TR_CAEN_BIT_UTILS_H
#define TR_CAEN_BIT_UTILS_H

template <typename IntType>
inline bool TR_CAEN_GetBit (IntType reg, int bit)
{
    return (reg & ((IntType)1 << bit)) != 0;
}

template <typename IntType>
inline void TR_CAEN_SetBit (IntType *reg, int bit, bool value)
{
    if (value) {
        *reg |= (IntType)1 << bit;
    } else {
        *reg &= ~((IntType)1 << bit);
    }
}

template <typename IntType>
inline IntType TR_CAEN_MakeMask (int num_bits)
{
    IntType rel_top_bit = (IntType)1 << (num_bits - 1);
    return rel_top_bit | (rel_top_bit - 1);
}

template <typename IntType>
inline IntType TR_CAEN_GetBits (IntType reg, int bit_offset, int num_bits)
{
    IntType rel_mask = TR_CAEN_MakeMask<IntType>(num_bits);
    IntType mask = rel_mask << bit_offset;
    
    return (reg & mask) >> bit_offset;
}

template <typename IntType>
inline void TR_CAEN_SetBits (IntType *reg, int bit_offset, int num_bits, IntType value)
{
    IntType rel_mask = TR_CAEN_MakeMask<IntType>(num_bits);
    IntType mask = rel_mask << bit_offset;
    
    *reg = (*reg & ~mask) | ((value & rel_mask) << bit_offset);
}

#endif
