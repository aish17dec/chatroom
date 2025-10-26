#ifndef DME_HPP
#define DME_HPP

#include <condition_variable>
#include <mutex>
#include <string>

/**
 * @brief Implements the Ricart–Agrawala Distributed Mutual Exclusion (DME)
 *        algorithm for two-node coordination.
 */
class DME
{
  public:
    DME(int selfId, int peerId, int peerFd);

    bool requestCriticalSection(); // Request critical section
    void releaseCriticalSection(); // Release critical section
    void handleRaMessage(const std::string &line);

    int getSelfId() const
    {
        return m_selfId;
    }
    int getPeerId() const
    {
        return m_peerId;
    }

  private:
    void sendLine(const std::string &line);

    std::mutex m_mutex;
    std::condition_variable m_cv;

    const int m_selfId;
    const int m_peerId;
    const int m_peerFd;

    int m_lamportTs{ 0 };
    int m_reqTs{ 0 };
    bool m_requesting{ false };
    bool m_inCs{ false };
    bool m_peerReplied{ false };
    bool m_deferReply{ false };
};

#endif
