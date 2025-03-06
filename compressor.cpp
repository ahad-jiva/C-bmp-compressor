#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <queue>
#include <vector>

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef unsigned int LONG;

struct bfh {
    WORD bfType; //specifies the file type
    DWORD bfSize; //specifies the size in bytes of the bitmap file
    WORD bfReserved1; //reserved; must be 0
    WORD bfReserved2; //reserved; must be 0
    DWORD bfOffBits; //species the offset in bytes from the bitmapfileheader to the bitmap bits
};

struct bih {
    DWORD biSize; //specifies the number of bytes required by the struct
    LONG biWidth; //specifies width in pixels
    LONG biHeight; //species height in pixels
    WORD biPlanes; //specifies the number of color planes, must be 1
    WORD biBitCount; //specifies the number of bit per pixel
    DWORD biCompression;//spcifies the type of compression
    DWORD biSizeImage; //size of image in bytes
    LONG biXPelsPerMeter; //number of pixels per meter in x axis
    LONG biYPelsPerMeter; //number of pixels per meter in y axis
    DWORD biClrUsed; //number of colors used by the bitmap
    DWORD biClrImportant; //number of colors that are important
};

struct compressed_image_header {
    LONG width;
    LONG height;
    DWORD quality;
    DWORD freq_table_size;
};

struct htn {
    int value;
    htn *left;
    htn *right;

    htn(int v, htn *l, htn *r) {
        value = v;
        left = l;
        right = r;
    }
};

struct color_freq {
    int color;
    int freq;
};

struct bitpattern {
    std::vector<BYTE> pattern;
    int digit = 0;
    void writebit(BYTE bit){
        pattern.push_back(bit);
        digit++;
    }
    void reset(){
        digit = 0;
    }
};

struct bitarray {
    std::vector<BYTE> bitdata;
    int bitp = 0;
    void putbit(BYTE bit){
        int i = bitp / 8;

        int shift_amount = 7 - (bitp % 8);
        bitdata[i] = bitdata[i] | (bit << shift_amount);
        bitp++;
    }

    void putbitpattern(bitpattern &bits){
        for (int u = 0; u < bits.digit; u++){
            putbit(bits.pattern[u]);
        }
    }

    bitarray(int size): bitdata(size, 0){}
};

// compare function for qsort of color_freq
int compare(const void *a, const void *b) {
    color_freq *c1 = (color_freq *)a;
    color_freq *c2 = (color_freq *)b;
    return (c1 -> freq) - (c2 -> freq);
};

int main(){ // program name, img path, quality (1-10)

    // reading input bitmap
    FILE *file = fopen("blend images/Mario.bmp", "rb");
    int quality = 5; // TODO: make sure this uses cli args
    bfh fileHeader;
    bih infoHeader;
    fread(&fileHeader, sizeof(bfh), 1, file);
    fread(&infoHeader, sizeof(bih), 1, file);
    BYTE *img_data = (BYTE *)malloc(infoHeader.biSizeImage);
    fread(img_data, infoHeader.biSizeImage, 1, file);
    fclose(file);

    // compression factor and division of pixel values
    for (int i = 0; i < infoHeader.biSizeImage; i++) {
        img_data[i] = (img_data[i] * quality) / 10; // quality = 10 means lossless, quality = 1 means 1/10 of original
    }

    // creating individual color arrays
    BYTE *red_data = (BYTE *)malloc(infoHeader.biSizeImage / 3);
    BYTE *green_data = (BYTE *)malloc(infoHeader.biSizeImage / 3);
    BYTE *blue_data = (BYTE *)malloc(infoHeader.biSizeImage / 3);
    for (int i = 0; i < infoHeader.biSizeImage; i += 3) {
        red_data[i / 3] = img_data[i];
        green_data[i / 3] = img_data[i + 1];
        blue_data[i / 3] = img_data[i + 2];
    }

    // frequency tables of each color
    color_freq *red_freq = (color_freq *)malloc(256 * sizeof(color_freq));
    color_freq *green_freq = (color_freq *)malloc(256 * sizeof(color_freq));
    color_freq *blue_freq = (color_freq *)malloc(256 * sizeof(color_freq));
    for (int i = 0; i < 256; i++) {
        red_freq[i].color = i;
        red_freq[i].freq = 0;
        green_freq[i].color = i;
        green_freq[i].freq = 0;
        blue_freq[i].color = i;
        blue_freq[i].freq = 0;
    }
    for (int i = 0; i < infoHeader.biSizeImage / 3; i++) {
        red_freq[red_data[i]].freq++;
        green_freq[green_data[i]].freq++;
        blue_freq[blue_data[i]].freq++;
    }

    // sorting each frequency table with qsort (least frequency first)
    qsort(red_freq, 256, sizeof(color_freq), compare);
    qsort(green_freq, 256, sizeof(color_freq), compare);
    qsort(blue_freq, 256, sizeof(color_freq), compare);

    // declaring vectors for each color
    std::vector<htn> red_list;
    std::vector<htn> green_list;
    std::vector<htn> blue_list;

    // creating huffman trees for each color
    for (int i = 0; i < 256; i++) {
        red_list.push_back(htn(red_freq[i].freq, NULL, NULL));
        green_list.push_back(htn(green_freq[i].freq, NULL, NULL));
        blue_list.push_back(htn(blue_freq[i].freq, NULL, NULL));
    }
    while (red_list.size() > 1) {
        htn *left = (htn *)malloc(sizeof(htn));
        htn *right = (htn *)malloc(sizeof(htn));
        *left = red_list[0];
        *right = red_list[1];
        red_list.erase(red_list.begin());
        red_list.erase(red_list.begin());
        red_list.push_back(htn(left -> value + right -> value, left, right));
        qsort(red_list.data(), red_list.size(), sizeof(htn), compare);
    }
    while (green_list.size() > 1) {
        htn *left = (htn *)malloc(sizeof(htn));
        htn *right = (htn *)malloc(sizeof(htn));
        *left = green_list[0];
        *right = green_list[1];
        green_list.erase(green_list.begin());
        green_list.erase(green_list.begin());
        green_list.push_back(htn(left -> value + right -> value, left, right));
        qsort(green_list.data(), green_list.size(), sizeof(htn), compare);
    }
    while (blue_list.size() > 1) {
        htn *left = (htn *)malloc(sizeof(htn));
        htn *right = (htn *)malloc(sizeof(htn));
        *left = blue_list[0];
        *right = blue_list[1];
        blue_list.erase(blue_list.begin());
        blue_list.erase(blue_list.begin());
        blue_list.push_back(htn(left -> value + right -> value, left, right));
        qsort(blue_list.data(), blue_list.size(), sizeof(htn), compare);
    }


    // printing huffman tree roots
    printf("Red Huffman Tree Root: %d\n", red_list[0].value);
    printf("Green Huffman Tree Root: %d\n", green_list[0].value);
    printf("Blue Huffman Tree Root: %d\n", blue_list[0].value);

    // writing red huffman codes to a bitpattern
    bitpattern red_pattern;
    std::queue<htn> red_queue;
    red_queue.push(red_list[0]);
    while (!red_queue.empty()) {
        htn current = red_queue.front();
        red_queue.pop();
        if (current.left == NULL && current.right == NULL) {
            red_pattern.writebit(1);
            for (int i = 0; i < 8; i++) {
                red_pattern.writebit((current.value >> i) & 1);
            }
        } else {
            red_pattern.writebit(0);
            red_queue.push(*(current.left));
            red_queue.push(*(current.right));
        }
    }

    // writing green huffman codes to a bitpattern
    bitpattern green_pattern;
    std::queue<htn> green_queue;
    green_queue.push(green_list[0]);
    while (!green_queue.empty()) {
        htn current = green_queue.front();
        green_queue.pop();
        if (current.left == NULL && current.right == NULL) {
            green_pattern.writebit(1);
            for (int i = 0; i < 8; i++) {
                green_pattern.writebit((current.value >> i) & 1);
            }
        } else {
            green_pattern.writebit(0);
            green_queue.push(*(current.left));
            green_queue.push(*(current.right));
        }
    }

    // writing blue huffman codes to a bitpattern
    bitpattern blue_pattern;
    std::queue<htn> blue_queue;
    blue_queue.push(blue_list[0]);
    while (!blue_queue.empty()) {
        htn current = blue_queue.front();
        blue_queue.pop();
        if (current.left == NULL && current.right == NULL) {
            blue_pattern.writebit(1);
            for (int i = 0; i < 8; i++) {
                blue_pattern.writebit((current.value >> i) & 1);
            }
        } else {
            blue_pattern.writebit(0);
            blue_queue.push(*(current.left));
            blue_queue.push(*(current.right));
        }
    }

    // writing red bitpattern to bitarray
    bitarray red_bitarray(red_pattern.pattern.size());
    red_bitarray.putbitpattern(red_pattern);

    // writing green bitpattern to bitarray
    bitarray green_bitarray(green_pattern.pattern.size());
    green_bitarray.putbitpattern(green_pattern);

    // writing blue bitpattern to bitarray
    bitarray blue_bitarray(blue_pattern.pattern.size());
    blue_bitarray.putbitpattern(blue_pattern);

    // writing header and patterns to a new file
    FILE *compressed_file = fopen("compressed_image.xxx", "wb");

    // setting up header
    compressed_image_header header;
    header.width = infoHeader.biWidth;
    header.height = infoHeader.biHeight;
    header.quality = quality;
    header.freq_table_size = 256;

    // writing header and patterns to file
    fwrite(&header, sizeof(compressed_image_header), 1, compressed_file);
    fwrite(red_freq, sizeof(color_freq), 256, compressed_file);
    fwrite(green_freq, sizeof(color_freq), 256, compressed_file);
    fwrite(blue_freq, sizeof(color_freq), 256, compressed_file);
    fwrite(red_bitarray.bitdata.data(), sizeof(BYTE), red_bitarray.bitdata.size(), compressed_file);
    fwrite(green_bitarray.bitdata.data(), sizeof(BYTE), green_bitarray.bitdata.size(), compressed_file);
    fwrite(blue_bitarray.bitdata.data(), sizeof(BYTE), blue_bitarray.bitdata.size(), compressed_file);

    return 0;
}