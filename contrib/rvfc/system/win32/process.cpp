
#include <stdexcept>
#include <string>
#include <io.h>

#include "rvfc/win.h"
#include <tlhelp32.h>

#include "rvfc/system/win32/process.h"

namespace rvfc
{

///////////////////////////////////////////////////////////////////////////////////////////////

std::string Process::programPath() const
{
	HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPALL, _pid);
	MODULEENTRY32 mod;
	if (! Module32First(h, &mod))
		throw std::runtime_error("cannot retreive process program path");
	return mod.szExePath;
}

///////////////////////////////////////////////////////////////////////////////////////////////

//NewProcess::NewProcess(const char *command, const char *cwd, const Environment &env)
//NewProcess::NewProcess(const char *cmdFile, const Arguments &args, const char *cwd, const Environment &env)

//---------------------------------------------------------------------------------------------

NewProcess::NewProcess(const std::string &command, const char *cwd) :
	_stdio(true)
{
	// Set up inherited stdin, stdout, stderr for child
	STARTUPINFO start_info;
	ZeroMemory(&start_info, sizeof(start_info)); 
//	GetStartupInfo(&start_info);
	start_info.dwFlags = STARTF_USESTDHANDLES;
	start_info.lpReserved = 0;
	start_info.cbReserved2 = 0;
	start_info.lpReserved2 = 0;
//	start_info.lpTitle = command;
	start_info.hStdInput = _stdio.in;
	start_info.hStdOutput = _stdio.out;
	start_info.hStdError = _stdio.err;

	DWORD flags = CREATE_SUSPENDED;
	PROCESS_INFORMATION proc_info;
	BOOL rc = CreateProcess(NULL, (LPSTR) command.c_str(),
		NULL,  // process security attributes
		NULL,  // thread security attributes
		TRUE,  // inherit handles (e.g. helper pipes, oserv socket)
		flags, // flags
		NULL,  // environment
		cwd,   // current directory
		&start_info, &proc_info);
	if (rc == FALSE) 
		throw std::runtime_error("NewProcess: cannot create process");

	_handle = proc_info.hProcess;
	thread_handle = proc_info.hThread;
	_pid = proc_info.dwProcessId;
	if (_pid == 0)
		throw std::runtime_error("NewProcess: create process failed: pid == 0");

	_stdio.close();

	register_();
}

//---------------------------------------------------------------------------------------------

NewProcess::~NewProcess()
{
	_stdio.close();

	thread_handle.close();
	_handle.close();
}

//---------------------------------------------------------------------------------------------

void
NewProcess::run()
{
	ResumeThread(thread_handle);
}

//---------------------------------------------------------------------------------------------

bool
NewProcess::kill(int retcode)
{
	return !!TerminateProcess(_handle, retcode);
}

//---------------------------------------------------------------------------------------------

NewProcess *NewProcess::static_running[MAX_STATIC_RUNNING];
NewProcess **NewProcess::_running = NewProcess::static_running;
int NewProcess::num_running = 0, NewProcess::max_num_running = MAX_STATIC_RUNNING;

//---------------------------------------------------------------------------------------------

void
NewProcess::register_()
{
	if (num_running >= max_num_running)
	{
		num_running *= 2;
		if (_running == static_running)
		{
			_running = (NewProcess**) malloc(sizeof(NewProcess*) * num_running);
			memcpy(_running, static_running, sizeof(static_running));
		}
		else
			_running = (NewProcess **) realloc(_running, num_running);
	}
	
	_running[num_running++] = this;
}

//---------------------------------------------------------------------------------------------

NewProcess *
NewProcess::_waitForAny(DWORD timeout)
{
	if (!num_running)
		return 0;

	HANDLE handles[MAXIMUM_WAIT_OBJECTS];
	for (int i = 0; i < num_running; ++i) 
		handles[i] = _running[i]->handle();

	DWORD rc = WaitForMultipleObjects(num_running, handles, FALSE, timeout);
	if (rc == WAIT_FAILED || rc == WAIT_TIMEOUT)
		return 0;

	int which = rc - WAIT_OBJECT_0;
	NewProcess *proc = _running[which];
	if (!proc)
		return 0;

	if (which >= num_running)
		return 0;
	if (--num_running != which)
		_running[which] = _running[num_running];
	_running[num_running] = 0;

	return proc;
}

//---------------------------------------------------------------------------------------------

NewProcess *
NewProcess::waitForAny()
{
	return _waitForAny(INFINITE);
}

//---------------------------------------------------------------------------------------------

NewProcess *
NewProcess::waitForAny(const Seconds &timeout)
{
	return _waitForAny((DWORD) Milliseconds(timeout).value());
}

///////////////////////////////////////////////////////////////////////////////////////////////

CurrentProcess::CurrentProcess()
{
	_pid = GetCurrentProcessId();
	_handle = GetCurrentProcess();
}

///////////////////////////////////////////////////////////////////////////////////////////////

#if 0
BOOL WINAPI 
Job::ConsoleCtrlHandler(DWORD dwCtrlType)
{
	switch (dwCtrlType)
	{
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
		SetConsoleCtrlHandler(ConsoleCtrlHandler, FALSE);
		TerminateJobObject(_job, 100);
		return TRUE;

	default:
		break;
	}
	
	return FALSE;
}
#endif

//---------------------------------------------------------------------------------------------

Job::Job()
{
	_job = CreateJobObject(NULL, NULL);
//	SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
}

//---------------------------------------------------------------------------------------------

Job &Job::operator+=(const Process &proc)
{
	if (!AssignProcessToJobObject(_job, proc.handle()))
		throw std::runtime_error("cannot assign process to job");
	return *this;
}

//---------------------------------------------------------------------------------------------

void Job::terminate()
{
	TerminateJobObject(_job, 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace rvfc
