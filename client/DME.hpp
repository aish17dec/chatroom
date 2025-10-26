#ifndef DME_HPP
#define DME_HPP

#include <condition_variable>
#include <mutex>
#include <string>

/*
 * Ricart–Agrawala (two-node) DME
 * Messages:
 *   REQ <ts> <fromId>
 *   REP <fromId>
 *   REL <fromId>
 *
 * Usage:
 *   - requestCs() blocks until REP from peer (or times out -> false)
 *   - releaseCs() sends REL and flushes any deferred reply
 *   - handleRaMessage(line) processes REQ/REP/REL from peer thread
 */
class DME
{
  public:
    DME(int selfId, int peerId, int peerFd);

    bool requestCs(); // acquire (blocks until REP or timeout)
    void releaseCs(); // release (sends REL; flush deferred REP)
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
    void sendLine(const std::string &line); // ensures trailing '\n'

    // state protected by m_mutex
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
