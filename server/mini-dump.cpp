
#include <windows.h>
#include <dbghelp.h>
#include <time.h>

namespace distcc
{

///////////////////////////////////////////////////////////////////////////////////////////////

class MiniDumpHandler
{
	static LONG WINAPI Filter(struct _EXCEPTION_POINTERS *pExceptionInfo);

public:
	MiniDumpHandler();
};

//---------------------------------------------------------------------------------------------

static MiniDumpHandler mini_dump_handler;

//---------------------------------------------------------------------------------------------

MiniDumpHandler::MiniDumpHandler()
{
	SetUnhandledExceptionFilter(Filter);
}

//---------------------------------------------------------------------------------------------

LONG MiniDumpHandler::Filter(struct _EXCEPTION_POINTERS *pExceptionInfo)
{
	char dump_path[_MAX_PATH];

	GetModuleFileName(NULL, dump_path, sizeof(dump_path));
	strcat(dump_path, ".");

	time_t t = time(0);
	struct tm *tm = localtime(&t);
	char ts[64];
	strftime(ts, sizeof(ts), "%Y%m%d-%H%M%S", tm);
	strcat(dump_path, ts);
	strcat(dump_path, ".dmp");

	HANDLE file = CreateFile(dump_path, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, 
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (file == INVALID_HANDLE_VALUE)
		return EXCEPTION_EXECUTE_HANDLER;

	_MINIDUMP_EXCEPTION_INFORMATION ex_info;
	ex_info.ThreadId = GetCurrentThreadId();
	ex_info.ExceptionPointers = pExceptionInfo;
	ex_info.ClientPointers = NULL;

	BOOL rc = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, 
		MiniDumpNormal, &ex_info, NULL, NULL);
	FlushFileBuffers(file);
	DWORD size = GetFileSize(file, NULL);
	CloseHandle(file);
	if (size != INVALID_FILE_SIZE && !size)
		DeleteFile(dump_path);

	return EXCEPTION_EXECUTE_HANDLER;
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
