#include "pch.h"
#include "NetworkModel.h"
#include <chrono>
#include <utility>
#include <sstream>
#include <fstream>

static void NMLog(const char* msg) {
    try {
        std::ofstream ofs("network_model.log", std::ios::app);
        if (ofs) ofs << msg << std::endl;
    } catch (...) {}
}

NetworkModel::NetworkModel() {}

NetworkModel::~NetworkModel() {
    disconnect();
}

bool NetworkModel::connectToServer(const std::string& ip, uint16_t port) {
    // create infra socket instance on demand (use m_netRes)
    if (!m_netRes->socket) m_netRes->socket = std::make_unique<Socket>();
    if (!m_netRes->socket->connectToServer(ip, port)) {
        m_netRes->socket.reset();
        return false;
    }
    // start recv thread
    m_netRes->running.store(true);
    m_netRes->recvThread = std::thread(&NetworkModel::RecvThreadLoop, this);
    if (m_netRes->statusCb) m_netRes->statusCb(true);
    return true;
}

void NetworkModel::disconnect() {
    m_netRes->running.store(false);
    if (m_netRes->recvThread.joinable()) m_netRes->recvThread.join();
    if (m_netRes->socket) {
        m_netRes->socket->CloseSocket();
        m_netRes->socket.reset();
    }
    {
        std::lock_guard<std::mutex> lk(m_netBuffer->queueMutex);
        m_netBuffer->packetQueue.clear();
    }
    if (m_netRes->statusCb) m_netRes->statusCb(false);
}

bool NetworkModel::sendPacket(const Packet& pkt) {
    Packet cp = ToInternal(pkt);
    if (!m_netRes->socket) return false;
    return m_netRes->socket->SendPacket(cp);
}

std::optional<Packet> NetworkModel::recvPacket() {
    std::lock_guard<std::mutex> lk(m_netBuffer->queueMutex);
    if (m_netBuffer->packetQueue.empty()) return std::nullopt;
    Packet p = std::move(m_netBuffer->packetQueue.front());
    m_netBuffer->packetQueue.pop_front();
    return p;
}

std::optional<Packet> NetworkModel::getNextPacketBlocking(int timeoutMs) {
    std::unique_lock<std::mutex> lk(m_netBuffer->queueMutex);
    if (timeoutMs == 0) {
        m_netBuffer->queueCv.wait(lk, [this]{ return !m_netBuffer->packetQueue.empty() || !m_netRes->running.load(); });
    } else {
        if (!m_netBuffer->queueCv.wait_for(lk, std::chrono::milliseconds(timeoutMs), [this]{ return !m_netBuffer->packetQueue.empty() || !m_netRes->running.load(); })) {
            return std::nullopt;
        }
    }
    if (m_netBuffer->packetQueue.empty()) return std::nullopt;
    Packet p = std::move(m_netBuffer->packetQueue.front());
    m_netBuffer->packetQueue.pop_front();
    return p;
}

std::optional<Packet> NetworkModel::getLatestPacket() {//Redundant
    std::lock_guard<std::mutex> lk(m_netBuffer->queueMutex);
    if (m_netBuffer->packetQueue.empty()) return std::nullopt;
    Packet latest = std::move(m_netBuffer->packetQueue.back());
    m_netBuffer->packetQueue.clear();
    return latest;
}

void NetworkModel::clearRecvBuffer() {
    if (m_netRes->socket) m_netRes->socket->ClearRecvBuffer();
}

void NetworkModel::clearPacketsByCmd(WORD cmd) {
    std::lock_guard<std::mutex> lk(m_netBuffer->queueMutex);
    if (m_netBuffer->packetQueue.empty()) return;
    std::deque<Packet> newq;
    for (auto &p : m_netBuffer->packetQueue) {
        if (p.sCmd != cmd) newq.push_back(p);
    }
    m_netBuffer->packetQueue.swap(newq);
}

void NetworkModel::clearAllPackets() {
    std::lock_guard<std::mutex> lk(m_netBuffer->queueMutex);
    m_netBuffer->packetQueue.clear();
}

void NetworkModel::setOnPacketReceived(PacketCb cb) {
    m_netRes->packetCb = std::move(cb);
}

void NetworkModel::setOnStatusChanged(StatusCb cb) {
    m_netRes->statusCb = std::move(cb);
}

void NetworkModel::RecvThreadLoop() {
    NMLog("RecvThreadLoop started");
    while (m_netRes->running.load()) {
        // Try to receive one packet from low-level socket
        if (!m_netRes->socket) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        auto opt = m_netRes->socket->RecvPacket();
        if (opt) {
            Packet p = ToPublic(*opt);
            
            // 添加日志
            std::ostringstream ss;
            ss << "RecvThreadLoop: received packet cmd=" << p.sCmd << " dataSize=" << p.data.size();
            NMLog(ss.str().c_str());
            
            {
                std::lock_guard<std::mutex> lk(m_netBuffer->queueMutex);
                m_netBuffer->packetQueue.push_back(p);
                while (m_netBuffer->packetQueue.size() > m_netBuffer->maxQueue) m_netBuffer->packetQueue.pop_front();
            }
            m_netBuffer->queueCv.notify_one();
            if (m_netRes->packetCb) {
                NMLog("RecvThreadLoop: calling packetCb");
                m_netRes->packetCb(p);
                NMLog("RecvThreadLoop: packetCb returned");
            }
            continue; // try again immediately
        }

        // No packet right now: sleep a bit to avoid busy loop
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    NMLog("RecvThreadLoop ended");
}

Packet NetworkModel::ToPublic(const Packet& p) {
    Packet out;
    out.sCmd = p.sCmd;
    out.data = p.data;
    return out;
}

Packet NetworkModel::ToInternal(const Packet& p) {
    Packet out;
    out.sCmd = p.sCmd;
    out.data = p.data;
    return out;
}
