#include "PieceManager.hpp"

#include <arpa/inet.h>
#include <unistd.h>

#include <chrono>
#include <cmath>
#include <iomanip>
#include <thread>

#ifndef DOWNLOADS_PATH
#define DOWNLOADS_PATH "./downloads/"
#endif

#ifndef AMOUNT_HASH_SYMBOLS
#define AMOUNT_HASH_SYMBOLS 50
#endif

PieceManager::PieceManager(const TorrentFileParser& tfp) : tfp(tfp), Pieces(initialisePieces())
{
    downloadedFile.open(DOWNLOADS_PATH + tfp.getFileName(), std::ios::binary | std::ios::out);
    // downloadedFile.seekp(tfp.getLengthOne() - 1);
    // downloadedFile.write("", 1);
}

PieceManager::~PieceManager()
{
    downloadedFile.close();
}

static std::vector<std::string> splitPiecesHashes(const std::string& pieces)
{
    std::vector<std::string> res;

    int piecesCount = pieces.size() / 20;  // Length of one HASH = 20
    res.reserve(piecesCount);
    for (int i = 0; i < piecesCount; ++i)
        res.push_back(pieces.substr(i * 20, 20));
    return res;
}

std::vector<std::unique_ptr<Piece> > PieceManager::initialisePieces()
{
    std::vector<std::unique_ptr<Piece> > res;
    std::vector<std::string> pieceHashes(splitPiecesHashes(tfp.getPieces()));
    totalPieces = pieceHashes.size();

    long long totalLength = tfp.getLengthOne();

    long long pieceLength = tfp.getPieceLength();
    int blockCount        = std::ceil(pieceLength / BLOCK_SIZE);

    for (int i = 0; i < totalPieces; ++i)
    {
        if (i == totalPieces - 1)
        {
            if ((totalLength % pieceLength) != 0)
                blockCount = std::ceil(static_cast<double>((totalLength % pieceLength)) / BLOCK_SIZE);
            // std::cerr << "blockCount = " << blockCount << std::endl;
            res.push_back(std::move(std::make_unique<Piece>(blockCount, totalLength, pieceHashes[i], true)));
        }
        else
            res.push_back(std::move(std::make_unique<Piece>(blockCount, totalLength, pieceHashes[i], false)));
    }
    return res;
}

void PieceManager::addPeerBitfield(const std::string& peerPeerId, const std::string& payload)
{
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<bool> bits;
    bits.resize(payload.size() * 8);
    for (int i = 0; i < payload.size(); ++i)
    {
        unsigned char byte = payload[i];
        for (int j = 0; j < 8; ++j)
        {
            int bitPos   = i * 8 + j;
            bits[bitPos] = (byte >> (7 - j)) & 1;
        }
    }
    peerBitfield.insert({peerPeerId, bits});
}

bool PieceManager::isComplete()
{
    std::lock_guard<std::mutex> lock(mutex);
    // std::cerr << "totalDownloaded = " << totalDownloaded << " and totalPieces = " << totalPieces << std::endl;
    return totalDownloaded == totalPieces;
}

const std::string PieceManager::requestPiece(const std::string& peerPeerId)
{
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<bool> bitfield(peerBitfield[peerPeerId]);
    for (int i = 0; i < totalPieces; ++i)
    {
        if (Pieces[i] != nullptr && bitfield[i] && Pieces[i]->haveMissingBlock())
        {
            std::string blockInfo(Pieces[i].get()->requestBlock());
            return intToBytes(htonl(i)) + blockInfo;
        }
    }
    throw std::runtime_error("No piece from this peer");
}

void PieceManager::blockReceived(int index, int begin, const std::string& blockStr)
{
    std::lock_guard<std::mutex> lock(mutex);
    Piece* ptr = Pieces[index].get();
    if (ptr)
    {
        ptr->fillData(begin, blockStr);
        std::string dataToFile;
        if (ptr->isFull())
        {
            if (ptr->isHashChecked(dataToFile))
            {
                writeDataToFile(index, dataToFile);
                Pieces[index] = nullptr;
                ++totalDownloaded;
            }
            else
            {
                ptr->resetAllBlocksToMissing();
                // throw std::runtime_error("Hashes doesn't match");
            }
        }
    }
}

void PieceManager::writeDataToFile(int index, const std::string& dataToFile)
{
    downloadedFile.seekp(index * tfp.getPieceLength());
    downloadedFile << dataToFile;
}

void PieceManager::addToBitfield(const std::string& peerPeerId, const std::string& payload)
{
    std::lock_guard<std::mutex> lock(mutex);
    int bitPos                       = getIntFromStr(payload);
    peerBitfield[peerPeerId][bitPos] = true;
}

void PieceManager::display(int n, long long fileSize, int lengthOfSize)
{
    std::cout << " [" << std::setw(3) << n << "%]" << '[' << std::setw(lengthOfSize) << std::fixed << std::setprecision(1)
              << fileSize / 100.0 * n / 1'048'576 << "Mb]" << '[';
    int i = 0;
    for (; i < n * AMOUNT_HASH_SYMBOLS / 100.0; ++i)
        std::cout << '#';
    for (; i < AMOUNT_HASH_SYMBOLS; ++i)
        std::cout << ' ';
    std::cout << ']';

    auto elapsedTime = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - startTime).count();

    int milliseconds = static_cast<int>(elapsedTime);
    int seconds      = milliseconds / 1000;
    int minutes      = seconds / 60;
    int hours        = minutes / 60;
    char prevFill    = std::cout.fill();
    std::cout << "[" << std::setfill('0') << std::setw(2) << hours % 24 << ":" << std::setfill('0') << std::setw(2)
              << minutes % 60 << ":" << std::setfill('0') << std::setw(2) << seconds % 60 << "]" << '\r' << std::flush;
    std::cout.fill(prevFill);
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

static int getLength(long long num)
{
    int i = 1;
    while (num / 10)
    {
        ++i;
        num /= 10;
    }
    return i;
}

void PieceManager::trackProgress()
{
    std::string fileName(tfp.getFileName());
    long long fileSize = tfp.getLengthOne();
    int lengthOfSize   = getLength(fileSize / 1'048'576) + 2;
    std::string firstLastLine(AMOUNT_HASH_SYMBOLS + 23 + lengthOfSize, '-');
    std::cout << firstLastLine << "\nDownload: " << tfp.getFileName() << std::endl;
    while (!isComplete())
    {
        mutex.lock();
        int downloadPercent = 100 * totalDownloaded / totalPieces;
        mutex.unlock();
        display(downloadPercent, fileSize, lengthOfSize);
    }
    std::cout << " [100%]"
              << "[" << std::fixed << std::setprecision(1) << fileSize * 1.0 / 1'048'576 << "Mb]" << '[';
    for (int i = 0; i < AMOUNT_HASH_SYMBOLS; ++i)
        std::cout << '#';
    std::cout << ']' << "\nDownload complete!\n" << firstLastLine << std::endl;
}
