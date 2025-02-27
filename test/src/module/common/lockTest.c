/***********************************************************************************************************************************
Test Lock Handler
***********************************************************************************************************************************/
#include "common/time.h"
#include "storage/posix/storage.h"

#include "common/harnessFork.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create default storage object for testing
    Storage *storageTest = storagePosixNewP(strNew(testPath()), .write = true);

    // *****************************************************************************************************************************
    if (testBegin("lockAcquireFile() and lockReleaseFile()"))
    {
        String *archiveLock = strNewFmt("%s/main-archive" LOCK_FILE_EXT, testPath());
        int lockFdTest = -1;

        TEST_RESULT_INT(system(strZ(strNewFmt("touch %s", strZ(archiveLock)))), 0, "touch lock file");
        TEST_ASSIGN(lockFdTest, lockAcquireFile(archiveLock, STRDEF("1-test"), 0, true), "get lock");
        TEST_RESULT_BOOL(lockFdTest != -1, true, "lock succeeds");
        TEST_RESULT_BOOL(storageExistsP(storageTest, archiveLock), true, "lock file was created");
        TEST_ERROR(lockAcquireFile(archiveLock, STRDEF("2-test"), 0, true), LockAcquireError,
            strZ(
                strNewFmt(
                    "unable to acquire lock on file '%s': Resource temporarily unavailable\n"
                    "HINT: is another pgBackRest process running?", strZ(archiveLock))));
        TEST_RESULT_BOOL(lockAcquireFile(archiveLock, STRDEF("2-test"), 0, false) == -1, true, "lock is already held");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("acquire file lock on the same exec-id");

        TEST_RESULT_INT(lockAcquireFile(archiveLock, STRDEF("1-test"), 0, true), -2, "allow lock with same exec id");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("fail file lock on the same exec-id when lock file is empty");

        TEST_RESULT_INT(system(strZ(strNewFmt("echo '' > %s", strZ(archiveLock)))), 0, "overwrite lock file");

        TEST_ERROR(lockAcquireFile(archiveLock, STRDEF("2-test"), 0, true), LockAcquireError,
            strZ(
                strNewFmt(
                    "unable to acquire lock on file '%s': Resource temporarily unavailable\n"
                    "HINT: is another pgBackRest process running?", strZ(archiveLock))));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(lockReleaseFile(lockFdTest, archiveLock), "release lock");

        TEST_RESULT_VOID(lockReleaseFile(lockFdTest, archiveLock), "release lock");
        TEST_RESULT_BOOL(storageExistsP(storageTest, archiveLock), false, "lock file was removed");
        TEST_RESULT_VOID(lockReleaseFile(lockFdTest, archiveLock), "release lock again without error");

        // -------------------------------------------------------------------------------------------------------------------------
        String *subPathLock = strNewFmt("%s/sub1/sub2/db-backup" LOCK_FILE_EXT, testPath());

        TEST_ASSIGN(lockFdTest, lockAcquireFile(subPathLock, STRDEF("1-test"), 0, true), "get lock in subpath");
        TEST_RESULT_BOOL(storageExistsP(storageTest, subPathLock), true, "lock file was created");
        TEST_RESULT_BOOL(lockFdTest != -1, true, "lock succeeds");
        TEST_RESULT_VOID(lockReleaseFile(lockFdTest, subPathLock), "release lock");
        TEST_RESULT_BOOL(storageExistsP(storageTest, subPathLock), false, "lock file was removed");

        // -------------------------------------------------------------------------------------------------------------------------
        String *dirLock = strNewFmt("%s/dir" LOCK_FILE_EXT, testPath());

        TEST_RESULT_INT(system(strZ(strNewFmt("mkdir -p 750 %s", strZ(dirLock)))), 0, "create dirtest.lock dir");

        TEST_ERROR(
            lockAcquireFile(dirLock, STRDEF("1-test"), 0, true), LockAcquireError,
            strZ(strNewFmt("unable to acquire lock on file '%s': Is a directory", strZ(dirLock))));

        // -------------------------------------------------------------------------------------------------------------------------
        String *noPermLock = strNewFmt("%s/noperm/noperm", testPath());
        TEST_RESULT_INT(system(strZ(strNewFmt("mkdir -p 750 %s", strZ(strPath(noPermLock))))), 0, "create noperm dir");
        TEST_RESULT_INT(system(strZ(strNewFmt("chmod 000 %s", strZ(strPath(noPermLock))))), 0, "chmod noperm dir");

        TEST_ERROR(
            lockAcquireFile(noPermLock, STRDEF("1-test"), 100, true), LockAcquireError,
            strZ(
                strNewFmt(
                    "unable to acquire lock on file '%s': Permission denied\n"
                        "HINT: does the user running pgBackRest have permissions on the '%s' file?",
                    strZ(noPermLock), strZ(noPermLock))));

        // -------------------------------------------------------------------------------------------------------------------------
        String *backupLock = strNewFmt("%s/main-backup" LOCK_FILE_EXT, testPath());

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, false)
            {
                TEST_RESULT_INT_NE(lockAcquireFile(backupLock, STRDEF("1-test"), 0, true), -1, "lock on fork");
                sleepMSec(500);
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                sleepMSec(250);
                TEST_ERROR(
                    lockAcquireFile(backupLock, STRDEF("2-test"), 0, true),
                    LockAcquireError,
                    strZ(
                        strNewFmt(
                            "unable to acquire lock on file '%s': Resource temporarily unavailable\n"
                            "HINT: is another pgBackRest process running?", strZ(backupLock))));
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();
    }

    // *****************************************************************************************************************************
    if (testBegin("lockAcquire(), lockRelease()"))
    {
        String *stanza = strNew("test");
        String *lockPath = strNew(testPath());
        String *archiveLockFile = strNewFmt("%s/%s-archive" LOCK_FILE_EXT, testPath(), strZ(stanza));
        String *backupLockFile = strNewFmt("%s/%s-backup" LOCK_FILE_EXT, testPath(), strZ(stanza));
        int lockFdTest = -1;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(lockRelease(true), AssertError, "no lock is held by this process");
        TEST_RESULT_BOOL(lockRelease(false), false, "release when there is no lock");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(lockFdTest, lockAcquireFile(archiveLockFile, STRDEF("1-test"), 0, true), "archive lock by file");
        TEST_RESULT_BOOL(
            lockAcquire(lockPath, stanza, STRDEF("2-test"), lockTypeArchive, 0, false), false, "archive already locked");
        TEST_ERROR(
            lockAcquire(lockPath, stanza, STRDEF("2-test"), lockTypeArchive, 0, true), LockAcquireError,
            strZ(strNewFmt(
                "unable to acquire lock on file '%s': Resource temporarily unavailable\n"
                "HINT: is another pgBackRest process running?", strZ(archiveLockFile))));
        TEST_ERROR(
            lockAcquire(lockPath, stanza, STRDEF("2-test"), lockTypeAll, 0, true), LockAcquireError,
            strZ(strNewFmt(
                "unable to acquire lock on file '%s': Resource temporarily unavailable\n"
                "HINT: is another pgBackRest process running?", strZ(archiveLockFile))));
        TEST_RESULT_VOID(lockReleaseFile(lockFdTest, archiveLockFile), "release lock");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(lockAcquire(lockPath, stanza, STRDEF("1-test"), lockTypeArchive, 0, true), true, "archive lock");
        TEST_RESULT_BOOL(storageExistsP(storageTest, archiveLockFile), true, "archive lock file was created");
        TEST_ERROR(
            lockAcquire(lockPath, stanza, STRDEF("1-test"), lockTypeArchive, 0, false), AssertError,
            "lock is already held by this process");
        TEST_RESULT_VOID(lockRelease(true), "release archive lock");

        // // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(lockFdTest, lockAcquireFile(backupLockFile, STRDEF("1-test"), 0, true), "backup lock by file");
        TEST_ERROR(
            lockAcquire(lockPath, stanza, STRDEF("2-test"), lockTypeBackup, 0, true), LockAcquireError,
            strZ(strNewFmt(
                "unable to acquire lock on file '%s': Resource temporarily unavailable\n"
                "HINT: is another pgBackRest process running?", strZ(backupLockFile))));
        TEST_ERROR(
            lockAcquire(lockPath, stanza, STRDEF("2-test"), lockTypeAll, 0, true), LockAcquireError,
            strZ(strNewFmt(
                "unable to acquire lock on file '%s': Resource temporarily unavailable\n"
                "HINT: is another pgBackRest process running?", strZ(backupLockFile))));
        TEST_RESULT_VOID(lockReleaseFile(lockFdTest, archiveLockFile), "release lock");
        TEST_RESULT_VOID(lockReleaseFile(lockFdTest, backupLockFile), "release lock");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(lockAcquire(lockPath, stanza, STRDEF("1-test"), lockTypeAll, 0, true), true, "all lock");
        TEST_RESULT_BOOL(storageExistsP(storageTest, archiveLockFile), true, "archive lock file was created");
        TEST_RESULT_BOOL(storageExistsP(storageTest, backupLockFile), true, "backup lock file was created");
        TEST_ERROR(
            lockAcquire(lockPath, stanza, STRDEF("1-test"), lockTypeAll, 0, false), AssertError,
            "assertion 'failOnNoLock || lockType != lockTypeAll' failed");
        TEST_RESULT_VOID(lockRelease(true), "release all locks");

        TEST_RESULT_BOOL(storageExistsP(storageTest, archiveLockFile), false, "archive lock file was removed");
        TEST_RESULT_BOOL(storageExistsP(storageTest, backupLockFile), false, "backup lock file was removed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("acquire lock on the same exec-id and release");

        TEST_RESULT_BOOL(lockAcquire(lockPath, stanza, STRDEF("1-test"), lockTypeBackup, 0, true), true, "backup lock");

        // Make it look there is no lock
        lockFdTest = lockFd[lockTypeBackup];
        String *lockFileTest = strDup(lockFile[lockTypeBackup]);
        lockTypeHeld = lockTypeNone;

        TEST_RESULT_BOOL(lockAcquire(lockPath, stanza, STRDEF("1-test"), lockTypeBackup, 0, true), true, "backup lock again");
        TEST_RESULT_VOID(lockRelease(true), "release backup lock");

        // Release lock manually
        lockReleaseFile(lockFdTest, lockFileTest);
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
