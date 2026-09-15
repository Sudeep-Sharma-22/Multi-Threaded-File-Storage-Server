#include "../src/common/protocol.h"
#include "../src/common/network_utils.h"

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <random>
#include <string>
#include <mutex>
#include <winsock2.h>
#include <ws2tcpip.h>

std::atomic<uint64_t> total_requests(0);
std::atomic<uint64_t> total_success(0);
std::atomic<uint64_t> total_latency_ms(0);

std::mutex print_mutex;

void log_msg(const std::string& msg) {
    std::lock_guard<std::mutex> lock(print_mutex);
    std::cout << msg << std::endl;
}

class WinsockGuard {
public:
    WinsockGuard() {
        WSADATA wsa_data;
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
    }
    ~WinsockGuard() { WSACleanup(); }
};

void client_thread(int thread_id, int num_requests) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        log_msg("[Thread " + std::to_string(thread_id) + "] Failed to create socket");
        return;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9090);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == SOCKET_ERROR) {
        log_msg("[Thread " + std::to_string(thread_id) + "] Failed to connect");
        closesocket(sock);
        return;
    }

    std::string username = "loadtest_" + std::to_string(thread_id);
    mtfss::MessageHeader login_req = mtfss::make_request(mtfss::CommandType::LOGIN, 0, 0, username.length());
    mtfss::send_header(sock, login_req);
    mtfss::send_all(sock, username.c_str(), username.length());

    mtfss::MessageHeader login_resp;
    mtfss::recv_header(sock, login_resp);
    if (login_resp.payload_length > 0) {
        std::vector<char> dummy(login_resp.payload_length);
        mtfss::recv_all(sock, dummy.data(), dummy.size());
    }

    std::vector<char> upload_data(8192, 'A');

    for (int i = 0; i < num_requests; ++i) {
        auto start = std::chrono::steady_clock::now();
        std::string filename = "file_" + std::to_string(i) + ".dat";

        mtfss::MessageHeader ul_req = mtfss::make_request(mtfss::CommandType::UPLOAD, upload_data.size(), filename.length(), 0);
        mtfss::send_header(sock, ul_req);
        mtfss::send_all(sock, filename.c_str(), filename.length());
        mtfss::send_all(sock, upload_data.data(), upload_data.size());

        mtfss::MessageHeader ul_resp;
        mtfss::recv_header(sock, ul_resp);
        if (ul_resp.payload_length > 0) {
            std::vector<char> p(ul_resp.payload_length);
            mtfss::recv_all(sock, p.data(), p.size());
        }
        
        mtfss::MessageHeader dl_req = mtfss::make_request(mtfss::CommandType::DOWNLOAD, 0, filename.length(), 0);
        mtfss::send_header(sock, dl_req);
        mtfss::send_all(sock, filename.c_str(), filename.length());
        
        mtfss::MessageHeader dl_resp;
        mtfss::recv_header(sock, dl_resp);
        if (dl_resp.payload_length > 0) {
            std::vector<char> p(dl_resp.payload_length);
            mtfss::recv_all(sock, p.data(), p.size());
        }
        
        mtfss::MessageHeader list_req = mtfss::make_request(mtfss::CommandType::LIST, 0, 0, 0);
        mtfss::send_header(sock, list_req);
        
        mtfss::MessageHeader list_resp;
        mtfss::recv_header(sock, list_resp);
        if (list_resp.payload_length > 0) {
            std::vector<char> p(list_resp.payload_length);
            mtfss::recv_all(sock, p.data(), p.size());
        }
        
        mtfss::MessageHeader del_req = mtfss::make_request(mtfss::CommandType::DELETE_F, 0, filename.length(), 0);
        mtfss::send_header(sock, del_req);
        mtfss::send_all(sock, filename.c_str(), filename.length());
        
        mtfss::MessageHeader del_resp;
        mtfss::recv_header(sock, del_resp);
        if (del_resp.payload_length > 0) {
            std::vector<char> p(del_resp.payload_length);
            mtfss::recv_all(sock, p.data(), p.size());
        }

        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        total_latency_ms += duration;
        total_requests += 4;
        if (ul_resp.status == static_cast<uint32_t>(mtfss::StatusCode::CREATED) &&
            dl_resp.status == static_cast<uint32_t>(mtfss::StatusCode::OK) &&
            list_resp.status == static_cast<uint32_t>(mtfss::StatusCode::OK) &&
            del_resp.status == static_cast<uint32_t>(mtfss::StatusCode::OK)) {
            total_success += 4;
        }
    }

    closesocket(sock);
}

int main(int argc, char** argv) {
    int num_threads = 10;
    int reqs_per_thread = 100;

    if (argc >= 3) {
        num_threads = std::stoi(argv[1]);
        reqs_per_thread = std::stoi(argv[2]);
    }

    std::cout << "Starting Load Test...\n";
    std::cout << "Threads (Concurrent Clients): " << num_threads << "\n";
    std::cout << "Cycles (UL+DL+LIST+DEL) per thread: " << reqs_per_thread << "\n\n";

    WinsockGuard wg;

    auto start_time = std::chrono::steady_clock::now();

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(client_thread, i, reqs_per_thread);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::steady_clock::now();
    double total_sec = std::chrono::duration<double>(end_time - start_time).count();

    std::cout << "\n=== Results ===\n";
    std::cout << "Time elapsed:    " << total_sec << " seconds\n";
    std::cout << "Total Requests:  " << total_requests.load() << "\n";
    std::cout << "Success:         " << total_success.load() << "\n";
    std::cout << "Throughput:      " << (total_requests.load() / total_sec) << " req/sec\n";
    if (total_requests.load() > 0) {
        std::cout << "Avg Latency:     " << (double)total_latency_ms.load() / (total_requests.load() / 4) << " ms per full cycle\n";
    }

    return 0;
}