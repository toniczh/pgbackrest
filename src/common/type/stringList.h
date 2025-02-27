/***********************************************************************************************************************************
String List Handler
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_STRINGLIST_H
#define COMMON_TYPE_STRINGLIST_H

/***********************************************************************************************************************************
StringList object
***********************************************************************************************************************************/
typedef struct StringList StringList;

#include "common/memContext.h"
#include "common/type/list.h"
#include "common/type/string.h"
#include "common/type/variantList.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
// Create empty StringList
__attribute__((always_inline)) static inline StringList *
strLstNew(void)
{
    return (StringList *)lstNewP(sizeof(String *), .comparator = lstComparatorStr);
}

// Split a string into a string list based on a delimiter
StringList *strLstNewSplitZ(const String *string, const char *delimiter);

__attribute__((always_inline)) static inline StringList *
strLstNewSplit(const String *string, const String *delimiter)
{
    return strLstNewSplitZ(string, strZ(delimiter));
}

// New string list from a variant list -- all variants in the list must be type string
StringList *strLstNewVarLst(const VariantList *sourceList);

// Duplicate a string list
StringList *strLstDup(const StringList *sourceList);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Set a new comparator
__attribute__((always_inline)) static inline StringList *
strLstComparatorSet(StringList *this, ListComparator *comparator)
{
    return (StringList *)lstComparatorSet((List *)this, comparator);
}

// List size
__attribute__((always_inline)) static inline unsigned int
strLstSize(const StringList *this)
{
    return lstSize((List *)this);
}

// Is the list empty?
__attribute__((always_inline)) static inline bool
strLstEmpty(const StringList *this)
{
    return strLstSize(this) == 0;
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add String to the list
String *strLstAdd(StringList *this, const String *string);
String *strLstAddZ(StringList *this, const char *string);
String *strLstAddIfMissing(StringList *this, const String *string);

// Does the specified string exist in the list?
__attribute__((always_inline)) static inline bool
strLstExists(const StringList *this, const String *string)
{
    return lstExists((List *)this, &string);
}

// Insert into the list
String *strLstInsert(StringList *this, unsigned int listIdx, const String *string);

// Get a string by index
__attribute__((always_inline)) static inline String *
strLstGet(const StringList *this, unsigned int listIdx)
{
    return *(String **)lstGet((List *)this, listIdx);
}

// Join a list of strings into a single string using the specified separator and quote with specified quote character
String *strLstJoinQuote(const StringList *this, const char *separator, const char *quote);

// Join a list of strings into a single string using the specified separator
__attribute__((always_inline)) static inline String *
strLstJoin(const StringList *this, const char *separator)
{
    return strLstJoinQuote(this, separator, "");
}

// Return all items in this list which are not in the anti list. The input lists must *both* be sorted ascending or the results will
// be undefined.
StringList *strLstMergeAnti(const StringList *this, const StringList *anti);

// Move to a new parent mem context
__attribute__((always_inline)) static inline StringList *
strLstMove(StringList *this, MemContext *parentNew)
{
    return (StringList *)lstMove((List *)this, parentNew);
}

// Return a null-terminated array of pointers to the zero-terminated strings in this list. DO NOT override const and modify any of
// the strings in this array, though it is OK to modify the array itself.
const char **strLstPtr(const StringList *this);

// Remove an item from the list
__attribute__((always_inline)) static inline bool
strLstRemove(StringList *this, const String *item)
{
    return lstRemove((List *)this, &item);
}

__attribute__((always_inline)) static inline StringList *
strLstRemoveIdx(StringList *this, unsigned int listIdx)
{
    return (StringList *)lstRemoveIdx((List *)this, listIdx);
}

// List sort
__attribute__((always_inline)) static inline StringList *
strLstSort(StringList *this, SortOrder sortOrder)
{
    return (StringList *)lstSort((List *)this, sortOrder);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
strLstFree(StringList *this)
{
    lstFree((List *)this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *strLstToLog(const StringList *this);

#define FUNCTION_LOG_STRING_LIST_TYPE                                                                                              \
    StringList *
#define FUNCTION_LOG_STRING_LIST_FORMAT(value, buffer, bufferSize)                                                                 \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, strLstToLog, buffer, bufferSize)

#endif
