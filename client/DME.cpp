#include "DME.hpp"
#include "../common/Logger.hpp"
#include "../common/NetUtils.hpp"

#include <algorithm>
#include <chrono>
#include <sstream>
#include <thread>

DME::DME(int selfId, int peerId, int peerFd) : m_selfId(selfId), m_peerId(peerId), m_peerFd(peerFd)
{
}

/**
 * @brief Send a Ricart–Agrawala message to the peer.
 * Ensures each line ends with '\n' so the peer’s RecvLine() returns correctly.
 */
void DME::sendLine(const std::string &line)
{
    std::string out = line;
    if (out.empty() || out.back() != '\n')
        out.push_back('\n');

    SendAll(m_peerFd, out.c_str(), out.size());
    LogLine("[RA] Sent message: %s", out.c_str());
}

/**
 * @brief Handle incoming Ricart–Agrawala messages.
 * Supports REQUEST, REPLY, and RELEASE message types.
 */
void DME::handleRaMessage(const std::string &msg)
{
    std::istringstream iss(msg);
    std::string type;
    iss >> type;

    std::unique_lock<std::mutex> lk(m_mutex);

    // ----------------------------------------------------------
    // Handle REQUEST message
    // ----------------------------------------------------------
    if (type == "REQUEST")
    {
        int t, fromId;
        iss >> t >> fromId;
        m_lamportTs = std::max(m_lamportTs, t) + 1;

        // Defer reply if currently in CS or has higher priority
        if (m_inCs || (m_requesting && (m_reqTs < t || (m_reqTs == t && m_selfId < fromId))))
        {
            m_deferReply = true;
            LogLine("[RA] REQUEST from %d (ts=%d) deferred — "
                    "currently in CS or has higher priority",
                    fromId, t);
        }
        else
        {
            sendLine("REPLY " + std::to_string(m_selfId));
            LogLine("[RA] REQUEST from %d (ts=%d) accepted — "
                    "sent REPLY (permission granted)",
                    fromId, t);
        }
    }

    // ----------------------------------------------------------
    // Handle REPLY message
    // ----------------------------------------------------------
    else if (type == "REPLY")
    {
        int fromId;
        iss >> fromId;
        m_peerReplied = true;
        LogLine("[RA] Received REPLY (permission granted) from peer %d", fromId);
        m_cv.notify_all();
    }

    // ----------------------------------------------------------
    // Handle RELEASE message
    // ----------------------------------------------------------
    else if (type == "RELEASE")
    {
        int fromId;
        iss >> fromId;
        LogLine("[RA] Received RELEASE from %d — peer exited CS", fromId);

        if (m_deferReply)
        {
            sendLine("REPLY " + std::to_string(m_selfId));
            m_deferReply = false;
            LogLine("[RA] Sent deferred REPLY to %d after receiving RELEASE", fromId);
        }
    }
}
bool DME::requestCriticalSection()
{
    std::unique_lock<std::mutex> lk(m_mutex);

    m_requesting = true;
    m_inCs = false;
    m_peerReplied = false;

    m_lamportTs++;
    m_reqTs = m_lamportTs;

    sendLine("REQUEST " + std::to_string(m_reqTs) + " " + std::to_string(m_selfId));
    LogLine("[RA] REQUEST sent to peer %d (ts=%d)", m_peerId, m_reqTs);

    while (!m_peerReplied)
    {
        if (m_cv.wait_for(lk, std::chrono::seconds(10)) == std::cv_status::timeout)
        {
            LogLine("[RA] TIMEOUT waiting for REPLY from peer %d", m_peerId);
            m_requesting = false;
            return false;
        }
    }

    m_requesting = false;
    m_inCs = true;
    LogLine("[RA] ENTER critical section (permission received)");
    return true;
}

void DME::releaseCriticalSection()
{
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!m_inCs)
        return;

    m_inCs = false;
    m_lamportTs++;

    sendLine("RELEASE " + std::to_string(m_selfId));
    LogLine("[RA] RELEASE sent — leaving critical section");
}
