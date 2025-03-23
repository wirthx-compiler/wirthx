//
// Created by stefan on 23.03.25.
//

#ifndef RANGETYPE_H
#define RANGETYPE_H
#include <cstdint>


class RangeType
{
public:
    virtual ~RangeType() = default;
    virtual int64_t lowerBounds() = 0;
    virtual int64_t upperBounds() = 0;
};


#endif // RANGETYPE_H
