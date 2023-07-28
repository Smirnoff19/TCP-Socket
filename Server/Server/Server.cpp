#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <fstream>
#include <ctime>
#include <experimental/filesystem>
#pragma warning(disable:4996)
#pragma comment(lib, "ws2_32.lib")

using namespace std;

struct Client
{
    SOCKET socket;
    std::string id;
};

std::vector<Client> clients;

void send_file(SOCKET clientSocket, const std::string& filePath) {

    std::fstream file;
    file.open(filePath, std::ios_base::in | std::ios_base::binary);

    if (file.is_open()) {
        int file_size = experimental::filesystem::file_size(filePath) ;

        char* bytes = new char[file_size];

        file.read(bytes, file_size);
        std::cout << "Путь к файлу: " << filePath << std::endl;

        std::cout << "size: " << file_size << std::endl;
        std::cout << "name: " << filePath << std::endl;
        std::cout << "data: " << bytes << std::endl;

        send(clientSocket, std::to_string(file_size).c_str(), 16, 0);
        send(clientSocket, filePath.c_str(), 32, 0);
        send(clientSocket, bytes, file_size, 0);

    }
    else
        std::cout << "Ошибка открытия файла: " << filePath << std::endl;;

    file.close();
}

void receive_file(SOCKET serverSocket) {
    char file_name[32];

    clock_t t1;
    recv(serverSocket, reinterpret_cast<char*>(&t1), sizeof(t1), 0);

    int file_size;
    recv(serverSocket, reinterpret_cast<char*>(&file_size), sizeof(file_size), 0); // Получение размера файла в двоичном формате

    recv(serverSocket, file_name, 32, 0);

    std::ofstream file(file_name, std::ios::binary);

    std::cout << "size: " << file_size << std::endl;
    std::cout << "name: " << file_name << std::endl;

    if (file.is_open()) {
        clock_t t4;
        recv(serverSocket, reinterpret_cast<char*>(&t4), sizeof(t4), 0);

        char* bytes = new char[file_size];
        int bytesReceived = 0;
        int totalBytesReceived = 0;

        while (totalBytesReceived < file_size &&
            (bytesReceived = recv(serverSocket, bytes + totalBytesReceived, file_size - totalBytesReceived, 0)) > 0)
        {
            totalBytesReceived += bytesReceived;
        }

        file.write(bytes, file_size);
        std::cout << "ok file save\n";

        delete[] bytes;

        clock_t tend = clock();

        clock_t TotalTime_c = tend - t1;
        double TotallTime = static_cast<double>(TotalTime_c) / CLOCKS_PER_SEC;

        clock_t Znac_c = tend - t4;
        double Znach = static_cast<double>(Znac_c) / CLOCKS_PER_SEC;

        std::cout << "Общее время :" << TotallTime << std::endl;
        std::cout << "Время с момента передачи буфера до момента принятия в директорию :" << Znach << std::endl;
    }
    else {
        std::cout << "Error file open\n";
    }

    file.close();
}


// Функция для удаления клиента из списка
void removeClient(SOCKET clientSocket)
{
    auto it = std::remove_if(clients.begin(), clients.end(), [clientSocket](const Client& client) {
        return client.socket == clientSocket;
        });
    clients.erase(it, clients.end());
}

// Функция для обработки подключенного клиента
void clientHandler(SOCKET clientSocket)
{

    char buffer[4096];
    std::string clientID;

    // Получение идентификатора клиента
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesReceived != SOCKET_ERROR)
    {
        clientID = buffer;
        std::cout << "Клиент подключился: " << clientID << std::endl;

        // Добавление клиента в список
        clients.push_back({ clientSocket, clientID });

        // Отправка сообщения о подключении клиента всем остальным клиентам
        std::string connectMsg = "Клиент " + clientID + " подключился.";
        for (const Client& client : clients)
        {
            if (client.socket != clientSocket)
                send(client.socket, connectMsg.c_str(), connectMsg.size() + 1, 0);
        }

        while (true)
        {
            // Прием сообщения от клиента
            memset(buffer, 0, sizeof(buffer));
            bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (bytesReceived == SOCKET_ERROR)
            {
                std::cerr << "Ошибка приема сообщения от клиента " << clientID << ". Код ошибки: " << WSAGetLastError() << std::endl;
                break;
            }

            std::string message = buffer;
            std::cout << "Клиент " << clientID << " отправил сообщение: " << message << std::endl;

            // Проверка на отправку приватного сообщения
            if (message.substr(0, 7) == "PRIVATE")
            {
                // Извлечение идентификатора получателя и текста сообщения из сообщения
                size_t delimiterPos = message.find('#');
                if (delimiterPos != std::string::npos && delimiterPos < message.size() - 1)
                {
                    std::string recipientID = message.substr(8, delimiterPos - 8);
                    std::string privateMessage = message.substr(delimiterPos + 1);

                    // Пересылка приватного сообщения конкретному клиенту
                    for (const Client& client : clients)
                    {
                        if (client.id == recipientID)
                        {
                            std::string privateMessageToSend = "PRIVATE from " + clientID + ": " + privateMessage;
                            send(client.socket, privateMessageToSend.c_str(), privateMessageToSend.size() + 1, 0);
                            break;
                        }
                    }
                }
            }
            else if (message.substr(0, 4) == "FILE")
            {
                receive_file(clientSocket);
            }
            else if (message.substr(0, 9) == "BROADCAST")
            {
                std::string broadcastMessage = "BROADCAST from " + clientID + ": " + message.substr(10);

                // Отправка широковещательного сообщения всем клиентам, кроме отправителя
                for (const Client& client : clients)
                {
                    if (client.socket != clientSocket)
                        send(client.socket, broadcastMessage.c_str(), broadcastMessage.size() + 1, 0);
                }
            }
            else
            {
                // Отправка сообщения по идентификатору получателя
                size_t delimiterPos = message.find(' ');
                if (delimiterPos != std::string::npos && delimiterPos < message.size() - 1)
                {
                    std::string recipientID = message.substr(0, delimiterPos);
                    std::string privateMessage = message.substr(delimiterPos + 1);

                    for (const Client& client : clients)
                    {
                        if (client.id == recipientID)
                        {
                            std::string privateMessageToSend = "PRIVATE from " + clientID + ": " + privateMessage;
                            send(client.socket, privateMessageToSend.c_str(), privateMessageToSend.size() + 1, 0);
                            break;
                        }
                    }
                }
            }

        }
    }
    else
    {
        std::cerr << "Ошибка приема идентификатора клиента. Код ошибки: " << WSAGetLastError() << std::endl;
    }

    // Закрытие сокета клиента и удаление его из списка
    closesocket(clientSocket);
    std::cout << "Клиент отключился: " << clientID << std::endl;
    removeClient(clientSocket);
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
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET)
    {
        std::cerr << "Ошибка создания сокета. Код ошибки: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return -1;
    }
    // Установка параметров адреса сервера
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(54000);
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    // Привязка сокета к адресу сервера
    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR)
    {
        std::cerr << "Ошибка привязки сокета к адресу сервера. Код ошибки: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return -1;
    }
    // Ожидание подключения клиентов
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        std::cerr << "Ошибка ожидания подключения клиентов. Код ошибки: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return -1;
    }
    std::cout << "Сервер запущен. Ожидание подключений..." << std::endl;
    // Создание потока для чтения ввода сервера
    std::thread inputThread([&]() {
        std::string input;
        while (true)
        {
            std::getline(std::cin, input);
            // Проверка на отправку сообщения всем клиентам или конкретному клиенту
            if (input.substr(0, 8) == "SENDTOID")
            {
                // Извлечение идентификатора получателя и текста сообщения из введенных данных
                size_t delimiterPos = input.find('#');
                if (delimiterPos != std::string::npos && delimiterPos < input.size() - 1)
                {
                    std::string recipientID = input.substr(9, delimiterPos - 9);
                    std::string message = input.substr(delimiterPos + 1);

                    // Пересылка сообщения конкретному клиенту
                    auto it = std::find_if(clients.begin(), clients.end(), [&](const Client& client) {
                        return client.id == recipientID;
                        });
                    if (it != clients.end())
                    {
                        std::string privateMessage = "SENDTOID from Server: " + message;
                        send(it->socket, privateMessage.c_str(), privateMessage.size() + 1, 0);
                    }
                    else
                    {
                        std::cout << "Клиент с идентификатором " << recipientID << " не найден." << std::endl;
                    }
                }

            }
            else if (input.substr(0, 4) == "FILE")
            {
                std::string filePath = input.substr(5);
                std::cout << "Путь к файлу: " << filePath << std::endl;
                send_file(serverSocket, filePath);

            }

            else
            {
                // Отправка введенных данных всем клиентам
                std::string broadcastMessage = "BROADCAST from Server: " + input;
                for (const Client& client : clients)
                {
                    send(client.socket, broadcastMessage.c_str(), broadcastMessage.size() + 1, 0);
                }
            }
        }
        });

    while (true)
    {
        // Принятие входящего подключения
        SOCKET clientSocket = accept(serverSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET)
        {
            std::cerr << "Ошибка принятия входящего подключения. Код ошибки: " << WSAGetLastError() << std::endl;
            closesocket(serverSocket);
            WSACleanup();
            return -1;
        }

        // Запуск обработчика клиента в отдельном потоке
        std::thread clientThread(clientHandler, clientSocket);
        clientThread.detach(); // Отсоединяем поток от основного потока сервера
    }

    // Закрытие сокета сервера
    closesocket(serverSocket);

    // Освобождение ресурсов Winsock
    WSACleanup();

    return 0;
}
