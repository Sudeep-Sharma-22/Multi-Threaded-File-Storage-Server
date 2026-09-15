#include "client.h"
#include "../common/protocol.h"
#include "../common/network_utils.h"

#include <iostream>
#include <vector>
#include <filesystem>
#include <fstream>
#include "../common/types.h"

namespace mtfss {

Client::Client(const std::string& host, uint16_t port)
    : host_(host), port_(port), socket_(INVALID_SOCKET) {}

Client::~Client() {
    disconnect();
}

bool Client::connect_to_server() {
    addrinfo hints{};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* result = nullptr;
    int ret = getaddrinfo(host_.c_str(), std::to_string(port_).c_str(), &hints, &result);
    if (ret != 0) {
        std::cerr << "[Client] getaddrinfo failed: " << ret << std::endl;
        return false;
    }

    socket_ = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (socket_ == INVALID_SOCKET) {
        std::cerr << "[Client] socket() failed: " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        return false;
    }

    ret = connect(socket_, result->ai_addr, static_cast<int>(result->ai_addrlen));
    freeaddrinfo(result);

    if (ret == SOCKET_ERROR) {
        std::cerr << "[Client] connect() failed: " << WSAGetLastError() << std::endl;
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
        return false;
    }

    std::cout << "[Client] Connected to " << host_ << ":" << port_ << std::endl;
    return true;
}

void Client::disconnect() {
    if (socket_ != INVALID_SOCKET) {
        shutdown(socket_, SD_BOTH);
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
        username_.clear();
        std::cout << "[Client] Disconnected" << std::endl;
    }
}

bool Client::login(const std::string& username) {
    if (!is_connected()) return false;
    username_ = username;
    return send_command(static_cast<uint32_t>(CommandType::LOGIN));
}

bool Client::list_files() {
    if (!is_connected()) return false;
    return send_command(static_cast<uint32_t>(CommandType::LIST));
}

bool Client::delete_file(const std::string& filename) {
    if (!is_connected()) return false;
    return send_command(static_cast<uint32_t>(CommandType::DELETE_F), filename);
}

bool Client::rename_file(const std::string& old_name, const std::string& new_name) {
    if (!is_connected()) return false;
    return send_command(static_cast<uint32_t>(CommandType::RENAME), old_name, new_name.c_str(), new_name.size());
}

bool Client::quit() {
    MessageHeader hdr = make_request(CommandType::QUIT);

    if (!send_message(socket_, hdr, "", "", nullptr, 0)) {
        std::cerr << "[Client] Failed to send QUIT" << std::endl;
        disconnect();
        return false;
    }

    receive_response();
    disconnect();
    return true;
}

bool Client::send_command(uint32_t cmd, const std::string& filename,
                          const char* payload, size_t payload_size) {
    MessageHeader hdr = make_request(
        static_cast<CommandType>(cmd),
        payload_size,
        filename.size(),
        username_.size()
    );

    if (!send_message(socket_, hdr, username_, filename, payload, payload_size)) {
        std::cerr << "[Client] Failed to send command" << std::endl;
        disconnect();
        return false;
    }

    return receive_response();
}

bool Client::receive_response() {
    MessageHeader resp_hdr;
    std::string resp_user, resp_filename;
    std::vector<char> resp_payload;

    if (!recv_message(socket_, resp_hdr, resp_user, resp_filename, resp_payload)) {
        std::cerr << "[Client] Connection to server lost." << std::endl;
        disconnect();
        return false;
    }

    std::cout << "[Server " << status_to_string(resp_hdr.status) << "] ";

    if (!resp_payload.empty()) {
        std::string payload_text(resp_payload.begin(), resp_payload.end());
        std::cout << payload_text;
    }

    std::cout << std::endl;
    return resp_hdr.status == static_cast<uint32_t>(StatusCode::OK) ||
           resp_hdr.status == static_cast<uint32_t>(StatusCode::CREATED);
}

bool Client::upload_file(const std::string& filepath) {
    if (!is_connected()) return false;
    if (username_.empty()) {
        std::cerr << "[Client] Must login before uploading" << std::endl;
        return false;
    }

    std::filesystem::path p(filepath);
    if (!std::filesystem::exists(p) || !std::filesystem::is_regular_file(p)) {
        std::cerr << "[Client] File does not exist: " << filepath << std::endl;
        return false;
    }

    uint64_t file_size = std::filesystem::file_size(p);
    std::string filename = p.filename().string();

    std::ifstream infile(p, std::ios::binary);
    if (!infile) {
        std::cerr << "[Client] Failed to open file: " << filepath << std::endl;
        return false;
    }

    MessageHeader hdr = make_request(CommandType::UPLOAD, file_size, filename.size(), username_.size());
    std::cout << "[Client] Uploading " << filename << " (" << file_size << " bytes)..." << std::endl;

    if (!send_message_meta(socket_, hdr, username_, filename)) {
        std::cerr << "[Client] Failed to send upload header" << std::endl;
        disconnect();
        return false;
    }

    std::vector<char> buffer(TRANSFER_BUFFER_SIZE);
    uint64_t bytes_sent = 0;
    
    while (bytes_sent < file_size) {
        infile.read(buffer.data(), buffer.size());
        size_t bytes_read = infile.gcount();
        
        if (bytes_read > 0) {
            if (!send_all(socket_, buffer.data(), bytes_read)) {
                std::cerr << "[Client] Network error during file transfer" << std::endl;
                disconnect();
                return false;
            }
            bytes_sent += bytes_read;
        } else {
            break; 
        }
    }

    return receive_response();
}

bool Client::download_file(const std::string& filename) {
    if (!is_connected()) return false;
    if (username_.empty()) {
        std::cerr << "[Client] Must login before downloading" << std::endl;
        return false;
    }

    MessageHeader hdr = make_request(CommandType::DOWNLOAD, 0, filename.size(), username_.size());
    if (!send_message_meta(socket_, hdr, username_, filename)) {
        std::cerr << "[Client] Failed to send download request" << std::endl;
        disconnect();
        return false;
    }

    MessageHeader resp_hdr;
    std::string resp_user, resp_filename;
    if (!recv_message_meta(socket_, resp_hdr, resp_user, resp_filename)) {
        std::cerr << "[Client] Failed to receive response header" << std::endl;
        disconnect();
        return false;
    }

    if (resp_hdr.status != static_cast<uint32_t>(StatusCode::OK)) {
        if (resp_hdr.payload_length > 0) {
            std::vector<char> err_buf(static_cast<size_t>(resp_hdr.payload_length));
            if (recv_all(socket_, err_buf.data(), static_cast<size_t>(resp_hdr.payload_length))) {
                std::string err_str(err_buf.begin(), err_buf.end());
                std::cout << "[Server " << status_to_string(resp_hdr.status) << "] " << err_str << std::endl;
            }
        } else {
            std::cout << "[Server " << status_to_string(resp_hdr.status) << "]" << std::endl;
        }
        return false;
    }

    std::cout << "[Client] Downloading " << filename << " (" << resp_hdr.payload_length << " bytes)..." << std::endl;
    
    std::filesystem::path dl_dir = "downloads";
    if (!std::filesystem::exists(dl_dir)) {
        std::filesystem::create_directories(dl_dir);
    }
    std::filesystem::path dl_path = dl_dir / filename;

    std::ofstream outfile(dl_path, std::ios::binary);
    if (!outfile) {
        std::cerr << "[Client] Failed to create local file: " << dl_path << std::endl;
        uint64_t bytes_remaining = resp_hdr.payload_length;
        std::vector<char> buffer(TRANSFER_BUFFER_SIZE);
        while (bytes_remaining > 0) {
            size_t chunk_size = static_cast<size_t>(std::min(static_cast<uint64_t>(buffer.size()), bytes_remaining));
            if (!recv_all(socket_, buffer.data(), chunk_size)) {
                disconnect();
                break;
            }
            bytes_remaining -= chunk_size;
        }
        return false;
    }

    uint64_t bytes_remaining = resp_hdr.payload_length;
    std::vector<char> buffer(TRANSFER_BUFFER_SIZE);
    
    while (bytes_remaining > 0) {
        size_t chunk_size = static_cast<size_t>(std::min(static_cast<uint64_t>(buffer.size()), bytes_remaining));
        if (!recv_all(socket_, buffer.data(), chunk_size)) {
            std::cerr << "[Client] Network error during download." << std::endl;
            outfile.close();
            std::filesystem::remove(dl_path); 
            disconnect();
            return false;
        }
        outfile.write(buffer.data(), chunk_size);
        bytes_remaining -= chunk_size;
    }

    std::cout << "[Client] Download complete: " << dl_path.string() << std::endl;
    return true;
}

} // namespace mtfss