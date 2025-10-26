#include "DME.hpp"
#include "../common/Logger.hpp"
#include "../common/NetUtils.hpp"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <thread>

/*
 *  DME.cpp
 *  --------
 *  Implements Ricart–Agrawala logic.
 */

DME::DME(int selfId, int peerId, int peerFd)
    : m_selfId(selfId), m_peerId(peerId), m_peerFd(peerFd), m_lamport(0), m_state(State::RELEASED), m_hasReply(false)
{
}

/*
 *  sendRaMessage()
 *  ----------------
 *  Sends a single Ricart–Agrawala message over the TCP socket.
 *  Format: "TYPE timestamp senderId\n"
 */
void DME::sendRaMessage(const std::string &type)
{
    std::string msg = type + " " + std::to_string(m_lamport) + " " + std::to_string(m_selfId);
    SendLine(m_peerFd, msg);
    LogLine("SEND %s ts=%d id=%d", type.c_str(), m_lamport, m_selfId);
}

/*
 *  sendReply()
 *  ------------
 *  Sends a REPLY message to the peer.
 */
void DME::sendReply()
{
    m_lamport++;
    sendRaMessage("REP");
}

/*
 *  flushDeferredReplies()
 *  -----------------------
 *  Sends REPLY messages for all deferred peer requests
 *  (called after exiting the critical section).
 */
void DME::flushDeferredReplies()
{
    while (!m_deferredRequests.empty())
    {
        m_deferredRequests.pop();
        sendReply();
    }
}

/*
 *  requestCs()
 *  ------------
 *  Attempts to enter the critical section.
 *  Steps:
 *    1. Increment Lamport clock.
 *    2. Send REQ to peer.
 *    3. Wait for REP or timeout.
 *    4. If granted, state = HELD.
 */
bool DME::requestCs()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lamport++;
    m_state = State::WANTED;
    m_hasReply = false;
    sendRaMessage("REQ");

    // Wait up to 5 seconds for reply
    for (int i = 0; i < 5000; ++i)
    {
        if (m_hasReply.load())
        {
            m_state = State::HELD;
            LogLine("ENTER CS (HELD)");
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    LogLine("TIMEOUT waiting for REP from peer (may be down)");
    return false;
}

/*
 *  releaseCs()
 *  ------------
 *  Called after leaving the critical section.
 *  Sends REL message and replies to deferred requests.
 */
void DME::releaseCs()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state = State::RELEASED;
    m_lamport++;
    sendRaMessage("REL");
    LogLine("EXIT CS (RELEASED)");
    flushDeferredReplies();
}

/*
 *  handleRaMessage()
 *  -----------------
 *  Handles incoming RA messages from the peer.
 *  Updates Lamport clock and acts according to message type.
 */
void DME::handleRaMessage(const std::string &line)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::istringstream iss(line);
    std::string type;
    int ts, senderId;
    if (!(iss >> type >> ts >> senderId))
        return;

    // Synchronise Lamport clock
    m_lamport = std::max(m_lamport, ts) + 1;

    if (type == "REQ")
    {
        // Determine if we should reply immediately or defer
        bool replyNow = (m_state == State::RELEASED) ||
                        (m_state == State::WANTED && (ts < m_lamport || (ts == m_lamport && senderId < m_selfId)));

        LogLine("RECV REQ ts=%d id=%d (L=%d) => %s", ts, senderId, m_lamport, replyNow ? "REPLY" : "DEFER");

        if (replyNow)
            sendReply();
        else
            m_deferredRequests.push({ ts, senderId });
    }
    else if (type == "REP")
    {
        LogLine("RECV REP from id=%d (L=%d)", senderId, m_lamport);
        m_hasReply.store(true);
    }
    else if (type == "REL")
    {
        LogLine("RECV REL from id=%d (L=%d)", senderId, m_lamport);
        // No special handling in 2-node RA
    }
}
