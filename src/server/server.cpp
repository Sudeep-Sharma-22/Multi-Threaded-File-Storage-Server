#include "server.h"
#include "logger.h"
#include "../common/protocol.h"
#include "../common/network_utils.h"
#include "../common/types.h"
#include "file_manager.h"

#include <iostream>
#include <vector>
#include <functional>

namespace mtfss {

Server::Server(uint16_t port, int num_workers)
    : port_(port), listen_socket_(INVALID_SOCKET), running_(false),
      task_queue_(64),
      active_connections_(0)
{
    listen_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket_ == INVALID_SOCKET) {
        throw std::runtime_error("socket() failed: " + std::to_string(WSAGetLastError()));
    }

    int opt = 1;
    if (setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&opt), sizeof(opt)) == SOCKET_ERROR) {
        closesocket(listen_socket_);
        throw std::runtime_error("setsockopt(SO_REUSEADDR) failed");
    }

    sockaddr_in server_addr{};
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(port_);

    if (bind(listen_socket_, reinterpret_cast<sockaddr*>(&server_addr),
             sizeof(server_addr)) == SOCKET_ERROR) {
        closesocket(listen_socket_);
        throw std::runtime_error("bind() failed on port " + std::to_string(port_) +
                                 ": " + std::to_string(WSAGetLastError()));
    }

    if (listen(listen_socket_, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(listen_socket_);
        throw std::runtime_error("listen() failed: " + std::to_string(WSAGetLastError()));
    }

    LOG_INFO("[Server] Listening on port " << port_);

    thread_pool_ = std::make_unique<ThreadPool>(num_workers, task_queue_);
}

Server::~Server() {
    stop();
}

void Server::start() {
    running_ = true;

    while (running_) {
        sockaddr_in client_addr{};
        int client_len = sizeof(client_addr);

        SOCKET client_socket = accept(listen_socket_,
                                      reinterpret_cast<sockaddr*>(&client_addr),
                                      &client_len);

        if (client_socket == INVALID_SOCKET) {
            if (running_) {
                LOG_ERROR("[Server] accept() failed: " << WSAGetLastError());
            }
            continue;
        }

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
        std::string client_ip = ip_str;
        uint16_t client_port = ntohs(client_addr.sin_port);
        std::string addr_str = client_ip + ":" + std::to_string(client_port);

        LOG_INFO("[Server] Connection from " << addr_str);

        auto task = [this, client_socket, addr_str]() {
            this->handle_client(client_socket, addr_str);
        };

        if (!task_queue_.push(task)) {
            LOG_WARN("[Server] Queue rejected task (shutting down). Dropping client " << addr_str);
            closesocket(client_socket);
        }
    }
}

void Server::stop() {
    bool expected = true;
    if (running_.compare_exchange_strong(expected, false)) {
        LOG_INFO("[Server] Initiating shutdown...");

        if (listen_socket_ != INVALID_SOCKET) {
            closesocket(listen_socket_);
            listen_socket_ = INVALID_SOCKET;
        }

        if (thread_pool_) {
            thread_pool_->shutdown();
        }
    }
}

void Server::handle_client(SOCKET client_socket, const std::string& client_addr) {
    active_connections_++;
    std::string session_user;

    while (running_) {
        MessageHeader req_hdr;
        std::string username, filename;

        if (!recv_message_meta(client_socket, req_hdr, username, filename)) {
            break;
        }

        CommandType cmd = static_cast<CommandType>(req_hdr.command);
        LOG_INFO("[Server] " << client_addr << " -> "
                  << command_to_string(req_hdr.command));

        std::vector<char> payload;
        if (cmd != CommandType::UPLOAD && req_hdr.payload_length > 0) {
            payload.resize(static_cast<size_t>(req_hdr.payload_length));
            if (!recv_all(client_socket, payload.data(), static_cast<size_t>(req_hdr.payload_length))) {
                break;
            }
        }

        MessageHeader resp_hdr;
        std::string resp_payload;

        switch (cmd) {
            case CommandType::LOGIN: {
                session_user = username;
                LOG_INFO("[Server] User logged in: " << session_user);
                resp_hdr = make_response(cmd, StatusCode::OK);
                resp_payload = "Welcome, " + session_user + "!";
                resp_hdr.payload_length = resp_payload.size();
                break;
            }

            case CommandType::UPLOAD: {
                if (session_user.empty()) {
                    resp_hdr = make_response(cmd, StatusCode::BAD_REQUEST);
                    resp_payload = "Must LOGIN first";
                    FileManager::discard_data(client_socket, req_hdr.payload_length);
                } else {
                    FileManager fm;
                    bool success = fm.save_file(session_user, filename, client_socket, req_hdr.payload_length);
                    if (success) {
                        resp_hdr = make_response(cmd, StatusCode::CREATED);
                        resp_payload = "File uploaded successfully.";
                    } else {
                        resp_hdr = make_response(cmd, StatusCode::INTERNAL_ERROR);
                        resp_payload = "Failed to save file.";
                    }
                }
                resp_hdr.payload_length = resp_payload.size();
                break;
            }
            case CommandType::DOWNLOAD: {
                if (session_user.empty()) {
                    resp_hdr = make_response(cmd, StatusCode::BAD_REQUEST);
                    resp_payload = "Must LOGIN first";
                    resp_hdr.payload_length = resp_payload.size();
                } else {
                    uint64_t file_size = FileManager::get_file_size(session_user, filename);
                    if (file_size == static_cast<uint64_t>(-1)) {
                        resp_hdr = make_response(cmd, StatusCode::NOT_FOUND);
                        resp_payload = "File not found.";
                        resp_hdr.payload_length = resp_payload.size();
                    } else {
                        resp_hdr = make_response(cmd, StatusCode::OK);
                        resp_hdr.payload_length = file_size;
                        
                        if (!send_message_meta(client_socket, resp_hdr, "", "")) {
                            goto session_end;
                        }
                        
                        if (!FileManager::stream_file_to_socket(session_user, filename, client_socket)) {
                            goto session_end;
                        }
                        
                        continue; 
                    }
                }
                break;
            }

            case CommandType::LIST: {
                if (session_user.empty()) {
                    resp_hdr = make_response(cmd, StatusCode::BAD_REQUEST);
                    resp_payload = "Must LOGIN first";
                } else {
                    resp_hdr = make_response(cmd, StatusCode::OK);
                    resp_payload = FileManager::list_files(session_user);
                }
                resp_hdr.payload_length = resp_payload.size();
                break;
            }
            
            case CommandType::DELETE_F: {
                if (session_user.empty()) {
                    resp_hdr = make_response(cmd, StatusCode::BAD_REQUEST);
                    resp_payload = "Must LOGIN first";
                } else {
                    if (FileManager::delete_file(session_user, filename)) {
                        resp_hdr = make_response(cmd, StatusCode::OK);
                        resp_payload = "File deleted successfully.";
                    } else {
                        resp_hdr = make_response(cmd, StatusCode::NOT_FOUND);
                        resp_payload = "File not found or cannot be deleted.";
                    }
                }
                resp_hdr.payload_length = resp_payload.size();
                break;
            }

            case CommandType::RENAME: {
                if (session_user.empty()) {
                    resp_hdr = make_response(cmd, StatusCode::BAD_REQUEST);
                    resp_payload = "Must LOGIN first";
                } else {
                    std::string new_name(payload.begin(), payload.end());
                    if (FileManager::rename_file(session_user, filename, new_name)) {
                        resp_hdr = make_response(cmd, StatusCode::OK);
                        resp_payload = "Renamed successfully.";
                    } else {
                        resp_hdr = make_response(cmd, StatusCode::NOT_FOUND);
                        resp_payload = "File not found or new name conflicts.";
                    }
                }
                resp_hdr.payload_length = resp_payload.size();
                break;
            }

            case CommandType::QUIT: {
                resp_hdr = make_response(cmd, StatusCode::OK);
                resp_payload = "Goodbye!";
                resp_hdr.payload_length = resp_payload.size();
                send_message(client_socket, resp_hdr, "", "",
                           resp_payload.c_str(), resp_payload.size());
                goto session_end;
            }

            default: {
                resp_hdr = make_response(cmd, StatusCode::BAD_REQUEST);
                resp_payload = "Unknown or unsupported command: " +
                            std::string(command_to_string(req_hdr.command));
                resp_hdr.payload_length = resp_payload.size();
                break;
            }
        }

        if (!send_message(client_socket, resp_hdr, "", "",
                         resp_payload.c_str(), resp_payload.size())) {
            break;
        }
    }

session_end:
    closesocket(client_socket);
    active_connections_--;
    LOG_INFO("[Server] Session ended: " << client_addr
              << " (user: " << (session_user.empty() ? "none" : session_user)
              << ", active: " << active_connections_.load() << ")");
}

} // namespace mtfss