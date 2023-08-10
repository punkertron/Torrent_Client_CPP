#include "TorrentClient.hpp"

#include <curl/curl.h>

#include "PeerConnection.hpp"
#include "PeerRetriever.hpp"
#include "PeersQueue.hpp"
#include "PieceManager.hpp"
#include "spdlog/spdlog.h"

#ifndef PORT
#define PORT 8080
#endif

#ifndef PEER_ID
#define PEER_ID "-TR1000-000123456789"  // should use random
#endif

#ifndef THREAD_NUM
#define THREAD_NUM 10
#endif

TorrentClient::TorrentClient(const char* filePath) : tfp(filePath)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    if (!tfp.IsSingle())
    {
        std::cerr << "No mitiple files right now!" << std::endl;
        std::abort();
    }
}

TorrentClient::~TorrentClient()
{
    curl_global_cleanup();
}

void TorrentClient::run()
{
    PieceManager pieceManager(tfp);
    PeerRetriever p(std::string(PEER_ID), PORT, tfp, 0);
    std::vector<std::pair<std::string, long long> > peers(p.getPeers());
    for (auto peer : peers)
        peersQueue.push_back(peer);

    // This thread displays download status
    std::thread thread(&PieceManager::trackProgress, std::ref(pieceManager));
    threads.push_back(std::move(thread));

    // These threads download file
    for (size_t i = 0; i < THREAD_NUM && i < peers.size() + 1; ++i)
    {
        std::thread thread(&PeerConnection::start,
                           PeerConnection(tfp.getInfoHash(), std::string(PEER_ID), &pieceManager, &peersQueue));
        threads.push_back(std::move(thread));
    }

    // in while loop we can update peers. Peers could be empty if curl failed
    auto lastUpdate = std::chrono::steady_clock::now();
    while (true)
    {
        if (pieceManager.isComplete())
            break;
        auto diff = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - lastUpdate).count();

        if ((diff / 1000) > p.getInterval() || !peersQueue.hasFreePeers())
        {
            lastUpdate = std::chrono::steady_clock::now();
            p          = PeerRetriever(std::string(PEER_ID), PORT, tfp, 0);
            peers      = p.getPeers();
            for (auto peer : peers)
                peersQueue.push_back(peer);
            SPDLOG_INFO("PeersQueue.size() = {}", peersQueue.size());
        }
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    for (auto& thr : threads)
    {
        if (thr.joinable())
            thr.join();
    }
}
