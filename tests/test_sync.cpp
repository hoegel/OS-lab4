#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <windows.h>
#include <cstring>

struct Message {
    DWORD senderId;
    char text[20];
};

struct FileHeader {
    int head;
    int tail;
    int maxRecords;
};

TEST_CASE("Sender updates tail after writing message") {
    FileHeader header{0, 0, 3};
    header.tail = (header.tail + 1) % header.maxRecords;
    CHECK(header.tail == 1);
}

TEST_CASE("Receiver updates head after reading message") {
    FileHeader header{0, 2, 3};
    header.head = (header.head + 1) % header.maxRecords;
    CHECK(header.head == 1);
}

TEST_CASE("Ring buffer wraps tail correctly") {
    FileHeader header{0, 2, 3};
    header.tail = (header.tail + 1) % header.maxRecords;
    CHECK(header.tail == 0);
}

TEST_CASE("Ring buffer wraps head correctly") {
    FileHeader header{2, 0, 3};
    header.head = (header.head + 1) % header.maxRecords;
    CHECK(header.head == 0);
}

TEST_CASE("Sender waits for empty slot before writing") {
    HANDLE hEmptySem = CreateSemaphoreA(NULL, 1, 1, NULL);
    REQUIRE(hEmptySem != NULL);
    // первое ожидание проходит
    DWORD first = WaitForSingleObject(hEmptySem, 100);
    CHECK(first == WAIT_OBJECT_0);
    // второе ожидание блокируется — мест больше нет
    DWORD second = WaitForSingleObject(hEmptySem, 100);
    CHECK(second == WAIT_TIMEOUT);
    CloseHandle(hEmptySem);
}

TEST_CASE("Receiver waits for message before reading") {
    HANDLE hFullSem = CreateSemaphoreA(NULL, 0, 1, NULL);
    REQUIRE(hFullSem != NULL);
    // сообщений нет
    DWORD beforeSend = WaitForSingleObject(hFullSem, 100);
    CHECK(beforeSend == WAIT_TIMEOUT);
    // sender добавил сообщение
    ReleaseSemaphore(hFullSem, 1, NULL);
    DWORD afterSend = WaitForSingleObject(hFullSem, 100);
    CHECK(afterSend == WAIT_OBJECT_0);
    CloseHandle(hFullSem);
}

TEST_CASE("Message text copied correctly") {
    Message msg{};
    strcpy_s(msg.text, "hello");
    CHECK(std::strcmp(msg.text, "hello") == 0);
}

TEST_CASE("Message stores sender id") {
    Message msg{};
    msg.senderId = 777;
    CHECK(msg.senderId == 777);
}