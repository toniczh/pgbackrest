/***********************************************************************************************************************************
IO Filter Group

Process data through an arbitrary group of filters in the order added by the user using ioFilterGroupAdd().  After processing
results can be gathered using ioFilterGroupResult() for any filters that produce results.

Processing is complex and asymmetric for read/write so should be done via the IoRead and IoWrite objects.  General users need
only call ioFilterGroupNew(), ioFilterGroupAdd(), and ioFilterGroupResult().
***********************************************************************************************************************************/
#ifndef COMMON_IO_FILTER_GROUP_H
#define COMMON_IO_FILTER_GROUP_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct IoFilterGroup IoFilterGroup;

#include "common/io/filter/filter.h"
#include "common/type/object.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
IoFilterGroup *ioFilterGroupNew(void);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct IoFilterGroupPub
{
    MemContext *memContext;                                         // Mem context
    List *filterList;                                               // List of filters to apply
    bool inputSame;                                                 // Same input required again?
    bool done;                                                      // Is processing done?

#ifdef DEBUG
    bool opened;                                                    // Has the filter set been opened?
    bool closed;                                                    // Has the filter set been closed?
#endif
} IoFilterGroupPub;

// Is the filter group done processing?
__attribute__((always_inline)) static inline bool
ioFilterGroupDone(const IoFilterGroup *const this)
{
    ASSERT_INLINE(THIS_PUB(IoFilterGroup)->opened && !THIS_PUB(IoFilterGroup)->closed);
    return THIS_PUB(IoFilterGroup)->done;
}

// Should the same input be passed again? A buffer of input can produce multiple buffers of output, e.g. when a file containing all
// zeroes is being decompressed.
__attribute__((always_inline)) static inline bool
ioFilterGroupInputSame(const IoFilterGroup *const this)
{
    ASSERT_INLINE(THIS_PUB(IoFilterGroup)->opened && !THIS_PUB(IoFilterGroup)->closed);
    return THIS_PUB(IoFilterGroup)->inputSame;
}

// Get all filters and their parameters so they can be passed to a remote
Variant *ioFilterGroupParamAll(const IoFilterGroup *this);

// Get filter results
const Variant *ioFilterGroupResult(const IoFilterGroup *this, const String *filterType);

// Get/set all filter results
const Variant *ioFilterGroupResultAll(const IoFilterGroup *this);
void ioFilterGroupResultAllSet(IoFilterGroup *this, const Variant *filterResult);

// Return total number of filters
__attribute__((always_inline)) static inline unsigned int
ioFilterGroupSize(const IoFilterGroup *const this)
{
    return lstSize(THIS_PUB(IoFilterGroup)->filterList);
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add a filter
IoFilterGroup *ioFilterGroupAdd(IoFilterGroup *this, IoFilter *filter);

// Insert a filter before an index
IoFilterGroup *ioFilterGroupInsert(IoFilterGroup *this, unsigned int listIdx, IoFilter *filter);

// Clear filters
IoFilterGroup *ioFilterGroupClear(IoFilterGroup *this);

// Open filter group
void ioFilterGroupOpen(IoFilterGroup *this);

// Process filters
void ioFilterGroupProcess(IoFilterGroup *this, const Buffer *input, Buffer *output);

// Close filter group and gather results
void ioFilterGroupClose(IoFilterGroup *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
ioFilterGroupFree(IoFilterGroup *this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *ioFilterGroupToLog(const IoFilterGroup *this);

#define FUNCTION_LOG_IO_FILTER_GROUP_TYPE                                                                                          \
    IoFilterGroup *
#define FUNCTION_LOG_IO_FILTER_GROUP_FORMAT(value, buffer, bufferSize)                                                             \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, ioFilterGroupToLog, buffer, bufferSize)

#endif
