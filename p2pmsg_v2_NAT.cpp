#include <cstddef>
#include <cstring>
#include <iostream>
#include <cstdlib>
#include <string>
#ifdef _WIN32
    #define _WIN32_WINNT 0x0601
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define close(s) closesocket(s) 
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>
#include <cerrno>
#include <ctime>

class Server {
  private:
    int srv = -1;
    int client = -1;
  public:
    class Listener {
      public:
        int startListenerClient(int port, const char* ip){
        while(true){
          int s = socket(AF_INET, SOCK_STREAM, 0);
          if(s < 0){ std::cerr << "socket: " << std::strerror(errno) << "\n"; return 1; }
          sockaddr_in addr{};
          addr.sin_family = AF_INET;
          addr.sin_port   = htons(port);
          inet_pton(AF_INET, ip, &addr.sin_addr);
          if(connect(s, (sockaddr*)&addr, sizeof(addr)) < 0){
            std::cerr << "connect: " << std::strerror(errno) << "\n";
            close(s); return 1;
          }
          char buf[1024];
          std::cout << "Enter msg: ";
          if(!std::cin.getline(buf, sizeof(buf))){ close(s); break; }
          if(std::strcmp(buf, "ex") == 0){ close(s); break; }
          ssize_t sent = send(s, buf, std::strlen(buf), 0);
          if(sent < 0){ std::cerr << "send: " << std::strerror(errno) << "\n"; close(s); break; }
          close(s);
        }
        return 0;
      }
    };
    int serverStart(int port, int backlog){
      std::system("clear");
      std::cout << "TCP Server is Start\n";
      srv = socket(AF_INET, SOCK_STREAM, 0);
      if(srv < 0){ std::cerr << "socket: " << std::strerror(errno) << "\n"; return -1; }
      int opt = 1;
      setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_port = htons(port);
      addr.sin_addr.s_addr = INADDR_ANY;
      if(bind(srv, (sockaddr*)&addr, sizeof(addr)) < 0){ std::cerr << "bind: " << std::strerror(errno) << "\n"; return -1; }
      if(listen(srv, backlog) < 0){ std::cerr << "listen: " << std::strerror(errno) << "\n"; return -1; }
      return 0;
    }
    int startClient(){
      client = accept(srv, nullptr, nullptr);
      if(client < 0){ std::cerr << "accept: " << std::strerror(errno) << "\n"; return -1; }
      return 0;
    }
    void closeServer(){ if(srv >= 0){ close(srv); srv = -1; } }
    void closeClient(){ if(client >= 0){ close(client); client = -1; } }
    int recvPkg(char* buf, size_t size){
      if(client < 0) return -1;
      int n = recv(client, buf, size - 1, 0);
      if(n > 0) buf[n] = '\0';
      return n;
    }
    int sendPkg(const char* buf, size_t len){
      if(client < 0) return -1;
      size_t total = 0;
      while(total < len){
        ssize_t sent = send(client, buf + total, len - total, 0);
        if(sent <= 0) return -1;
        total += sent;
      }
      return (int)total;
    }
    Listener listener;
    ~Server(){ closeClient(); closeServer(); }
};

class Rendezvous {
    int sock = -1;
    sockaddr_in addrA{};
    sockaddr_in addrB{};
    bool hasA = false, hasB = false;
public:
    int start(uint16_t port) {
        sock = socket(AF_INET, SOCK_DGRAM, 0);   // UDP
        if (sock < 0) { std::cerr << "socket: " << std::strerror(errno) << "\n"; return -1; }
        int opt = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;
        if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
            std::cerr << "bind: " << std::strerror(errno) << "\n"; return -1;
        }
        std::cout << "Rendezvous-сервер listen UDP " << port << "\n";
        return 0;
    }

    void run() {
        char buf[256];
        while (true) {
            sockaddr_in from{};
            socklen_t fl = sizeof(from);
            int n = recvfrom(sock, buf, sizeof(buf) - 1, 0, (sockaddr*)&from, &fl);
            if (n <= 0) continue;
            buf[n] = '\0';

            char who = 0;
            if (std::sscanf(buf, "REGISTER %c", &who) != 1) continue;

            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &from.sin_addr, ip, sizeof(ip));
            std::cout << "REGISTER " << who << " from " << ip << ":" << ntohs(from.sin_port) << "\n";

            if (who == 'A') { addrA = from; hasA = true; }
            else if (who == 'B') { addrB = from; hasB = true; }
            else continue;

            if (hasA && hasB) {
                char msgA[64];
                char ipB[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &addrB.sin_addr, ipB, sizeof(ipB));
                std::snprintf(msgA, sizeof(msgA), "PEER %s %u", ipB, ntohs(addrB.sin_port));
                sendto(sock, msgA, std::strlen(msgA), 0, (sockaddr*)&addrA, sizeof(addrA));

                char msgB[64];
                char ipA[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &addrA.sin_addr, ipA, sizeof(ipA));
                std::snprintf(msgB, sizeof(msgB), "PEER %s %u", ipA, ntohs(addrA.sin_port));
                sendto(sock, msgB, std::strlen(msgB), 0, (sockaddr*)&addrB, sizeof(addrB));

                std::cout << "Synch A(" << ipA << ":" << ntohs(addrA.sin_port)
                          << ") and B(" << ipB << ":" << ntohs(addrB.sin_port) << ")\n";
                hasA = hasB = false;
            }
        }
    }
    ~Rendezvous() { if (sock >= 0) close(sock); }
};

class NATHolePunching {
    int sock = -1;
    sockaddr_in rendezvous{};
    sockaddr_in peerPublic{};
    bool punched = false;
    char myId = 0;

    int recvWithTimeout(char* buf, size_t size, sockaddr_in* from, int ms) {
        fd_set rfds; FD_ZERO(&rfds); FD_SET(sock, &rfds);
        timeval tv{ ms / 1000, (ms % 1000) * 1000 };
        int r = select(sock + 1, &rfds, nullptr, nullptr, &tv);
        if (r <= 0) return r;
        socklen_t fl = sizeof(*from);
        return recvfrom(sock, buf, size, 0, (sockaddr*)from, &fl);
    }

public:
    void startMes() {
        std::cout << "1. Client A\n"
                  << "2. Client B\n"
                  << "3. Rendezvous-server (UDP 9100)\n"
                  << "4. TCP-test\n"
                  << "e. Exit\n"
                  << "Choose: ";
    }

    int runAs(char id, const char* rendezvousIp = "127.0.0.1", uint16_t rendezvousPort = 9100) {
        myId = id;
        std::system("clear");

        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) { std::cerr << "socket: " << std::strerror(errno) << "\n"; return 1; }

        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_port = 0;
        local.sin_addr.s_addr = INADDR_ANY;
        if (bind(sock, (sockaddr*)&local, sizeof(local)) < 0) {
            std::cerr << "bind: " << std::strerror(errno) << "\n"; return 1;
        }

        rendezvous.sin_family = AF_INET;
        rendezvous.sin_port = htons(rendezvousPort);
        inet_pton(AF_INET, rendezvousIp, &rendezvous.sin_addr);

        std::cout << "Client " << myId << ". Register on "
                  << rendezvousIp << ":" << rendezvousPort << " ...\n";

        char reg[32];
        std::snprintf(reg, sizeof(reg), "REGISTER %c", myId);
        sendto(sock, reg, std::strlen(reg), 0, (sockaddr*)&rendezvous, sizeof(rendezvous));

        char buf[256];
        sockaddr_in from{};
        int n = recvWithTimeout(buf, sizeof(buf) - 1, &from, 30000);
        if (n <= 0) {
            std::cerr << "PEER.ERR\n";
            return 1;
        }
        buf[n] = '\0';

        char peerIp[INET_ADDRSTRLEN]; unsigned peerPort = 0;
        if (std::sscanf(buf, "PEER %15s %u", peerIp, &peerPort) != 2) {
            std::cerr << "ANSW.UNEXP: " << buf << "\n";
            return 1;
        }
        peerPublic.sin_family = AF_INET;
        peerPublic.sin_port = htons(peerPort);
        inet_pton(AF_INET, peerIp, &peerPublic.sin_addr);
        std::cout << "PEER: " << peerIp << ":" << peerPort
                  << "\nNAT.Crack.\n";

        if (!punchHole()) {
            std::cerr << "NAT.ERR\n";
            return 1;
        }
        std::cout << "P2P initialize.\n";

        messageLoop();
        return 0;
    }

    bool punchHole() {
        const char* punch = "PUNCH";
        for (int i = 0; i < 100; ++i) {
            sendto(sock, punch, 5, 0, (sockaddr*)&peerPublic, sizeof(peerPublic));

            char buf[64]; sockaddr_in from{};
            int n = recvWithTimeout(buf, sizeof(buf), &from, 100);
            if (n == 5 && std::memcmp(buf, "PUNCH", 5) == 0) {
                punched = true;
                return true;
            }

            if (n > 0 && from.sin_addr.s_addr == peerPublic.sin_addr.s_addr) {
                peerPublic.sin_port = from.sin_port;
            }
        }
        return false;
    }

    void messageLoop() {
        time_t lastHeartbeat = time(nullptr);
        std::string input;
        while (true) {
            fd_set rfds; FD_ZERO(&rfds);
            FD_SET(0, &rfds);       // stdin
            FD_SET(sock, &rfds);    // UDP
            timeval tv{0, 200000};  // 200 мс
            int r = select(std::max(sock, 0) + 1, &rfds, nullptr, nullptr, &tv);

            if (r > 0 && FD_ISSET(sock, &rfds)) {
                char buf[1024]; sockaddr_in from{};
                socklen_t fl = sizeof(from);
                int n = recvfrom(sock, buf, sizeof(buf) - 1, 0, (sockaddr*)&from, &fl);
                if (n > 0) {
                    buf[n] = '\0';
                    if (std::strcmp(buf, "HEARTBEAT") == 0) {
                    } else {
                        std::cout << "\n[PEER]: " << buf << "\n> " << std::flush;
                    }
                }
            }

            if (r > 0 && FD_ISSET(0, &rfds)) {
                if (!std::getline(std::cin, input)) break;
                if (input == "exit") {
                    const char* bye = "BYE";
                    sendto(sock, bye, 3, 0, (sockaddr*)&peerPublic, sizeof(peerPublic));
                    break;
                }
                if (!input.empty()) {
                    sendto(sock, input.c_str(), input.size(), 0,
                           (sockaddr*)&peerPublic, sizeof(peerPublic));
                }
                std::cout << "> " << std::flush;
            }

            time_t now = time(nullptr);
            if (now - lastHeartbeat >= 15) {
                const char* hb = "HEARTBEAT";
                sendto(sock, hb, std::strlen(hb), 0,
                       (sockaddr*)&peerPublic, sizeof(peerPublic));
                lastHeartbeat = now;
            }
        }
    }

    ~NATHolePunching() { if (sock >= 0) close(sock); }
};

int main() {
    #ifdef _WIN32
      WSADATA wsaData;
      if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return 1;
      }
    #endif
    NATHolePunching nat;
    Server tcpServer;
    std::string choice;

    while (true) {
        nat.startMes();
        if (!std::getline(std::cin, choice)) break;

        if (choice == "1") {
            nat.runAs('A');
        } else if (choice == "2") {
            nat.runAs('B');
        } else if (choice == "3") {
            Rendezvous rv;
            if (rv.start(9100) == 0) rv.run();
        } else if (choice == "4") {
            std::cout << "1.Server  2.Client\n";
            std::getline(std::cin, choice);
            if (choice == "1") {
                tcpServer.serverStart(9000, 8);
                while (true) {
                    if (tcpServer.startClient() != 0) continue;
                    char buf[1024];
                    int n = tcpServer.recvPkg(buf, sizeof(buf));
                    if (n > 0) {
                        std::cout << "Accept: ";
                        std::cout.write(buf, n);
                        std::cout << "\n";
                        tcpServer.sendPkg(buf, n);
                    }
                    tcpServer.closeClient();
                }
            } else if (choice == "2") {
                tcpServer.listener.startListenerClient(9000, "127.0.0.1");
            }
        } else if (choice == "e") {
            break;
        } else {
            std::cout << "Enter 1, 2, 3, 4 or 'e'\n";
        }
    }
    return 0;
    #ifdef _WIN32
      WSACleanup();
    #endif
}