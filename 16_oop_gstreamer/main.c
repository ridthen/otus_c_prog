#include <stdio.h>
#include <stdlib.h>

#define HEADER_SIZE 44

typedef unsigned char byte;

union int_data {
    int int_value;
    byte int_bytes[4];
};

union short_data {
    short short_value;
    byte short_bytes[2];
};

struct header_struct {
    char chunk_id[4];
    union int_data chunk_size;
    char format[4];
    char subchunk1_id[4];
    union int_data subchunk1_size;
    union short_data audio_format;
    union short_data num_channels;
    union int_data sample_rate;
    union int_data byte_rate;
    union short_data block_align;
    union short_data bits_per_sample;
    char subchunk2_id[4];
    union int_data subchunk2_size;
};

union header_data {
    byte data[HEADER_SIZE];
    struct header_struct header;
};

void read_header(FILE *fp, union header_data *file_bytes, char *file_name) {
    // Verifying int is 4 bytes long
    if (sizeof(int) != 4) {
        fprintf(stderr, "Error: machine data type INT isn't 4 bytes long\nCan't process wav header ");
        exit(4);
    }

    int i;
    for (i = 0; i < HEADER_SIZE && (file_bytes->data[i] = getc(fp)) != EOF; i++)
        ;

    if (i != HEADER_SIZE) {
        fprintf(stderr, "Error: file %s header data incomplete\n", file_name);
        exit(3);
    }
}

void print_header_data(union header_data *file_bytes) {
    printf("ChunkID: %.4s\n", file_bytes->header.chunk_id);
    printf("ChunkSize: %d\n", file_bytes->header.chunk_size.int_value);
    printf("Format: %.4s\n", file_bytes->header.format);
    printf("Subchunk1 ID: %.4s\n", file_bytes->header.subchunk1_id);
    printf("Subchunk1 Size: %d\n", file_bytes->header.subchunk1_size.int_value);
    printf("Audio Format: %d\n", file_bytes->header.audio_format.short_value);
    printf("Num Channels: %d\n", file_bytes->header.num_channels.short_value);
    printf("Sample Rate: %d\n", file_bytes->header.sample_rate.int_value);
    printf("ByteRate: %d\n", file_bytes->header.byte_rate.int_value);
    printf("Block Align: %d\n", file_bytes->header.block_align.short_value);
    printf("Bits per sample: %d\n", file_bytes->header.bits_per_sample.short_value);
    printf("Subchunk2 Id: %.4s\n", file_bytes->header.subchunk2_id);
    printf("Subchunk2 size: %d\n", file_bytes->header.subchunk2_size.int_value);
}

short *read_data(FILE *fp_in, union header_data *header_bytes){
    short *data;

    short num_channels = header_bytes->header.num_channels.short_value;
    short bits_per_sample = header_bytes->header.bits_per_sample.short_value;
    int data_size = header_bytes->header.subchunk2_size.int_value;

    int bytes_per_sample = bits_per_sample / 8;
    int num_samples_per_channel = data_size / bytes_per_sample / num_channels;

    data = (short *) malloc(num_samples_per_channel * num_channels * sizeof(short));

    if (data == NULL) {
        fprintf(stderr, "Error: Couldn't allocate memory for samples\n");
        exit(6);
    }

    int i, j;
    short *dp = data;
    for (i=0; i < num_samples_per_channel; i++) {
        for (j=0; j < num_channels; j++) {
            if (fread(dp, bytes_per_sample, 1, fp_in) != 1) {
                fprintf(stderr, "Error: Couldn't read all samples\n");
                exit(6);
            }
            dp++;
        }
    }

    return data;
}


int main(int argc, char *argv[]) {
    FILE *fp_in = NULL, *fp_out = NULL;
    char *input_file_name;

    fp_in = fopen(argv[1], "r");
    input_file_name = argv[1];

    if (fp_in == NULL) {
        fprintf(stderr, "Specify an input file\n"
                "Usage: wave gstLmywavstreamer <file>\n");
        return 2;
    }

    union header_data *header;
    short *data;
    
    header = (union header_data *) malloc(sizeof(union header_data));
    read_header(fp_in, header, input_file_name);
    data = read_data(fp_in, header);

    print_header_data(header);
    fclose(fp_in);
    free(header);
    free(data);

}