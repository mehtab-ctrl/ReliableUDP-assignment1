#include <cstdio>
#include <stdint.h>
#include <cstdint>
#include "Net.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <chrono>
#include <unistd.h>
#include <algorithm>

using namespace std;
using namespace net;

const int ServerPort = 30000;
const int ClientPort = 30001;
const int ProtocolId = 0x11223344;
const float TimeOut = 10.0f;
const int PacketSize = 1024;

unsigned int ComputeCRC32(const vector<unsigned char>& data);

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <server_ip> <file_path>" << endl;
        return 1;
    }

    string serverIp = argv[1];
    string filePath = argv[2];

    if (!InitializeSockets()) {
        cerr << "Failed to initialize sockets" << endl;
        return 1;
    }

    ReliableConnection client(ProtocolId, TimeOut);

    if (!client.Start(ClientPort)) {
        cerr << "Failed to start client on port " << ClientPort << endl;
        return 1;
    }

    int a, b, c, d;
    if (sscanf(serverIp.c_str(), "%d.%d.%d.%d", &a, &b, &c, &d) != 4) {
        cerr << "Invalid IP address format!" << endl;
        return 1;
    }
    Address serverAddress(a, b, c, d, ServerPort);

    client.Connect(serverAddress);
    cout << "Connecting to server at " << serverIp << "..." << endl;

    ifstream inputFile(filePath, ios::binary);
    if (!inputFile) {
        cerr << "Failed to open file: " << filePath << endl;
        return 1;
    }

    inputFile.seekg(0, ios::end);
    size_t fileSize = inputFile.tellg();
    inputFile.seekg(0, ios::beg);

    vector<unsigned char> fileData(fileSize);
    inputFile.read(reinterpret_cast<char*>(fileData.data()), fileSize);
    inputFile.close();

    unsigned int crc32Checksum = ComputeCRC32(fileData);
    cout << "Sending file: " << filePath << " (Size: " << fileSize << " bytes, CRC32: " << crc32Checksum << ")" << endl;

    auto startTime = chrono::high_resolution_clock::now();

    unsigned char metadata[sizeof(size_t) + sizeof(unsigned int)];
    memcpy(metadata, &fileSize, sizeof(size_t));
    memcpy(metadata + sizeof(size_t), &crc32Checksum, sizeof(unsigned int));
    client.SendPacket(metadata, sizeof(metadata));

    size_t bytesSent = 0;
    while (bytesSent < fileSize) {
        size_t chunkSize = min(static_cast<size_t>(PacketSize), fileSize - bytesSent);
        client.SendPacket(&fileData[bytesSent], chunkSize);
        bytesSent += chunkSize;
        cout << "Sent " << bytesSent << " of " << fileSize << " bytes..." << endl;
        usleep(10000);
    }

    unsigned char ack[4];
    int ackBytes = client.ReceivePacket(ack, sizeof(ack));
    if (ackBytes > 0 && memcmp(ack, "DONE", 4) == 0) {
        cout << "Acknowledgment received from server!" << endl;
    }
    else {
        cerr << "Warning: No acknowledgment received from server!" << endl;
    }

    auto endTime = chrono::high_resolution_clock::now();
    double transferTime = chrono::duration<double>(endTime - startTime).count();
    double transferSpeed = (fileSize * 8.0) / (transferTime * 1e6);

    cout << "File transfer complete!" << endl;
    cout << "Transfer time: " << transferTime << " seconds" << endl;
    cout << "Transfer speed: " << transferSpeed << " Mbps" << endl;

    ShutdownSockets();
    return 0;
}