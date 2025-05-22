
#include "common/config.h"

#include "common/distcc.h"
#include "common/trace.h"

#include "server/server.h"
#include "server/daemon.h"

extern bool g_dbg;

namespace distcc
{

///////////////////////////////////////////////////////////////////////////////////////////////

//static SERVICE_STATUS ServiceStatus; 
static SERVICE_STATUS_HANDLE service_status_handle;
StandaloneServer *g_server = 0;

//---------------------------------------------------------------------------------------------

BOOL UpdateServiceStatus(DWORD currentState)
{
	DWORD win32ExitCode = NO_ERROR;
	DWORD serviceSpecificExitCode = 0;
	DWORD checkPoint = 0;
	DWORD waitHint = 0;

	SERVICE_STATUS status;
	status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	status.dwCurrentState = currentState;
	status.dwControlsAccepted = currentState == SERVICE_START_PENDING ? 0 : SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	status.dwWin32ExitCode = serviceSpecificExitCode == 0 ? win32ExitCode : ERROR_SERVICE_SPECIFIC_ERROR;
	status.dwServiceSpecificExitCode = 0;
	status.dwCheckPoint = checkPoint;
	status.dwWaitHint = waitHint;

	return SetServiceStatus(service_status_handle, &status);
}

//---------------------------------------------------------------------------------------------

static unsigned int __stdcall ServiceShutdownThread(void *x)
{
	if (g_server)
		g_server->terminate();
	UpdateServiceStatus(SERVICE_STOPPED);
	return 0;
}

//---------------------------------------------------------------------------------------------

static DWORD WINAPI ServiceControlHandler(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext) 
{ 
	switch(dwControl) 
	{ 
	case SERVICE_CONTROL_INTERROGATE:
		return NO_ERROR;

	case SERVICE_CONTROL_STOP:
	case SERVICE_CONTROL_SHUTDOWN: 
		{
		unsigned int tid;
		UpdateServiceStatus(SERVICE_STOP_PENDING);
		_beginthreadex(0, 0, (unsigned (__stdcall *)(void *)) ServiceShutdownThread, (void *) 0, 0, &tid);
		return NO_ERROR;
		}

	default:
		break;
	}

	return NO_ERROR;
}

//---------------------------------------------------------------------------------------------

static void ServiceMain(int argc, char *argv[]) 
{ 
	try
	{
		service_status_handle = RegisterServiceCtrlHandlerEx("distccd", (LPHANDLER_FUNCTION_EX) ServiceControlHandler, 0);
		if (service_status_handle == (SERVICE_STATUS_HANDLE) 0) 
			return; 
		if (g_dbg)
		{
			while (g_dbg)
				sleep(1);
		}

		UpdateServiceStatus(SERVICE_START_PENDING);

		Configuration cfg(argc, argv);
		if (cfg.exit())
		{
			UpdateServiceStatus(SERVICE_STOPPED);
			return;
		}

		g_server = new StandaloneServer(false, false);

		UpdateServiceStatus(SERVICE_RUNNING);

		g_server->run();
		g_server->terminate();

		delete g_server;
		g_server = 0;

		UpdateServiceStatus(SERVICE_STOPPED);
	}
	catch (std::exception &x)
	{
		rs_trace("distccd: exception: %s", x.what());
		UpdateServiceStatus(SERVICE_STOPPED);
	}
}

//---------------------------------------------------------------------------------------------

SERVICE_TABLE_ENTRY Win32Service::service_table[]= 
{ 
	{ "distccd", (LPSERVICE_MAIN_FUNCTION) ServiceMain },
	{ NULL, NULL }
};

//---------------------------------------------------------------------------------------------

Win32Service::Win32Service()
{
}

//---------------------------------------------------------------------------------------------

void Win32Service::run()
{
	StartServiceCtrlDispatcher(service_table);  
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
