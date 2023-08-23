#include<iostream>
#include<fstream>
#include<string>
#include<thread>
#include"winsock2.h"
#include"time.h"
#pragma comment(lib,"ws2_32.lib")
using namespace std;
#define SYN 1
#define ACK 2
#define FIN 4
#define PSH 8
#define OVE 16

#define maxSize 8000

string file = "1.jpg";
string send_file = "D:\\OneDrive - mail.nankai.edu.cn\\[2022.9.5]计算机网络\\计算机网络Lab03\\Computer_Internet_HW2\\Sen_File\\" + file;
string rec_path = "D:\\OneDrive - mail.nankai.edu.cn\\[2022.9.5]计算机网络\\计算机网络Lab03\\Computer_Internet_HW2\\Rec_File\\" + file;

int error_rate = 0;
int rerror_rate = 0;
int lost_rate = 20;
int rlost_rate = 0;
int maxTime = 1000;

int delay = 5;

unsigned char seq = 1;
unsigned char lastAck = 0;

int Sender();
int Receiver();

int My_Sender(SOCKET clientSocket);
int My_Receiver(SOCKET soc);

int Send_Message(SOCKET clientSocket);
int Receive_Message(SOCKET soc);

struct MsgHead {
    u_short len;			// 数据长度，16位
    u_short checkSum;		// 校验和，16位
    unsigned char type;		// 消息类型
    unsigned char seq;		// 序列号，可以表示0-255
};

u_short checkSumVerify(u_short* msg, int length) {
    int count = (length + 1) / 2;
    u_short* buf = (u_short*)malloc(length + 1);
    memset(buf, 0, length + 1);
    memcpy(buf, msg, length);
    u_long checkSum = 0;
    while (count--) {
        checkSum += *buf++;
        if (checkSum & 0xffff0000) {
            checkSum &= 0xffff;
            checkSum++;
        }
    }
    return ~(checkSum & 0xffff);
}

int main() {
    ::cout << "Welcome to SoChat, are u sender or receiver? ( sen / rec )" << endl;
    srand((unsigned int)(time(0)));

    // initial dll
    WSADATA wsd;
    if (WSAStartup(MAKEWORD(2, 2), &wsd) != 0) {
        ::cout << "Fail to initial! Error code: " << WSAGetLastError() << endl;
        return 1;
    }

    // sneder or receiver
    bool is_sender = false, is_receiver = false;
    string idt = "";
    while (!is_sender && !is_receiver) {
        cin >> idt;
        if (idt == "sen" || idt == "sender") {
            ::cout << "You are the sender!" << endl;
            is_sender = true;
        }
        else if (idt == "rec" || idt == "receiver") {
            ::cout << "You are the receiver!" << endl;
            is_receiver = true;
        }
        else {
            ::cout << "Invalid input, please try again ( sen / rec )" << endl;
        }
    }

    if (is_sender) {
        Sender();
    }
    else if (is_receiver) {
        Receiver();
    }

    return 0;
}

int Sender() {
    // create socket
    SOCKET soc = socket(AF_INET, SOCK_STREAM, 0);
    if (soc == INVALID_SOCKET) {
        ::cout << "Fail to create socket! Error code: " << WSAGetLastError() << endl;
        return 1;
    }

    // Input port
    int port = 10122;
    // cout << "Please input your port number: ";
    // cin >> port;

    // bond ID and port
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.S_un.S_addr = INADDR_ANY;
    int err = bind(soc, (sockaddr*)&sin, sizeof(SOCKADDR_IN));
    if (err != 0)
    {
        ::cout << "Bind error! Error code: " << WSAGetLastError() << endl;
        return 1;
    }
    ::cout << "Bind sunccessfully!" << endl;

    // Begin listening
    err = listen(soc, 10);
    if (err != 0) {
        ::cout << "Listen error! Error code: " << WSAGetLastError() << endl;
        return 1;
    }

    // Build connection
    SOCKET clientSocket;
    sockaddr_in client_sin;
    int len = sizeof(client_sin);
    ::cout << "Waiting for connection..." << endl;
    clientSocket = accept(soc, (SOCKADDR*)&client_sin, &len);

    if (clientSocket == INVALID_SOCKET) {
        ::cout << "Accept error" << endl;
        return 1;
    }
    ::cout << "Accept：" << inet_ntoa(client_sin.sin_addr) << endl;


    My_Sender(clientSocket);


    // Send message
    /*
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
        
        
        
    }*/


    closesocket(clientSocket);
    closesocket(soc);
    WSACleanup();
    return 0;
}

int Receiver() {
    // create client socket
    SOCKET soc = socket(AF_INET, SOCK_STREAM, 0);
    if (soc == INVALID_SOCKET) {
        ::cout << "Fail to create socket! Error code: " << WSAGetLastError() << endl;
        return 1;
    }

    // Input port
    int port = 10122;
    // cout << "Please input your port number: ";
    // cin >> port;

    // bind ID and port
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
    int err = connect(soc, (sockaddr*)&sin, sizeof(SOCKADDR_IN));
    if (err != 0)
    {
        ::cout << "Connect error! Error code: " << WSAGetLastError() << endl;
        return 1;
    }

    ::cout << "Connected!" << endl;

    My_Receiver(soc);

    closesocket(soc);
    WSACleanup();

    return 0;
}

int My_Sender(SOCKET clientSocket) {

    ::cout << "----------CONNECTING----------" << endl;
    // Shake Hand
    MsgHead h1;
    h1.type = SYN;
    h1.len = 1024;
    h1.seq = seq;
    h1.checkSum = 0;
    char* sendBuf = new char[1024];
    ::memset(sendBuf, 0, sizeof(sendBuf));
    ::memcpy(sendBuf, &h1, sizeof(h1));
    h1.checkSum = checkSumVerify((u_short*)sendBuf, 1024);
    ::memcpy(sendBuf, &h1, sizeof(h1));
    if (send(clientSocket, sendBuf, 1024, 0) == -1) {
        ::cout << "Shake hand error! Error code: " << WSAGetLastError() << endl;
        return false;
    }
    else {
        ::cout << "Sender: [SYN] Seq=" << (int)seq << endl;
        seq = (seq + 1) % 256;
    }

    char* recvBuf = new char[1024];
    ::memset(recvBuf, 0, sizeof(recvBuf));
    int addrlen = sizeof(SOCKADDR);
    MsgHead h2;
    while (true) {
        if (recv(clientSocket, recvBuf, 1024, 0) > 0) {
            ::memcpy(&h2, recvBuf, sizeof(h2));
            if (h2.type == SYN + ACK && !checkSumVerify((u_short*)recvBuf, 1024)) {
                ::cout << "Receiver: [SYN, ACK] Seq=" << (int)h2.seq << endl;
                break;
            }
            else {
                ::cout << "Shake hand error! Error code: " << WSAGetLastError() << endl;
                return false;
            }
        }
    }

    h1.type = ACK;
    h1.len = 1024;
    h1.seq = seq;
    h1.checkSum = 0;
    ::memset(sendBuf, 0, sizeof(sendBuf));
    ::memcpy(sendBuf, &h1, sizeof(h1));
    h1.checkSum = checkSumVerify((u_short*)sendBuf, 1024);
    ::memcpy(sendBuf, &h1, sizeof(h1));
    if (send(clientSocket, sendBuf, 1024, 0) == -1) {
        ::cout << "Shake hand error! Error code: " << WSAGetLastError() << endl;
        return false;
    }
    else {
        ::cout << "Sender: [ACK] Seq=" << (int)seq << endl;
        seq = (seq + 1) % 256;
    }
    ::cout << "----------CONNECT-SUCCESSFULLY----------" << endl;

    // send messages
    Send_Message(clientSocket);

    ::cout << "----------DISCONNECTING----------" << endl;
    // Wave Hand
    h1.type = FIN;
    h1.len = 1024;
    h1.seq = seq;
    h1.checkSum = 0;
    ::memset(sendBuf, 0, sizeof(sendBuf));
    ::memcpy(sendBuf, &h1, sizeof(h1));
    h1.checkSum = checkSumVerify((u_short*)sendBuf, 1024);
    ::memcpy(sendBuf, &h1, sizeof(h1));
    if (send(clientSocket, sendBuf, 1024, 0) == -1) {
        ::cout << "Shake hand error! Error code: " << WSAGetLastError() << endl;
        return false;
    }
    else {
        ::cout << "Sender: [FIN] Seq=" << (int)seq << endl;
        seq = (seq + 1) % 256;
    }

    ::memset(recvBuf, 0, sizeof(recvBuf));
    addrlen = sizeof(SOCKADDR);
    while (true) {
        if (recv(clientSocket, recvBuf, 1024, 0) > 0) {
            ::memcpy(&h2, recvBuf, sizeof(h2));
            if (h2.type == ACK && !checkSumVerify((u_short*)recvBuf, 1024)) {
                ::cout << "Receiver: [ACK] Seq=" << (int)h2.seq << endl;
                break;
            }
            else {
                ::cout << "Shake hand error! Error code: " << WSAGetLastError() << endl;
                return false;
            }
        }
    }

    ::memset(recvBuf, 0, sizeof(recvBuf));
    addrlen = sizeof(SOCKADDR);
    while (true) {
        if (recv(clientSocket, recvBuf, 1024, 0) > 0) {
            ::memcpy(&h2, recvBuf, sizeof(h2));
            if (h2.type == ACK + FIN && !checkSumVerify((u_short*)recvBuf, 1024)) {
                ::cout << "Receiver: [FIN ACK] Seq=" << (int)h2.seq << endl;
                break;
            }
            else {
                ::cout << "Shake hand error! Error code: " << WSAGetLastError() << endl;
                return false;
            }
        }
    }

    h1.type = ACK;
    h1.len = 1024;
    h1.seq = seq;
    h1.checkSum = 0;
    ::memset(sendBuf, 0, sizeof(sendBuf));
    ::memcpy(sendBuf, &h1, sizeof(h1));
    h1.checkSum = checkSumVerify((u_short*)sendBuf, 1024);
    ::memcpy(sendBuf, &h1, sizeof(h1));
    if (send(clientSocket, sendBuf, 1024, 0) == -1) {
        ::cout << "Shake hand error! Error code: " << WSAGetLastError() << endl;
        return false;
    }
    else {
        ::cout << "Sender: [ACK] Seq=" << (int)seq << endl;
        seq = (seq + 1) % 256;
    }

    ::cout << "----------DISCONNECT-SUCCESSFULLY----------" << endl;

    return 0;
}

int My_Receiver(SOCKET soc) {
    // Shake Hand
    ::cout << "----------CONNECTING----------" << endl;

    char* recvBuf = new char[1024];
    ::memset(recvBuf, 0, sizeof(recvBuf));
    int addrlen = sizeof(SOCKADDR);
    MsgHead h2;
    while (true) {
        if (recv(soc, recvBuf, 1024, 0) > 0) {
            ::memcpy(&h2, recvBuf, sizeof(h2));
            if (h2.type == SYN && !checkSumVerify((u_short*)recvBuf, 1024)) {
                ::cout << "Sender: [SYN] Seq=" << (int)h2.seq << endl;
                lastAck = h2.seq;
                break;
            }
            else {
                ::cout << "Shake hand error! Error code: " << WSAGetLastError() << endl;
                return false;
            }
        }
    }

    MsgHead h1;
    h1.len = 1024;
    h1.seq = seq;
    h1.checkSum = 0;
    h1.type = SYN + ACK;
    char* sendBuf = new char[1024];
    ::memset(sendBuf, 0, sizeof(sendBuf));
    ::memcpy(sendBuf, &h1, sizeof(h1));
    h1.checkSum = checkSumVerify((u_short*)sendBuf, 1024);
    ::memcpy(sendBuf, &h1, sizeof(h1));
    if (send(soc, sendBuf, 1024, 0) == -1) {
        ::cout << "Shake hand error! Error code: " << WSAGetLastError() << endl;
        return false;
    }
    else {
        ::cout << "Receiver: [SYN, ACK] Seq=" << (int)seq << endl;
        seq = (seq + 1) % 256;
    }

    ::memset(recvBuf, 0, sizeof(recvBuf));
    addrlen = sizeof(SOCKADDR);
    while (true) {
        if (recv(soc, recvBuf, 1024, 0) > 0) {
            ::memcpy(&h2, recvBuf, sizeof(h2));
            if (h2.type == ACK && !checkSumVerify((u_short*)recvBuf, 1024)) {
                ::cout << "Sender: [ACK] Seq=" << (int)h2.seq << endl;
                lastAck = h2.seq;
                break;
            }
            else {
                ::cout << "Shake hand error! Error code: " << WSAGetLastError() << endl;
                return false;
            }
        }
    }

    ::cout << "----------CONNECT-SUCCESSFULLY----------" << endl;

    
    // Receive message
    Receive_Message(soc);


    ::cout << "----------DISCONNECTING----------" << endl;
    // Wave Hand
    ::memset(recvBuf, 0, sizeof(recvBuf));
    addrlen = sizeof(SOCKADDR);
    while (true) {
        if (recv(soc, recvBuf, 1024, 0) > 0) {
            ::memcpy(&h2, recvBuf, sizeof(h2));
            if (h2.type == FIN && !checkSumVerify((u_short*)recvBuf, 1024)) {
                ::cout << "Sender: [FIN] Seq=" << (int)h2.seq << endl;
                lastAck = h2.seq;
                break;
            }
            else {
                ::cout << "Shake hand error! Error code: " << WSAGetLastError() << endl;
                return false;
            }
        }
    }

    h1.type = ACK;
    h1.len = 1024;
    h1.seq = seq;
    h1.checkSum = 0;
    ::memset(sendBuf, 0, sizeof(sendBuf));
    ::memcpy(sendBuf, &h1, sizeof(h1));
    h1.checkSum = checkSumVerify((u_short*)sendBuf, 1024);
    ::memcpy(sendBuf, &h1, sizeof(h1));
    if (send(soc, sendBuf, 1024, 0) == -1) {
        ::cout << "Shake hand error! Error code: " << WSAGetLastError() << endl;
        return false;
    }
    else {
        ::cout << "Receiver: [ACK] Seq=" << (int)seq << endl;
        seq = (seq + 1) % 256;
    }

    h1.type = FIN + ACK;
    h1.len = 1024;
    h1.seq = seq;
    h1.checkSum = 0;
    ::memset(sendBuf, 0, sizeof(sendBuf));
    ::memcpy(sendBuf, &h1, sizeof(h1));
    h1.checkSum = checkSumVerify((u_short*)sendBuf, 1024);
    ::memcpy(sendBuf, &h1, sizeof(h1));
    if (send(soc, sendBuf, 1024, 0) == -1) {
        ::cout << "Shake hand error! Error code: " << WSAGetLastError() << endl;
        return false;
    }
    else {
        ::cout << "Receiver: [FIN ACK] Seq=" << (int)seq << endl;
        seq = (seq + 1) % 256;
    }

    ::memset(recvBuf, 0, sizeof(recvBuf));
    addrlen = sizeof(SOCKADDR);
    while (true) {
        if (recv(soc, recvBuf, 1024, 0) > 0) {
            ::memcpy(&h2, recvBuf, sizeof(h2));
            if (h2.type == ACK && !checkSumVerify((u_short*)recvBuf, 1024)) {
                ::cout << "Sender: [ACK] Seq=" << (int)h2.seq << endl;
                lastAck = h2.seq;
                break;
            }
            else {
                ::cout << "Shake hand error! Error code: " << WSAGetLastError() << endl;
                return false;
            }
        }
    }
    ::cout << "----------DISCONNECT-SUCCESSFULLY----------" << endl;
}

int Send_Message(SOCKET clientSocket) {
    // 以二进制方式打开文件
    ifstream fin(send_file, ifstream::in | ios::binary);
    if (!fin) {
        cout << "Error: cannot open file!" << endl;
        return -1;
    }
    setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&maxTime, sizeof(int));
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&maxTime, sizeof(int));
    fin.seekg(0, fin.end);		// 指针定位在文件尾
    int length = fin.tellg();	// 获取文件大小（字节）
    fin.seekg(0, fin.beg);		// 指针定位在文件头
    char* data = new char[length];
    memset(data, 0, sizeof(data));
    fin.read(data, length);
    fin.close();
    int sentLen = 0;
    cout << "data length: " << length << " bit" << endl;
    int randflag;
    bool send_suc;
    clock_t start = clock();
    while (true) {
        // 设置信息头
        send_suc = false;
        MsgHead h;
        h.seq = seq;
        // cout << "Seq " << (int)h.seq << endl;
        if (maxSize < length - sentLen) {
            h.len = maxSize;
            h.type = PSH;
        }
        else {
            h.len = length - sentLen;
            h.type = PSH + OVE;
        }
        h.checkSum = 0;
        char* sendBuf = new char[h.len + sizeof(h)];
        memset(sendBuf, 0, sizeof(sendBuf));
        memcpy(sendBuf, &h, sizeof(h));
        // data存放的是读入的二进制数据，sentLen是已发送的长度
        memcpy(sendBuf + sizeof(h), data + sentLen, h.len);
        // 计算校验和
        h.checkSum = checkSumVerify((u_short*)sendBuf, sizeof(h) + h.len);
        // cout << "check sum: " << (int)h.checkSum << endl;
        memcpy(sendBuf, &h, sizeof(h));

        // cout << "Packge ready!" << endl;
        // 发送消息
        if (randflag = 7 * clock() % 100 >= lost_rate) {
            if (send(clientSocket, sendBuf, h.len + sizeof(h), 0) == -1) {
                cout << "Error: fail to send messages!" << endl;
                return -1;
            }
        }
        else {
            cout << "Sender packet lost!" << endl;
        }
        cout << "Send length: " << (int)h.len << ", seq: " << (int)h.seq << ", rate: " << 100 * sentLen / length << "\%" << endl;
        
        // 等待接收消息
        char* recvBuf = new char[1024];
        memset(recvBuf, 0, sizeof(recvBuf));
        int addrlen = sizeof(SOCKADDR);
        while (true) {
            if (recv(clientSocket, recvBuf, 1024, 0) > 0) {
                Sleep(delay);
                // 收到消息需要验证消息类型、序列号和校验和
                MsgHead h1;
                memcpy(&h1, recvBuf, sizeof(h1));
                if(h1.type == ACK && h1.seq == seq && !checkSumVerify((u_short*)recvBuf, sizeof(h1) + h1.len)) {
                    seq = (seq + 1) % 256;
                    cout << "Send successfully" << endl;
                    send_suc = true;
                    sentLen += (int)h.len;
                    break;
                }
                else {
                    // 差错重传
                    cout << "Send fail!" << endl;
                    break;
                }
            }
            else {
                // 超时重传
                cout << "Send time error!" << endl;
                break;
            }
        }
        // cout << "Sent message packet!" << h.len << endl;
        if (h.type == PSH + OVE && send_suc) {
            break;
        }
    }
    cout << "Time cost: " << clock() - start << "millisecond" << endl;
    cout << "Through output: " << length * CLOCKS_PER_SEC / (clock() - start) << " bit/second" << endl;
    return 0;
}

int Receive_Message(SOCKET soc) {
    int totalLen = 0;
    char* data = new char[999999999];
    MsgHead h1;
    char* recvBuf = new char[maxSize + sizeof(h1)];
    memset(recvBuf, 0, sizeof(recvBuf));
    // 等待接收消息
    while (true) {
        // 收到消息需要验证校验和及序列号
        if (recv(soc, recvBuf, maxSize + sizeof(h1), 0) > 0) {
            Sleep(delay);
            cout << "Rec packet" << endl;
            int randflag;
            memcpy(&h1, recvBuf, sizeof(h1));
            if (randflag = rand() % 100 < error_rate) {
                h1.type = 0;
            }
            else {
                cout << "Sender packet error" << endl;
            }
            MsgHead h2;
            h2.type = ACK;
            if (rand() % 100 < rerror_rate) {
                h2.type = 0;
            }
            else {
                cout << "Receiver packet error" << endl;
            }
            h2.len = 1024 - sizeof(h2);
            char* sendBuf = new char[1024];
            memset(sendBuf, 0, sizeof(sendBuf));
            if ((h1.type == PSH) && (h1.seq == (lastAck + 1) % 256) && !checkSumVerify((u_short*)recvBuf, sizeof(h1) + h1.len)) {
                lastAck = (lastAck + 1) % 256;
                h2.seq = lastAck;
                h2.checkSum = 0;
                memcpy(sendBuf, &h2, sizeof(h2));
                h2.checkSum = checkSumVerify((u_short*)sendBuf, 1024);
                memcpy(sendBuf, &h2, sizeof(h2));
                memcpy(data + totalLen, recvBuf + sizeof(h1), h1.len);
                totalLen += (int)h1.len;
                if (randflag = rand() % 100 >= rlost_rate) {
                    send(soc, sendBuf, 1024, 0);
                }
                else {
                    cout << "Receiver packet lost" << endl;
                }
                cout << "Receive length: " << (int)h1.len << ", seq: " << (int)h1.seq << endl;
            }
            else if ((h1.type == PSH + OVE) && (h1.seq == (lastAck + 1) % 256) && !checkSumVerify((u_short*)recvBuf, sizeof(h1) + h1.len)) {
                lastAck = (lastAck + 1) % 256;
                h2.seq = lastAck;
                h2.checkSum = 0;
                memcpy(sendBuf, &h2, sizeof(h2));
                h2.checkSum = checkSumVerify((u_short*)sendBuf, 1024);
                memcpy(sendBuf, &h2, sizeof(h2));
                memcpy(data + totalLen, recvBuf + sizeof(h1), h1.len);
                totalLen += (int)h1.len;
                send(soc, sendBuf, 1024, 0);
                cout << "Receive length: " << (int)h1.len << ", seq: " << (int)h1.seq << endl;
                cout << "-----------------------Receive-over--------------------------" << endl;
                break;
            }
            else if ((h1.type == PSH || h1.type == PSH + OVE) && (h1.seq == lastAck) && !checkSumVerify((u_short*)recvBuf, sizeof(h1) + h1.len)) {
                if (h1.type == PSH + OVE) {
                    break;
                }
            }
            else {	// 差错重传
                cout << "Receive Wrong!" << endl;
                h2.seq = lastAck;
                h2.checkSum = checkSumVerify((u_short*)&h2, sizeof(h2));
                memcpy(sendBuf, &h2, sizeof(h2));
                send(soc, sendBuf, 1024, 0);
            }
        }
    }

    // 以二进制方式写入文件
    ofstream fout(rec_path, ofstream::out | ios::binary);
    if (!fout) {
        cout << "Error: cannot open file!" << endl;
        return -1;
    }
    fout.write(data, totalLen);
    fout.close();
    cout << "File saved successfully!" << endl;
    delete[]data;
    return 0;
}