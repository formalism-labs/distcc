
#ifndef _rvfc_process_
#define _rvfc_process_

#include <string>

// #include "rvfc/sync/thread.h"
#include "rvfc/system/win32/handle.h"
#include "rvfc/time/defs.h"
#include "rvfc/win.h"

namespace rvfc
{

///////////////////////////////////////////////////////////////////////////////////////////////

class ProcStdStreams
{
	ProcStdStreams() {}

public:
	ProcStdStreams(bool dup) :
		in(StdHandle::in),
		out(StdHandle::out),
		err(StdHandle::err)
	{
		if (dup)
		{
			in = in.dupx();
			out = out.dupx();
			err = err.dupx();
		}
	}

	ProcStdStreams(Handle in_, Handle out_, Handle err_) :
		in(in_), out(out_), err(err_) {} 

	HandleX in;
	HandleX out;
	HandleX err;

	ProcStdStreams dup()
	{
		ProcStdStreams handles;
		handles.in = in.dupx();
		handles.out = out.dupx();
		handles.err = err.dupx();
		return handles;
	}

	void close()
	{
		in.close();
		out.close();
		err.close();
	}
};

//---------------------------------------------------------------------------------------------

class Process
{
protected:
	DWORD _pid;
	HandleX _handle;

public:
	DWORD id() const { return _pid; }
	HandleX handle() const { return _handle; }

	std::string programPath() const;
};

//---------------------------------------------------------------------------------------------

#define MAX_STATIC_RUNNING	100

class NewProcess : public Process
{                                       
	static NewProcess *static_running[MAX_STATIC_RUNNING];
	static NewProcess **_running;
	static int num_running, max_num_running;

	HandleX thread_handle;

	void register_();

protected:
	ProcStdStreams _stdio;

public:
//	NewProcess(const char *command, const char *cwd = 0, const Environment &env = UnspecifiedEnvironment());
//	NewProcess(const char *cmdFile, const Arguments &args, const char *cwd = 0, const Environment &env = UnspecifiedEnvironment());

	NewProcess(const std::string &command, const char *cwd = 0);

//	NewProcess(const ProcHandles &handles);
	virtual ~NewProcess();

	void run();
	int wait(const Seconds &timeout);
	int runAndWait();

	bool kill(int retcode);

protected:
	static NewProcess *_waitForAny(DWORD timeout);

public:
	static NewProcess *waitForAny();
	static NewProcess *waitForAny(const Seconds &timeout);
};

//---------------------------------------------------------------------------------------------

class CurrentProcess : public Process
{
public:
	CurrentProcess();
};

//---------------------------------------------------------------------------------------------

class Job
{
	HandleX _job;

//	static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType);

public:
	Job();

	Handle handle() const { return _job; }

	Job &operator+=(const Process &proc);

	void terminate();
};

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace rvfc

#endif // _rvfc_process_
