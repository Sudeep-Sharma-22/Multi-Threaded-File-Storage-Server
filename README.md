# Multi-Threaded File Storage Server (MTFSS)

A concurrent, application-layer TCP file storage server built from scratch in C++17. 

This project was built to implement and demonstrate core Operating Systems and Computer Networks concepts, specifically: Thread Pools, Producer-Consumer queues, Reader-Writer locking, socket programming, message framing, and graceful shutdown handling.

## 🛠️ Architecture & Core Components

### 1. Networking & Custom Protocol (CN)
*   **Custom Application Protocol:** Implements a custom binary TCP protocol to handle file storage operations. Every message begins with a fixed 32-byte header.
*   **Message Framing:** The fixed header contains the exact lengths of the subsequent variable-length fields (username, filename, payload), cleanly solving the TCP byte-stream framing problem.
*   **Network Byte Order:** Multi-byte protocol fields are serialized using network byte order. Integer fields are converted to big-endian before transmission using `htonl/htonll` and converted back upon receipt.
*   **Binary Streaming:** Files are not loaded entirely into memory. They are streamed to and from the socket in fixed-size chunks (e.g., 8KB) to bound memory usage during transfers.
*   **Path Sanitization:** File paths are validated to prevent clients from escaping their assigned storage directory via traversal attacks.

### 2. Concurrency & Synchronization (OS)
*   **Thread Pool & Task Queue:** The server utilizes a fixed-size pool of worker threads synchronized via `std::mutex` and `std::condition_variable`.
*   **Bounded Backpressure:** The producer-consumer task queue is bounded. If the queue is full, the server blocks on `accept()`, providing backpressure to prevent unbounded memory growth under load.
*   **Reader-Writer Locking:** The `FileLockManager` assigns a `std::shared_mutex` to each file. Multiple readers can access the same file concurrently while writes/deletes require exclusive access, subject to available system resources and worker threads.
*   **Safe Lock Lifecycle:** The lock manager uses a thread-safe, reference-counted `std::shared_ptr` strategy. Locks are dynamically created when needed and safely erased from memory when the reference count drops to zero.
*   **Asynchronous Logger:** A dedicated background consumer thread processes logs from a synchronized `std::queue`. Network worker threads push events to the queue rather than performing blocking disk I/O.

### 3. Graceful Shutdown
*   The server intercepts `Ctrl+C` termination signals. It cleanly stops accepting new connections, drains the task queue, joins all worker threads, flushes the logging queue, and closes active file handles before exiting.

### 4. Metadata Index
*   The server maintains an in-memory metadata index to avoid repeatedly scanning/querying the filesystem during normal metadata operations. The filesystem remains the source of truth and the index is rebuilt at startup.

## 📈 Benchmark

The repository includes a multi-threaded load-testing utility (`load_test.exe`) to measure the performance of the Thread Pool and locking architecture.

**Local benchmark using 5 concurrent workers and 2,000 requests:**
- 5 concurrent worker threads executing 100 complete cycles each (1 Cycle = UPLOAD -> DOWNLOAD -> LIST -> DELETE).
- **Throughput:** ~4,700 requests per second.
- **Latency:** ~1.8 ms per full 4-operation lifecycle.
- **Success Rate:** 100%.

*(Note: These metrics measure the overhead of the Thread Pool, Protocol parsing, and synchronization over the local loopback interface.)*

## ⚙️ Build Instructions

**Requirements:**
- Windows OS (uses Winsock2 API)
- MinGW-w64 (`g++` compiler with C++17 support)
- `mingw32-make`

**Building:**
```bash
git clone <repository_url>
cd mtfss
mingw32-make all
```

This compiles three binaries into the `bin/` directory:
1.  `server.exe` — The multi-threaded storage server.
2.  `client.exe` — An interactive CLI for connecting to the server.
3.  `load_test.exe` — The performance benchmark utility.

## 💻 Usage

**Start the Server:**
```bash
./bin/server.exe
```

**Connect via Client:**
```bash
./bin/client.exe
```

**Client Commands:**
```text
mtfss> login <username>
mtfss> upload <filepath>
mtfss> download <filename>
mtfss> list
mtfss> delete <filename>
mtfss> rename <old_name> <new_name>
mtfss> quit
```
