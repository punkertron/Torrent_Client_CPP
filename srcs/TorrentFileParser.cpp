#include "TorrentFileParser.hpp"

#include <fstream>
#include <stdexcept>

#include "spdlog/spdlog.h"
#include "utils.hpp"

TorrentFileParser::TorrentFileParser(const char* filePath) : SingleMultiFile(multiFile())
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open())
    {
        SPDLOG_CRITICAL("Can't open {}", filePath);
        throw std::runtime_error("Can't open torrent file");
    }

    try
    {
        auto data   = bencode::decode(file);
        auto dict   = std::get<bencode::dict>(data);
        announce    = std::get<bencode::string>(dict["announce"]);
        auto info   = std::get<bencode::dict>(dict["info"]);
        infoHash    = sha1(bencode::encode(info));
        pieceLength = std::get<bencode::integer>(info["piece length"]);
        pieces      = std::get<bencode::string>(info["pieces"]);

        try
        {
            auto announce_list = std::get<bencode::list>(dict["announce-list"]);
            fillAnnounceList(announce_list);
        }
        catch (...)
        {
            SPDLOG_INFO("No announce-list");
        }

        try
        {
            std::get<multiFile>(SingleMultiFile).files = std::get<bencode::list>(info["files"]);
        }
        catch (...)
        {
            SingleMultiFile                                = singleFile();
            isSingle                                       = true;
            std::get<singleFile>(SingleMultiFile).fileName = std::get<bencode::string>(info["name"]);
            std::get<singleFile>(SingleMultiFile).length   = std::get<bencode::integer>(info["length"]);
            file.close();
            return;
        }
        std::get<multiFile>(SingleMultiFile).dirName = std::get<bencode::string>(info["name"]);
        file.close();
    }

    catch (...)
    {
        file.close();
        SPDLOG_CRITICAL("Something bad with torrent file parsing");
        throw std::runtime_error("Something bad with torrent file parsing");
    }

    if (!IsSingle())
    {
        SPDLOG_ERROR("Attempt to download several files");
        throw std::runtime_error("No mitiple files right now!");
    }
}

const std::string& TorrentFileParser::getAnnounce() const
{
    return announce;
}

const long long& TorrentFileParser::getPieceLength() const
{
    return pieceLength;
}

const std::string& TorrentFileParser::getPieces() const
{
    return pieces;
}

bool TorrentFileParser::IsSingle() const
{
    return isSingle;
}

const std::string& TorrentFileParser::getFileName() const
{
    return std::get<singleFile>(SingleMultiFile).fileName;
}

long long TorrentFileParser::getLengthOne() const
{
    return std::get<singleFile>(SingleMultiFile).length;
}

const std::string& TorrentFileParser::getDirName() const
{
    return std::get<multiFile>(SingleMultiFile).dirName;
}

const bencode::list& TorrentFileParser::getFiles() const
{
    return std::get<multiFile>(SingleMultiFile).files;
}

const std::string& TorrentFileParser::getInfoHash() const
{
    return infoHash;
}

void TorrentFileParser::fillAnnounceList(const bencode::list& announce_list)
{
    for (const auto& ll : announce_list)
    {
        auto list = std::get<bencode::list>(ll);
        for (const auto& l : list)
            announceList.push_back(std::get<bencode::string>(l));
    }
}

size_t TorrentFileParser::getAnnounce_listSize() const
{
    return announceList.size();
}

const std::string& TorrentFileParser::getAnnounce_listI(size_t i) const
{
    return announceList[i];
}
