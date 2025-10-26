#pragma once
#include <string>
#include <queue>
#include <mutex>
#include <atomic>

/*
 *  DME.hpp
 *  --------
 *  Implements Ricart–Agrawala Distributed Mutual Exclusion
 *  for two user nodes (peer-to-peer coordination).
 *
 *  States:
 *    RELEASED – not in CS and not requesting
 *    WANTED   – requested access, waiting for REPLY
 *    HELD     – currently in CS (critical section)
 *
 *  Messages:
 *    REQ <timestamp> <senderId>
 *    REP <timestamp> <senderId>
 *    REL <timestamp> <senderId>
 */

enum class State
{
    RELEASED,
    WANTED,
    HELD
};

// Represents a deferred request from another node.
struct Request
{
    int timestamp;
    int nodeId;
};

/*
 *  DME (Distributed Mutual Exclusion)
 *  ----------------------------------
 *  Handles the logic for acquiring and releasing the critical
 *  section using the Ricart–Agrawala algorithm.
 */
class DME
{
public:
    DME(int selfId, int peerId, int peerFd);

    // Requests to enter the critical section.
    // Returns true if granted, false if peer unresponsive.
    bool requestCs();

    // Releases the critical section and replies to deferred requests.
    void releaseCs();

    // Handles an incoming RA message (REQ / REP / REL).
    void handleRaMessage(const std::string& line);

    // Accessor for Lamport clock (for logging/debug).
    int lamportClock() const { return m_lamport; }

private:
    int m_selfId;                  // This node’s ID (1 or 2)
    int m_peerId;                  // Peer node’s ID
    int m_peerFd;                  // Connected socket to peer
    int m_lamport;                 // Lamport logical clock
    State m_state;                 // Current node state
    std::queue<Request> m_deferredRequests; // Deferred peer requests
    std::mutex m_mutex;            // Protect shared state
    std::atomic<bool> m_hasReply;  // True if peer has granted permission

    // Helper methods for sending RA protocol messages.
    void sendRaMessage(const std::string& type);
    void sendReply();
    void flushDeferredReplies();
};

