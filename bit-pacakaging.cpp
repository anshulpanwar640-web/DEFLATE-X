#include <iostream>
#include <fstream>
using namespace std;
class BitWriter{
private:
    unsigned char buf;
    int bit_count;
    ofstream file;
public:
    BitWriter(const char* filename){
        file.open(filename, ios::binary);
        buf=0;
        bit_count=0;
    }
    void writeBit(int bit){
        buf=buf<<1;
        if(bit==1){
            buf=buf|1;
        }
        bit_count=bit_count+1;
        if(bit_count==8){
            file.put(buf);
            buf=0;
            bit_count=0;
        }
    }
    void flush(){
        if(bit_count>0){
            buf=buf<<(8-bit_count);
            file.put(buf);
            buf=0;
            bit_count=0;
        }
    }
    ~BitWriter(){
        flush();
        file.close();
    }
};
class BitReader{
private:
    unsigned char buf;
    int bit_count;
    ifstream file;
public:
    BitReader(const char* filename) {
        file.open(filename, ios::binary);
        buf=0;
        bit_count=0;
    }
    int readBit() {
        if (bit_count==0) {
            buf=file.get();
            bit_count=8;
        }
        int bit=(buf >> 7)&1;
        buf=buf<<1;
        bit_count=bit_count-1;
        return bit;
    }
    ~BitReader(){
        file.close();
    }
};
int main(){
    int testBits[]={1, 0, 1, 1, 0, 0, 1, 0, 1, 1, 1, 0, 1};
    int numBits=13;
    cout<<"Writing bits to demo.bin"<<endl;
    {
        BitWriter writer("demo.bin");
        for (int i=0;i<numBits;i++) {
            writer.writeBit(testBits[i]);
        }
    }
    cout << "Now reading bits back" << endl;
    BitReader reader("demo.bin");
    int allMatch = 1;
    for (int i = 0; i < numBits; i++) {
        int bit = reader.readBit();
        cout << "bit " << i << " wrote " << testBits[i] << " read " << bit << endl;
        if (bit != testBits[i]) {
            allMatch = 0;
        }
    }
    if (allMatch == 1) {
        cout << "PASS all bits matched" << endl;
    } else {
        cout << "FAIL some bits did not match" << endl;
    }
    return 0;
}
