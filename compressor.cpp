#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <queue>
#include <vector>
#include <stack>
#include <algorithm>

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

struct bitpattern {
    std::vector<BYTE> pattern;
    int digit = 0;
    int colorvalue;
    void writebit(BYTE bit){
        pattern.push_back(bit);
        digit++;
    }
    void reset(){
        digit = 0;
    }
    void remove_last(){
        digit--;
    }
};

struct htn {
    int value, freq;
    htn *left = nullptr;
    htn *right = nullptr;
    int index;
    int il = -1;
    int ir = -1;

    htn(){
        value = 0;
        left = NULL;
        right = NULL;
        il = -1;
        ir = -1;
    }

    htn(int v, int f, htn *l, htn *r) {
        value = v;
        freq = f;
        left = l;
        right = r;
    }

    void get_bit_pattern(std::vector<bitpattern> &bp) {
        std::stack<std::pair<htn*, bitpattern> > s;
        s.push(std::make_pair(this, bitpattern()));
    
        while (!s.empty()) {
            auto [node, current] = s.top();
            s.pop();
            if (node->left == NULL && node->right == NULL) {
                current.colorvalue = node->value;
                bp.push_back(current);
            } else {
                if (node->right) {
                    bitpattern right_copy = current;
                    right_copy.writebit(1);
                    s.push(std::make_pair(node->right, right_copy));
                }
                if (node->left) {
                    bitpattern left_copy = current;
                    left_copy.writebit(0);
                    s.push(std::make_pair(node->left, left_copy));
                }
            }
        }
    }

    void assign_indices(int &i){
        index = i;
        i++;
        if (left){
            left->assign_indices(i);
            il = left->index;
        }
        if (right){
            right->assign_indices(i);
            ir = right->index;
        }
    }

    int count_children(htn *node){
        if (node == NULL){
            return 0;
        }
        return 1 + count_children(node->left) + count_children(node->right);
    }

    void write_to_array(std::vector<htn> &arr, htn *node){
        if (node == NULL){
            return;
        }
        arr[node->index] = *node;
        if (node->left){
            arr[node->left->index] = *node->left;
            write_to_array(arr, node->left);
        }
        if (node->right){
            arr[node->right->index] = *node->right;
            write_to_array(arr, node->right);
        }
    }
        
};

struct color_freq {
    int color;
    int freq;
};

struct bitarray {
    BYTE *bitdata = NULL;
    int bitp = 0;
    void putbit(BYTE bit){
        int i = bitp / 8;

        int shift_amount = 7 - (bitp % 8);
        bitdata[i] |= (bit << shift_amount);
        bitp++;
    }

    void putbitpattern(bitpattern &bits){
        for (int u = 0; u < bits.digit; u++){
            putbit(bits.pattern[u]);
        }
    }

    bitarray(int size){
        bitdata = new BYTE[size]();
    }
};

bool compare_color_freq(color_freq a, color_freq b){
    return a.freq < b.freq;
}

bool compare_htn(htn a, htn b){
    return a.freq > b.freq;
}

int main(int argc, char *argv[]){ // program name, img path, quality (1-10)

    // reading input bitmap
    FILE *file = fopen(argv[1], "rb");
    int quality = atoi(argv[2]);
    bfh fileHeader;
    bih infoHeader;
    fread(&fileHeader, sizeof(bfh), 1, file);
    fread(&infoHeader, sizeof(bih), 1, file);
    BYTE *img_data = (BYTE *)malloc(infoHeader.biSizeImage);
    fread(img_data, infoHeader.biSizeImage, 1, file);
    fclose(file);

    // pixel sizes and padding calculations
    int pixel_width = infoHeader.biWidth * 3;
    int padding;
    if (pixel_width % 4 == 0){
        padding = 0;
    } else {
        padding = 4 - (pixel_width % 4);
    }

    pixel_width += padding;

    // creating individual color arrays
    BYTE *red_data = (BYTE *)malloc(infoHeader.biSizeImage);
    BYTE *green_data = (BYTE *)malloc(infoHeader.biSizeImage);
    BYTE *blue_data = (BYTE *)malloc(infoHeader.biSizeImage);
    for (int row = 0; row < infoHeader.biHeight; row++){
        for (int col = 0; col < infoHeader.biWidth; col++){
            int index = row * pixel_width + col * 3;
            red_data[row * infoHeader.biWidth + col] = img_data[index + 2];
            green_data[row * infoHeader.biWidth + col] = img_data[index + 1];
            blue_data[row * infoHeader.biWidth + col] = img_data[index];
        }
    }

    int quality_factor = quality * 10;
    for (int i = 0; i < infoHeader.biSizeImage; i++) {
        red_data[i] = (red_data[i] / quality_factor);
        green_data[i] = (green_data[i] / quality_factor);
        blue_data[i] = (blue_data[i] / quality_factor);
    }

    // frequency tables of each color
    std::vector<color_freq> red_freq(256);
    std::vector<color_freq> green_freq(256);
    std::vector<color_freq> blue_freq(256);
    for (int i = 0; i < 256; i++) {
        red_freq[i].color = i;
        red_freq[i].freq = 0;

        green_freq[i].color = i;
        green_freq[i].freq = 0;

        blue_freq[i].color = i;
        blue_freq[i].freq = 0;
    }
    for (int i = 0; i < infoHeader.biHeight * infoHeader.biWidth; i++) {
        red_freq[red_data[i]].freq++;
        green_freq[green_data[i]].freq++;
        blue_freq[blue_data[i]].freq++;
    }

    // sorting each frequency table with qsort (least frequency first)
    std::sort(red_freq.begin(), red_freq.end(), compare_color_freq);
    std::sort(green_freq.begin(), green_freq.end(), compare_color_freq);
    std::sort(blue_freq.begin(), blue_freq.end(), compare_color_freq);

    // declaring vectors for each color, these will be used to construct the trees
    std::vector<htn> red_list;
    std::vector<htn> green_list;
    std::vector<htn> blue_list;

    // creating red huff tree, skipping colors that don't appear
    for (int i = 0; i < 256; i++) {
        if (red_freq[i].freq == 0) {
            continue;
        }
        red_list.emplace_back(htn(red_freq[i].color, red_freq[i].freq, NULL, NULL));
    }
    while (red_list.size() > 1) {
        std::sort(red_list.begin(), red_list.end(), compare_htn);
        htn *left = new htn(red_list[0]);
        htn *right = new htn(red_list[1]);
        red_list.erase(red_list.begin(), red_list.begin() + 2);
        red_list.emplace_back(htn(-1, left->freq + right->freq, left, right));
    }

    // creating green huff tree, skipping colors that don't appear
    for (int i = 0; i < 256; i++) {
        if (green_freq[i].freq == 0) {
            continue;
        }
        green_list.emplace_back(htn(green_freq[i].color, green_freq[i].freq, NULL, NULL));
    }
    while (green_list.size() > 1) {
        std::sort(green_list.begin(), green_list.end(), compare_htn);
        htn *left = new htn(green_list[0]);
        htn *right = new htn(green_list[1]);
        green_list.erase(green_list.begin(), green_list.begin() + 2);
        green_list.emplace_back(htn(-1, left->freq + right->freq, left, right));

    }

    // creating blue huff tree, skipping colors that don't appear
    for (int i = 0; i < 256; i++) {
        if (blue_freq[i].freq == 0) {
            continue;
        }
        blue_list.emplace_back(htn(blue_freq[i].color, blue_freq[i].freq, NULL, NULL));
    }
    while (blue_list.size() > 1) {
        std::sort(blue_list.begin(), blue_list.end(), compare_htn);
        htn *left = new htn(blue_list[0]);
        htn *right = new htn(blue_list[1]);
        blue_list.erase(blue_list.begin(), blue_list.begin() + 2);
        blue_list.emplace_back(htn(-1, left->freq + right->freq, left, right));

    }

    // writing red huffman codes to a vector of bitpatterns
    std::vector<bitpattern> red_patterns;
    red_list[0].get_bit_pattern(red_patterns);

    // writing green huffman codes to a vector of bitpatterns
    std::vector<bitpattern> green_patterns;
    green_list[0].get_bit_pattern(green_patterns);

    // writing blue huffman codes to a vector of bitpatterns
    std::vector<bitpattern> blue_patterns;
    blue_list[0].get_bit_pattern(blue_patterns);

    // combining bitpatterns from vectors into a single bitpattern using image data
    bitpattern red_pattern;
    bitpattern green_pattern;
    bitpattern blue_pattern;
    for (int i = 0; i < infoHeader.biHeight * infoHeader.biWidth; i++){
        for (int j = 0; j < red_patterns.size(); j++){
            if (red_data[i] == red_patterns[j].colorvalue){
                for (int k = 0; k < red_patterns[j].digit; k++){
                    red_pattern.writebit(red_patterns[j].pattern[k]);
                }
            }
        }
        for (int j = 0; j < green_patterns.size(); j++){
            if (green_data[i] == green_patterns[j].colorvalue){
                for (int k = 0; k < green_patterns[j].digit; k++){
                    green_pattern.writebit(green_patterns[j].pattern[k]);
                }
            }
        }
        for (int j = 0; j < blue_patterns.size(); j++){
            if (blue_data[i] == blue_patterns[j].colorvalue){
                for (int k = 0; k < blue_patterns[j].digit; k++){
                    blue_pattern.writebit(blue_patterns[j].pattern[k]);
                }
            }
        }
    }
    
    // writing red bitpattern to bitarray
    bitarray red_bitarray((red_pattern.pattern.size() + 7) / 8);
    red_bitarray.putbitpattern(red_pattern);

    // writing green bitpattern to bitarray
    bitarray green_bitarray((green_pattern.pattern.size() + 7) / 8);
    green_bitarray.putbitpattern(green_pattern);

    // writing blue bitpattern to bitarray
    bitarray blue_bitarray((blue_pattern.pattern.size() + 7) / 8);
    blue_bitarray.putbitpattern(blue_pattern);

    // assigning indices for each tree
    int i = 0;
    red_list[0].assign_indices(i);
    i = 0;
    green_list[0].assign_indices(i);
    i = 0;
    blue_list[0].assign_indices(i);

    // getting tree node counts
    int red_node_count = red_list[0].count_children(&red_list[0]);
    int green_node_count = green_list[0].count_children(&green_list[0]);
    int blue_node_count = blue_list[0].count_children(&blue_list[0]);

    // creating arrays for huff trees
    std::vector<htn> red_arr(red_node_count);
    std::vector<htn> green_arr(green_node_count);
    std::vector<htn> blue_arr(blue_node_count);

    // copying huff trees to arrays using left and right indices
    red_list[0].write_to_array(red_arr, &red_list[0]);
    green_list[0].write_to_array(green_arr, &green_list[0]);
    blue_list[0].write_to_array(blue_arr, &blue_list[0]);
    
    // creating compressed file
    FILE *compressed_file = fopen("compressed_image.xxx", "wb");

    // setting up header
    compressed_image_header header;
    header.width = infoHeader.biWidth;
    header.height = infoHeader.biHeight;
    header.quality = quality;
    header.red_tree_size = red_node_count;
    header.green_tree_size = green_node_count;
    header.blue_tree_size = blue_node_count;
    header.red_bitdata_size = red_pattern.pattern.size();
    header.green_bitdata_size = green_pattern.pattern.size();
    header.blue_bitdata_size = blue_pattern.pattern.size();

    // writing header, huff arrays, and bitarrays to file
    fwrite(&header, sizeof(compressed_image_header), 1, compressed_file);
    for (int i = 0; i < red_node_count; i++){
        fwrite(&red_arr[i].value, sizeof(int), 1, compressed_file);
        fwrite(&red_arr[i].freq, sizeof(int), 1, compressed_file);
        fwrite(&red_arr[i].il, sizeof(int), 1, compressed_file);
        fwrite(&red_arr[i].ir, sizeof(int), 1, compressed_file);
    }
    for (int i = 0; i < green_node_count; i++){
        fwrite(&green_arr[i].value, sizeof(int), 1, compressed_file);
        fwrite(&green_arr[i].freq, sizeof(int), 1, compressed_file);
        fwrite(&green_arr[i].il, sizeof(int), 1, compressed_file);
        fwrite(&green_arr[i].ir, sizeof(int), 1, compressed_file);
    }
    for (int i = 0; i < blue_node_count; i++){
        fwrite(&blue_arr[i].value, sizeof(int), 1, compressed_file);
        fwrite(&blue_arr[i].freq, sizeof(int), 1, compressed_file);
        fwrite(&blue_arr[i].il, sizeof(int), 1, compressed_file);
        fwrite(&blue_arr[i].ir, sizeof(int), 1, compressed_file);
    }
    fwrite(red_bitarray.bitdata, 1, (red_pattern.digit+7)/8, compressed_file);
    fwrite(green_bitarray.bitdata, 1, (green_pattern.digit+7)/8, compressed_file);
    fwrite(blue_bitarray.bitdata, 1, (blue_pattern.digit+7)/8, compressed_file);

    // writing original headers to reconstruct image
    fwrite(&fileHeader, sizeof(bfh), 1, compressed_file);
    fwrite(&infoHeader, sizeof(bih), 1, compressed_file);

    // cleanup
    fclose(compressed_file);
    free(img_data);
    free(red_data);
    free(green_data);
    free(blue_data);

    return 0;
}