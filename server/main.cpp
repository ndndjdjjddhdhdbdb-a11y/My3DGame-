#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <csignal>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <algorithm>

namespace {
constexpr int kPort = 7777;
constexpr std::size_t kMaxPlayers = 20;
constexpr auto kTick = std::chrono::milliseconds(100);
constexpr char kWsGuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

struct Player {
    int id = 0;
    int socket = -1;
    std::string name = "Player";
    float x = 0.0f, y = 0.0f, z = 0.0f, yaw = 0.0f;
    std::mutex sendMutex;
};

std::atomic<bool> running{true};
std::mutex playersMutex;
std::unordered_map<int, std::shared_ptr<Player>> players;
int nextId = 1;

std::array<std::uint8_t, 20> sha1(const std::string& input) {
    std::vector<std::uint8_t> data(input.begin(), input.end());
    const std::uint64_t bitLength = static_cast<std::uint64_t>(data.size()) * 8;
    data.push_back(0x80);
    while ((data.size() % 64) != 56) data.push_back(0);
    for (int i = 7; i >= 0; --i) data.push_back(static_cast<std::uint8_t>(bitLength >> (i * 8)));

    std::uint32_t h0=0x67452301, h1=0xEFCDAB89, h2=0x98BADCFE, h3=0x10325476, h4=0xC3D2E1F0;
    for (std::size_t offset=0; offset<data.size(); offset+=64) {
        std::uint32_t w[80]{};
        for (int i=0;i<16;++i) {
            const auto p=offset+static_cast<std::size_t>(i)*4;
            w[i]=(std::uint32_t(data[p])<<24)|(std::uint32_t(data[p+1])<<16)|(std::uint32_t(data[p+2])<<8)|data[p+3];
        }
        for (int i=16;i<80;++i) w[i]=(w[i-3]^w[i-8]^w[i-14]^w[i-16])<<1 | (w[i-3]^w[i-8]^w[i-14]^w[i-16])>>31;
        std::uint32_t a=h0,b=h1,c=h2,d=h3,e=h4;
        for (int i=0;i<80;++i) {
            std::uint32_t f=0,k=0;
            if(i<20){f=(b&c)|((~b)&d);k=0x5A827999;}
            else if(i<40){f=b^c^d;k=0x6ED9EBA1;}
            else if(i<60){f=(b&c)|(b&d)|(c&d);k=0x8F1BBCDC;}
            else {f=b^c^d;k=0xCA62C1D6;}
            const std::uint32_t rot=(a<<5)|(a>>27);
            const std::uint32_t temp=rot+f+e+k+w[i];
            e=d;d=c;c=(b<<30)|(b>>2);b=a;a=temp;
        }
        h0+=a;h1+=b;h2+=c;h3+=d;h4+=e;
    }
    std::array<std::uint8_t,20> out{};
    const std::uint32_t hs[5]={h0,h1,h2,h3,h4};
    for(int i=0;i<5;++i) for(int j=0;j<4;++j) out[i*4+j]=static_cast<std::uint8_t>(hs[i]>>(24-j*8));
    return out;
}

std::string base64(const std::uint8_t* data, std::size_t size) {
    static const char table[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for(std::size_t i=0;i<size;i+=3) {
        const std::uint32_t a=data[i], b=i+1<size?data[i+1]:0, c=i+2<size?data[i+2]:0;
        const std::uint32_t n=(a<<16)|(b<<8)|c;
        out.push_back(table[(n>>18)&63]); out.push_back(table[(n>>12)&63]);
        out.push_back(i+1<size?table[(n>>6)&63]:'='); out.push_back(i+2<size?table[n&63]:'=');
    }
    return out;
}

bool sendAll(int socket, const std::uint8_t* data, std::size_t size) {
    std::size_t sent=0;
    while(sent<size) {
        const ssize_t n=send(socket,data+sent,size-sent,MSG_NOSIGNAL);
        if(n<=0) return false;
        sent+=static_cast<std::size_t>(n);
    }
    return true;
}

bool sendFrame(int socket, std::uint8_t opcode, const std::string& payload) {
    std::vector<std::uint8_t> frame;
    frame.push_back(static_cast<std::uint8_t>(0x80 | opcode));
    const std::size_t n=payload.size();
    if(n<126) frame.push_back(static_cast<std::uint8_t>(n));
    else if(n<=65535) { frame.push_back(126); frame.push_back(static_cast<std::uint8_t>(n>>8)); frame.push_back(static_cast<std::uint8_t>(n)); }
    else {
        frame.push_back(127);
        for(int i=7;i>=0;--i) frame.push_back(static_cast<std::uint8_t>(static_cast<std::uint64_t>(n)>>(i*8)));
    }
    frame.insert(frame.end(),payload.begin(),payload.end());
    return sendAll(socket,frame.data(),frame.size());
}

bool sendText(const std::shared_ptr<Player>& player, const std::string& text) {
    std::lock_guard<std::mutex> lock(player->sendMutex);
    return sendFrame(player->socket,1,text);
}

bool websocketHandshake(int socket) {
    std::string request;
    char buffer[1024];
    while(request.find("\r\n\r\n")==std::string::npos && request.size()<8192) {
        const ssize_t n=recv(socket,buffer,sizeof(buffer),0);
        if(n<=0) return false;
        request.append(buffer,static_cast<std::size_t>(n));
    }
    const std::string marker="Sec-WebSocket-Key:";
    auto pos=request.find(marker);
    if(pos==std::string::npos) return false;
    pos+=marker.size();
    while(pos<request.size() && (request[pos]==' ' || request[pos]=='\t')) ++pos;
    auto end=request.find("\r\n",pos);
    if(end==std::string::npos) return false;
    const std::string key=request.substr(pos,end-pos);
    const auto digest=sha1(key+kWsGuid);
    const std::string accept=base64(digest.data(),digest.size());
    const std::string response="HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: "+accept+"\r\n\r\n";
    return sendAll(socket,reinterpret_cast<const std::uint8_t*>(response.data()),response.size());
}

std::string makeSnapshot() {
    std::lock_guard<std::mutex> lock(playersMutex);
    std::ostringstream out;
    out << "STATE " << players.size();
    for (const auto& entry : players) {
        const auto& p = *entry.second;
        out << " " << p.id << " " << p.name << " " << p.x << " " << p.y << " " << p.z << " " << p.yaw;
    }
    return out.str();
}

void broadcastLoop() {
    while (running) {
        std::this_thread::sleep_for(kTick);
        std::vector<std::shared_ptr<Player>> clients;
        { std::lock_guard<std::mutex> lock(playersMutex); for (const auto& entry : players) clients.push_back(entry.second); }
        const std::string snapshot=makeSnapshot();
        for(const auto& client:clients) if(!sendText(client,snapshot)) shutdown(client->socket,SHUT_RDWR);
    }
}

void removePlayer(int id,int socket) {
    {
        std::lock_guard<std::mutex> lock(playersMutex);
        auto it=players.find(id);
        if(it!=players.end() && it->second->socket==socket) {
            std::cout<<"[LEAVE] "<<it->second->name<<" ("<<players.size()-1<<"/"<<kMaxPlayers<<")\n";
            players.erase(it);
        }
    }
    shutdown(socket,SHUT_RDWR); close(socket);
}

bool processCommand(const std::shared_ptr<Player>& player,const std::string& message) {
    std::istringstream input(message);
    std::string command; input>>command;
    if(command=="JOIN") {
        std::string name; input>>name;
        if(!name.empty()) {
            for(char& c:name) if(!std::isalnum(static_cast<unsigned char>(c)) && c!='_' && c!='-') c='_';
            player->name=name.substr(0,16);
            std::cout<<"[NAME] player-"<<player->id<<" is "<<player->name<<"\n";
        }
    } else if(command=="MOVE") {
        float x,y,z,yaw;
        if(input>>x>>y>>z>>yaw && std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z)&&std::isfinite(yaw)) {
            std::lock_guard<std::mutex> lock(playersMutex);
            player->x=x;player->y=y;player->z=z;player->yaw=yaw;
        }
    } else if(command=="PING") sendText(player,"PONG");
    return true;
}

void clientLoop(int socket) {
    if(!websocketHandshake(socket)){close(socket);return;}
    auto player=std::make_shared<Player>(); player->socket=socket;
    {
        std::lock_guard<std::mutex> lock(playersMutex);
        if(players.size()>=kMaxPlayers){sendFrame(socket,1,"FULL");close(socket);return;}
        player->id=nextId++;players.emplace(player->id,player);
        std::cout<<"[JOIN] player-"<<player->id<<" ("<<players.size()<<"/"<<kMaxPlayers<<")\n";
    }
    sendText(player,"WELCOME "+std::to_string(player->id)+" "+std::to_string(kMaxPlayers));

    std::vector<std::uint8_t> pending;
    std::uint8_t buffer[2048];
    while(running) {
        const ssize_t count=recv(socket,buffer,sizeof(buffer),0);
        if(count<=0) break;
        pending.insert(pending.end(),buffer,buffer+count);
        if(pending.size()>16384) break;
        while(pending.size()>=2) {
            const std::uint8_t b0=pending[0],b1=pending[1];
            const std::uint8_t opcode=b0&0x0f;
            const bool masked=(b1&0x80)!=0;
            std::uint64_t length=b1&0x7f;
            std::size_t header=2;
            if(length==126){if(pending.size()<4)break;length=(std::uint64_t(pending[2])<<8)|pending[3];header=4;}
            else if(length==127){if(pending.size()<10)break;length=0;for(int i=0;i<8;++i)length=(length<<8)|pending[2+i];header=10;}
            if(length>8192 || !masked){pending.clear();goto disconnected;}
            if(pending.size()<header+4+length)break;
            const std::uint8_t* mask=pending.data()+header;
            std::string payload(static_cast<std::size_t>(length),'\\0');
            for(std::size_t i=0;i<length;++i)payload[i]=static_cast<char>(pending[header+4+i]^mask[i%4]);
            pending.erase(pending.begin(),pending.begin()+static_cast<std::ptrdiff_t>(header+4+length));
            if(opcode==8) goto disconnected;
            if(opcode==9){std::lock_guard<std::mutex> lock(player->sendMutex);sendFrame(socket,10,payload);}
            else if(opcode==1) processCommand(player,payload);
        }
    }
disconnected:
    removePlayer(player->id,socket);
}
} // namespace

int main(int argc,char** argv) {
    int port=kPort;
    if(argc>1){try{port=std::stoi(argv[1]);}catch(...){std::cerr<<"Invalid port.\n";return 1;}}
    const int serverSocket=socket(AF_INET,SOCK_STREAM,0);
    if(serverSocket<0){perror("socket");return 1;}
    int reuse=1;setsockopt(serverSocket,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
    sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_ANY);address.sin_port=htons(static_cast<std::uint16_t>(port));
    if(bind(serverSocket,reinterpret_cast<sockaddr*>(&address),sizeof(address))<0){perror("bind");close(serverSocket);return 1;}
    if(listen(serverSocket,32)<0){perror("listen");close(serverSocket);return 1;}
    std::signal(SIGINT,[](int){running=false;});std::signal(SIGTERM,[](int){running=false;});
    std::cout<<"Bolt Survival WebSocket server listening on 0.0.0.0:"<<port<<" (max "<<kMaxPlayers<<" players)\n"<<std::flush;
    std::thread broadcaster(broadcastLoop);
    while(running){
        sockaddr_in clientAddress{};socklen_t clientLength=sizeof(clientAddress);
        const int client=accept(serverSocket,reinterpret_cast<sockaddr*>(&clientAddress),&clientLength);
        if(client<0){if(!running)break;if(errno==EINTR)continue;perror("accept");continue;}
        std::thread(clientLoop,client).detach();
    }
    close(serverSocket);running=false;if(broadcaster.joinable())broadcaster.join();
    std::cout<<"Server stopped.\n";return 0;
}
