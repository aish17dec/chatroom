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

/* Always append '\n' so peer's RecvLine() returns */
void DME::sendLine(const std::string &line)
{
    std::string out = line;
    if (out.empty() || out.back() != '\n')
        out.push_back('\n');
    SendAll(m_peerFd, out.c_str(), out.size());
    LogLine("SEND %s", out.c_str());
}

/* Handle incoming REQ/REP/REL (called by peer thread) */
void DME::handleRaMessage(const std::string &msg)
{
    std::istringstream iss(msg);
    std::string type;
    iss >> type;

    std::unique_lock<std::mutex> lk(m_mutex);

    if (type == "REQ")
    {
        int t, fromId;
        iss >> t >> fromId;

        // Lamport update
        m_lamportTs = std::max(m_lamportTs, t) + 1;

        bool shouldDefer = false;

        // Defer if:
        //  - we are in CS, or
        //  - we are requesting and our request has priority (lower ts or tie-breaker by id)
        if (m_inCs || (m_requesting && (m_reqTs < t || (m_reqTs == t && m_selfId < fromId))))
        {
            shouldDefer = true;
            m_deferReply = true;
            LogLine("DEFER to %d (ts=%d)", fromId, t);
        }

        if (!shouldDefer)
        {
            sendLine("REP " + std::to_string(m_selfId));
            LogLine("SEND REP to %d", fromId);
        }
    }
    else if (type == "REP")
    {
        int fromId;
        iss >> fromId;
        m_peerReplied = true;
        LogLine("RECV REP from %d", fromId);
        m_cv.notify_all();
    }
    else if (type == "REL")
    {
        int fromId;
        iss >> fromId;
        LogLine("RECV REL from %d", fromId);

        if (m_deferReply)
        {
            sendLine("REP " + std::to_string(m_selfId));
            LogLine("SEND deferred REP to %d", fromId);
            m_deferReply = false;
        }
    }
    // else: ignore malformed
}

/* Request the critical section (block until REP or timeout) */
bool DME::requestCs()
{
    std::unique_lock<std::mutex> lk(m_mutex);

    m_requesting = true;
    m_inCs = false;
    m_peerReplied = false;

    m_lamportTs++;
    m_reqTs = m_lamportTs;

    sendLine("REQ " + std::to_string(m_reqTs) + " " + std::to_string(m_selfId));
    LogLine("SEND REQ ts=%d", m_reqTs);

    // Wait until peer replies (or timeout -> false)
    while (!m_peerReplied)
    {
        if (m_cv.wait_for(lk, std::chrono::seconds(10)) == std::cv_status::timeout)
        {
            LogLine("TIMEOUT waiting for REP");
            m_requesting = false;
            return false;
        }
    }

    // Enter CS
    m_requesting = false;
    m_inCs = true;
    LogLine("ENTER CS");
    return true;
}

/* Release the critical section */
void DME::releaseCs()
{
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!m_inCs)
        return;

    m_inCs = false;
    m_lamportTs++;
    sendLine("REL " + std::to_string(m_selfId));
    LogLine("SEND REL");
}
