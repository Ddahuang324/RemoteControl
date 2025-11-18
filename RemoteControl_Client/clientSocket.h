#pragma once
#include "pch.h"
#include "framework.h"
#include "Enities.h"
#include <string>
#include <memory>
#include <stdexcept>
#include <ws2tcpip.h>
#include <vector>
#include <atomic>


#define BUFFER_SIZE (4 * 1024 * 1024) // 4MB buffer to accommodate large PNG frames



class Cpacket {
public:
    Cpacket() :sHead(0), nLength(0), sCmd(0), sSum(0) {}
    Cpacket(WORD nCmd, const BYTE* pData, size_t nSize) {
        sHead = 0xFEFF;
        nLength = nSize + 2 + 2;//���ݳ���=����+������+У���
        sCmd = nCmd;

        if (nSize > 0) {
            strData.resize(nSize);
            memcpy((void*)strData.c_str(), pData, nSize);
        }
        else {
            strData.clear();
        }
        sSum = 0;
        for (size_t j = 0; j < strData.size(); j++) {//����У���
            sSum += BYTE(strData[j]) & 0xFF;
        }
    }
    Cpacket(const Cpacket& pkt) {
        sHead = pkt.sHead;
        nLength = pkt.nLength;
        sCmd = pkt.sCmd;
        strData = pkt.strData;
        sSum = pkt.sSum;
    }
    Cpacket(const BYTE* pData, size_t& nSize) {
        // 初始化所有成员，防止解析失败时留下未定义值
        sHead = 0;
        nLength = 0;
        sCmd = 0;
        sSum = 0;
        strData.clear();

        size_t  i = 0;

        for (; i < nSize; i++) {
            if (*(WORD*)((char*)pData + i) == 0xFEFF) {
                sHead = *(WORD*)((char*)pData + i);
                i += 2;//�����Ѿ���ȡ�İ�ͷ����ֹ��ȡ����
                break;
            }
        }//������ͷ

        if (i + 4 + 2 + 2 > nSize) {//�߽��飬����ȷ���ӵ�ǰƫ�� i ��ʼ�����ݻ����� pData ���������㹻�ֽ������������Ĺ̶��ֶ�
            nSize = 0;//�õ���0���ֽڣ���������������ǰ����ܳ��ȣ�������õ����ֽ���
            return;
        }//���û���ҵ���ͷ������ʧ�ܣ�ֱ�ӷ���

        nLength = *(DWORD*)((char*)pData + i); i += 4;//���ݳ���
        sCmd = *(WORD*)((char*)pData + i); i += 2;//������
        if (i + (nLength - 2 - 2) + 2 > nSize) {//�߽��飬����ȷ���ӵ�ǰƫ�� i ��ʼ�����ݻ����� pData ���������㹻�ֽ������������������ֶκ�У����ֶ�
            nSize = 0;//�õ���0���ֽڣ���������������ǰ����ܳ��ȣ�������õ����ֽ���
            return;
        }

        if (nLength > 4) {//���ݳ�������Ҫ����4�����ܰ��������ֺ�У���
            strData.resize(nLength - 2 - 2);
            memcpy((void*)strData.c_str(), (char*)pData + i, nLength - 2 - 2);
            i += nLength - 2 - 2;//����
        }
        sSum = *(WORD*)((char*)pData + i); i += 2;//У���
        WORD sum = 0;
        for (size_t j = 0; j < strData.size(); j++) {//����У���
            sum += BYTE(strData[j]) & 0xFF;
        }
        if (sum == sSum) {//У�����ȷ
            nSize = i;
            return;
        }
        nSize = 0;//�õ���0���ֽڣ�����ʧ��
    }
    ~Cpacket() {}
    Cpacket& operator=(const Cpacket& pkt) {
        if (this != &pkt) {
            sHead = pkt.sHead;
            nLength = pkt.nLength;
            sCmd = pkt.sCmd;
            strData = pkt.strData;
            sSum = pkt.sSum;
        }
        return *this;
    }

    int Size() {//�����ݴ�С
        return nLength + 2 + 4;
    }
    const char* Data() {//������ָ��
        strOut.resize(nLength + 6);
        BYTE* pData = (BYTE*)strOut.c_str();
        *(WORD*)pData = sHead; pData += 2;
        *(DWORD*)pData = nLength; pData += 4;
        *(WORD*)pData = sCmd; pData += 2;
        memcpy(pData, strData.c_str(), strData.size()); pData += strData.size();
        *(WORD*)pData = sSum;

        return strOut.c_str();
    }
public:
    WORD sHead;//��ͷ,�̶�λFEFF
    DWORD nLength;//���ݳ���
    WORD sCmd;//������
    std::string strData;//����
    WORD sSum;//У���

    std::string strOut;//�������ݣ����ڵ���
};

typedef struct MouseEvent {
    MouseEvent() :nAction(0), nButton(-1) {
        ptXY.x = 0;
        ptXY.y = 0;
    }
    WORD nAction;//点击 移动 双击
    WORD nButton;//鼠标按键 左/右/中 键
    POINT ptXY;//坐标
}MOUSEEVENT, * PMOUSEEVENT;



inline std::string GetErrInfo(int nError);


class CClientSocket
{
private:
    CClientSocket(const CClientSocket& ss) {
        m_serv = ss.m_serv;
    }
    CClientSocket& operator=(const CClientSocket& ss) = default;
    
    CClientSocket() {
        m_serv = INVALID_SOCKET;

        if (InitSocketEnv() == false) {
            // Initialization failed
            throw std::runtime_error("Socket environment initialization failed");
        }
        m_Buffer.resize(BUFFER_SIZE);
        m_hScreenViewWnd = NULL;

    }
    ~CClientSocket() {
        closesocket(m_serv);
        WSACleanup();
    };

    bool InitSocketEnv() {
        WSADATA data;
        if (WSAStartup(MAKEWORD(1, 1), &data) != 0) {
            return false;
        }
        return true;
    };
    std::vector<char> m_Buffer;
    SOCKET m_serv;
    Cpacket m_packet;
    HWND m_hScreenViewWnd;

public:

    static void CleanUp() {
        WSACleanup();
    }
    void SetScreenViewWnd(HWND hWnd);
    static CClientSocket& GetInstance() {
        static CClientSocket instance;
        return instance;
    }

    bool initSocket(const std::string& ip, int port) {
        if(m_serv != INVALID_SOCKET){
        closesocket(m_serv);
        }
        m_serv = socket(PF_INET, SOCK_STREAM, 0);
        //У��
        if (m_serv == INVALID_SOCKET) {
            return false;
        }

        sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        if (inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr) != 1) {
            return false;
        }
        serv_addr.sin_port = htons(port);

    if (connect(m_serv, (SOCKADDR*)&serv_addr, sizeof(SOCKADDR)) == SOCKET_ERROR) {
        closesocket(m_serv);
        m_serv = INVALID_SOCKET;
        return false;
    }        return true;

    }

    

    int DealCommand(int timeoutMs = -1, std::atomic<bool>* stopFlag = nullptr) {
        if (m_serv == INVALID_SOCKET) return -1;
        
        auto Buffer = std::make_unique < char[]>(BUFFER_SIZE);
        if(Buffer == nullptr){
			TRACE("Buffer is nullptr\n");
            return -2;
		}
        memset(Buffer.get(), 0, BUFFER_SIZE);
        
        size_t index = 0;
        int consecutiveErrors = 0;
        const int MAX_CONSECUTIVE_ERRORS = 5;
        
        while (true) {
            if (stopFlag && !stopFlag->load(std::memory_order_acquire)) {
                delete[] Buffer.release();
                return -1;
            }
            
            // 检查缓冲区是否已满
            if (index >= BUFFER_SIZE - 1) {
                char dbg[256];
                _snprintf_s(dbg, _countof(dbg), _TRUNCATE, "DealCommand: Buffer overflow! Index: %zu, Buffer size: %d\n", index, BUFFER_SIZE);
                OutputDebugStringA(dbg);
                delete[] Buffer.release();
                return -3;
            }
            
            if (timeoutMs >= 0) {
                fd_set readSet;
                FD_ZERO(&readSet);
                FD_SET(m_serv, &readSet);
                timeval tv;
                tv.tv_sec = static_cast<long>(timeoutMs / 1000);
                tv.tv_usec = static_cast<long>((timeoutMs % 1000) * 1000);
                int ready = select(0, &readSet, NULL, NULL, &tv);
                if (ready == 0) {
                    continue;
                }
                if (ready == SOCKET_ERROR) {
                    delete[] Buffer.release();
                    return -1;
                }
            }
            
            int len = recv(m_serv, Buffer.get() + index, BUFFER_SIZE - index, 0);
            if (len <= 0) {
                if (len == 0) {
                    // 连接关闭
                    char dbg[256];
                    _snprintf_s(dbg, _countof(dbg), _TRUNCATE, "DealCommand: Connection closed by peer\n");
                    OutputDebugStringA(dbg);
                } else {
                    // 接收错误
                    int error = WSAGetLastError();
                    char dbg[256];
                    _snprintf_s(dbg, _countof(dbg), _TRUNCATE, "DealCommand: recv error: %d\n", error);
                    OutputDebugStringA(dbg);
                    
                    consecutiveErrors++;
                    if (consecutiveErrors >= MAX_CONSECUTIVE_ERRORS) {
                        _snprintf_s(dbg, _countof(dbg), _TRUNCATE, "DealCommand: Too many consecutive errors, giving up\n");
                        OutputDebugStringA(dbg);
                        delete[] Buffer.release();
                        return -1;
                    }
                    continue; // 继续尝试接收
                }
				delete[] Buffer.release();
                return -1;
            }
            
            consecutiveErrors = 0; // 重置错误计数
            index += len;
            
            // 处理缓冲区中的数据包
            size_t processed = 0;
            while (processed < index) {
                size_t remaining = index - processed;
                size_t packetSize = remaining;
                
                // 尝试解析数据包
                m_packet = Cpacket(reinterpret_cast<const BYTE*>(Buffer.get() + processed), packetSize);
                
                if (packetSize > 0) {
                    // 成功解析数据包
                    processed += packetSize;
                    
                    // If this is a screen data packet and a window is registered, post it asynchronously
                    if (m_packet.sCmd == SCREEN_DATA_PACKET) {
                        size_t dataLen = m_packet.strData.size();
                        if (dataLen > 0) {
                            char* pBuf = new char[dataLen];
                            memcpy(pBuf, m_packet.strData.data(), dataLen);
                            // Diagnostic: log reception and destination window
                            char dbg[256];
                            _snprintf_s(dbg, _countof(dbg), _TRUNCATE, "DealCommand: received SCREEN_DATA_PACKET size=%zu, hwnd=0x%p\n", dataLen, m_hScreenViewWnd);
                            OutputDebugStringA(dbg);
                            if (m_hScreenViewWnd && ::IsWindow(m_hScreenViewWnd)) {
                                ::PostMessage(m_hScreenViewWnd, WM_USER + 200, (WPARAM)dataLen, (LPARAM)pBuf);
                            } else {
                                delete[] pBuf;
                            }
                        }
                        // 继续处理下一个数据包
                        continue;
                    }
                    
                    // 普通命令：返回命令ID
                    if (processed < index) {
                        // 还有剩余数据，移动缓冲区
                        memmove(Buffer.get(), Buffer.get() + processed, index - processed);
                        index -= processed;
                    } else {
                        index = 0;
                    }
                    delete[] Buffer.release();
                    return m_packet.sCmd;
                } else {
                    // 无法解析完整数据包，需要更多数据
                    break;
                }
            }
            
            // 移动未处理的数据到缓冲区开头
            if (processed > 0) {
                memmove(Buffer.get(), Buffer.get() + processed, index - processed);
                index -= processed;
            }
        }
        
        delete[] Buffer.release();
        return -1;
    }

    bool Send(const void* pData, size_t size) {
        if (m_serv == INVALID_SOCKET) return false;
        return send(m_serv, (const char*)pData, size, 0) > 0;
    }
    bool Send(Cpacket& pkt) {
        if (m_serv == INVALID_SOCKET) return false;
        return send(m_serv, pkt.Data(), pkt.Size(), 0) > 0;
    }

    bool GetFilePath(std::string& strPath) {
        if ((m_packet.sCmd == 2) || (m_packet.sCmd == 3) || (m_packet.sCmd == 4)) {
            strPath = m_packet.strData;
            return true;
        }
        return false;
    }

    bool GetMouseEvent(MOUSEEVENT& mouse) {
        if (m_packet.sCmd == 5) {
            memcpy(&mouse, m_packet.strData.c_str(), sizeof(MOUSEEVENT));
            return true;
        }
        return false;
    }

   Cpacket& GetPacket() {
        return m_packet;
    }

    void CloseSocket() {
        if (m_serv != INVALID_SOCKET) {
            closesocket(m_serv);
            m_serv = INVALID_SOCKET;
        }
    }
};
