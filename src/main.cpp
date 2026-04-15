#include <windows.h>
#include <iostream>
#include <cstdint>
#include <string>
#include <vector>

struct Message {
    DWORD senderId;
    char text[20];
};

struct FileHeader {
    int head;
    int tail;
    int maxRecords;
};

DWORD WINAPI SenderRoutine(const char * filename,
    const char * eventName) {
    std::cout << "Sender started..." << std::endl;

    HANDLE hFile = CreateFileA(
        filename,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Sender: Could not open file. Error: " << GetLastError() << std::endl;
        return 1;
    }

    HANDLE hReady = OpenEventA(EVENT_MODIFY_STATE, FALSE, eventName);
    if (hReady) {
        SetEvent(hReady);
        CloseHandle(hReady);
    }

    //открываем существующие объекты сихнронизации
    HANDLE hMutex = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, "Global\\FileMutex"); //мьютекс чтобы только один процесс мог открыть файл на запись
    HANDLE hEmptySem = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, "Global\\EmptySem"); //семафор свободным мест
    HANDLE hFullSem = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, "Global\\FullSem"); //семафор заполненных сообщений

    if (!hMutex || !hEmptySem || !hFullSem) {
        std::cerr << "sender: could not open sync objects. Error: " << GetLastError() << std::endl;
        return 1;
    }

    std::cout << "sender started. target file: " << filename << std::endl;

    while (true) {
        std::cout << "Enter command (1 - send, 0 - exit): ";
        int32_t cmd;
        std::cin >> cmd;
        if (cmd == 0) break;

        Message msg;
        msg.senderId = GetCurrentProcessId();
        std::cout << "Text (max 20): ";
        std::string text;
        std::cin >> text;
        strncpy_s(msg.text, text.c_str(), _TRUNCATE);

        //ожидание свободного места (если файл полон, процесс уснет)
        WaitForSingleObject(hEmptySem, INFINITE);
        //захват файла
        WaitForSingleObject(hMutex, INFINITE);

        //читаем Header, пишем сообщение, обновляем tail (кольцевая логика)
        FileHeader currentHeader;
        DWORD read, written;
        SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
        ReadFile(hFile, & currentHeader, sizeof(FileHeader), & read, NULL);

        //считаем позицию для записи: заголовок + tail * размер сообщения
        LONG offset = sizeof(FileHeader) + (currentHeader.tail * sizeof(Message));
        SetFilePointer(hFile, offset, NULL, FILE_BEGIN);
        WriteFile(hFile, & msg, sizeof(Message), & written, NULL);

        //обновляем хвост
        currentHeader.tail = (currentHeader.tail + 1) % currentHeader.maxRecords;
        SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
        WriteFile(hFile, & currentHeader, sizeof(FileHeader), & written, NULL);

        //освобождаем файл и сигнализируем, что добавилось сообщение
        ReleaseMutex(hMutex);
        ReleaseSemaphore(hFullSem, 1, NULL);
    }

    return 0;
}

int main(int argc, char * argv[]) {
    if (argc >= 4 && std::string(argv[1]) == "sender") {
        SenderRoutine(argv[2], argv[3]);
    } else {

        //ввод имени файла
        std::string filename;
        std::cout << "please, enter filename... ";
        if (!(std::cin >> filename) || filename.size() > 100) {
            std::cerr << "filename input error\n";
            return 1;
        }

        //ввод количества записей
        int32_t recordNum;
        std::cout << "please, enter the number or records... ";
        if (!(std::cin >> recordNum) || recordNum > 100 || recordNum <= 0) {
            std::cerr << "record number input error: must be between 1 and 100\n";
            return 1;
        }

        //создание файла
        HANDLE hFile = CreateFileA(
            filename.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (hFile == INVALID_HANDLE_VALUE) {
            std::cerr << "сould not create file. Error code: " << GetLastError() << "\n";
            return 1;
        }

        FileHeader header = {
            0,
            0,
            recordNum
        };
        DWORD bytesWritten;

        BOOL result = WriteFile(
            hFile, &
            header,
            sizeof(header), &
            bytesWritten,
            NULL
        );

        if (!result) {
            std::cerr << "WriteFile Header error: " << GetLastError() << "\n";
            CloseHandle(hFile);
            return 1;
        }

        // 4. Опционально: Подготовка места под данные (чтобы файл сразу имел нужный размер)
        // Это гарантирует, что на диске хватит места
        SetFilePointer(hFile, recordNum * 20, NULL, FILE_CURRENT);
        SetEndOfFile(hFile);

        std::cout << "binary file created and initialized successfully.\n";
        CloseHandle(hFile);

        //ввод senderов
        int32_t SenderNum;
        std::cout << "please, enter the number of senders... ";
        if (!(std::cin >> SenderNum) || SenderNum > 100 || SenderNum <= 0) {
            std::cerr << "sender number input error: must be between 1 and 100\n";
            return 1;
        }

        //создание мьютекса для защиты доступа к файлу
        HANDLE hMutex = CreateMutexA(NULL, FALSE, "Global\\FileMutex");

        //cемафор свободных мест
        HANDLE hEmptySem = CreateSemaphoreA(NULL, recordNum, recordNum, "Global\\EmptySem");

        //cемафор заполненных сообщений
        HANDLE hFullSem = CreateSemaphoreA(NULL, 0, recordNum, "Global\\FullSem");

        if (!hMutex || !hEmptySem || !hFullSem) {
            std::cerr << "failed to create sync objects: " << GetLastError() << "\n";
            return 1;
        }

        std::vector < HANDLE > hReadyEvents(SenderNum);

        char szPath[MAX_PATH];
        GetModuleFileNameA(NULL, szPath, MAX_PATH);

        for (int i = 0; i < SenderNum; ++i) {
            std::string eventName = "ReadyEvent_" + std::to_string(i);
            hReadyEvents[i] = CreateEventA(NULL, FALSE, FALSE, eventName.c_str());

            //path_exe sender имя_файла имя_события
            std::string cmdLine = "\"" + std::string(szPath) + "\" sender " + filename + " " + eventName;

            STARTUPINFOA si;
            PROCESS_INFORMATION pi;
            ZeroMemory( & si, sizeof(si));
            si.cb = sizeof(si);
            ZeroMemory( & pi, sizeof(pi));

            if (!CreateProcessA(
                    NULL,
                    (LPSTR) cmdLine.c_str(),
                    NULL,
                    NULL,
                    FALSE,
                    CREATE_NEW_CONSOLE,
                    NULL,
                    NULL, &
                    si, &
                    pi
                )) {
                std::cerr << "createProcess failed: " << GetLastError() << "\n";
            } else {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }
        }

        std::cout << "receiver is waiting for all senders to be ready...\n";

        WaitForMultipleObjects(SenderNum, hReadyEvents.data(), TRUE, INFINITE);
        std::cout << "all senders are ready! you can now read messages\n";

        for (HANDLE h: hReadyEvents) CloseHandle(h);

        HANDLE hFileToRead = CreateFileA(filename.c_str(), GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
            OPEN_EXISTING, 0, NULL);

        bool running = true;
        while (running) {
            std::cout << "\nreceiver Menu:\n1. read message\n0. exit\nchoice... ";
            int cmd;
            if (!(std::cin >> cmd)) break;

            if (cmd == 1) {
                std::cout << "waiting for message..." << std::endl;

                //ожидание появления хотя бы одного сообщения
                WaitForSingleObject(hFullSem, INFINITE);

                //захват мьютекса для доступа к файлу
                WaitForSingleObject(hMutex, INFINITE);

                FileHeader currentHeader;
                DWORD bytesProcessed;

                //читаем заголовок чтобы получить head (откуда читать)
                SetFilePointer(hFileToRead, 0, NULL, FILE_BEGIN);
                ReadFile(hFileToRead, & currentHeader, sizeof(FileHeader), & bytesProcessed, NULL);

                //смещение для чтения: заголовок + head * message
                LONG offset = sizeof(FileHeader) + (currentHeader.head * sizeof(Message));
                SetFilePointer(hFileToRead, offset, NULL, FILE_BEGIN);

                Message msg;
                if (ReadFile(hFileToRead, & msg, sizeof(Message), & bytesProcessed, NULL)) {
                    std::cout << "\n[NEW MESSAGE]" << std::endl;
                    std::cout << "from sender PID: " << msg.senderId << std::endl;
                    std::cout << "text: " << msg.text << std::endl;

                    //обновление head для следующего чтения
                    currentHeader.head = (currentHeader.head + 1) % currentHeader.maxRecords;

                    //запись заголовка обратно
                    SetFilePointer(hFileToRead, 0, NULL, FILE_BEGIN);
                    WriteFile(hFileToRead, & currentHeader, sizeof(FileHeader), & bytesProcessed, NULL);
                }
                ReleaseMutex(hMutex);

                //сигнал, что место освободилось
                ReleaseSemaphore(hEmptySem, 1, NULL);

            } else if (cmd == 0) {
                running = false;
            }
        }
        CloseHandle(hFileToRead);
        CloseHandle(hMutex);
        CloseHandle(hEmptySem);
        CloseHandle(hFullSem);
    }

    return 0;
}