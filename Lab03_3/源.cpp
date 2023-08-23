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

#define maxSize 8000    // �����������ߴ�

string file = "1.jpg";
string send_file = "D:\\WorkSpace\\CppProjects\\Computer_Internet_HW4\\Sen_File\\" + file;
string rec_path = "D:\\WorkSpace\\CppProjects\\Computer_Internet_HW4\\Rec_File\\" + file;

#define error_rate 0    // ���Ͷ˴�����
#define lost_rate 10     // ���Ͷ˶�����
#define rerror_rate 0   // ���ն˴�����
#define rlost_rate 0    // ���ն˶�����

int lost_num = 0, error_num = 0;    // ͳ��ʵ�ʶ�ʧ�İ�����

float winLen = 1, ssth = 65;        // ���ڴ�С��ssthresh���ֱ��ʼ��Ϊ1��65
std::mutex wmtx;

int lptr = 0, rptr = (int)winLen, senptr = 0; // lptr��rptr��������ָ��  senptr��ǰ���Ͱ�ָ��
std::mutex mtx;         // С��һ�֣���ֹ���ֲ�������


int maxPackge = 0;      // �����ļ����������������ڳ�������ʱ����ʼ����
int maxTime = 10000;    // ���Ӵ���ʱ��

int lastAck = -1;
bool send_not_over = true;

clock_t *sendTime;
int outTime = 500;      // ��ʱʱ��

int Sender();
int Receiver();

int My_Sender(SOCKET clientSocket);
int My_Receiver(SOCKET soc);

void Sender_Rec(SOCKET clientSocket);
int Send_Message(SOCKET clientSocket);
int Receive_Message(SOCKET soc);

struct MsgHead {
    u_short len;			// ���ݳ��ȣ�16λ
    u_short checkSum;		// У��ͣ�16λ
    unsigned char type;		// ��Ϣ���ͣ�8λ
    u_short ack;		    // �ۼ�ack��16λ
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
    ::cout << "Accept��" << inet_ntoa(client_sin.sin_addr) << endl;

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
    h1.ack = clock() % 100; // �����ʼ��ack
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
        cout << "���ڳ��ȴ�" << winLen;
        winLen += len_chg;
        cout << "��Ϊ" << winLen << endl;
        
    }
    else {
        cout << "���ڳ��ȴ�" << winLen;
        winLen += 1 / winLen;
        cout << "��Ϊ" << winLen << endl;
    }
}

void Sender_Rec(SOCKET clientSocket) {
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&maxTime, sizeof(int));
    // �ȴ�������Ϣ
    char* recvBuf = new char[1024];
    memset(recvBuf, 0, sizeof(recvBuf));
    int addrlen = sizeof(SOCKADDR);
    int rec_last_ack = -1;
    int rec_time = 1;
    while (send_not_over) {
        if (recv(clientSocket, recvBuf, 1024, 0) > 0) {
            clock_t rt = clock();
            // �յ���Ϣ��Ҫ��֤��Ϣ���͡����кź�У���
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
                    cout << "���ݰ�" << senptr << "�����ظ�ack����Ҫ�ش�" << endl;

                    cout << "ssthresh��" << ssth;
                    ssth = winLen / 2;
                    cout << "��Ϊ" << ssth << endl;

                    cout << "���ڳ��ȴ�" << winLen;
                    winLen = ssth + 3;
                    cout << "��Ϊ" << winLen << endl;
                }
                if (lptr - (int)h1.ack == 0) {
                    change_win(1);
                    lptr++;
                    rptr = lptr + (int)winLen;
                    cout << "��������ƶ���" << lptr << ", �����Ҳ��ƶ���" << rptr << endl;
                    cout << "�հ�ʱ��: " << rt - sendTime[(int)h1.ack] << "ms" << endl;
                    if (lptr == maxPackge) {
                        wmtx.unlock();
                        mtx.unlock();
                        return;
                    }
                }
                else if (lptr - (int)h1.ack < 0) {
                    cout << endl;
                    change_win(lptr - (int)h1.ack + 1);
                    cout << "senptr�� " << senptr;
                    senptr = (int)h1.ack + 1;
                    cout << " �ƶ���" << senptr << endl;
                    lptr = (int)h1.ack;
                    rptr = lptr + (int)winLen;
                    cout << "��������ƶ���" << lptr << ", �����Ҳ��ƶ���" << rptr << endl;
                    cout << "�հ�ʱ��: " << rt - sendTime[(int)h1.ack] << "ms" << endl;
                }
                if (sendTime[(int)h1.ack + 1] != 0) {
                    if (rt - sendTime[(int)h1.ack + 1] > outTime) {
                        cout << "Out time: " << rt - sendTime[(int)h1.ack + 1] << endl;
                        cout << "-----------------------out-time------------------------" << endl;
                        cout << "ssthresh��" << ssth;
                        ssth = max(2, winLen / 2);
                        cout << "��Ϊ" << ssth << endl;
                        winLen = 1;
                        cout << "winLen���1" << endl;
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
                cout << "senptr�� " << senptr;
                senptr = lptr;
                cout << " �ƶ���" << senptr << endl;
            }
            wmtx.unlock();
            mtx.unlock();
        }
    }

    delete[]recvBuf;
}

int Send_Message(SOCKET clientSocket) {
    // �Զ����Ʒ�ʽ���ļ�
    ifstream fin(send_file, ifstream::in | ios::binary);
    if (!fin) {
        cout << "Error: cannot open file!" << endl;
        return -1;
    }
    fin.seekg(0, fin.end);		// ָ�붨λ���ļ�β
    int length = (int)fin.tellg();	// ��ȡ�ļ���С���ֽڣ�
    maxPackge = length / maxSize;
    if (length % maxSize != 0) {
        maxPackge++;
    }
    sendTime = new clock_t[maxPackge + 1];
    for (int i = 0; i < maxPackge; i++) {
        sendTime[i] = 0;
    }
    fin.seekg(0, fin.beg);		// ָ�붨λ���ļ�ͷ
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
            // ������Ϣͷ
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
            // data��ŵ��Ƕ���Ķ��������ݣ�sentLen���ѷ��͵ĳ���
            memcpy(sendBuf + sizeof(h), data + senptr * 8000, h.len);
            // ����У���
            h.checkSum = checkSumVerify((u_short*)sendBuf, sizeof(h) + h.len);
            memcpy(sendBuf, &h, sizeof(h));
            // ������Ϣ
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
    // �ȴ�������Ϣ
    while (true) {
        // �յ���Ϣ��Ҫ��֤У��ͼ����к�
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
            else {	// ����ش�
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

    // �Զ����Ʒ�ʽд���ļ�
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
