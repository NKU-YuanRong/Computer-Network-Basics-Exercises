#include<iostream>
#include<string>
#include<thread>
#include"winsock2.h"
#include"time.h"
#pragma comment(lib,"ws2_32.lib")
using namespace std;
int Server();
int Client();
void RecMsg(SOCKET soc);

int main() {
    cout << "Welcome to SoChat, are u host or client? ( host / client )" << endl;
    // initial dll
    WSADATA wsd;
    if (WSAStartup(MAKEWORD(2, 2), &wsd) != 0) {
        cout << "Fail to initial! Error code: " << WSAGetLastError() << endl;
        return 1;
    }

    // host or client
    bool is_host = false, is_client = false;
    string idt = "";
    while (!is_host && !is_client) {
        cin >> idt;
        if (idt == "host") {
            cout << "You are the host!" << endl;
            is_host = true;
        }
        else if (idt == "client") {
            cout << "You are the client!" << endl;
            is_client = true;
        }
        else {
            cout << "Invalid input, please try again ( host / client)" << endl;
        }
    }

    if (is_host) {
        Server();
    }
    else if (is_client) {
        Client();
    }

	return 0;
}

void RecMsg(SOCKET soc) {
    char msg[200];// save message
    while (true) {
        int num = recv(soc, msg, 200, 0);
        // 判断消息是否为空
        msg[num] = '\0';
        // 检查幻数
        string magic = "abcdefgh";
        string t_magic = "";
        for (int i = 0; i < 8; i++) {
            t_magic += msg[i];
        }
        if (magic == t_magic) {
            int pos = 0;
            for (int i = 8; i < num; i++) {
                if (msg[i] == 'M') {
                    if (msg[i + 1] == 's') {
                        pos = i + 5;
                        break;
                    }
                }
            }
            if (pos < num) {
                cout << msg << endl;
            }
        }
    }
}

int Server() {

    // create socket
    SOCKET soc = socket(AF_INET, SOCK_STREAM, 0);
    if (soc == INVALID_SOCKET) {
        cout << "Fail to create socket! Error code: " << WSAGetLastError() << endl;
        return 1;
    }

    // Input port
    int port = 10122;
    cout << "Please input your port number: ";
    cin >> port;

    // bond ID and port
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.S_un.S_addr = INADDR_ANY;
    int err = bind(soc, (sockaddr*)&sin, sizeof(SOCKADDR_IN));
    if (err != 0)
    {
        cout << "Bind error! Error code: " << WSAGetLastError() << endl;
        return 1;
    }
    cout << "Bind sunccessfully!" << endl;

    // Begin listening
    err = listen(soc, 10);
    if (err != 0) {
        cout << "Listen error! Error code: " << WSAGetLastError() << endl;
        return 1;
    }

    // Build connection
    SOCKET clientSocket;
    sockaddr_in client_sin;
    int len = sizeof(client_sin);
    cout << "Waiting for connection..." << endl;
    clientSocket = accept(soc, (SOCKADDR*)&client_sin, &len);

    if (clientSocket == INVALID_SOCKET) {
        cout << "Accept error" << endl;
        return 1;
    }
    cout << "Accept：" << inet_ntoa(client_sin.sin_addr) << endl;

    // Receive message
    thread in_out[1];
    in_out[0] = thread(RecMsg, clientSocket);

    // Send message
    while (true) {
        string data;
        getline(cin, data);
        time_t now = time(NULL);
        tm* tm_t = localtime(&now);
        data = "abcdefgh" + to_string(tm_t->tm_year + 1900) + '.' + to_string(tm_t->tm_mon + 1) + '.' + to_string(tm_t->tm_mday) + "-" +
            to_string(tm_t->tm_hour) + ':' + to_string(tm_t->tm_min) + ':' + to_string(tm_t->tm_sec) +
            ", Msg: " + data;
        const char* sendData;
        sendData = data.c_str();
        send(clientSocket, sendData, strlen(sendData), 0);
    }
    closesocket(clientSocket);
    closesocket(soc);
    WSACleanup();
    return 0;
}

int Client()
{
    // create client socket
    SOCKET soc = socket(AF_INET, SOCK_STREAM, 0);
    if (soc == INVALID_SOCKET) {
        cout << "Fail to create socket! Error code: " << WSAGetLastError() << endl;
        return 1;
    }

    // Input port
    int port = 10122;
    cout << "Please input your port number: ";
    cin >> port;

    // bind ID and port
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
    int err = connect(soc, (sockaddr*)&sin, sizeof(SOCKADDR_IN));
    if (err != 0)
    {
        cout << "Connect error! Error code: " << WSAGetLastError() << endl;
        return 1;
    }

    cout << "Connected!" << endl;

    // New thread to receive message
    thread in_out[1];
    in_out[0] = thread(RecMsg, soc);

    // Send message
    while (true) {
        string data;
        getline(cin, data);
        time_t now = time(NULL);
        tm* tm_t = localtime(&now);
        data = "abcdefgh" + to_string(tm_t->tm_year + 1900) + '.'+ to_string(tm_t->tm_mon + 1) + '.' + to_string(tm_t->tm_mday) + "-" +
            to_string(tm_t->tm_hour) + ':' + to_string(tm_t->tm_min) + ':' + to_string(tm_t->tm_sec) +
            ", Msg: " + data;
        const char* msg;
        msg = data.c_str();
        send(soc, msg, strlen(msg), 0);
    }

    closesocket(soc);
    WSACleanup();

    return 0;
}