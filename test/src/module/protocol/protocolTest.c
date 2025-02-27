/***********************************************************************************************************************************
Test Protocol
***********************************************************************************************************************************/
#include "common/io/fdRead.h"
#include "common/io/fdWrite.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "common/regExp.h"
#include "storage/storage.h"
#include "storage/posix/storage.h"
#include "version.h"

#include "common/harnessConfig.h"
#include "common/harnessFork.h"

/***********************************************************************************************************************************
Test protocol request handler
***********************************************************************************************************************************/
static unsigned int testServerProtocolErrorTotal = 0;

static void
testServerAssertProtocol(const VariantList *paramList, ProtocolServer *server)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(VARIANT_LIST, paramList);
        FUNCTION_HARNESS_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_HARNESS_END();

    ASSERT(paramList == NULL);
    ASSERT(server != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        THROW(AssertError, "test assert");
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN_VOID();
}

static void
testServerRequestSimpleProtocol(const VariantList *paramList, ProtocolServer *server)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(VARIANT_LIST, paramList);
        FUNCTION_HARNESS_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_HARNESS_END();

    ASSERT(paramList == NULL);
    ASSERT(server != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        protocolServerResponse(server, varNewBool(true));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN_VOID();
}

static void
testServerRequestComplexProtocol(const VariantList *paramList, ProtocolServer *server)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(VARIANT_LIST, paramList);
        FUNCTION_HARNESS_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_HARNESS_END();

    ASSERT(paramList == NULL);
    ASSERT(server != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        protocolServerResponse(server, varNewBool(false));
        protocolServerWriteLine(server, strNew("LINEOFTEXT"));
        protocolServerWriteLine(server, NULL);
        ioWriteFlush(protocolServerIoWrite(server));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN_VOID();
}

static void
testServerErrorUntil0Protocol(const VariantList *paramList, ProtocolServer *server)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(VARIANT_LIST, paramList);
        FUNCTION_HARNESS_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_HARNESS_END();

    ASSERT(paramList == NULL);
    ASSERT(server != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        if (testServerProtocolErrorTotal > 0)
        {
            testServerProtocolErrorTotal--;
            THROW(FormatError, "error-until-0");
        }

        protocolServerResponse(server, varNewBool(true));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN_VOID();
}

/***********************************************************************************************************************************
Test ParallelJobCallback
***********************************************************************************************************************************/
typedef struct TestParallelJobCallback
{
    List *jobList;                                                  // List of jobs to process
    unsigned int jobIdx;                                            // Current index in the list to be processed
    bool clientSeen[2];                                             // Make sure the client idx was seen
} TestParallelJobCallback;

static ProtocolParallelJob *testParallelJobCallback(void *data, unsigned int clientIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(UINT, clientIdx);
    FUNCTION_TEST_END();

    TestParallelJobCallback *listData = data;

    // Mark the client idx as seen
    listData->clientSeen[clientIdx] = true;

    // Get a new job if there are any left
    if (listData->jobIdx < lstSize(listData->jobList))
    {
        ProtocolParallelJob *job = *(ProtocolParallelJob **)lstGet(listData->jobList, listData->jobIdx);
        listData->jobIdx++;

        FUNCTION_TEST_RETURN(protocolParallelJobMove(job, memContextCurrent()));
    }

    FUNCTION_TEST_RETURN(NULL);
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    Storage *storageTest = storagePosixNewP(strNew(testPath()), .write = true);

    // *****************************************************************************************************************************
    if (testBegin("protocolStorageTypeEnum() and protocolStorageTypeEnum()"))
    {
        TEST_RESULT_UINT(protocolStorageTypeEnum(PROTOCOL_REMOTE_TYPE_PG_STR), protocolStorageTypePg, "pg enum");
        TEST_RESULT_UINT(protocolStorageTypeEnum(PROTOCOL_REMOTE_TYPE_REPO_STR), protocolStorageTypeRepo, "repo enum");
        TEST_ERROR(protocolStorageTypeEnum(STRDEF(BOGUS_STR)), AssertError, "invalid protocol storage type 'BOGUS'");

        TEST_RESULT_STR(protocolStorageTypeStr(protocolStorageTypePg), PROTOCOL_REMOTE_TYPE_PG_STR, "pg str");
        TEST_RESULT_STR(protocolStorageTypeStr(protocolStorageTypeRepo), PROTOCOL_REMOTE_TYPE_REPO_STR, "repo str");
        TEST_ERROR(protocolStorageTypeStr((ProtocolStorageType)999), AssertError, "invalid protocol storage type 999");
    }

    // *****************************************************************************************************************************
    if (testBegin("repoIsLocal() and pgIsLocal()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        strLstAddZ(argList, "--repo1-path=/repo-local");
        strLstAddZ(argList, "--repo4-path=/remote-host-new");
        strLstAddZ(argList, "--repo4-host=remote-host-new");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        strLstAddZ(argList, "archive-get");
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_BOOL(repoIsLocal(0), true, "repo is local");
        TEST_RESULT_VOID(repoIsLocalVerify(), "    local verified");
        TEST_RESULT_VOID(repoIsLocalVerifyIdx(0), "    local by index verified");
        TEST_ERROR_FMT(
            repoIsLocalVerifyIdx(cfgOptionGroupIdxTotal(cfgOptGrpRepo) - 1), HostInvalidError,
            "archive-get command must be run on the repository host");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        strLstAddZ(argList, "--repo1-host=remote-host");
        strLstAddZ(argList, "archive-get");
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_BOOL(repoIsLocal(0), false, "repo is remote");
        TEST_ERROR_FMT(repoIsLocalVerify(), HostInvalidError, "archive-get command must be run on the repository host");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pg1 is local");

        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--pg1-path=/path/to");
        strLstAddZ(argList, "--repo1-retention-full=1");
        strLstAddZ(argList, "backup");
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_BOOL(pgIsLocal(0), true, "pg is local");
        TEST_RESULT_VOID(pgIsLocalVerify(), "verify pg is local");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pg1 is not local");

        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
        hrnCfgArgRawZ(argList, cfgOptPgHost, "test1");
        strLstAddZ(argList, "--pg1-path=/path/to");
        strLstAddZ(argList, "restore");
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_BOOL(pgIsLocal(0), false, "pg1 is remote");
        TEST_ERROR_FMT(pgIsLocalVerify(), HostInvalidError, "restore command must be run on the PostgreSQL host");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pg7 is not local");

        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/bogus");
        strLstAddZ(argList, "--pg7-path=/path/to");
        strLstAddZ(argList, "--pg7-host=test1");
        hrnCfgArgRawZ(argList, cfgOptPg, "7");
        strLstAddZ(argList, "--" CFGOPT_REMOTE_TYPE "=" PROTOCOL_REMOTE_TYPE_PG);
        strLstAddZ(argList, "--process=0");
        strLstAddZ(argList, CFGCMD_BACKUP ":" CONFIG_COMMAND_ROLE_LOCAL);
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_BOOL(pgIsLocal(1), false, "pg7 is remote");
    }

    // *****************************************************************************************************************************
    if (testBegin("protocolHelperClientFree()"))
    {
        TEST_TITLE("free with errors output as warnings");

        // Create and free a mem context to give us an error to use
        MemContext *memContext = memContextNew("test");
        memContextFree(memContext);

        // Create bogus client and exec with the freed memcontext to generate errors
        ProtocolClient client = {.pub = {.memContext = memContext}, .name = STRDEF("test")};
        Exec exec = {.pub = {.memContext = memContext}, .name = STRDEF("test"), .command = strNew("test")};
        ProtocolHelperClient protocolHelperClient = {.client = &client, .exec = &exec};

        TEST_RESULT_VOID(protocolHelperClientFree(&protocolHelperClient), "free");

        harnessLogResult(
            "P00   WARN: cannot free inactive context\n"
            "P00   WARN: cannot free inactive context");
    }

    // *****************************************************************************************************************************
    if (testBegin("protocolLocalParam()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        strLstAddZ(argList, "archive-get");
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_STRLST_Z(
            protocolLocalParam(protocolStorageTypeRepo, 0, 0),
            "--exec-id=1-test\n--log-level-console=off\n--log-level-file=off\n--log-level-stderr=error\n--pg1-path=/path/to/pg\n"
                "--process=0\n--remote-type=repo\n--stanza=test1\narchive-get:local\n",
            "local repo protocol params");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--pg1-path=/pg");
        strLstAddZ(argList, "--repo1-retention-full=1");
        strLstAddZ(argList, "--log-subprocess");
        strLstAddZ(argList, "backup");
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_STRLST_Z(
            protocolLocalParam(protocolStorageTypePg, 0, 1),
            "--exec-id=1-test\n--log-level-console=off\n--log-level-file=info\n--log-level-stderr=error\n--log-subprocess\n--pg=1\n"
                "--pg1-path=/pg\n--process=1\n--remote-type=pg\n--stanza=test1\nbackup:local\n",
            "local pg protocol params");
    }

    // *****************************************************************************************************************************
    if (testBegin("protocolRemoteParam()"))
    {
        storagePutP(storageNewWriteP(storageTest, strNew("pgbackrest.conf")), bufNew(0));

        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        strLstAddZ(argList, "--repo1-host=repo-host");
        strLstAddZ(argList, "--repo1-host-user=repo-host-user");
        // Local config settings should never be passed to the remote
        strLstAdd(argList, strNewFmt("--config=%s/pgbackrest.conf", testPath()));
        strLstAdd(argList, strNewFmt("--config-include-path=%s", testPath()));
        strLstAdd(argList, strNewFmt("--config-path=%s", testPath()));
        strLstAddZ(argList, "archive-get");
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_STRLST_Z(
            protocolRemoteParam(protocolStorageTypeRepo, 0),
            "-o\nLogLevel=error\n-o\nCompression=no\n-o\nPasswordAuthentication=no\nrepo-host-user@repo-host\n"
                "pgbackrest --exec-id=1-test --log-level-console=off --log-level-file=off --log-level-stderr=error"
                " --pg1-path=/path/to/pg --process=0 --remote-type=repo --repo=1 --stanza=test1 archive-get:remote\n",
            "remote protocol params");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--log-subprocess");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/unused");            // Will be passed to remote (required)
        hrnCfgArgRawZ(argList, cfgOptPgPort, "777");                // Not passed to remote (required but has default)
        strLstAddZ(argList, "--repo1-host=repo-host");
        strLstAddZ(argList, "--repo1-host-port=444");
        strLstAddZ(argList, "--repo1-host-config=/path/pgbackrest.conf");
        strLstAddZ(argList, "--repo1-host-config-include-path=/path/include");
        strLstAddZ(argList, "--repo1-host-config-path=/path/config");
        strLstAddZ(argList, "--repo1-host-user=repo-host-user");
        strLstAddZ(argList, CFGCMD_CHECK);
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_STRLST_Z(
            protocolRemoteParam(protocolStorageTypeRepo, 0),
            "-o\nLogLevel=error\n-o\nCompression=no\n-o\nPasswordAuthentication=no\n-p\n444\nrepo-host-user@repo-host\n"
                "pgbackrest --config=/path/pgbackrest.conf --config-include-path=/path/include --config-path=/path/config"
                " --exec-id=1-test --log-level-console=off --log-level-file=info --log-level-stderr=error --log-subprocess"
                " --pg1-path=/unused --process=0 --remote-type=repo --repo=1 --stanza=test1 check:remote\n",
            "remote protocol params with replacements");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        strLstAddZ(argList, "--process=3");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        strLstAddZ(argList, "--" CFGOPT_REMOTE_TYPE "=" PROTOCOL_REMOTE_TYPE_REPO);
        strLstAddZ(argList, "--repo1-host=repo-host");
        strLstAddZ(argList, CFGCMD_ARCHIVE_GET ":" CONFIG_COMMAND_ROLE_LOCAL);
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_STRLST_Z(
            protocolRemoteParam(protocolStorageTypeRepo, 0),
            "-o\nLogLevel=error\n-o\nCompression=no\n-o\nPasswordAuthentication=no\npgbackrest@repo-host\n"
                "pgbackrest --exec-id=1-test --log-level-console=off --log-level-file=off --log-level-stderr=error"
                " --pg1-path=/path/to/pg --process=3 --remote-type=repo --repo=1 --stanza=test1 archive-get:remote\n",
            "remote protocol params for backup local");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--pg1-path=/path/to/1");
        strLstAddZ(argList, "--pg1-host=pg1-host");
        strLstAddZ(argList, "--repo1-retention-full=1");
        strLstAddZ(argList, "backup");
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_STRLST_Z(
            protocolRemoteParam(protocolStorageTypePg, 0),
            "-o\nLogLevel=error\n-o\nCompression=no\n-o\nPasswordAuthentication=no\npostgres@pg1-host\n"
                "pgbackrest --exec-id=1-test --log-level-console=off --log-level-file=off --log-level-stderr=error"
                " --pg1-path=/path/to/1 --process=0 --remote-type=pg --stanza=test1 backup:remote\n",
            "remote protocol params for db backup");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--process=4");
        hrnCfgArgRawZ(argList, cfgOptPg, "2");
        strLstAddZ(argList, "--pg1-path=/path/to/1");
        strLstAddZ(argList, "--pg1-socket-path=/socket3");
        strLstAddZ(argList, "--pg1-port=1111");
        strLstAddZ(argList, "--pg2-path=/path/to/2");
        strLstAddZ(argList, "--pg2-host=pg2-host");
        strLstAddZ(argList, "--" CFGOPT_REMOTE_TYPE "=" PROTOCOL_REMOTE_TYPE_PG);
        strLstAddZ(argList, CFGCMD_BACKUP ":" CONFIG_COMMAND_ROLE_LOCAL);
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_STRLST_Z(
            protocolRemoteParam(protocolStorageTypePg, 1),
            "-o\nLogLevel=error\n-o\nCompression=no\n-o\nPasswordAuthentication=no\npostgres@pg2-host\n"
                "pgbackrest --exec-id=1-test --log-level-console=off --log-level-file=off --log-level-stderr=error"
                " --pg1-path=/path/to/2 --process=4 --remote-type=pg --stanza=test1 backup:remote\n",
            "remote protocol params for db local");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--process=4");
        hrnCfgArgRawZ(argList, cfgOptPg, "3");
        strLstAddZ(argList, "--pg1-path=/path/to/1");
        strLstAddZ(argList, "--pg3-path=/path/to/3");
        strLstAddZ(argList, "--pg3-host=pg3-host");
        strLstAddZ(argList, "--pg3-socket-path=/socket3");
        strLstAddZ(argList, "--pg3-port=3333");
        strLstAddZ(argList, "--" CFGOPT_REMOTE_TYPE "=" PROTOCOL_REMOTE_TYPE_PG);
        strLstAddZ(argList, CFGCMD_BACKUP ":" CONFIG_COMMAND_ROLE_LOCAL);
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_STRLST_Z(
            protocolRemoteParam(protocolStorageTypePg, 1),
            "-o\nLogLevel=error\n-o\nCompression=no\n-o\nPasswordAuthentication=no\npostgres@pg3-host\n"
                "pgbackrest --exec-id=1-test --log-level-console=off --log-level-file=off --log-level-stderr=error"
                " --pg1-path=/path/to/3 --pg1-port=3333 --pg1-socket-path=/socket3 --process=4 --remote-type=pg --stanza=test1"
                " backup:remote\n",
            "remote protocol params for db local");
    }

    // *****************************************************************************************************************************
    if (testBegin("ProtocolCommand"))
    {
        ProtocolCommand *command = NULL;

        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(command, protocolCommandNew(strNew("command1")), "create command");
            TEST_RESULT_PTR(protocolCommandParamAdd(command, varNewStr(strNew("param1"))), command, "add param");
            TEST_RESULT_PTR(protocolCommandParamAdd(command, varNewStr(strNew("param2"))), command, "add param");

            TEST_RESULT_PTR(protocolCommandMove(command, memContextPrior()), command, "move protocol command");
            TEST_RESULT_PTR(protocolCommandMove(NULL, memContextPrior()), NULL, "move null protocol command");
        }
        MEM_CONTEXT_TEMP_END();

        TEST_RESULT_STR_Z(protocolCommandToLog(command), "{command: command1}", "check log");
        TEST_RESULT_STR_Z(protocolCommandJson(command), "{\"cmd\":\"command1\",\"param\":[\"param1\",\"param2\"]}", "check json");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(command, protocolCommandNew(strNew("command2")), "create command");
        TEST_RESULT_STR_Z(protocolCommandToLog(command), "{command: command2}", "check log");
        TEST_RESULT_STR_Z(protocolCommandJson(command), "{\"cmd\":\"command2\"}", "check json");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(protocolCommandFree(command), "free command");
    }

    // *****************************************************************************************************************************
    if (testBegin("ProtocolClient"))
    {
        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                IoRead *read = ioFdReadNew(strNew("server read"), HARNESS_FORK_CHILD_READ(), 2000);
                ioReadOpen(read);
                IoWrite *write = ioFdWriteNew(strNew("server write"), HARNESS_FORK_CHILD_WRITE(), 2000);
                ioWriteOpen(write);

                // Various bogus greetings
                ioWriteStrLine(write, strNew("bogus greeting"));
                ioWriteFlush(write);
                ioWriteStrLine(write, strNew("{\"name\":999}"));
                ioWriteFlush(write);
                ioWriteStrLine(write, strNew("{\"name\":null}"));
                ioWriteFlush(write);
                ioWriteStrLine(write, strNew("{\"name\":\"bogus\"}"));
                ioWriteFlush(write);
                ioWriteStrLine(write, strNew("{\"name\":\"pgBackRest\",\"service\":\"bogus\"}"));
                ioWriteFlush(write);
                ioWriteStrLine(write, strNew("{\"name\":\"pgBackRest\",\"service\":\"test\",\"version\":\"bogus\"}"));
                ioWriteFlush(write);

                // Correct greeting with noop
                ioWriteStrLine(write, strNew("{\"name\":\"pgBackRest\",\"service\":\"test\",\"version\":\"" PROJECT_VERSION "\"}"));
                ioWriteFlush(write);

                TEST_RESULT_STR_Z(ioReadLine(read), "{\"cmd\":\"noop\"}", "noop");
                ioWriteStrLine(write, strNew("{}"));
                ioWriteFlush(write);

                // Throw errors
                TEST_RESULT_STR_Z(ioReadLine(read), "{\"cmd\":\"noop\"}", "noop with error text");
                ioWriteStrLine(write, strNew("{\"err\":25,\"out\":\"sample error message\",\"errStack\":\"stack data\"}"));
                ioWriteFlush(write);

                TEST_RESULT_STR_Z(ioReadLine(read), "{\"cmd\":\"noop\"}", "noop with no error text");
                ioWriteStrLine(write, strNew("{\"err\":255}"));
                ioWriteFlush(write);

                // No output expected
                TEST_RESULT_STR_Z(ioReadLine(read), "{\"cmd\":\"noop\"}", "noop with parameters returned");
                ioWriteStrLine(write, strNew("{\"out\":[\"bogus\"]}"));
                ioWriteFlush(write);

                // Send output
                TEST_RESULT_STR_Z(ioReadLine(read), "{\"cmd\":\"test\"}", "test command");
                ioWriteStrLine(write, strNew(".OUTPUT"));
                ioWriteStrLine(write, strNew("{\"out\":[\"value1\",\"value2\"]}"));
                ioWriteFlush(write);

                // invalid line
                TEST_RESULT_STR_Z(ioReadLine(read), "{\"cmd\":\"invalid-line\"}", "invalid line command");
                ioWrite(write, LF_BUF);
                ioWriteFlush(write);

                // error instead of output
                TEST_RESULT_STR_Z(ioReadLine(read), "{\"cmd\":\"error-instead-of-output\"}", "error instead of output command");
                ioWriteStrLine(write, strNew("{\"err\":255}"));
                ioWriteFlush(write);

                // unexpected output
                TEST_RESULT_STR_Z(ioReadLine(read), "{\"cmd\":\"unexpected-output\"}", "unexpected output");
                ioWriteStrLine(write, strNew("{}"));
                ioWriteFlush(write);

                // invalid prefix
                TEST_RESULT_STR_Z(ioReadLine(read), "{\"cmd\":\"invalid-prefix\"}", "invalid prefix");
                ioWriteStrLine(write, strNew("~line"));
                ioWriteFlush(write);

                // Wait for exit
                TEST_RESULT_STR_Z(ioReadLine(read), "{\"cmd\":\"exit\"}", "exit command");
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                IoRead *read = ioFdReadNew(strNew("client read"), HARNESS_FORK_PARENT_READ_PROCESS(0), 2000);
                ioReadOpen(read);
                IoWrite *write = ioFdWriteNew(strNew("client write"), HARNESS_FORK_PARENT_WRITE_PROCESS(0), 2000);
                ioWriteOpen(write);

                // Various bogus greetings
                TEST_ERROR(
                    protocolClientNew(strNew("test client"), strNew("test"), read, write), JsonFormatError,
                    "expected '{' at 'bogus greeting'");
                TEST_ERROR(
                    protocolClientNew(strNew("test client"), strNew("test"), read, write), ProtocolError,
                    "greeting key 'name' must be string type");
                TEST_ERROR(
                    protocolClientNew(strNew("test client"), strNew("test"), read, write), ProtocolError,
                    "unable to find greeting key 'name'");
                TEST_ERROR(
                    protocolClientNew(strNew("test client"), strNew("test"), read, write), ProtocolError,
                    "expected value 'pgBackRest' for greeting key 'name' but got 'bogus'\n"
                    "HINT: is the same version of " PROJECT_NAME " installed on the local and remote host?");
                TEST_ERROR(
                    protocolClientNew(strNew("test client"), strNew("test"), read, write), ProtocolError,
                    "expected value 'test' for greeting key 'service' but got 'bogus'\n"
                    "HINT: is the same version of " PROJECT_NAME " installed on the local and remote host?");
                TEST_ERROR(
                    protocolClientNew(strNew("test client"), strNew("test"), read, write), ProtocolError,
                    "expected value '" PROJECT_VERSION "' for greeting key 'version' but got 'bogus'\n"
                    "HINT: is the same version of " PROJECT_NAME " installed on the local and remote host?");

                // Correct greeting
                ProtocolClient *client = NULL;

                MEM_CONTEXT_TEMP_BEGIN()
                {
                    TEST_ASSIGN(
                        client,
                        protocolClientMove(
                            protocolClientNew(strNew("test client"), strNew("test"), read, write), memContextPrior()),
                        "create client");
                    TEST_RESULT_VOID(protocolClientMove(NULL, memContextPrior()), "move null client");
                }
                MEM_CONTEXT_TEMP_END();

                TEST_RESULT_PTR(protocolClientIoRead(client), client->pub.read, "get read io");
                TEST_RESULT_PTR(protocolClientIoWrite(client), client->pub.write, "get write io");

                // Throw errors
                TEST_ERROR(
                    protocolClientNoOp(client), AssertError,
                    "raised from test client: sample error message\nstack data");

                harnessLogLevelSet(logLevelDebug);
                TEST_ERROR(
                    protocolClientNoOp(client), UnknownError,
                    "raised from test client: no details available\nno stack trace available");
                harnessLogLevelReset();

                // No output expected
                TEST_ERROR(protocolClientNoOp(client), AssertError, "no output required by command");

                // Get command output
                const VariantList *output = NULL;

                TEST_RESULT_VOID(
                    protocolClientWriteCommand(client, protocolCommandNew(strNew("test"))), "execute command with output");
                TEST_RESULT_STR_Z(protocolClientReadLine(client), "OUTPUT", "check output");
                TEST_ASSIGN(output, varVarLst(protocolClientReadOutput(client, true)), "execute command with output");
                TEST_RESULT_UINT(varLstSize(output), 2, "check output size");
                TEST_RESULT_STR_Z(varStr(varLstGet(output, 0)), "value1", "check value1");
                TEST_RESULT_STR_Z(varStr(varLstGet(output, 1)), "value2", "check value2");

                // Invalid line
                TEST_RESULT_VOID(
                    protocolClientWriteCommand(client, protocolCommandNew(strNew("invalid-line"))),
                    "execute command that returns invalid line");
                TEST_ERROR(protocolClientReadLine(client), FormatError, "unexpected empty line");

                // Error instead of output
                TEST_RESULT_VOID(
                    protocolClientWriteCommand(client, protocolCommandNew(strNew("error-instead-of-output"))),
                    "execute command that returns error instead of output");
                TEST_ERROR(protocolClientReadLine(client), UnknownError, "raised from test client: no details available");

                // Unexpected output
                TEST_RESULT_VOID(
                    protocolClientWriteCommand(client, protocolCommandNew(strNew("unexpected-output"))),
                    "execute command that returns unexpected output");
                TEST_ERROR(protocolClientReadLine(client), FormatError, "expected error but got output");

                // Invalid prefix
                TEST_RESULT_VOID(
                    protocolClientWriteCommand(client, protocolCommandNew(strNew("invalid-prefix"))),
                    "execute command that returns an invalid prefix");
                TEST_ERROR(protocolClientReadLine(client), FormatError, "invalid prefix in '~line'");

                // Free client
                TEST_RESULT_VOID(protocolClientFree(client), "free client");
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();
    }

    // *****************************************************************************************************************************
    if (testBegin("ProtocolServer"))
    {
        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                IoRead *read = ioFdReadNew(strNew("client read"), HARNESS_FORK_CHILD_READ(), 2000);
                ioReadOpen(read);
                IoWrite *write = ioFdWriteNew(strNew("client write"), HARNESS_FORK_CHILD_WRITE(), 2000);
                ioWriteOpen(write);

                // Check greeting
                TEST_RESULT_STR_Z(
                    ioReadLine(read), "{\"name\":\"pgBackRest\",\"service\":\"test\",\"version\":\"" PROJECT_VERSION "\"}",
                    "check greeting");

                // Noop
                TEST_RESULT_VOID(ioWriteStrLine(write, strNew("{\"cmd\":\"noop\"}")), "write noop");
                TEST_RESULT_VOID(ioWriteFlush(write), "flush noop");
                TEST_RESULT_STR_Z(ioReadLine(read), "{}", "noop result");

                // Invalid command
                KeyValue *result = NULL;

                TEST_RESULT_VOID(ioWriteStrLine(write, strNew("{\"cmd\":\"bogus\"}")), "write bogus");
                TEST_RESULT_VOID(ioWriteFlush(write), "flush bogus");
                TEST_ASSIGN(result, varKv(jsonToVar(ioReadLine(read))), "parse error result");
                TEST_RESULT_INT(varIntForce(kvGet(result, VARSTRDEF("err"))), 39, "    check code");
                TEST_RESULT_STR_Z(varStr(kvGet(result, VARSTRDEF("out"))), "invalid command 'bogus'", "    check message");
                TEST_RESULT_BOOL(kvGet(result, VARSTRDEF("errStack")) != NULL, true, "    check stack exists");

                // Simple request
                TEST_RESULT_VOID(ioWriteStrLine(write, strNew("{\"cmd\":\"request-simple\"}")), "write simple request");
                TEST_RESULT_VOID(ioWriteFlush(write), "flush simple request");
                TEST_RESULT_STR_Z(ioReadLine(read), "{\"out\":true}", "simple request result");

                // Throw an assert error which will include a stack trace
                TEST_RESULT_VOID(ioWriteStrLine(write, strNew("{\"cmd\":\"assert\"}")), "write assert");
                TEST_RESULT_VOID(ioWriteFlush(write), "flush assert error");
                TEST_ASSIGN(result, varKv(jsonToVar(ioReadLine(read))), "parse error result");
                TEST_RESULT_INT(varIntForce(kvGet(result, VARSTRDEF("err"))), 25, "    check code");
                TEST_RESULT_STR_Z(varStr(kvGet(result, VARSTRDEF("out"))), "test assert", "    check message");
                TEST_RESULT_BOOL(kvGet(result, VARSTRDEF("errStack")) != NULL, true, "    check stack exists");

                // Complex request -- after process loop has been restarted
                TEST_RESULT_VOID(ioWriteStrLine(write, strNew("{\"cmd\":\"request-complex\"}")), "write complex request");
                TEST_RESULT_VOID(ioWriteFlush(write), "flush complex request");
                TEST_RESULT_STR_Z(ioReadLine(read), "{\"out\":false}", "complex request result");
                TEST_RESULT_STR_Z(ioReadLine(read), ".LINEOFTEXT", "complex request result");
                TEST_RESULT_STR_Z(ioReadLine(read), ".", "complex request result");

                // Exit
                TEST_RESULT_VOID(ioWriteStrLine(write, strNew("{\"cmd\":\"exit\"}")), "write exit");
                TEST_RESULT_VOID(ioWriteFlush(write), "flush exit");

                // Retry errors until success
                TEST_RESULT_VOID(ioWriteStrLine(write, strNew("{\"cmd\":\"error-until-0\"}")), "write error-until-0");
                TEST_RESULT_VOID(ioWriteFlush(write), "flush error-until-0");
                TEST_RESULT_STR_Z(ioReadLine(read), "{\"out\":true}", "error-until-0 result");

                // Exit
                TEST_RESULT_VOID(ioWriteStrLine(write, strNew("{\"cmd\":\"exit\"}")), "write exit");
                TEST_RESULT_VOID(ioWriteFlush(write), "flush exit");
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                IoRead *read = ioFdReadNew(strNew("server read"), HARNESS_FORK_PARENT_READ_PROCESS(0), 2000);
                ioReadOpen(read);
                IoWrite *write = ioFdWriteNew(strNew("server write"), HARNESS_FORK_PARENT_WRITE_PROCESS(0), 2000);
                ioWriteOpen(write);

                // Send greeting
                ProtocolServer *server = NULL;

                MEM_CONTEXT_TEMP_BEGIN()
                {
                    TEST_ASSIGN(
                        server,
                        protocolServerMove(
                            protocolServerNew(strNew("test server"), strNew("test"), read, write), memContextPrior()),
                        "create server");
                    TEST_RESULT_VOID(protocolServerMove(NULL, memContextPrior()), "move null server");
                }
                MEM_CONTEXT_TEMP_END();

                TEST_RESULT_PTR(protocolServerIoRead(server), server->pub.read, "get read io");
                TEST_RESULT_PTR(protocolServerIoWrite(server), server->pub.write, "get write io");

                static const ProtocolServerHandler commandHandler[] =
                {
                    {.command = "assert", .handler = testServerAssertProtocol},
                    {.command = "request-simple", .handler = testServerRequestSimpleProtocol},
                    {.command = "request-complex", .handler = testServerRequestComplexProtocol},
                    {.command = "error-until-0", .handler = testServerErrorUntil0Protocol},
                };

                TEST_RESULT_VOID(
                    protocolServerProcess(server, NULL, commandHandler, PROTOCOL_SERVER_HANDLER_LIST_SIZE(commandHandler)),
                    "run process loop");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("run process loop with retries");

                VariantList *retryInterval = varLstNew();
                varLstAdd(retryInterval, varNewUInt64(0));
                varLstAdd(retryInterval, varNewUInt64(50));

                testServerProtocolErrorTotal = 2;

                TEST_RESULT_VOID(
                    protocolServerProcess(server, retryInterval, commandHandler, PROTOCOL_SERVER_HANDLER_LIST_SIZE(commandHandler)),
                    "run process loop");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("free server");

                TEST_RESULT_VOID(protocolServerFree(server), "free");
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();
    }

    // *****************************************************************************************************************************
    if (testBegin("ProtocolParallel and ProtocolParallelJob"))
    {
        ProtocolParallelJob *job = NULL;

        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(job, protocolParallelJobNew(varNewStr(strNew("test")), protocolCommandNew(strNew("command"))), "new job");
            TEST_RESULT_PTR(protocolParallelJobMove(job, memContextPrior()), job, "move job");
            TEST_RESULT_PTR(protocolParallelJobMove(NULL, memContextPrior()), NULL, "move null job");
        }
        MEM_CONTEXT_TEMP_END();

        TEST_ERROR(
            protocolParallelJobStateSet(job, protocolParallelJobStateDone), AssertError,
            "invalid state transition from 'pending' to 'done'");
        TEST_RESULT_VOID(protocolParallelJobStateSet(job, protocolParallelJobStateRunning), "transition to running");
        TEST_ERROR(
            protocolParallelJobStateSet(job, protocolParallelJobStatePending), AssertError,
            "invalid state transition from 'running' to 'pending'");

        // Free job
        TEST_RESULT_VOID(protocolParallelJobFree(job), "free job");

        // -------------------------------------------------------------------------------------------------------------------------
        HARNESS_FORK_BEGIN()
        {
            // Local 1
            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                IoRead *read = ioFdReadNew(strNew("server read"), HARNESS_FORK_CHILD_READ(), 10000);
                ioReadOpen(read);
                IoWrite *write = ioFdWriteNew(strNew("server write"), HARNESS_FORK_CHILD_WRITE(), 2000);
                ioWriteOpen(write);

                // Greeting with noop
                ioWriteStrLine(write, strNew("{\"name\":\"pgBackRest\",\"service\":\"test\",\"version\":\"" PROJECT_VERSION "\"}"));
                ioWriteFlush(write);

                TEST_RESULT_STR_Z(ioReadLine(read), "{\"cmd\":\"noop\"}", "noop");
                ioWriteStrLine(write, strNew("{}"));
                ioWriteFlush(write);

                TEST_RESULT_STR_Z(ioReadLine(read), "{\"cmd\":\"command1\",\"param\":[\"param1\",\"param2\"]}", "command1");
                sleepMSec(4000);
                ioWriteStrLine(write, strNew("{\"out\":1}"));
                ioWriteFlush(write);

                // Wait for exit
                TEST_RESULT_STR_Z(ioReadLine(read), "{\"cmd\":\"exit\"}", "exit command");
            }
            HARNESS_FORK_CHILD_END();

            // Local 2
            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                IoRead *read = ioFdReadNew(strNew("server read"), HARNESS_FORK_CHILD_READ(), 10000);
                ioReadOpen(read);
                IoWrite *write = ioFdWriteNew(strNew("server write"), HARNESS_FORK_CHILD_WRITE(), 2000);
                ioWriteOpen(write);

                // Greeting with noop
                ioWriteStrLine(write, strNew("{\"name\":\"pgBackRest\",\"service\":\"test\",\"version\":\"" PROJECT_VERSION "\"}"));
                ioWriteFlush(write);

                TEST_RESULT_STR_Z(ioReadLine(read), "{\"cmd\":\"noop\"}", "noop");
                ioWriteStrLine(write, strNew("{}"));
                ioWriteFlush(write);

                TEST_RESULT_STR_Z(ioReadLine(read), "{\"cmd\":\"command2\",\"param\":[\"param1\"]}", "command2");
                sleepMSec(1000);
                ioWriteStrLine(write, strNew("{\"out\":2}"));
                ioWriteFlush(write);

                TEST_RESULT_STR_Z(ioReadLine(read), "{\"cmd\":\"command3\",\"param\":[\"param1\"]}", "command3");

                ioWriteStrLine(write, strNew("{\"err\":39,\"out\":\"very serious error\"}"));
                ioWriteFlush(write);

                // Wait for exit
                TEST_RESULT_STR_Z(ioReadLine(read), "{\"cmd\":\"exit\"}", "exit command");
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                // -----------------------------------------------------------------------------------------------------------------
                TestParallelJobCallback data = {.jobList = lstNewP(sizeof(ProtocolParallelJob *))};
                ProtocolParallel *parallel = NULL;
                TEST_ASSIGN(parallel, protocolParallelNew(2000, testParallelJobCallback, &data), "create parallel");
                TEST_RESULT_STR_Z(protocolParallelToLog(parallel), "{state: pending, clientTotal: 0, jobTotal: 0}", "check log");

                // Add client
                unsigned int clientTotal = 2;
                ProtocolClient *client[HARNESS_FORK_CHILD_MAX];

                for (unsigned int clientIdx = 0; clientIdx < clientTotal; clientIdx++)
                {
                    IoRead *read = ioFdReadNew(
                        strNewFmt("client %u read", clientIdx), HARNESS_FORK_PARENT_READ_PROCESS(clientIdx), 2000);
                    ioReadOpen(read);
                    IoWrite *write = ioFdWriteNew(
                        strNewFmt("client %u write", clientIdx), HARNESS_FORK_PARENT_WRITE_PROCESS(clientIdx), 2000);
                    ioWriteOpen(write);

                    TEST_ASSIGN(
                        client[clientIdx],
                        protocolClientNew(strNewFmt("test client %u", clientIdx), strNew("test"), read, write),
                        "create client %u", clientIdx);
                    TEST_RESULT_VOID(protocolParallelClientAdd(parallel, client[clientIdx]), "add client %u", clientIdx);
                }

                // Attempt to add client without an fd
                String *protocolString = strNew(
                    "{\"name\":\"pgBackRest\",\"service\":\"error\",\"version\":\"" PROJECT_VERSION "\"}\n"
                    "{}\n");

                IoRead *read = ioBufferReadNew(BUFSTR(protocolString));
                ioReadOpen(read);
                IoWrite *write = ioBufferWriteNew(bufNew(1024));
                ioWriteOpen(write);

                ProtocolClient *clientError = protocolClientNew(strNew("error"), strNew("error"), read, write);
                TEST_ERROR(protocolParallelClientAdd(parallel, clientError), AssertError, "client with read fd is required");
                protocolClientFree(clientError);

                // Add jobs
                ProtocolCommand *command = protocolCommandNew(strNew("command1"));
                protocolCommandParamAdd(command, varNewStr(strNew("param1")));
                protocolCommandParamAdd(command, varNewStr(strNew("param2")));
                ProtocolParallelJob *job = protocolParallelJobNew(varNewStr(strNew("job1")), command);
                TEST_RESULT_VOID(lstAdd(data.jobList, &job), "add job");

                command = protocolCommandNew(strNew("command2"));
                protocolCommandParamAdd(command, varNewStr(strNew("param1")));
                job = protocolParallelJobNew(varNewStr(strNew("job2")), command);
                TEST_RESULT_VOID(lstAdd(data.jobList, &job), "add job");

                command = protocolCommandNew(strNew("command3"));
                protocolCommandParamAdd(command, varNewStr(strNew("param1")));
                job = protocolParallelJobNew(varNewStr(strNew("job3")), command);
                TEST_RESULT_VOID(lstAdd(data.jobList, &job), "add job");

                // Process jobs
                TEST_RESULT_INT(protocolParallelProcess(parallel), 0, "process jobs");

                TEST_RESULT_PTR(protocolParallelResult(parallel), NULL, "check no result");

                // Process jobs
                TEST_RESULT_INT(protocolParallelProcess(parallel), 1, "process jobs");

                TEST_ASSIGN(job, protocolParallelResult(parallel), "get result");
                TEST_RESULT_STR_Z(varStr(protocolParallelJobKey(job)), "job2", "check key is job2");
                TEST_RESULT_BOOL(
                    protocolParallelJobProcessId(job) >= 1 && protocolParallelJobProcessId(job) <= 2, true,
                    "check process id is valid");
                TEST_RESULT_INT(varIntForce(protocolParallelJobResult(job)), 2, "check result is 2");

                TEST_RESULT_PTR(protocolParallelResult(parallel), NULL, "check no more results");

                // Process jobs
                TEST_RESULT_INT(protocolParallelProcess(parallel), 1, "process jobs");

                TEST_ASSIGN(job, protocolParallelResult(parallel), "get result");
                TEST_RESULT_STR_Z(varStr(protocolParallelJobKey(job)), "job3", "check key is job3");
                TEST_RESULT_INT(protocolParallelJobErrorCode(job), 39, "check error code");
                TEST_RESULT_STR_Z(
                    protocolParallelJobErrorMessage(job), "raised from test client 1: very serious error",
                    "check error message");
                TEST_RESULT_PTR(protocolParallelJobResult(job), NULL, "check result is null");

                TEST_RESULT_PTR(protocolParallelResult(parallel), NULL, "check no more results");

                // Process jobs
                TEST_RESULT_INT(protocolParallelProcess(parallel), 0, "process jobs");

                TEST_RESULT_PTR(protocolParallelResult(parallel), NULL, "check no result");
                TEST_RESULT_BOOL(protocolParallelDone(parallel), false, "check not done");

                // Process jobs
                TEST_RESULT_INT(protocolParallelProcess(parallel), 1, "process jobs");

                TEST_ASSIGN(job, protocolParallelResult(parallel), "get result");
                TEST_RESULT_STR_Z(varStr(protocolParallelJobKey(job)), "job1", "check key is job1");
                TEST_RESULT_INT(varIntForce(protocolParallelJobResult(job)), 1, "check result is 1");

                TEST_RESULT_BOOL(protocolParallelDone(parallel), true, "check done");
                TEST_RESULT_BOOL(protocolParallelDone(parallel), true, "check still done");

                TEST_RESULT_VOID(protocolParallelFree(parallel), "free parallel");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("process zero jobs");

                data = (TestParallelJobCallback){.jobList = lstNewP(sizeof(ProtocolParallelJob *))};
                TEST_ASSIGN(parallel, protocolParallelNew(2000, testParallelJobCallback, &data), "create parallel");
                TEST_RESULT_VOID(protocolParallelClientAdd(parallel, client[0]), "add client");

                TEST_RESULT_INT(protocolParallelProcess(parallel), 0, "process zero jobs");
                TEST_RESULT_BOOL(protocolParallelDone(parallel), true, "check done");

                TEST_RESULT_VOID(protocolParallelFree(parallel), "free parallel");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("free clients");

                for (unsigned int clientIdx = 0; clientIdx < clientTotal; clientIdx++)
                    TEST_RESULT_VOID(protocolClientFree(client[clientIdx]), "free client %u", clientIdx);
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();
    }

    // *****************************************************************************************************************************
    if (testBegin("protocolGet()"))
    {
        // Call remote free before any remotes exist
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(protocolRemoteFree(1), "free remote (non exist)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("free local that does not exist");

        TEST_RESULT_VOID(protocolLocalFree(2), "free");

        // Call keep alive before any remotes exist
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(protocolKeepAlive(), "keep alive");

        // Simple protocol start
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argList = strLstNew();
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--protocol-timeout=10");
        strLstAddZ(argList, "--repo1-host=localhost");
        strLstAdd(argList, strNewFmt("--repo1-host-user=%s", testUser()));
        strLstAdd(argList, strNewFmt("--repo1-path=%s", testPath()));
        harnessCfgLoad(cfgCmdInfo, argList);

        ProtocolClient *client = NULL;

        TEST_RESULT_VOID(protocolFree(), "free protocol objects before anything has been created");

        TEST_ASSIGN(client, protocolRemoteGet(protocolStorageTypeRepo, 0), "get remote protocol");
        TEST_RESULT_PTR(protocolRemoteGet(protocolStorageTypeRepo, 0), client, "get remote cached protocol");
        TEST_RESULT_PTR(protocolHelper.clientRemote[0].client, client, "check position in cache");
        TEST_RESULT_VOID(protocolKeepAlive(), "keep alive");
        TEST_RESULT_VOID(protocolFree(), "free remote protocol objects");
        TEST_RESULT_VOID(protocolFree(), "free remote protocol objects again");

        // Start protocol with local encryption settings
        // -------------------------------------------------------------------------------------------------------------------------
        storagePut(
            storageNewWriteP(storageTest, strNew("pgbackrest.conf")),
            BUFSTRDEF(
                "[global]\n"
                "repo1-cipher-type=aes-256-cbc\n"
                "repo1-cipher-pass=acbd\n"));

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=db");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        strLstAddZ(argList, "--protocol-timeout=10");
        strLstAdd(argList, strNewFmt("--config=%s/pgbackrest.conf", testPath()));
        strLstAddZ(argList, "--repo1-host=localhost");
        strLstAdd(argList, strNewFmt("--repo1-host-user=%s", testUser()));
        strLstAdd(argList, strNewFmt("--repo1-path=%s", testPath()));
        strLstAddZ(argList, "--process=999");
        strLstAddZ(argList, "--" CFGOPT_REMOTE_TYPE "=" PROTOCOL_REMOTE_TYPE_PG);
        harnessCfgLoadRole(cfgCmdArchiveGet, cfgCmdRoleLocal, argList);

        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptRepoCipherPass), "acbd", "check cipher pass before");
        TEST_ASSIGN(client, protocolRemoteGet(protocolStorageTypeRepo, 0), "get remote protocol");
        TEST_RESULT_PTR(protocolHelper.clientRemote[0].client, client, "check position in cache");
        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptRepoCipherPass), "acbd", "check cipher pass after");

        TEST_RESULT_VOID(protocolFree(), "free remote protocol objects");

        // Start protocol with remote encryption settings
        // -------------------------------------------------------------------------------------------------------------------------
        storagePut(
            storageNewWriteP(storageTest, strNew("pgbackrest.conf")),
            BUFSTRDEF(
                "[global]\n"
                "repo1-cipher-type=aes-256-cbc\n"
                "repo1-cipher-pass=dcba\n"
                "repo2-cipher-type=aes-256-cbc\n"
                "repo2-cipher-pass=xxxx\n"));

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--pg1-path=/pg");
        strLstAddZ(argList, "--protocol-timeout=10");
        strLstAdd(argList, strNewFmt("--repo1-host-config=%s/pgbackrest.conf", testPath()));
        strLstAddZ(argList, "--repo1-host=localhost");
        strLstAdd(argList, strNewFmt("--repo1-host-user=%s", testUser()));
        strLstAdd(argList, strNewFmt("--repo1-path=%s", testPath()));
        strLstAdd(argList, strNewFmt("--repo2-host-config=%s/pgbackrest.conf", testPath()));
        strLstAddZ(argList, "--repo2-host=localhost");
        strLstAdd(argList, strNewFmt("--repo2-host-user=%s", testUser()));
        strLstAdd(argList, strNewFmt("--repo2-path=%s2", testPath()));
        harnessCfgLoad(cfgCmdCheck, argList);

        TEST_RESULT_PTR(cfgOptionIdxStrNull(cfgOptRepoCipherPass, 0), NULL, "check repo1 cipher pass before");
        TEST_ASSIGN(client, protocolRemoteGet(protocolStorageTypeRepo, 0), "get repo1 remote protocol");
        TEST_RESULT_STR_Z(cfgOptionIdxStr(cfgOptRepoCipherPass, 0), "dcba", "check repo1 cipher pass after");

        TEST_RESULT_PTR(cfgOptionIdxStrNull(cfgOptRepoCipherPass, 1), NULL, "check repo2 cipher pass before");
        TEST_RESULT_VOID(protocolRemoteGet(protocolStorageTypeRepo, 1), "get repo2 remote protocol");
        TEST_RESULT_STR_Z(cfgOptionIdxStr(cfgOptRepoCipherPass, 1), "xxxx", "check repo2 cipher pass after");

        TEST_RESULT_VOID(protocolFree(), "free remote protocol objects");

        // Start remote protocol
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--protocol-timeout=10");
        strLstAddZ(argList, "--repo1-retention-full=1");
        strLstAddZ(argList, "--pg1-host=localhost");
        strLstAdd(argList, strNewFmt("--pg1-host-user=%s", testUser()));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", testPath()));
        harnessCfgLoad(cfgCmdBackup, argList);

        TEST_ASSIGN(client, protocolRemoteGet(protocolStorageTypePg, 0), "get remote protocol");

        TEST_RESULT_VOID(protocolFree(), "free local and remote protocol objects");

        // Start local protocol
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=db");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        strLstAddZ(argList, "--protocol-timeout=10");
        strLstAddZ(argList, "--process-max=2");
        harnessCfgLoad(cfgCmdArchiveGet, argList);

        TEST_ASSIGN(client, protocolLocalGet(protocolStorageTypeRepo, 0, 1), "get local protocol");
        TEST_RESULT_PTR(protocolLocalGet(protocolStorageTypeRepo, 0, 1), client, "get local cached protocol");
        TEST_RESULT_PTR(protocolHelper.clientLocal[0].client, client, "check location in cache");

        TEST_RESULT_VOID(protocolFree(), "free local and remote protocol objects");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
