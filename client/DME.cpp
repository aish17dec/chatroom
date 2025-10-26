#include "DME.hpp"
#include "../common/NetUtils.hpp"
#include "../debug.hpp"

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
    std::cout << Timestamp() << " [DME][RA] Sent message: " << out.c_str() << std::endl;
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

    std::cout << Timestamp() << " [DME] Message Received : " << msg << std::endl;
    std::cout << Timestamp() << " [DME] Extracted Type: " << type << std::endl;

    // ----------------------------------------------------------
    // Handle REQUEST message
    // ----------------------------------------------------------
    if (type == "REQUEST")
    {
        int t, fromId;
        iss >> t >> fromId;
        std::cout << Timestamp() << " [DME] Extracted timestamp from message: " << t
                  << ", extracted peer Node Id: " << fromId << std::endl;
        m_lamportTs = std::max(m_lamportTs, t) + 1;
        std::cout << Timestamp() << " [DME] Calculated Lamport timestamp to: " << m_lamportTs << std::endl;

        std::cout << Timestamp() << " [DME][IN] Received REQUEST for Critical Section from Node: " << fromId
                  << " with Lamport ts=" << t << ")" << std::endl;

        std::cout << Timestamp() << " [DME] Current State - InCS: " << m_inCriticalSection
                  << ", Requesting: " << m_requesting << ", ReqTs: " << m_reqTs << std::endl;

        if (m_inCriticalSection || (m_requesting && (m_reqTs < t || (m_reqTs == t && m_selfId < fromId))))
        {
            m_deferReply = true;
            std::cout << Timestamp() << " [DME][RA] REQUEST from:" << fromId << " ts=" << t
                      << " deferred — currently in CS or has higher priority" << std::endl;
        }
        else
        {
            sendLine("REPLY " + std::to_string(m_selfId));
            std::cout << Timestamp() << " [DME][RA][OUT] REQUEST from peer node " << fromId << " (timestamp=" << t
                      << ") accepted — sent REPLY (permission granted)" << std::endl;
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
        std::cout << Timestamp() << " [DME][RA] Received REPLY (permission granted) from peer " << fromId << std::endl;
        m_cv.notify_all();
    }

    // ----------------------------------------------------------
    // Handle RELEASE message
    // ----------------------------------------------------------
    else if (type == "RELEASE")
    {
        int fromId;
        iss >> fromId;
        std::cout << Timestamp() << " [DME][RA] Received RELEASE from " << fromId << " — peer exited CS" << std::endl;

        if (m_deferReply)
        {
            sendLine("REPLY " + std::to_string(m_selfId));
            m_deferReply = false;
            std::cout << Timestamp() << " [DME][RA] Sent deferred REPLY to " << fromId << " after receiving RELEASE"
                      << std::endl;
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
    std::cout << Timestamp() << " [DME][RA] REQUEST sent to peer ID: " << m_peerId << " request ID:" << m_reqTs
              << std::endl;

    while (!m_peerReplied)
    {
        if (m_cv.wait_for(lk, std::chrono::seconds(10)) == std::cv_status::timeout)
        {
            std::cerr << Timestamp() << " [DME][RA] TIMEOUT waiting for REPLY from peer " << m_peerId << std::endl;
            m_requesting = false;
            return false;
        }
    }

    m_requesting = false;
    m_inCriticalSection = true;
    std::cout << Timestamp() << " [DME][RA] ENTER critical section (permission received)" << std::endl;
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
    std::cout << Timestamp() << " [DME][RA] RELEASE sent — leaving critical section" << std::endl;
}
