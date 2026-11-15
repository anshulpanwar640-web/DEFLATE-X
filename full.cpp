#include <iostream>
#include <fstream>
#include <queue>
#include <vector>
#include <string>
#include <algorithm>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif
using namespace std;



class BitWriter {
private:
    unsigned char buf;
    int bit_count;
    ofstream file;

public:
    BitWriter(const char* filename) {
        file.open(filename, ios::binary);
        buf = 0;
        bit_count = 0;
    }

    void writeBit(int bit) {
        buf = buf << 1;
        if (bit == 1) {
            buf = buf | 1;
        }
        bit_count = bit_count + 1;

        if (bit_count == 8) {
            file.put(buf);
            buf = 0;
            bit_count = 0;
        }
    }

    void writeBits(int value, int numBits) {
        for (int i = numBits - 1; i >= 0; i--) {
            writeBit((value >> i) & 1);
        }
    }

    void flush() {
        if (bit_count > 0) {
            buf = buf << (8 - bit_count);
            file.put(buf);
            buf = 0;
            bit_count = 0;
        }
    }

    ~BitWriter() {
        flush();
        file.close();
    }
};

class BitReader {
private:
    unsigned char buf;
    int bit_count;
    ifstream file;

public:
    BitReader(const char* filename) {
        file.open(filename, ios::binary);
        buf = 0;
        bit_count = 0;
    }

    int readBit() {
        if (bit_count == 0) {
            int c = file.get();
            if (c < 0) {
                c = 0;
            }
            buf = c;
            bit_count = 8;
        }

        int bit = (buf >> 7) & 1;
        buf = buf << 1;
        bit_count = bit_count - 1;
        return bit;
    }

    int readBits(int numBits) {
        int value = 0;
        for (int i = 0; i < numBits; i++) {
            value = (value << 1) | readBit();
        }
        return value;
    }

    ~BitReader() {
        file.close();
    }
};


const int TOTAL = 512;

struct Node {
    int symbol;
    int freq;
    Node* left;
    Node* right;
};

Node* makeNode(int symbol, int freq, Node* left, Node* right) {
    Node* n = new Node();
    n->symbol = symbol;
    n->freq = freq;
    n->left = left;
    n->right = right;
    return n;
}


struct Compare {
    bool operator()(Node* a, Node* b) {
        return a->freq > b->freq;
    }
};

Node* buildTree(int freq[]) {
    priority_queue<Node*, vector<Node*>, Compare> heap;

    for (int i = 0; i < TOTAL; i++) {
        if (freq[i] > 0) {
            heap.push(makeNode(i, freq[i], NULL, NULL));
        }
    }

    if (heap.empty()) {
        return NULL;
    }

    while (heap.size() > 1) {
        Node* a = heap.top();
        heap.pop();
        Node* b = heap.top();
        heap.pop();
        heap.push(makeNode(-1, a->freq + b->freq, a, b));
    }

    return heap.top();
}


void makeCodes(Node* root, string code, string codes[]) {
    if (root == NULL) {
        return;
    }

    if (root->left == NULL && root->right == NULL) {
        if (code == "") {
            code = "0";
        }
        codes[root->symbol] = code;
        return;
    }

    makeCodes(root->left, code + "0", codes);
    makeCodes(root->right, code + "1", codes);
}

void freeTree(Node* root) {
    if (root == NULL) {
        return;
    }
    freeTree(root->left);
    freeTree(root->right);
    delete root;
}

int readSymbol(BitReader* reader, Node* root) {
    Node* cur = root;

    if (cur->left == NULL && cur->right == NULL) {
        reader->readBit();
        return cur->symbol;
    }

    while (cur->left != NULL) {
        if (reader->readBit() == 1) {
            cur = cur->right;
        } else {
            cur = cur->left;
        }
    }
    return cur->symbol;
}



const int WINDOW = 4096;
const int MIN_MATCH = 3;
const int MAX_MATCH = 258;

int doLZ77(unsigned char* data, int size, int sym[], int dist[]) {
    int count = 0;
    int i = 0;

    while (i < size) {
        int bestLen = 0;
        int bestDist = 0;

        int start = i - WINDOW;
        if (start < 0) {
            start = 0;
        }

        for (int j = start; j < i; j++) {
            int len = 0;
            while (len < MAX_MATCH && i + len < size && data[j + len] == data[i + len]) {
                len++;
            }
            if (len > bestLen) {
                bestLen = len;
                bestDist = i - j;
            }
        }

        if (bestLen >= MIN_MATCH) {
            sym[count] = 256 + bestLen - MIN_MATCH;
            dist[count] = bestDist;
            i = i + bestLen;
        } else {
            sym[count] = data[i];
            dist[count] = 0;
            i = i + 1;
        }
        count++;
    }

    return count;
}



int getSize(const char* name) {
    ifstream in(name, ios::binary);
    if (!in) {
        return 0;
    }
    in.seekg(0, ios::end);
    return in.tellg();
}

void compress(const char* inName, const char* outName) {
    ifstream in(inName, ios::binary);
    if (!in) {
        cout << "Cannot open input file" << endl;
        return;
    }
    in.seekg(0, ios::end);
    int size = in.tellg();
    in.seekg(0, ios::beg);
    unsigned char* data = new unsigned char[size + 1];
    in.read((char*)data, size);
    in.close();

    
    int* sym = new int[size + 1];
    int* dist = new int[size + 1];
    int tokens = doLZ77(data, size, sym, dist);

    
    int freq[TOTAL];
    for (int i = 0; i < TOTAL; i++) {
        freq[i] = 0;
    }
    for (int i = 0; i < tokens; i++) {
        freq[sym[i]]++;
    }

    
    Node* root = buildTree(freq);
    string codes[TOTAL];
    makeCodes(root, "", codes);

    int distinct = 0;
    for (int i = 0; i < TOTAL; i++) {
        if (freq[i] > 0) {
            distinct++;
        }
    }

    
    BitWriter writer(outName);
    writer.writeBits(size, 32);
    writer.writeBits(distinct, 10);
    for (int i = 0; i < TOTAL; i++) {
        if (freq[i] > 0) {
            writer.writeBits(i, 9);
            writer.writeBits(freq[i], 32);
        }
    }

    for (int t = 0; t < tokens; t++) {
        string code = codes[sym[t]];
        for (int k = 0; k < (int)code.length(); k++) {
            if (code[k] == '1') {
                writer.writeBit(1);
            } else {
                writer.writeBit(0);
            }
        }
        if (sym[t] >= 256) {
            writer.writeBits(dist[t] - 1, 12);
        }
    }

    delete[] data;
    delete[] sym;
    delete[] dist;
    freeTree(root);
}

void decompress(const char* inName, const char* outName) {
    BitReader reader(inName);

    int size = reader.readBits(32);
    int distinct = reader.readBits(10);

    int freq[TOTAL];
    for (int i = 0; i < TOTAL; i++) {
        freq[i] = 0;
    }
    for (int i = 0; i < distinct; i++) {
        int s = reader.readBits(9);
        freq[s] = reader.readBits(32);
    }

    Node* root = buildTree(freq);
    if (root == NULL && size > 0) {
        cout << "File is corrupted" << endl;
        return;
    }

    unsigned char* out = new unsigned char[size + 1];
    int pos = 0;

    while (pos < size) {
        int s = readSymbol(&reader, root);

        if (s < 256) {
            out[pos] = s;
            pos++;
        } else {
            int len = s - 256 + MIN_MATCH;
            int d = reader.readBits(12) + 1;

            if (d > pos) {
                cout << "File is corrupted" << endl;
                delete[] out;
                freeTree(root);
                return;
            }

            for (int k = 0; k < len && pos < size; k++) {
                out[pos] = out[pos - d];
                pos++;
            }
        }
    }

    ofstream outFile(outName, ios::binary);
    outFile.write((char*)out, size);
    outFile.close();

    delete[] out;
    freeTree(root);
}

void runServer(int port = 8080) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cout << "Failed to initialize Winsock" << endl;
        return;
    }
#endif

    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        cout << "Error: Could not create socket." << endl;
        return;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (::bind(server_fd, (sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        cout << "Error: Port " << port << " is already in use or cannot be bound." << endl;
        closesocket(server_fd);
        return;
    }

    if (listen(server_fd, 10) == SOCKET_ERROR) {
        cout << "Error: Listen failed." << endl;
        closesocket(server_fd);
        return;
    }

    cout << "\n========================================================" << endl;
    cout << "   DEFLATE-X Native C++ Backend Server Active           " << endl;
    cout << "   Listening on: http://localhost:" << port << endl;
    cout << "   Open this URL in your web browser!                   " << endl;
    cout << "========================================================\n" << endl;

    while (true) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        SOCKET client_sock = accept(server_fd, (sockaddr*)&client_addr, &client_len);
        if (client_sock == INVALID_SOCKET) {
            continue;
        }

        string raw_request;
        char buf[2048];
        int bytes = 0;
        size_t header_end = string::npos;

        // Read until header boundary
        while ((bytes = recv(client_sock, buf, sizeof(buf), 0)) > 0) {
            raw_request.append(buf, bytes);
            header_end = raw_request.find("\r\n\r\n");
            if (header_end != string::npos) break;
        }

        if (raw_request.empty()) {
            closesocket(client_sock);
            continue;
        }

        string header_part = raw_request.substr(0, header_end);
        string body_part = "";
        if (header_end != string::npos && header_end + 4 < raw_request.size()) {
            body_part = raw_request.substr(header_end + 4);
        }

        // Parse Request Method & Path
        size_t line_end = header_part.find("\r\n");
        string req_line = (line_end != string::npos) ? header_part.substr(0, line_end) : header_part;
        string method = "", path = "";
        size_t p1 = req_line.find(' ');
        size_t p2 = req_line.find(' ', p1 + 1);
        if (p1 != string::npos && p2 != string::npos) {
            method = req_line.substr(0, p1);
            path = req_line.substr(p1 + 1, p2 - p1 - 1);
        }

        // Parse Content-Length for POST body
        size_t content_len = 0;
        size_t cl_pos = header_part.find("Content-Length: ");
        if (cl_pos == string::npos) cl_pos = header_part.find("content-length: ");
        if (cl_pos != string::npos) {
            size_t cl_end = header_part.find("\r\n", cl_pos);
            content_len = stoul(header_part.substr(cl_pos + 16, cl_end - (cl_pos + 16)));
        }

        // Read remaining body bytes
        while (body_part.length() < content_len) {
            size_t needed = content_len - body_part.length();
            int n = recv(client_sock, buf, min((size_t)sizeof(buf), needed), 0);
            if (n <= 0) break;
            body_part.append(buf, n);
        }

        // Handle CORS Pre-flight
        if (method == "OPTIONS") {
            string resp = "HTTP/1.1 204 No Content\r\n"
                          "Access-Control-Allow-Origin: *\r\n"
                          "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                          "Access-Control-Allow-Headers: Content-Type\r\n"
                          "Connection: close\r\n\r\n";
            send(client_sock, resp.c_str(), (int)resp.length(), 0);
        }
        // Handle Static HTML UI
        else if (method == "GET" && (path == "/" || path == "/index.html")) {
            ifstream file("index.html", ios::binary);
            if (file) {
                file.seekg(0, ios::end);
                size_t fsize = file.tellg();
                file.seekg(0, ios::beg);
                string html(fsize, ' ');
                file.read(&html[0], fsize);
                file.close();

                string resp = "HTTP/1.1 200 OK\r\n"
                              "Content-Type: text/html\r\n"
                              "Content-Length: " + to_string(html.length()) + "\r\n"
                              "Access-Control-Allow-Origin: *\r\n"
                              "Connection: close\r\n\r\n" + html;
                send(client_sock, resp.c_str(), (int)resp.length(), 0);
            } else {
                string err = "HTTP/1.1 404 Not Found\r\nContent-Length: 20\r\n\r\nindex.html not found";
                send(client_sock, err.c_str(), (int)err.length(), 0);
            }
        }
        // Handle C++ Compression API
        else if (method == "POST" && path == "/api/compress") {
            cout << "[C++ Backend] Received compress request: " << body_part.size() << " bytes" << endl;

            ofstream in_file("__tmp_compress_in.bin", ios::binary);
            in_file.write(body_part.data(), body_part.size());
            in_file.close();

            // Run full.cpp C++ compression engine
            compress("__tmp_compress_in.bin", "__tmp_compress_out.dfx");

            ifstream out_file("__tmp_compress_out.dfx", ios::binary);
            string compressed;
            if (out_file) {
                out_file.seekg(0, ios::end);
                size_t csize = out_file.tellg();
                out_file.seekg(0, ios::beg);
                compressed.resize(csize);
                out_file.read(&compressed[0], csize);
                out_file.close();
            }
            remove("__tmp_compress_in.bin");
            remove("__tmp_compress_out.dfx");

            cout << "[C++ Backend] Compression done: " << compressed.size() << " bytes" << endl;

            string resp = "HTTP/1.1 200 OK\r\n"
                          "Content-Type: application/octet-stream\r\n"
                          "Content-Length: " + to_string(compressed.size()) + "\r\n"
                          "Access-Control-Allow-Origin: *\r\n"
                          "X-Powered-By: DeflateX-Cpp-Engine\r\n"
                          "Connection: close\r\n\r\n" + compressed;
            send(client_sock, resp.c_str(), (int)resp.length(), 0);
        }
        // Handle C++ Decompression API
        else if (method == "POST" && path == "/api/decompress") {
            cout << "[C++ Backend] Received decompress request: " << body_part.size() << " bytes" << endl;

            ofstream in_file("__tmp_decompress_in.dfx", ios::binary);
            in_file.write(body_part.data(), body_part.size());
            in_file.close();

            // Run full.cpp C++ decompression engine
            decompress("__tmp_decompress_in.dfx", "__tmp_decompress_out.txt");

            ifstream out_file("__tmp_decompress_out.txt", ios::binary);
            string decompressed;
            if (out_file) {
                out_file.seekg(0, ios::end);
                size_t dsize = out_file.tellg();
                out_file.seekg(0, ios::beg);
                decompressed.resize(dsize);
                out_file.read(&decompressed[0], dsize);
                out_file.close();
            }
            remove("__tmp_decompress_in.dfx");
            remove("__tmp_decompress_out.txt");

            cout << "[C++ Backend] Decompression done: " << decompressed.size() << " bytes" << endl;

            string resp = "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/plain\r\n"
                          "Content-Length: " + to_string(decompressed.size()) + "\r\n"
                          "Access-Control-Allow-Origin: *\r\n"
                          "X-Powered-By: DeflateX-Cpp-Engine\r\n"
                          "Connection: close\r\n\r\n" + decompressed;
            send(client_sock, resp.c_str(), (int)resp.length(), 0);
        }
        else {
            string err = "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\nNot Found";
            send(client_sock, err.c_str(), (int)err.length(), 0);
        }

        closesocket(client_sock);
    }

#ifdef _WIN32
    WSACleanup();
#endif
}

int main(int argc, char* argv[]) {
    // If command-line flags are passed for batch processing:
    if (argc >= 4 && string(argv[1]) == "-c") {
        compress(argv[2], argv[3]);
        return 0;
    }
    if (argc >= 4 && string(argv[1]) == "-d") {
        decompress(argv[2], argv[3]);
        return 0;
    }

    // Default: Start backend server and automatically open browser UI
    cout << "========================================================" << endl;
    cout << "   DEFLATE-X File Compressor (C++ Core Backend)         " << endl;
    cout << "========================================================" << endl;
    cout << "[*] Starting C++ Backend on: http://localhost:8080" << endl;
    cout << "[*] Opening Web UI in your default browser..." << endl;

#ifdef _WIN32
    system("start http://localhost:8080");
#elif __APPLE__
    system("open http://localhost:8080");
#else
    system("xdg-open http://localhost:8080 &");
#endif

    // Runs the server directly (no terminal typing required!)
    runServer(8080);
    return 0;
}