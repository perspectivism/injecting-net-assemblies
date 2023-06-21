#include <string>
#include "windows.h"
#include "Shlwapi.h"
#include "TlHelp32.h"

#pragma comment(lib, "shlwapi")

using namespace std;

#pragma region constants

static const wchar_t* BOOTSTRAP_DLL = L"Bootstrap.dll";

static const wstring DELIM = L"\t";

static const wstring NOTICE =
wstring(L"\n\nInject v1.0 - Inject managed code into unmanaged and managed applications.\n") +
wstring(L"Copyright (C) 2013 Pero Matic\n") +
wstring(L"Plan A Software - www.planasoftware.com\n\n");

static const wstring SUMMARY = 
    wstring(L"Inject.exe takes a managed assembly, loads it in a remote process, and executes\nmethods inside the assembly within the context of the remote process.\n\n");

static const wstring USAGE =
    wstring(L"\n\tInject.exe -m [methodName] -i [@assemblyFile] -l [typeName]\n\t\t   -a [arguments] -n [processName|processID]\n\n");

static const wchar_t* PARAMETERS[] = { L"-m", L"-i", L"-l", L"-a", L"-n" };    

static const wchar_t* DESCRIPTIONS[] = 
{         
    L"The name of the managed method to execute. ex~ EntryPoint",
    L"The fully qualified path of the managed assembly to inject\n\t\tinside of the remote process. ex~ C:\\InjectExample.exe",
    L"The fully qualified type name of the managed assembly.\n\t\tex~ InjectExample.Program",
    L"An optional argument to pass in to the managed function.",
    L"The process ID or the name of the process to inject.\n\t\tex~ 1500 or notepad.exe"
};

static const wstring NOTES = 
    wstring(L"\n\t+ Arguments that require spaces, such as file paths, should be enclosed\n\t  in quotes.") +
    wstring(L"\n\t+ Options and arguments are case sensitive.\n\n");

static const wchar_t* DEDICATIONS = L"This app is dedicated to my brother Milan.\n\n";

#pragma endregion

// prototypes
DWORD_PTR Inject(const HANDLE hProcess, const LPVOID function, const wstring& argument);
DWORD_PTR GetFunctionOffset(const wstring& library, const char* functionName);
int GetProcessIdByName(wchar_t* processName);
DWORD_PTR GetRemoteModuleHandle(const int processId, const wchar_t* moduleName);
void EnablePrivilege(const wchar_t* lpPrivilegeName, const bool bEnable);
inline size_t GetStringAllocSize(const wstring& str);
wstring GetBootstrapPath();
bool ParseArgs(int argc, wchar_t* argv[]);
void PrintUsage();

// global variables
int g_processId       = 0;       // id of process to inject
wchar_t* g_moduleName = NULL;    // full path of managed assembly (exe or dll) to inject
wchar_t* g_typeName   = NULL;    // the assembly and type of the managed assembly 
wchar_t* g_methodName = NULL;    // the managed method to execute
wchar_t* g_Argument   = NULL;    // the optional argument to pass into the method to execute

int wmain(int argc, wchar_t* argv[])
{    
    // parse args
    if (!ParseArgs(argc, argv))
    {
        PrintUsage();
        return -1;
    }

    // enable debug privileges
    EnablePrivilege(SE_DEBUG_NAME, TRUE);

    // get handle to remote process
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, g_processId);

    // inject bootstrap.dll into the remote process
    FARPROC fnLoadLibrary = GetProcAddress(GetModuleHandle(L"Kernel32"), "LoadLibraryW");
    Inject(hProcess, fnLoadLibrary, GetBootstrapPath()); // ret val on x86 is the base addr; on x64 addr gets truncated    

    // add the function offset to the base of the module in the remote process
    DWORD_PTR hBootstrap = GetRemoteModuleHandle(g_processId, BOOTSTRAP_DLL);
    DWORD_PTR offset = GetFunctionOffset(GetBootstrapPath(), "ImplantDotNetAssembly");
    DWORD_PTR fnImplant = hBootstrap + offset;

    // build argument; use DELIM as tokenizer
    wstring argument = g_moduleName + DELIM + g_typeName + DELIM + g_methodName + DELIM + g_Argument;

    // inject the managed assembly into the remote process
    Inject(hProcess, (LPVOID)fnImplant, argument);

    // unload bootstrap.dll out of the remote process
    FARPROC fnFreeLibrary = GetProcAddress(GetModuleHandle(L"Kernel32"), "FreeLibrary");
    CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)fnFreeLibrary, (LPVOID)hBootstrap, NULL, 0);

    // close process handle
    CloseHandle(hProcess);

    return 0;
}

DWORD_PTR Inject(const HANDLE hProcess, const LPVOID function, const wstring& argument)
{
    // allocate some memory in remote process
    LPVOID baseAddress = VirtualAllocEx(hProcess, NULL, GetStringAllocSize(argument), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    // write argument into remote process    
    BOOL isSucceeded = WriteProcessMemory(hProcess, baseAddress, argument.c_str(), GetStringAllocSize(argument), NULL);

    // make the remote process invoke the function
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)function, baseAddress, NULL, 0);

    // wait for thread to exit
    WaitForSingleObject(hThread, INFINITE);

    // free memory in remote process
    VirtualFreeEx(hProcess, baseAddress, 0, MEM_RELEASE);

    // get the thread exit code
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);

    // close thread handle
    CloseHandle(hThread);

    // return the exit code
    return exitCode;
}

DWORD_PTR GetFunctionOffset(const wstring& library, const char* functionName)
{
    // load library into this process
    HMODULE hLoaded = LoadLibrary(library.c_str());

    // get address of function to invoke
    void* lpInject = GetProcAddress(hLoaded, functionName);

    // compute the distance between the base address and the function to invoke
    DWORD_PTR offset = (DWORD_PTR)lpInject - (DWORD_PTR)hLoaded;

    // unload library from this process
    FreeLibrary(hLoaded);

    // return the offset to the function
    return offset;
}

int GetProcessIdByName(wchar_t* processName)
{
    PROCESSENTRY32 pe32;
    HANDLE hSnapshot = INVALID_HANDLE_VALUE;

    // get snapshot of all processes
    pe32.dwSize = sizeof(PROCESSENTRY32);
    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    // can we start looking?
    if (!Process32First(hSnapshot, &pe32))
    {
        CloseHandle(hSnapshot);
        return 0;
    }
    
    // enumerate all processes till we find the one we are looking for or until every one of them is checked
    while (wcscmp(pe32.szExeFile, processName) != 0 && Process32Next(hSnapshot, &pe32));

    // close the handle
    CloseHandle(hSnapshot);        

    // check if process id was found and return it
    if (wcscmp(pe32.szExeFile, processName) == 0)
        return pe32.th32ProcessID;

    return 0;
}

DWORD_PTR GetRemoteModuleHandle(const int processId, const wchar_t* moduleName)
{
    MODULEENTRY32 me32; 
    HANDLE hSnapshot = INVALID_HANDLE_VALUE;

    // get snapshot of all modules in the remote process 
    me32.dwSize = sizeof(MODULEENTRY32); 
    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId);

    // can we start looking?
    if (!Module32First(hSnapshot, &me32)) 
    {
        CloseHandle(hSnapshot);
        return 0;
    }

    // enumerate all modules till we find the one we are looking for or until every one of them is checked
    while (wcscmp(me32.szModule, moduleName) != 0 && Module32Next(hSnapshot, &me32));

    // close the handle
    CloseHandle(hSnapshot);

    // check if module handle was found and return it
    if (wcscmp(me32.szModule, moduleName) == 0)
        return (DWORD_PTR)me32.modBaseAddr;

    return 0;
}


void EnablePrivilege(const wchar_t* lpPrivilegeName, const bool enable)
{  
    // get the access token of the current process
    HANDLE token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
        return;

    // build the privilege
    TOKEN_PRIVILEGES privileges;
    ZeroMemory(&privileges, sizeof(privileges)); 
    privileges.PrivilegeCount = 1;
    privileges.Privileges[0].Attributes = (enable) ? SE_PRIVILEGE_ENABLED : 0;
    if (!LookupPrivilegeValue(NULL, lpPrivilegeName, &privileges.Privileges[0].Luid))
    {
        CloseHandle(token);
        return;
    }

    // add the privilege
    BOOL result = AdjustTokenPrivileges(token, FALSE, &privileges, sizeof(privileges), NULL, NULL);

    // close the handle
    CloseHandle(token);
}

inline size_t GetStringAllocSize(const wstring& str)
{
    // size (in bytes) of string
    return (wcsnlen(str.c_str(), 65536) * sizeof(wchar_t)) + sizeof(wchar_t); // +sizeof(wchar_t) for null
}

wstring GetBootstrapPath()
{
    // get full path to exe
    wchar_t buffer[MAX_PATH];
    GetModuleFileName(NULL, buffer, MAX_PATH);

    // remove filename
    PathRemoveFileSpec(buffer);

    // append bootstrap to buffer if it will fit
    size_t size = wcslen(buffer) + wcslen(BOOTSTRAP_DLL);
    if (size <= MAX_PATH)
        PathAppend(buffer, BOOTSTRAP_DLL);
    else
        throw runtime_error("Module name cannot exceed MAX_PATH");

    // return path
    return buffer;
}


bool ParseArgs(int argc, wchar_t* argv[])
{
    // bare minimum number of args expected 
    static const int MIN_ARGC = ((sizeof PARAMETERS) / sizeof(wchar_t*) * 2) // size of array
                                - 2  // ignore the optional -a arg if not present, args come in pairs
                                + 1; // take in to account argv[0]
    if (argc < MIN_ARGC)
        return false;

    // parse cmd line
    for (int i = 1; (i < argc) && ((i + 1) < argc); i += 2)
    {
        if (wcscmp(argv[i], L"-m") == 0)
            g_methodName = argv[i + 1];
        else if (wcscmp(argv[i], L"-i") == 0)
            g_moduleName = argv[i + 1];
        else if (wcscmp(argv[i], L"-l") == 0)
            g_typeName = argv[i + 1];
        else if (wcscmp(argv[i], L"-a") == 0)
            g_Argument = argv[i + 1];
        else if (wcscmp(argv[i], L"-n") == 0)
            g_processId = _wtoi(argv[i + 1]) != 0 ? _wtoi(argv[i + 1]) : GetProcessIdByName(argv[i + 1]);
    }

    // basic validation
    if (g_processId == NULL || g_moduleName == NULL || g_typeName == NULL || g_methodName == NULL)
        return false;
    else if (wcslen(g_moduleName) > MAX_PATH)
        throw runtime_error("Module name cannot exceed MAX_PATH");

    return true;
}

void PrintUsage()
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    // make sure all text is visible
    ShowWindow(GetConsoleWindow() , SW_MAXIMIZE);

    if (IsDebuggerPresent())
    {
        // add features to cmd line we know and love
        DWORD mode;
        HANDLE hInConsole = GetStdHandle(STD_INPUT_HANDLE);
        GetConsoleMode(hInConsole, &mode);  
        SetConsoleMode(hInConsole, ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_ECHO_INPUT);
    }
    
    SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY); // light green
    wprintf_s(NOTICE.c_str());

    SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY); // cyan
    wprintf_s(DEDICATIONS);

    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // normal
    wprintf_s(SUMMARY.c_str());

    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY); // light purple
    wprintf_s(L"Usage:");

    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // normal
    wprintf_s(USAGE.c_str());    

    for (int i = 0; i < (sizeof PARAMETERS) / sizeof(wchar_t*); ++i)
    {
        wstring str = wstring(L"\t") + wstring(PARAMETERS[i]) + wstring(L"\t") + wstring(DESCRIPTIONS[i]) + wstring(L"\n");
        wprintf_s(str.c_str());
    }

    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY); // light purple
    wprintf_s(L"\nNotes:");

    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // normal
    wprintf_s(NOTES.c_str());
}
