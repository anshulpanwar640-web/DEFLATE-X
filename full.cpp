#include <iostream>
#include <fstream>
#include <queue>
#include <vector>
#include <string>
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

int main() {
    int choice;
    char inName[200];
    char outName[200];

    cout << "DeflateX file compressor" << endl;
    cout << "1. Compress" << endl;
    cout << "2. Decompress" << endl;
    cout << "Enter choice: ";
    cin >> choice;
    cout << "Input file name: ";
    cin >> inName;
    cout << "Output file name: ";
    cin >> outName;

    if (choice == 1) {
        compress(inName, outName);
        int before = getSize(inName);
        int after = getSize(outName);
        cout << "Original size   : " << before << " bytes" << endl;
        cout << "Compressed size : " << after << " bytes" << endl;
        if (before > 0) {
            cout << "Compressed file is " << (100.0 * after / before) << " % of original" << endl;
        }
    } else if (choice == 2) {
        decompress(inName, outName);
        cout << "Done, file saved as " << outName << endl;
    } else {
        cout << "Wrong choice" << endl;
    }

    return 0;
}
