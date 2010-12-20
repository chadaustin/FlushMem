#define WINVER 0x0500
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Guaranteed 64kib on x86? 
// http://blogs.msdn.com/oldnewthing/archive/2003/10/08/55239.aspx
const size_t ALLOCATION_SIZE = 64 * 1024;
const size_t FILL_SIZE = ALLOCATION_SIZE - 4;

unsigned char fill[FILL_SIZE];

void my_print(const char* line) {
    DWORD dummy;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), line, lstrlenA(line), &dummy, 0);
}

void my_print(size_t s) {
    char buf[80];
    wsprintf(buf, "%u", s);
    my_print(buf);
}

struct Node {
    Node* next;
    unsigned char data[FILL_SIZE];
};

bool next(Node*& head) {
    Node* p = static_cast<Node*>(VirtualAlloc(0, 64 * 1024, MEM_COMMIT, PAGE_READWRITE));
    if (!p) {
        return false;
    }

    p->next = head;
    head = p;
    return true;
}

int main(int argc, const char* argv[]) {
    if (argc == 1) {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        my_print("allocation granularity: ");
        my_print(static_cast<size_t>(si.dwAllocationGranularity));
        my_print("\n");

        WCHAR exe[MAX_PATH + 3] = {0};
        GetModuleFileNameW(0, exe, MAX_PATH);
        lstrcatW(exe, L" -");

        MEMORYSTATUSEX ms;
        ms.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&ms);
        unsigned long long totalPageFile = ms.ullTotalPageFile;
        const unsigned long long approximateProcessConsumption = 1536 * 1024 * 1024;

        unsigned processCount = (totalPageFile + approximateProcessConsumption - 1) / approximateProcessConsumption;

        my_print("forking ");
        my_print(processCount);
        my_print(" subprocesses\n");

        HANDLE processes[processCount];
        for (unsigned i = 0; i < processCount; ++i) {
            STARTUPINFOW si;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            
            PROCESS_INFORMATION pi;
            if (!CreateProcessW(0, exe, 0, 0, FALSE, 0, 0, 0, &si, &pi)) {
                my_print("failed to create process\n");
                return 1;
            }
            processes[i] = pi.hProcess;
            CloseHandle(pi.hThread);
        }

        for (unsigned i = 0; i < processCount; ++i) {
            WaitForMultipleObjects(processCount, processes, TRUE, INFINITE);
        }

        my_print("done\n");
        return 0;
    }

    // Maybe zeroes are magic.
    for (size_t i = 0; i < FILL_SIZE; ++i) {
        fill[i] = i;
    }

    size_t total = 0;

    my_print("allocating... ");
    Node* head = 0;
    while (next(head)) {
        ++total;
    }

    my_print(total * 64 * 1024);
    my_print(" bytes\n");
    my_print("flushing...\n");

    // Should we make multiple passes?
    Node* p = head;
    while (p) {
        CopyMemory(p->data, fill, FILL_SIZE);
        p = p->next;
    }
}
