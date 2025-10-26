#include "DME.hpp"
#include "../common/Logger.hpp"
#include "../common/NetUtils.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
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
    std::cout << "[RA] Sent message: " << out.c_str() << "\n";
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

    std::cout << "Message Received : " << msg << "\n";
    std::cout << "Extracted Type: " << type << "\n";

    // ----------------------------------------------------------
    // Handle REQUEST message
    // ----------------------------------------------------------
    if (type == "REQUEST")
    {
        int t, fromId;
        iss >> t >> fromId;
        std::cout << "Extracted timestamp from message: " << t << ", extracted peer Node Id: " << fromId << "\n";
        m_lamportTs = std::max(m_lamportTs, t) + 1;
        std::cout << "Calculated Lamport timestamp to: " << m_lamportTs << "\n";

        std::cout << "[DME][IN] Received REQUEST for Critical Section from Node: " << fromId << " with Lamport ts=" << t
                  << ")\n";

        // Defer reply if currently in CS or has higher priority
        std::cout << "Current State - InCS: " << m_inCriticalSection << ", Requesting: " << m_requesting
                  << ", ReqTs: " << m_reqTs << "\n";
        if (m_inCriticalSection || (m_requesting && (m_reqTs < t || (m_reqTs == t && m_selfId < fromId))))
        {
            m_deferReply = true;
            std::cout << "[RA] REQUEST from:" << fromId << "ts=" << t
                      << "deferred — currently in CS or has higher priority\n";
        }
        else
        {
            sendLine("REPLY " + std::to_string(m_selfId));
            std::cout << "[RA][OUT] REQUEST from peer node " << fromId << " (timestamp=" << t << ") accepted — "
                      << "sent REPLY (permission granted)\n";
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
        std::cout << "[RA] Received REPLY (permission granted) from peer " << fromId << "\n";
        m_cv.notify_all();
    }

    // ----------------------------------------------------------
    // Handle RELEASE message
    // ----------------------------------------------------------
    else if (type == "RELEASE")
    {
        int fromId;
        iss >> fromId;
        std::cout << "[RA] Received RELEASE from " << fromId << " — peer exited CS \n";

        if (m_deferReply)
        {
            sendLine("REPLY " + std::to_string(m_selfId));
            m_deferReply = false;
            std::cout << "[RA] Sent deferred REPLY to " << fromId << "  after receiving RELEASE"
                      << "\n";
        }
    }
}
bool DME::requestCriticalSection()
{
    std::unique_lock<std::mutex> lk(m_mutex);

    m_requesting = true;
    m_inCriticalSection = false;
    m_peerReplied = false;

    m_lamportTs++;
    m_reqTs = m_lamportTs;

    sendLine("REQUEST " + std::to_string(m_reqTs) + " " + std::to_string(m_selfId));
    std::cout << "[RA] REQUEST sent to peer ID: " << m_peerId << " request ID:" << m_reqTs;

    while (!m_peerReplied)
    {
        if (m_cv.wait_for(lk, std::chrono::seconds(10)) == std::cv_status::timeout)
        {
            std::cout << "[RA] TIMEOUT waiting for REPLY from peer " << m_peerId << "\n";
            m_requesting = false;
            return false;
        }
    }

    m_requesting = false;
    m_inCriticalSection = true;
    std::cout << "[RA] ENTER critical section (permission received)"
              << "\n";
    return true;
}

void DME::releaseCriticalSection()
{
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!m_inCriticalSection)
        return;

    m_inCriticalSection = false;
    m_lamportTs++;

    sendLine("RELEASE " + std::to_string(m_selfId));
    std::cout << "[RA] RELEASE sent — leaving critical section"
              << "\n";
}
