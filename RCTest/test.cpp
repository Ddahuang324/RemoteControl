#include "pch.h"
#include "network/ServerSocket.h"
#include "core/Enities.h"
#include "core/screenCapture.h"
#include <iostream>
#include <string>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <thread>
#include <chrono>
#include <fstream>
#include <atomic>



class FakeClient {
public:
    FakeClient(const std::string& serverIP, int port)
        : serverIP(serverIP), port(port), sock(INVALID_SOCKET) {
        WSAData wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
    }

    ~FakeClient() {
        if (sock != INVALID_SOCKET) {
            closesocket(sock);
        }
        WSACleanup();
    }

    bool connectToServer() {
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            std::cerr << "Socket creation failed" << std::endl;
            return false;
        }

        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr);

        if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "Connect failed with error: " << WSAGetLastError() << std::endl;
            closesocket(sock);
            sock = INVALID_SOCKET;
            return false;
        }

        std::cout << "Connected to server!" << std::endl;
        return true;
    }

    void sendPacket(const Cpacket& packet) {
        if (sock == INVALID_SOCKET) {
            std::cerr << "Not connected to server." << std::endl;
            return;
        }

        auto buffer = packet.SerializePacket();
        send(sock, reinterpret_cast<const char*>(buffer.data()), static_cast<int>(buffer.size()), 0);
    }

    void receiveLoop() {
        // New stateful receiver: assemble TCP stream into packets using Cpacket::DeserializePacket
        char readBuffer[4096];
        while (true) {
            int bytesReceived = recv(sock, readBuffer, static_cast<int>(sizeof(readBuffer)), 0);
            if (bytesReceived <= 0) {
                if (bytesReceived == 0) std::cout << "Server closed the connection." << std::endl;
                else std::cerr << "Recv failed with error: " << WSAGetLastError() << std::endl;
                break;
            }

            // append new data to receive buffer
            m_recvBuffer.insert(m_recvBuffer.end(), reinterpret_cast<BYTE*>(readBuffer), reinterpret_cast<BYTE*>(readBuffer) + bytesReceived);

            // Try to parse one or more Cpacket from m_recvBuffer
            while (true) {
                size_t bytesConsumed = 0;
                auto packetOpt = Cpacket::DeserializePacket(m_recvBuffer, bytesConsumed);
                if (!packetOpt) {
                    // If DeserializePacket suggests some bytes can be discarded, remove them
                    if (bytesConsumed > 0 && bytesConsumed <= m_recvBuffer.size()) {
                        m_recvBuffer.erase(m_recvBuffer.begin(), m_recvBuffer.begin() + bytesConsumed);
                        continue; // try parsing again
                    }
                    break; // need more data
                }

                auto packet = *packetOpt;
                // Consume parsed bytes
                if (bytesConsumed <= m_recvBuffer.size()) {
                    m_recvBuffer.erase(m_recvBuffer.begin(), m_recvBuffer.begin() + bytesConsumed);
                } else {
                    m_recvBuffer.clear();
                }

                // Process the complete Cpacket
                ProcessPacket(packet);
            }
        }
    }

    void ProcessPacket(const Cpacket& packet) {
        if (packet.sCmd == static_cast<WORD>(CMD::CMD_SCREEN_CAPTURE)) {
            try {
                int idx = ++recvCounter;
                std::filesystem::path baseDir = std::filesystem::current_path() / "captured_frames";
                std::error_code ec;
                std::filesystem::create_directories(baseDir, ec);

                using namespace std::chrono;
                auto now = system_clock::now();
                auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
                std::time_t t = system_clock::to_time_t(now);
                std::tm tm;
                localtime_s(&tm, &t);
                std::ostringstream oss;
                oss << std::put_time(&tm, "%Y%m%d_%H%M%S") << '_' << std::setfill('0') << std::setw(3) << ms.count();
                oss << '_' << idx << ".png";
                std::filesystem::path outPath = baseDir / oss.str();

                // Protocol: first 4*sizeof(int) bytes are x,y,w,h; following bytes are PNG data when present
                if (packet.data.size() >= 4 * sizeof(int)) {
                    const BYTE* raw = packet.data.data();
                    int x = *reinterpret_cast<const int*>(raw);
                    int y = *reinterpret_cast<const int*>(raw + 4);
                    int w = *reinterpret_cast<const int*>(raw + 8);
                    int h = *reinterpret_cast<const int*>(raw + 12);
                    size_t imgOffset = 4 * sizeof(int);
                    size_t imgSize = (packet.data.size() > imgOffset) ? (packet.data.size() - imgOffset) : 0;

                    // PNG signature check
                    const unsigned char png_sig[8] = {0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A};
                    bool looksLikePng = false;
                    if (imgSize >= 8) {
                        looksLikePng = true;
                        for (size_t i = 0; i < 8; ++i) {
                            if (packet.data[imgOffset + i] != png_sig[i]) { looksLikePng = false; break; }
                        }
                    }

                    if (looksLikePng) {
                        std::ofstream ofs(outPath, std::ios::binary);
                        if (ofs) {
                            ofs.write(reinterpret_cast<const char*>(packet.data.data() + imgOffset), static_cast<std::streamsize>(imgSize));
                            ofs.close();
                            std::cout << "Saved received PNG (cmd=" << packet.sCmd << ") roi=(" << x << "," << y << ") size=" << w << "x" << h << " -> " << std::filesystem::absolute(outPath).u8string() << " (" << imgSize << " bytes)" << std::endl;
                        } else {
                            std::cerr << "Failed to open file to save received PNG." << std::endl;
                        }
                    } else {
                        // No PNG payload (could be empty-diff placeholder). Save raw packet as .bin for inspection
                        std::filesystem::path binPath = outPath;
                        binPath.replace_extension(".bin");
                        std::ofstream ofs(binPath, std::ios::binary);
                        if (ofs) {
                            ofs.write(reinterpret_cast<const char*>(packet.data.data()), static_cast<std::streamsize>(packet.data.size()));
                            ofs.close();
                            std::cout << "Received non-PNG screen packet (cmd=" << packet.sCmd << ") saved as: " << std::filesystem::absolute(binPath).u8string() << " (" << packet.data.size() << " bytes)" << std::endl;
                        } else {
                            std::cerr << "Failed to open file to save received non-PNG packet." << std::endl;
                        }
                    }
                } else {
                    std::cerr << "Received screen packet too small to contain header." << std::endl;
                }
            } catch (const std::exception& ex) {
                std::cerr << "Exception saving received packet PNG: " << ex.what() << std::endl;
            }
        } else {
            std::cout << "Received packet cmd=" << packet.sCmd << ", data size=" << packet.data.size() << std::endl;
        }
    }

private:
    std::string serverIP;
    int port;
    SOCKET sock;
    std::atomic<int> recvCounter{0};
    std::vector<BYTE> m_recvBuffer;
};

// 简化的屏幕捕获：捕获指定时长（秒）内的屏幕并按时间戳保存到源文件同级的 captured_frames 文件夹
static void SimpleCapture5s(int durationSeconds = 5, int intervalMs = 200) {
    using namespace std::chrono;
    // 初始化 GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    try {
        // Use the current working directory (executable run location) so it's
        // easy to find the `captured_frames` folder when running the test.
        std::filesystem::path baseDir = std::filesystem::current_path() / "captured_frames";
        std::error_code ec;
        std::filesystem::create_directories(baseDir, ec);
        if (ec) {
            std::cerr << "Failed to create captured_frames dir: " << ec.message() << std::endl;
            return;
        }
        std::cout << "Capturing to directory: " << std::filesystem::absolute(baseDir).u8string() << std::endl;

        auto t0 = steady_clock::now();
        int index = 0;
        while (duration_cast<seconds>(steady_clock::now() - t0).count() < durationSeconds) {
            try {
                auto [imgPtr, pixels, w, h, bpp] = CaptureScreenImage();
                if (!imgPtr) {
                    std::cerr << "CaptureScreenImage returned null image" << std::endl;
                } else {
                    // 时间戳用于文件名（可排序）
                    auto now = system_clock::now();
                    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
                    std::time_t t = system_clock::to_time_t(now);
                    std::tm tm;
                    localtime_s(&tm, &t);
                    std::ostringstream oss;
                    oss << std::put_time(&tm, "%Y%m%d_%H%M%S") << '_' << std::setfill('0') << std::setw(3) << ms.count();
                    oss << '_' << index << ".png";
                    std::filesystem::path outPath = baseDir / oss.str();

                    // 保存为 PNG（CImage::Save 支持按照扩展名推断格式）
                    std::wstring wpath = outPath.wstring();
                    // Try saving by filename (letting the image class infer format
                    // from the extension). This is the most portable approach.
                    HRESULT hr = imgPtr->Save(wpath.c_str());
                    if (FAILED(hr)) {
                        std::cerr << "Failed to save captured image, HRESULT=0x" << std::hex << hr << std::dec << std::endl;
                    } else {
                        std::cout << "Saved frame: " << std::filesystem::absolute(outPath).u8string() << std::endl;
                    }
                }
            } catch (const std::exception& ex) {
                std::cerr << "Exception during capture: " << ex.what() << std::endl;
            }
            ++index;
            std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
        }
    } catch (const std::exception& ex) {
        std::cerr << "SimpleCapture5s failed: " << ex.what() << std::endl;
    }

    if (gdiplusToken) Gdiplus::GdiplusShutdown(gdiplusToken);
}

// Capture a fixed number of frames (更适合用于断电等测试场景)
static void SimpleCaptureNFrames(int frames /*=20*/, int intervalMs /*=200*/) {
    using namespace std::chrono;
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    try {
        std::filesystem::path baseDir = std::filesystem::current_path() / "captured_frames";
        std::error_code ec;
        std::filesystem::create_directories(baseDir, ec);
        if (ec) {
            std::cerr << "Failed to create captured_frames dir: " << ec.message() << std::endl;
            return;
        }
        std::cout << "Capturing to directory: " << std::filesystem::absolute(baseDir).u8string() << std::endl;

        int index = 0;
        while (index < frames) {
            try {
                auto [imgPtr, pixels, w, h, bpp] = CaptureScreenImage();
                if (!imgPtr) {
                    std::cerr << "CaptureScreenImage returned null image" << std::endl;
                } else {
                    auto now = system_clock::now();
                    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
                    std::time_t t = system_clock::to_time_t(now);
                    std::tm tm;
                    localtime_s(&tm, &t);
                    std::ostringstream oss;
                    oss << std::put_time(&tm, "%Y%m%d_%H%M%S") << '_' << std::setfill('0') << std::setw(3) << ms.count();
                    oss << '_' << index << ".png";
                    std::filesystem::path outPath = baseDir / oss.str();

                    std::wstring wpath = outPath.wstring();
                    HRESULT hr = imgPtr->Save(wpath.c_str());
                    if (FAILED(hr)) {
                        std::cerr << "Failed to save captured image, HRESULT=0x" << std::hex << hr << std::dec << std::endl;
                    } else {
                        std::cout << "Saved frame: " << std::filesystem::absolute(outPath).u8string() << std::endl;
                    }
                }
            } catch (const std::exception& ex) {
                std::cerr << "Exception during capture: " << ex.what() << std::endl;
            }
            ++index;
            std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
        }
    } catch (const std::exception& ex) {
        std::cerr << "SimpleCaptureNFrames failed: " << ex.what() << std::endl;
    }

    if (gdiplusToken) Gdiplus::GdiplusShutdown(gdiplusToken);
}

// Manual per-frame capture: capture one frame, save it, then等待用户按 Enter 继续
// 输入 "q" 然后 Enter 可提前退出。
static void SimpleCaptureManualNFrames(int frames /*=20*/) {
    using namespace std::chrono;
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    try {
        std::filesystem::path baseDir = std::filesystem::current_path() / "captured_frames";
        std::error_code ec;
        std::filesystem::create_directories(baseDir, ec);
        if (ec) {
            std::cerr << "Failed to create captured_frames dir: " << ec.message() << std::endl;
            return;
        }
        std::cout << "Manual capturing to directory: " << std::filesystem::absolute(baseDir).u8string() << std::endl;

        int index = 0;
        while (index < frames) {
            try {
                auto [imgPtr, pixels, w, h, bpp] = CaptureScreenImage();
                if (!imgPtr) {
                    std::cerr << "CaptureScreenImage returned null image" << std::endl;
                } else {
                    auto now = system_clock::now();
                    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
                    std::time_t t = system_clock::to_time_t(now);
                    std::tm tm;
                    localtime_s(&tm, &t);
                    std::ostringstream oss;
                    oss << std::put_time(&tm, "%Y%m%d_%H%M%S") << '_' << std::setfill('0') << std::setw(3) << ms.count();
                    oss << '_' << index << ".png";
                    std::filesystem::path outPath = baseDir / oss.str();

                    std::wstring wpath = outPath.wstring();
                    HRESULT hr = imgPtr->Save(wpath.c_str());
                    if (FAILED(hr)) {
                        std::cerr << "Failed to save captured image, HRESULT=0x" << std::hex << hr << std::dec << std::endl;
                    } else {
                        std::cout << "Saved frame: " << std::filesystem::absolute(outPath).u8string() << std::endl;
                    }
                }
            } catch (const std::exception& ex) {
                std::cerr << "Exception during capture: " << ex.what() << std::endl;
            }

            ++index;

            if (index >= frames) break;

            std::cout << "Press Enter to capture next frame (or type 'q' + Enter to quit): ";
            std::string cmd;
            std::getline(std::cin, cmd);
            if (!cmd.empty() && (cmd == "q" || cmd == "Q")) {
                std::cout << "Manual capture aborted by user." << std::endl;
                break;
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "SimpleCaptureManualNFrames failed: " << ex.what() << std::endl;
    }

    if (gdiplusToken) Gdiplus::GdiplusShutdown(gdiplusToken);
}

int main() {
    try {
        FakeClient client("127.0.0.1", 12345);
        if (!client.connectToServer()) {
            return 1;
        }

        // Start a separate thread to receive responses
        std::thread receiverThread(&FakeClient::receiveLoop, &client);
        receiverThread.detach();

        std::string line;
        while (true) {
            std::cout << "> ";
            std::getline(std::cin, line);
            if (line == "exit") {
                break;
            }

            if (line.empty()) {
                continue;
            }

            std::string command;
            std::string argument;
            size_t space_pos = line.find(' ');
            if (space_pos != std::string::npos) {
                command = line.substr(0, space_pos);
                argument = line.substr(space_pos + 1);
            } else {
                command = line;
            }

            if (command == "help") {
                std::cout << "--- Fake Client Commands ---\n"
                          << "help            - Show this help message\n"
                          << "exit            - Exit the client\n"
                          << "driver_info     - Request driver information\n"
                          << "dir_info <path> - Request directory information\n"
                          << "run_file <path> - Request to run a file\n"
                          << "download_file <path> - Request to download a file\n"
                          << "lock_machine    - Lock the machine\n"
                          << "unlock_machine  - Unlock the machine\n"
                          << "screen5         - Request screen diffs repeatedly for 5s (networked)\n"
                          << "capture20       - Manual 20-frame capture (press Enter per frame, 'q' to quit)\n"
                          << "--------------------------\n";
            } else if (command == "driver_info") {
                Cpacket packet(CMD::CMD_DRIVER_INFO, {});
                client.sendPacket(packet);
            } else if (command == "dir_info") {
                if (argument.empty()) {
                    std::cout << "Usage: dir_info <path>\n";
                } else {
                    std::vector<BYTE> data(argument.begin(), argument.end());
                    Cpacket packet(CMD::CMD_DIRECTORY_INFO, data);
                    client.sendPacket(packet);
                }
            } else if (command == "run_file") {
                if (argument.empty()) {
                    std::cout << "Usage: run_file <path>\n";
                } else {
                    std::vector<BYTE> data(argument.begin(), argument.end());
                    Cpacket packet(CMD::CMD_RUN_FILE, data);
                    client.sendPacket(packet);
                }
            } else if (command == "download_file") {
                if (argument.empty()) {
                    std::cout << "Usage: download_file <path>\n";
                } else {
                    std::vector<BYTE> data(argument.begin(), argument.end());
                    Cpacket packet(CMD::CMD_DOWNLOAD_FILE, data);
                    client.sendPacket(packet);
                }
            } else if (command == "lock_machine") {
                Cpacket packet(CMD::CMD_LOCK_MACHINE, {});
                client.sendPacket(packet);
            } else if (command == "unlock_machine") {
                Cpacket packet(CMD::CMD_UNLOCK_MACHINE, {});
                client.sendPacket(packet);
            // 新增：在 5 秒内重复请求屏幕 diff，用于验证 diff 算法随时间的输出大小变化
            } else if (command == "screen5") {
                std::cout << "Requesting screen diffs from server for 5 seconds..." << std::endl;
                using namespace std::chrono;
                int durationSeconds = 5;
                int intervalMs = 200;
                auto t0 = steady_clock::now();
                while (duration_cast<seconds>(steady_clock::now() - t0).count() < durationSeconds) {
                    Cpacket packet(CMD::CMD_SCREEN_CAPTURE, {});
                    client.sendPacket(packet);
                    std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
                }
            } else if (command == "capture20") {
                std::cout << "Starting manual capture of 20 frames..." << std::endl;
                SimpleCaptureManualNFrames(20);
            } else {
                std::cout << "Unknown command: " << command << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
