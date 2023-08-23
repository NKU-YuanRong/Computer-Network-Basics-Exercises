#include<iostream>
#include<fstream>
#include<string>
#include<thread>
#include<mutex>
#include"winsock2.h"
#include"time.h"
#pragma comment(lib,"ws2_32.lib")
using namespace std;

#define SYN 1
#define ACK 2
#define FIN 4
#define PSH 8
#define OVE 16

#define maxSize 8000    // 单个包的最大尺寸

string file = "1.jpg";
string send_file = "D:\\WorkSpace\\CppProjects\\Computer_Internet_HW4\\Sen_File\\" + file;
string rec_path = "D:\\WorkSpace\\CppProjects\\Computer_Internet_HW4\\Rec_File\\" + file;

#define error_rate 0    // 发送端错误率
#define lost_rate 10     // 发送端丢包率
#define rerror_rate 0   // 接收端错误率
#define rlost_rate 0    // 接收端丢包率

int lost_num = 0, error_num = 0;    // 统计实际丢失的包数量

float winLen = 1, ssth = 65;        // 窗口大小与ssthresh，分别初始化为1和65
std::mutex wmtx;

int lptr = 0, rptr = (int)winLen, senptr = 0; // lptr、rptr滑动窗口指针  senptr当前发送包指针
std::mutex mtx;         // 小锁一手，防止出现并行问题


int maxPackge = 0;      // 发送文件的最大包数量（会在程序运行时被初始化）
int maxTime = 10000;    // 连接错误时间

int lastAck = -1;
bool send_not_over = true;

clock_t *sendTime;
int outTime = 500;      // 超时时间

int Sender();
int Receiver();

int My_Sender(SOCKET clientSocket);
int My_Receiver(SOCKET soc);

void Sender_Rec(SOCKET clientSocket);
int Send_Message(SOCKET clientSocket);
int Receive_Message(SOCKET soc);

struct MsgHead {
    u_short len;			// 数据长度，16位
    u_short checkSum;		// 校验和，16位
    unsigned char type;		// 消息类型，8位
    u_short ack;		    // 累计ack，16位
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
        return -1;
    }

    // Input port
    int port = 10122;

    // bond ID and port
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.S_un.S_addr = INADDR_ANY;
    int err = bind(soc, (sockaddr*)&sin, sizeof(SOCKADDR_IN));
    if (err != 0)
    {
        ::cout << "Bind error! Error code: " << WSAGetLastError() << endl;
        return -1;
    }
    ::cout << "Bind sunccessfully!" << endl;

    // Begin listening
    err = listen(soc, 10);
    if (err != 0) {
        ::cout << "Listen error! Error code: " << WSAGetLastError() << endl;
        return -1;
    }

    // Build connection
    SOCKET clientSocket;
    sockaddr_in client_sin;
    int len = sizeof(client_sin);
    ::cout << "Waiting for connection..." << endl;
    clientSocket = accept(soc, (SOCKADDR*)&client_sin, &len);

    if (clientSocket == INVALID_SOCKET) {
        ::cout << "Accept error" << endl;
        return -1;
    }
    ::cout << "Accept：" << inet_ntoa(client_sin.sin_addr) << endl;

    My_Sender(clientSocket);

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
        return -1;
    }

    // Input port
    int port = 10122;

    // bind ID and port
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
    int err = connect(soc, (sockaddr*)&sin, sizeof(SOCKADDR_IN));
    if (err != 0)
    {
        ::cout << "Connect error! Error code: " << WSAGetLastError() << endl;
        return -1;
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
    h1.ack = clock() % 100; // 随机初始化ack
    h1.checkSum = 0;
    char* sendBuf = new char[1024];
    ::memset(sendBuf, 0, sizeof(sendBuf));
    ::memcpy(sendBuf, &h1, sizeof(h1));
    h1.checkSum = checkSumVerify((u_short*)sendBuf, 1024);
    ::memcpy(sendBuf, &h1, sizeof(h1));
    while (true) {
        if (send(clientSocket, sendBuf, 1024, 0) == -1) {
            ::cout << "Shake hand error! Error code: " << WSAGetLastError() << endl;
            return false;
        }
        else {
            ::cout << "Sender: [SYN] ack=" << (int)h1.ack << endl;
            break;
        }
    }

    char* recvBuf = new char[1024];
    ::memset(recvBuf, 0, sizeof(recvBuf));
    int addrlen = sizeof(SOCKADDR);
    MsgHead h2;
    while (true) {
        if (recv(clientSocket, recvBuf, 1024, 0) > 0) {
            ::memcpy(&h2, recvBuf, sizeof(h2));
            if (h2.type == SYN + ACK && h2.ack == h1.ack + 1 && !checkSumVerify((u_short*)recvBuf, 1024)) {
                ::cout << "Receiver: [SYN, ACK] ack=" << (int)h2.ack << endl;
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
    h1.ack = h2.ack + 1;
    h1.checkSum = 0;
    ::memset(sendBuf, 0, sizeof(sendBuf));
    ::memcpy(sendBuf, &h1, sizeof(h1));
    h1.checkSum = checkSumVerify((u_short*)sendBuf, 1024);
    ::memcpy(sendBuf, &h1, sizeof(h1));
    while (true) {
        if (send(clientSocket, sendBuf, 1024, 0) == -1) {
            ::cout << "Shake hand error! Error code: " << WSAGetLastError() << endl;
            return false;
        }
        else {
            ::cout << "Sender: [ACK] Seq=" << (int)h1.ack << endl;
            break;
        }
    }

    ::cout << "----------CONNECT-SUCCESSFULLY----------" << endl;

    // send messages
    Send_Message(clientSocket);

    ::cout << "----------DISCONNECTING----------" << endl;
    // Wave Hand
    h1.type = FIN;
    h1.len = 1024;
    h1.ack = clock() % 1000;
    h1.checkSum = 0;
    ::memset(sendBuf, 0, sizeof(sendBuf));
    ::memcpy(sendBuf, &h1, sizeof(h1));
    h1.checkSum = checkSumVerify((u_short*)sendBuf, 1024);
    ::memcpy(sendBuf, &h1, sizeof(h1));
    while (true) {
        if (send(clientSocket, sendBuf, 1024, 0) == -1) {
            ::cout << "Shake hand error! Error code: " << WSAGetLastError() << endl;
            return false;
        }
        else {
            ::cout << "Sender: [FIN] Ack=" << (int)h1.ack << endl;
            break;
        }
    }

    ::memset(recvBuf, 0, sizeof(recvBuf));
    addrlen = sizeof(SOCKADDR);
    while (true) {
        if (recv(clientSocket, recvBuf, 1024, 0) > 0) {
            ::memcpy(&h2, recvBuf, sizeof(h2));
            if (h2.type == ACK && h2.ack == h1.ack + 1 && !checkSumVerify((u_short*)recvBuf, 1024)) {
                ::cout << "Receiver: [ACK] Ack=" << (int)h2.ack << endl;
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
            if (h2.type == ACK + FIN && h2.ack == h1.ack + 2 && !checkSumVerify((u_short*)recvBuf, 1024)) {
                ::cout << "Receiver: [FIN ACK] Seq=" << (int)h2.ack << endl;
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
    h1.ack = h2.ack + 1;
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
        ::cout << "Sender: [ACK] Ack=" << (int)h1.ack << endl;
    }

    ::cout << "----------DISCONNECT-SUCCESSFULLY----------" << endl;

    delete[]sendBuf;
    delete[]recvBuf;
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
                ::cout << "Sender: [SYN] Seq=" << (int)h2.ack << endl;
                // lastAck = h2.seq;
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
    h1.ack = h2.ack + 1;
    h1.checkSum = 0;
    h1.type = SYN + ACK;
    char* sendBuf = new char[1024];
    ::memset(sendBuf, 0, sizeof(sendBuf));
    ::memcpy(sendBuf, &h1, sizeof(h1));
    h1.checkSum = checkSumVerify((u_short*)sendBuf, 1024);
    ::memcpy(sendBuf, &h1, sizeof(h1));
    while (true) {
        if (send(soc, sendBuf, 1024, 0) == -1) {
            ::cout << "Shake hand error! Error code: " << WSAGetLastError() << endl;
            return false;
        }
        else {
            ::cout << "Receiver: [SYN, ACK] Seq=" << (int)0 << endl;
            break;
        }
    }

    ::memset(recvBuf, 0, sizeof(recvBuf));
    addrlen = sizeof(SOCKADDR);
    while (true) {
        if (recv(soc, recvBuf, 1024, 0) > 0) {
            ::memcpy(&h2, recvBuf, sizeof(h2));
            if (h2.type == ACK && h2.ack == h1.ack + 1 && !checkSumVerify((u_short*)recvBuf, 1024)) {
                ::cout << "Sender: [ACK] Seq=" << (int)h2.ack << endl;
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
                ::cout << "Sender: [FIN] Ack=" << (int)h2.ack << endl;
                lastAck = h2.ack;
                break;
            }
        }
    }

    h1.type = ACK;
    h1.len = 1024;
    h1.ack = h2.ack + 1;
    h1.checkSum = 0;
    ::memset(sendBuf, 0, sizeof(sendBuf));
    ::memcpy(sendBuf, &h1, sizeof(h1));
    h1.checkSum = checkSumVerify((u_short*)sendBuf, 1024);
    ::memcpy(sendBuf, &h1, sizeof(h1));
    while (true) {
        if (send(soc, sendBuf, 1024, 0) == -1) {
            ::cout << "Shake hand error! Error code: " << WSAGetLastError() << endl;
            return false;
        }
        else {
            ::cout << "Receiver: [ACK] Ack=" << (int)h1.ack << endl;
            break;
        }
    }


    h1.type = FIN + ACK;
    h1.len = 1024;
    h1.ack = h1.ack + 1;
    h1.checkSum = 0;
    ::memset(sendBuf, 0, sizeof(sendBuf));
    ::memcpy(sendBuf, &h1, sizeof(h1));
    h1.checkSum = checkSumVerify((u_short*)sendBuf, 1024);
    ::memcpy(sendBuf, &h1, sizeof(h1));
    while (true) {
        if (send(soc, sendBuf, 1024, 0) == -1) {
            ::cout << "Shake hand error! Error code: " << WSAGetLastError() << endl;
            return false;
        }
        else {
            ::cout << "Receiver: [FIN ACK] Ack=" << (int)h1.ack << endl;
            break;
        }
    }


    ::memset(recvBuf, 0, sizeof(recvBuf));
    addrlen = sizeof(SOCKADDR);
    while (true) {
        if (recv(soc, recvBuf, 1024, 0) > 0) {
            ::memcpy(&h2, recvBuf, sizeof(h2));
            if (h2.type == ACK && h2.ack == h1.ack + 1 && !checkSumVerify((u_short*)recvBuf, 1024)) {
                ::cout << "Sender: [ACK] Ack=" << (int)h2.ack << endl;
                lastAck = h2.ack;
                break;
            }
        }
    }
    ::cout << "----------DISCONNECT-SUCCESSFULLY----------" << endl;
    delete[]sendBuf;
    delete[]recvBuf;
    return 0;
}

void change_win(int len_chg = 0) {
    if (winLen < ssth) {
        cout << "窗口长度从" << winLen;
        winLen += len_chg;
        cout << "变为" << winLen << endl;
        
    }
    else {
        cout << "窗口长度从" << winLen;
        winLen += 1 / winLen;
        cout << "变为" << winLen << endl;
    }
}

void Sender_Rec(SOCKET clientSocket) {
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&maxTime, sizeof(int));
    // 等待接收消息
    char* recvBuf = new char[1024];
    memset(recvBuf, 0, sizeof(recvBuf));
    int addrlen = sizeof(SOCKADDR);
    int rec_last_ack = -1;
    int rec_time = 1;
    while (send_not_over) {
        if (recv(clientSocket, recvBuf, 1024, 0) > 0) {
            clock_t rt = clock();
            // 收到消息需要验证消息类型、序列号和校验和
            MsgHead h1;
            memcpy(&h1, recvBuf, sizeof(h1));
            if (7 * clock() % 100 < rerror_rate) {
                error_num++;
                cout << "---------------------------------------------------------------" << endl;
                cout << "Receiver packet error!" << endl;
                cout << "---------------------------------------------------------------" << endl;
                h1.checkSum = 0;
                memcpy(recvBuf, &h1, sizeof(h1));
            }
            if (h1.type == ACK && !checkSumVerify((u_short*)recvBuf, sizeof(h1) + h1.len)) {
                cout << "Send" << (int)h1.ack << " successfully" << endl;
                mtx.lock();
                wmtx.lock();
                if ((int)h1.ack == rec_last_ack) {
                    rec_time++;
                }
                else {
                    rec_last_ack = (int)h1.ack;
                    rec_time = 0;
                }
                if (rec_time == 3) {
                    senptr = rec_last_ack + 1;
                    cout << "数据包" << senptr << "三次重复ack，需要重传" << endl;

                    cout << "ssthresh从" << ssth;
                    ssth = winLen / 2;
                    cout << "变为" << ssth << endl;

                    cout << "窗口长度从" << winLen;
                    winLen = ssth + 3;
                    cout << "变为" << winLen << endl;
                }
                if (lptr - (int)h1.ack == 0) {
                    change_win(1);
                    lptr++;
                    rptr = lptr + (int)winLen;
                    cout << "窗口左侧移动至" << lptr << ", 窗口右侧移动至" << rptr << endl;
                    cout << "收包时间: " << rt - sendTime[(int)h1.ack] << "ms" << endl;
                    if (lptr == maxPackge) {
                        wmtx.unlock();
                        mtx.unlock();
                        return;
                    }
                }
                else if (lptr - (int)h1.ack < 0) {
                    cout << endl;
                    change_win(lptr - (int)h1.ack + 1);
                    cout << "senptr从 " << senptr;
                    senptr = (int)h1.ack + 1;
                    cout << " 移动至" << senptr << endl;
                    lptr = (int)h1.ack;
                    rptr = lptr + (int)winLen;
                    cout << "窗口左侧移动至" << lptr << ", 窗口右侧移动至" << rptr << endl;
                    cout << "收包时间: " << rt - sendTime[(int)h1.ack] << "ms" << endl;
                }
                if (sendTime[(int)h1.ack + 1] != 0) {
                    if (rt - sendTime[(int)h1.ack + 1] > outTime) {
                        cout << "Out time: " << rt - sendTime[(int)h1.ack + 1] << endl;
                        cout << "-----------------------out-time------------------------" << endl;
                        cout << "ssthresh从" << ssth;
                        ssth = max(2, winLen / 2);
                        cout << "变为" << ssth << endl;
                        winLen = 1;
                        cout << "winLen变回1" << endl;
                        rec_time = 0;
                    }
                }
                wmtx.unlock();
                mtx.unlock();
            }
            else if (h1.type == OVE && !checkSumVerify((u_short*)recvBuf, sizeof(h1) + h1.len)) {
                cout << "----------Send-over----------" << endl;
                break;
            }
        }
        else {
            mtx.lock();
            wmtx.lock();
            if (senptr == rptr) {
                cout << "senptr从 " << senptr;
                senptr = lptr;
                cout << " 移动至" << senptr << endl;
            }
            wmtx.unlock();
            mtx.unlock();
        }
    }

    delete[]recvBuf;
}

int Send_Message(SOCKET clientSocket) {
    // 以二进制方式打开文件
    ifstream fin(send_file, ifstream::in | ios::binary);
    if (!fin) {
        cout << "Error: cannot open file!" << endl;
        return -1;
    }
    fin.seekg(0, fin.end);		// 指针定位在文件尾
    int length = (int)fin.tellg();	// 获取文件大小（字节）
    maxPackge = length / maxSize;
    if (length % maxSize != 0) {
        maxPackge++;
    }
    sendTime = new clock_t[maxPackge + 1];
    for (int i = 0; i < maxPackge; i++) {
        sendTime[i] = 0;
    }
    fin.seekg(0, fin.beg);		// 指针定位在文件头
    char* data = new char[length + 10];
    memset(data, 0, sizeof(data));
    fin.read(data, length);
    fin.close();
    cout << "data length: " << length << " bit" << endl;

    setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&maxTime, sizeof(int));
    clock_t start = clock();

    thread* in_out = new thread[1];
    in_out[0] = thread(Sender_Rec, clientSocket);

    int not_to_last = -1;
    int resend_last = -1;

    MsgHead h;
    while (lptr < maxPackge) {
        if (senptr < rptr && senptr < maxPackge) {
            mtx.lock();
            wmtx.lock();
            // 设置信息头
            h.ack = senptr;
            cout << "senptr: " << (int)h.ack << endl;
            if (maxSize < length - senptr * 8000) {
                h.len = maxSize;
                h.type = PSH;
            }
            else {
                h.len = length - senptr * 8000;
                h.type = PSH + OVE;
            }
            h.checkSum = 0;
            char* sendBuf = new char[h.len + sizeof(h)];
            memset(sendBuf, 0, sizeof(sendBuf));
            memcpy(sendBuf, &h, sizeof(h));
            // data存放的是读入的二进制数据，sentLen是已发送的长度
            memcpy(sendBuf + sizeof(h), data + senptr * 8000, h.len);
            // 计算校验和
            h.checkSum = checkSumVerify((u_short*)sendBuf, sizeof(h) + h.len);
            memcpy(sendBuf, &h, sizeof(h));
            // 发送消息
            if (clock() % 100 >= lost_rate) {
                if (send(clientSocket, sendBuf, h.len + sizeof(h), 0) == -1) {
                    cout << "Error: fail to send messages!" << endl;
                    wmtx.unlock();
                    mtx.unlock();
                    return -1;
                }
            }
            else {
                lost_num++;
                cout << "---------------------------------------------------------------" << endl;
                cout << "Sender packet lost!" << endl;
                cout << "---------------------------------------------------------------" << endl;
            }
            sendTime[senptr] = clock();
            senptr++;
            delete[]sendBuf;
            cout << "Send length: " << (int)h.len << ", senptr: " << (int)h.ack << ", rate: " << 800000 * senptr / length << "%" << endl;
            wmtx.unlock();
            mtx.unlock();
        }
        else {
            mtx.lock();
            wmtx.lock();
            if (senptr == maxPackge) {
                cout << "not last: " << not_to_last << endl;
                if (not_to_last == 1) {
                    if (resend_last == 1) {
                        wmtx.unlock();
                        mtx.unlock();
                        break;
                    }
                    not_to_last = -1;
                    resend_last++;
                }
                not_to_last += 1;
            }
            wmtx.unlock();
            mtx.unlock();
            Sleep(100);
        }
    }
    send_not_over = false;

    cout << endl << "----------File-transfer-succesfully!----------" << endl;
    cout << "Receiver error number: " << error_num << ", Sender lost number: " << lost_num << endl;
    cout << "Time cost: " << clock() - start << " millisecond" << endl;
    cout << "Through output: " << length * CLOCKS_PER_SEC / (clock() - start) << " bit/second" << endl;
    in_out[0].join();
    delete[]in_out;
    delete[]data;
    return 0;
}

int Receive_Message(SOCKET soc) {
    setsockopt(soc, SOL_SOCKET, SO_RCVTIMEO, (char*)&maxTime, sizeof(int));
    int totalLen = 0;
    char* data = new char[999999999];
    MsgHead h1;
    char* recvBuf = new char[maxSize + sizeof(h1)];
    memset(recvBuf, 0, sizeof(recvBuf));
    // 等待接收消息
    while (true) {
        // 收到消息需要验证校验和及序列号
        if (recv(soc, recvBuf, maxSize + sizeof(h1), 0) > 0) {
            cout << "Rec packet" << endl;
            memcpy(&h1, recvBuf, sizeof(h1));

            if (7 * clock() % 100 < error_rate) {
                error_num++;
                h1.type = 0;
                cout << "---------------------------------------------------------------" << endl;
                cout << "Sender packet error" << endl;
                cout << "---------------------------------------------------------------" << endl;
            }

            MsgHead h2;
            h2.type = ACK;
            h2.len = 1024 - sizeof(h2);
            char* sendBuf = new char[1024];
            memset(sendBuf, 0, sizeof(sendBuf));
            if ((h1.type == PSH) && (h1.ack == lastAck + 1) && !checkSumVerify((u_short*)recvBuf, sizeof(h1) + h1.len)) {
                lastAck++;
                h2.ack = lastAck;
                h2.checkSum = 0;
                memcpy(sendBuf, &h2, sizeof(h2));
                h2.checkSum = checkSumVerify((u_short*)sendBuf, 1024);

                memcpy(sendBuf, &h2, sizeof(h2));
                memcpy(data + totalLen, recvBuf + sizeof(h1), h1.len);
                totalLen += (int)h1.len;

                if (clock() % 100 >= rlost_rate) {
                    send(soc, sendBuf, 1024, 0);
                }
                else {
                    lost_num++;
                    cout << "---------------------------------------------------------------" << endl;
                    cout << "Packge lost!" << endl;
                    cout << "---------------------------------------------------------------" << endl;
                }

                cout << "Receive length: " << (int)h1.len << ", seq: " << (int)h1.ack << endl;
            }
            else if ((h1.type == PSH + OVE) && (h1.ack == lastAck + 1) && !checkSumVerify((u_short*)recvBuf, sizeof(h1) + h1.len)) {
                lastAck++;
                h2.ack = lastAck;
                h2.checkSum = 0;
                memcpy(sendBuf, &h2, sizeof(h2));
                h2.checkSum = checkSumVerify((u_short*)sendBuf, 1024);

                memcpy(sendBuf, &h2, sizeof(h2));
                memcpy(data + totalLen, recvBuf + sizeof(h1), h1.len);
                totalLen += (int)h1.len;
                send(soc, sendBuf, 1024, 0);
                cout << "Receive length: " << (int)h1.len << ", seq: " << (int)h1.ack << endl;
                cout << "-----------------------Receive-over--------------------------" << endl;
                break;
            }
            else {	// 差错重传
                cout << (int)h1.ack << endl;
                cout << (int)lastAck << endl;
                cout << "Receive Wrong!" << endl;
                h2.ack = lastAck;
                h2.checkSum = checkSumVerify((u_short*)&h2, sizeof(h2));
                memcpy(sendBuf, &h2, sizeof(h2));
                send(soc, sendBuf, 1024, 0);
            }
            delete[]sendBuf;
        }
        else {
            MsgHead h2;
            h2.type = ACK;
            h2.len = 1024 - sizeof(h2);
            char* sendBuf = new char[1024];
            memset(sendBuf, 0, sizeof(sendBuf));
            cout << (int)h1.ack << endl;
            cout << (int)lastAck << endl;
            cout << "Time Out!" << endl;
            h2.ack = lastAck;
            h2.checkSum = checkSumVerify((u_short*)&h2, sizeof(h2));
            memcpy(sendBuf, &h2, sizeof(h2));
            send(soc, sendBuf, 1024, 0);
        }
    }

    cout << "Sender error number: " << error_num << ", Receiver lost number: " << lost_num << endl;

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
    delete[]recvBuf;
    return 0;
}
