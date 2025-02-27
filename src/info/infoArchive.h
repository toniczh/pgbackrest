/***********************************************************************************************************************************
Archive Info Handler
***********************************************************************************************************************************/
#ifndef INFO_INFOARCHIVE_H
#define INFO_INFOARCHIVE_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct InfoArchive InfoArchive;

#include "common/crypto/common.h"
#include "common/type/object.h"
#include "common/type/string.h"
#include "info/infoPg.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Archive info filename
***********************************************************************************************************************************/
#define INFO_ARCHIVE_FILE                                           "archive.info"
#define REGEX_ARCHIVE_DIR_DB_VERSION                                "^[0-9]+(\\.[0-9]+)*-[0-9]+$"

#define INFO_ARCHIVE_PATH_FILE                                      STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE
    STRING_DECLARE(INFO_ARCHIVE_PATH_FILE_STR);
#define INFO_ARCHIVE_PATH_FILE_COPY                                 INFO_ARCHIVE_PATH_FILE INFO_COPY_EXT
    STRING_DECLARE(INFO_ARCHIVE_PATH_FILE_COPY_STR);

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
InfoArchive *infoArchiveNew(const unsigned int pgVersion, const uint64_t pgSystemId, const String *cipherPassSub);

// Create new object and load contents from IoRead
InfoArchive *infoArchiveNewLoad(IoRead *read);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct InfoArchivePub
{
    MemContext *memContext;                                         // Mem context
    InfoPg *infoPg;                                                 // Contents of the DB data
} InfoArchivePub;

// PostgreSQL info
__attribute__((always_inline)) static inline InfoPg *
infoArchivePg(const InfoArchive *const this)
{
    return THIS_PUB(InfoArchive)->infoPg;
}

InfoArchive *infoArchivePgSet(InfoArchive *this, unsigned int pgVersion, uint64_t pgSystemId);

// Current archive id
__attribute__((always_inline)) static inline const String *
infoArchiveId(const InfoArchive *const this)
{
    return infoPgArchiveId(infoArchivePg(this), infoPgDataCurrentId(infoArchivePg(this)));
}

// Cipher passphrase
__attribute__((always_inline)) static inline const String *
infoArchiveCipherPass(const InfoArchive *const this)
{
    return infoPgCipherPass(infoArchivePg(this));
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Given a backrest history id and postgres systemId and version, return the archiveId of the best match
const String *infoArchiveIdHistoryMatch(
    const InfoArchive *this, const unsigned int historyId, const unsigned int pgVersion, const uint64_t pgSystemId);

// Move to a new parent mem context
__attribute__((always_inline)) static inline InfoArchive *
infoArchiveMove(InfoArchive *this, MemContext *parentNew)
{
    return objMove(this, parentNew);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
infoArchiveFree(InfoArchive *this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
// Load archive info
InfoArchive *infoArchiveLoadFile(
    const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass);

// Save archive info
void infoArchiveSaveFile(
    InfoArchive *infoArchive, const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_INFO_ARCHIVE_TYPE                                                                                             \
    InfoArchive *
#define FUNCTION_LOG_INFO_ARCHIVE_FORMAT(value, buffer, bufferSize)                                                                \
    objToLog(value, "InfoArchive", buffer, bufferSize)

#endif
