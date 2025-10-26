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

void DME::sendLine(const std::string &line)
{
    std::string out = line;
    if (out.empty() || out.back() != '\n')
        out.push_back('\n');
    SendAll(m_peerFd, out.c_str(), out.size());
    LogLine("SEND %s", out.c_str());
}

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
        m_lamportTs = std::max(m_lamportTs, t) + 1;

        bool defer = false;
        if (m_inCs || (m_requesting && (m_reqTs < t || (m_reqTs == t && m_selfId < fromId))))
        {
            defer = true;
            m_deferReply = true;
        }

        if (!defer)
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
            m_deferReply = false;
        }
    }
}

bool DME::requestCs()
{
    std::unique_lock<std::mutex> lk(m_mutex);

    m_requesting = true;
    m_inCs = false;
    m_peerReplied = false;

    m_lamportTs++;
    m_reqTs = m_lamportTs;

    sendLine("REQ " + std::to_string(m_reqTs) + " " + std::to_string(m_selfId));

    while (!m_peerReplied)
    {
        if (m_cv.wait_for(lk, std::chrono::seconds(10)) == std::cv_status::timeout)
        {
            LogLine("TIMEOUT waiting for REP");
            m_requesting = false;
            return false;
        }
    }

    m_requesting = false;
    m_inCs = true;
    LogLine("ENTER CS");
    return true;
}

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
