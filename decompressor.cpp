#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <queue>
#include <vector>

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef unsigned int LONG;

#pragma pack(push, 1)
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
    LONG quality;
    LONG red_tree_size;
    LONG green_tree_size;
    LONG blue_tree_size;
    LONG red_bitdata_size;
    LONG green_bitdata_size;
    LONG blue_bitdata_size;
};
#pragma pack(pop)
struct htn {
    int value, freq;
    htn *left = nullptr;
    htn *right = nullptr;
    int index;
    int il, ir = -1;

    htn(){
        value = 0;
        left = NULL;
        right = NULL;
        il = 0;
        ir = 0;
    }

    htn(int v, int f, htn *l, htn *r) {
        value = v;
        freq = f;
        left = l;
        right = r;
        il = 0;
        ir = 0;
    }   
};

struct bitarray {
    BYTE *bitdata = NULL;
    int bitp = 0;
    bitarray(int size){
        bitdata = new BYTE[size];
    }

    BYTE getbit(){
        int i = bitp / 8;
        int actual_bitp = bitp - i * 8;

        BYTE bit = 1;
        int shift_amount = 7 - (actual_bitp);
        bit <<= shift_amount;

        BYTE targetbit = bitdata[i] & bit;
        bitp++;
        if (targetbit == 0){
            return 0;
        }
        else {
            return 1;
        }
    }
};

int main(){
    FILE *compressed_file = fopen("compressed_image.xxx", "rb");

    // read header
    compressed_image_header header;
    bfh fileHeader;
    bih infoHeader;
    fread(&header, sizeof(compressed_image_header), 1, compressed_file);

    // read huff arrays
    std::vector<htn> red_arr(header.red_tree_size);
    std::vector<htn> green_arr(header.green_tree_size);
    std::vector<htn> blue_arr(header.blue_tree_size);

    for (int i = 0; i < header.red_tree_size; i++){
        fread(&red_arr[i].value, sizeof(int), 1, compressed_file);
        fread(&red_arr[i].freq, sizeof(int), 1, compressed_file);
        fread(&red_arr[i].il, sizeof(int), 1, compressed_file);
        fread(&red_arr[i].ir, sizeof(int), 1, compressed_file);
    }
    for (int i = 0; i < header.green_tree_size; i++){
        fread(&green_arr[i].value, sizeof(int), 1, compressed_file);
        fread(&green_arr[i].freq, sizeof(int), 1, compressed_file);
        fread(&green_arr[i].il, sizeof(int), 1, compressed_file);
        fread(&green_arr[i].ir, sizeof(int), 1, compressed_file);
    }
    for (int i = 0; i < header.blue_tree_size; i++){
        fread(&blue_arr[i].value, sizeof(int), 1, compressed_file);
        fread(&blue_arr[i].freq, sizeof(int), 1, compressed_file);
        fread(&blue_arr[i].il, sizeof(int), 1, compressed_file);
        fread(&blue_arr[i].ir, sizeof(int), 1, compressed_file);
    }

    // read bitarrays
    bitarray red_bitarray(header.red_bitdata_size);
    bitarray green_bitarray(header.green_bitdata_size);
    bitarray blue_bitarray(header.blue_bitdata_size);

    fread(red_bitarray.bitdata, sizeof(BYTE), (header.red_bitdata_size/8)+1, compressed_file);
    fread(green_bitarray.bitdata, sizeof(BYTE), (header.green_bitdata_size/8)+1, compressed_file);
    fread(blue_bitarray.bitdata, sizeof(BYTE), (header.blue_bitdata_size/8)+1, compressed_file);

    // reading original headers
    fread(&fileHeader, sizeof(bfh), 1, compressed_file);
    fread(&infoHeader, sizeof(bih), 1, compressed_file);

    // close file
    fclose(compressed_file);

    // reconstruct red huff tree
    for (int i = 0; i < header.red_tree_size; i++){
        if (red_arr[i].il != -1){
            red_arr[i].left = &red_arr[red_arr[i].il];
        }
        if (red_arr[i].ir != -1){
            red_arr[i].right = &red_arr[red_arr[i].ir];
        }
    }

    // reconstruct green huff tree
    for (int i = 0; i < header.green_tree_size; i++){
        if (green_arr[i].il != -1){
            green_arr[i].left = &green_arr[green_arr[i].il];
        }
        if (green_arr[i].ir != -1){
            green_arr[i].right = &green_arr[green_arr[i].ir];
        }
    }

    // reconstruct blue huff tree
    for (int i = 0; i < header.blue_tree_size; i++){
        if (blue_arr[i].il != -1){
            blue_arr[i].left = &blue_arr[blue_arr[i].il];
        }
        if (blue_arr[i].ir != -1){
            blue_arr[i].right = &blue_arr[blue_arr[i].ir];
        }
    }

    // storing roots for ease of access
    htn *red_root = &red_arr[0];
    htn *green_root = &green_arr[0];
    htn *blue_root = &blue_arr[0];

    // creating arrays for each color for the image
    std::vector<BYTE> red_vals(infoHeader.biSizeImage);
    std::vector<BYTE> green_vals(infoHeader.biSizeImage);
    std::vector<BYTE> blue_vals(infoHeader.biSizeImage);

    // traversing red huff tree with bitarray getbit()
    htn *current = red_root;
    int byte_index = 0;
    while (red_bitarray.bitp < header.red_bitdata_size){
        BYTE bit = red_bitarray.getbit();
        if (bit == 0){
            current = current->left;
        }
        else {
            current = current->right;
        }
        if (current->left == NULL && current->right == NULL){
            red_vals[byte_index] = current->value * header.quality;
            byte_index++;
            current = red_root;
        }
    }

    // traversing green huff tree with bitarray getbit()
    current = green_root;
    byte_index = 0;
    while (green_bitarray.bitp < header.green_bitdata_size){
        BYTE bit = green_bitarray.getbit();
        if (bit == 0){
            current = current->left;
        }
        else {
            current = current->right;
        }
        if (current->left == NULL && current->right == NULL){
            green_vals[byte_index] = current->value * header.quality;
            byte_index++;
            current = green_root;
        }
    }

    // traversing blue huff tree with bitarray getbit()
    current = blue_root;
    byte_index = 0;
    while (blue_bitarray.bitp < header.blue_bitdata_size){
        BYTE bit = blue_bitarray.getbit();
        if (bit == 0){
            current = current->left;
        }
        else {
            current = current->right;
        }
        if (current->left == NULL && current->right == NULL){
            blue_vals[byte_index] = current->value * header.quality;
            byte_index++;
            current = blue_root;
        }
    }

    // writing decompressed image headers
    FILE *decompressed_file = fopen("decompressed_image.bmp", "wb");
    fwrite(&fileHeader, sizeof(bfh), 1, decompressed_file);
    fwrite(&infoHeader, sizeof(bih), 1, decompressed_file);
    
    // padding and pixel dimensions
    int pixel_width = header.width * 3;
    int padding;
    if (pixel_width % 4 == 0){
        padding = 0;
    } else {
        padding = 4 - (pixel_width % 4);
    }

    pixel_width += padding;

    // writing decompressed image data
    for (int row = 0; row < header.height; row++){
        for (int col = 0; col < pixel_width; col++){
            int index = row * pixel_width + col * 3;
            fwrite(&blue_vals[index], sizeof(BYTE), 1, decompressed_file);
            fwrite(&green_vals[index], sizeof(BYTE), 1, decompressed_file);
            fwrite(&red_vals[index], sizeof(BYTE), 1, decompressed_file);
        }

    }

    fclose(decompressed_file);

    return 0;
}