#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include<fstream>
#include <ctime>
#include <experimental/filesystem>
#pragma warning(disable:4996)

#pragma comment(lib, "ws2_32.lib")
void send_file(SOCKET serverSocket, const std::string& filePath) {
    clock_t t1 = clock();
    send(serverSocket, reinterpret_cast<const char*>(&t1), sizeof(t1), 0);
    /*std::cout << t1 << std::endl;*/
    std::fstream file;
    file.open(filePath, std::ios_base::in | std::ios_base::binary);

    if (file.is_open()) {

        int file_size = std::experimental::filesystem::file_size(filePath) + 1;

        char* bytes = new char[file_size];

        file.read(bytes, file_size);
        std::cout << "Путь к файлу: " << filePath << std::endl;

        std::cout << "size: " << file_size << std::endl;
        std::cout << "name: " << filePath << std::endl;
        std::cout << "data: " << bytes << std::endl;

        send(serverSocket, std::to_string(file_size).c_str(), 16, 0);
        send(serverSocket, filePath.c_str(), 32, 0);
        clock_t t4 = clock();
        send(serverSocket, reinterpret_cast<const char*>(&t4), sizeof(t4), 0);
        /*std::cout << t4 << std::endl;*/
        send(serverSocket, bytes, file_size, 0);

        delete[] bytes;

    }
    else
        std::cout << "Ошибка открытия файла: " << filePath << std::endl;;

    file.close();
}

void receive_file(SOCKET clientSocket) {
    char file_size_str[16];
    char file_name[32];

    recv(clientSocket, file_size_str, 16, 0);
    int file_size = atoi(file_size_str);
    char* bytes = new char[file_size];

    recv(clientSocket, file_name, 32, 0);

    std::fstream file;
    file.open(file_name, std::ios_base::out | std::ios_base::binary);

    std::cout << "size: " << file_size << std::endl;
    std::cout << "name: " << file_name << std::endl;

    if (file.is_open()) {

        recv(clientSocket, bytes, file_size, 0);
        std::cout << "data: " << bytes << std::endl;

        file.write(bytes, file_size);
        std::cout << "ok save\n";
    }
    else
        std::cout << "Error file open\n";
    delete[] bytes;
    file.close();
}

int main()
{
    SetConsoleCP(1251); // Установка кодировки ввода
    SetConsoleOutputCP(1251); // Установка кодировки вывода

    setlocale(LC_ALL, "Russian");
    // Инициализация Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "Ошибка инициализации Winsock." << std::endl;
        return -1;
    }
    // Создание сокета
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET)
    {
        std::cerr << "Ошибка создания сокета. Код ошибки: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return -1;
    }

    // Ввод IP-адреса и порта сервера
    std::string serverIP;
    std::cout << "Введите IP-адрес сервера: ";
    std::getline(std::cin, serverIP);

    unsigned short serverPort;
    std::cout << "Введите порт сервера: ";
    std::cin >> serverPort;

    // Очищаем буфер после ввода порта
    std::cin.ignore();

    // Установка параметров адреса сервера
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(serverPort);
    /*serverAddress.sin_port = htons(54000);*/



    if (inet_pton(AF_INET, serverIP.c_str(), &(serverAddress.sin_addr)) != 1)
    {
        std::cerr << "Ошибка преобразования IP-адреса. Код ошибки: " << WSAGetLastError() << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return -1;
    }
    /*if (inet_pton(AF_INET, "127.0.0.1", &(serverAddress.sin_addr)) != 1)
    {
        std::cerr << "Ошибка преобразования IP-адреса. Код ошибки: " << WSAGetLastError() << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return -1;
    }*/
    // Установка соединения с сервером
    if (connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR)
    {
        std::cerr << "Ошибка подключения к серверу. Код ошибки: " << WSAGetLastError() << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return -1;
    }
    // Ввод идентификатора клиента
    std::string clientID;
    std::cout << "Введите идентификатор клиента: ";
    std::getline(std::cin, clientID);
    // Отправка идентификатора клиента на сервер
    if (send(clientSocket, clientID.c_str(), clientID.size() + 1, 0) == SOCKET_ERROR)
    {
        std::cerr << "Ошибка отправки идентификатора клиента. Код ошибки: " << WSAGetLastError() << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return -1;
    }
    // Создание потока для приема сообщений от сервера
    std::thread receiveThread([&]() {
        char buffer[4096];

        while (true)
        {
            memset(buffer, 0, sizeof(buffer));
            int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (bytesReceived > 0)
            {
                std::string message = buffer;
                if (message.substr(0, 4) == "FILE")
                {
                    receive_file(clientSocket);
                }
                else
                {
                    std::cout << "Сообщение от сервера: " << message << std::endl;
                }
            }
            else if (bytesReceived == 0)
            {
                std::cout << "Сервер отключился." << std::endl;
                break;
            }
            else
            {
                std::cerr << "Ошибка приема сообщения от сервера. Код ошибки: " << WSAGetLastError() << std::endl;
                break;
            }
        }
        });
    // Отправка сообщений на сервер
    std::string message;
    while (true)
    {
        std::cout << "Введите сообщение для отправки на сервер: ";
        std::getline(std::cin, message);

        if (message == "exit")
        {
            break;
        }

        if (send(clientSocket, message.c_str(), message.size() + 1, 0) == SOCKET_ERROR)
        {
            std::cerr << "Ошибка отправки сообщения на сервер. Код ошибки: " << WSAGetLastError() << std::endl;
            break;
        }
        if (message.substr(0, 4) == "FILE")
        {
            std::string filePath = message.substr(5);
            std::cout << "Путь к файлу: " << filePath << std::endl;
            send_file(clientSocket, filePath);

        }

    }

    // Закрытие сокета клиента
    closesocket(clientSocket);

    // Ожидание завершения потока приема сообщений
    receiveThread.join();

    // Освобождение ресурсов Winsock
    WSACleanup();

    return 0;
}
