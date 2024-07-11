#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_THREADS 8

typedef struct {
    char path[MAX_PATH];
    char searchStr[256];
} SearchData;

HANDLE threadPool[MAX_THREADS];
int threadCount = 0;
CRITICAL_SECTION cs;
HANDLE semaphore;

DWORD WINAPI searchDirectoryThread(LPVOID lpParam) {
    SearchData *data = (SearchData *)lpParam;
    WIN32_FIND_DATA findData;
    HANDLE findHandle;
    char searchPath[MAX_PATH];

    snprintf(searchPath, sizeof(searchPath), "%s\\*", data->path);
    findHandle = FindFirstFile(searchPath, &findData);

    if (findHandle == INVALID_HANDLE_VALUE) {
        free(data);
        ReleaseSemaphore(semaphore, 1, NULL);
        return 1;
    }

    do {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (strcmp(findData.cFileName, ".") != 0 && strcmp(findData.cFileName, "..") != 0) {
                SearchData *newData = (SearchData *)malloc(sizeof(SearchData));
                snprintf(newData->path, sizeof(newData->path), "%s\\%s", data->path, findData.cFileName);
                snprintf(newData->searchStr, sizeof(newData->searchStr), "%s", data->searchStr);

                EnterCriticalSection(&cs);
                if (threadCount < MAX_THREADS) {
                    threadPool[threadCount++] = CreateThread(NULL, 0, searchDirectoryThread, newData, 0, NULL);
                } else {
                    searchDirectoryThread(newData);
                }
                LeaveCriticalSection(&cs);
            }
        } else {
            if (strstr(findData.cFileName, data->searchStr) != NULL) {
                printf("Match found in file: %s\\%s\n", data->path, findData.cFileName);
            }
        }
    } while (FindNextFile(findHandle, &findData) != 0);

    FindClose(findHandle);
    free(data);
    ReleaseSemaphore(semaphore, 1, NULL);
    return 0;
}

void searchDirectory(const char *path, const char *searchStr) {
    SearchData *data = (SearchData *)malloc(sizeof(SearchData));
    snprintf(data->path, sizeof(data->path), "%s", path);
    snprintf(data->searchStr, sizeof(data->searchStr), "%s", searchStr);

    EnterCriticalSection(&cs);
    if (threadCount < MAX_THREADS) {
        threadPool[threadCount++] = CreateThread(NULL, 0, searchDirectoryThread, data, 0, NULL);
    } else {
        searchDirectoryThread(data);
    }
    LeaveCriticalSection(&cs);
}

int main() {
    char searchStr[256];
    DWORD drives = GetLogicalDrives();

    InitializeCriticalSection(&cs);
    semaphore = CreateSemaphore(NULL, 0, MAX_THREADS, NULL);

    printf("Enter the string to search for in filenames: ");
    scanf("%255s", searchStr);

    for (char drive = 'A'; drive <= 'Z'; drive++) {
        if (drives & (1 << (drive - 'A'))) {
            char path[4] = { drive, ':', '\\', '\0' };
            searchDirectory(path, searchStr);
        }
    }

    // Wait for all threads to finish
    for (int i = 0; i < threadCount; i++) {
        WaitForSingleObject(threadPool[i], INFINITE);
        CloseHandle(threadPool[i]);
    }

    // Wait for all semaphores to be released
    for (int i = 0; i < threadCount; i++) {
        WaitForSingleObject(semaphore, INFINITE);
    }

    CloseHandle(semaphore);
    DeleteCriticalSection(&cs);

    return 0;
}