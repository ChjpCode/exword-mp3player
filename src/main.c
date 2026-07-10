#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscalls/syscalls.h>

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_SIMD 
#include "minimp3.h"
#include "bg_image.h"

extern void graphics_init(int width, int height, void *framebuffer);
extern void graphics_clear(unsigned short color);
extern void draw_rect(int x, int y, int w, int h, unsigned short color);
extern void draw_string(int x, int y, const char *str, unsigned short color);
extern unsigned int sys_get_key(void);
extern void pcm_init(int sample_rate, int channels);
extern void pcm_submit(short *buffer, int samples);

#define COLOR_BG        0xE79E  // Light Cyan
#define COLOR_TEXT      0x0000  // Black
#define COLOR_CASSETTE  0x4208  // Dark gray
#define COLOR_PINK      0xF812
#define COLOR_BLUE      0x041F
#define COLOR_ACCENT    0xF812  // Pink progress bar

#define IN_BUF_SIZE     16384
#define PCM_BUF_SIZE    1152 * 2

#define MAX_SONGS       100

char playlist[MAX_SONGS][256];
int song_count = 0;
int current_song_index = 0;

void draw_walkman_ui_static(const char *format, const char *filename) {
    memcpy((void*)0xAC200000, bg_image, 528 * 320 * 2);
    
    char header[64];
    sprintf(header, "[%d/%d] %s", current_song_index + 1, song_count, format);
    draw_string(20, 30, header, COLOR_TEXT);
    
    char name_trunc[64];
    strncpy(name_trunc, filename, 60);
    name_trunc[60] = '\0';
    draw_string(20, 50, name_trunc, COLOR_TEXT);

    draw_string(20, 140, "[ UP/DN = TUA ] [ L/R = CHUYEN BAI ]", COLOR_TEXT);
}

void update_walkman_progress(int current_sec, int total_sec) {
    draw_rect(20, 90, 160, 16, COLOR_BG); 
    
    char time_str[32];
    sprintf(time_str, "%02d:%02d / %02d:%02d", current_sec / 60, current_sec % 60, total_sec / 60, total_sec % 60);
    draw_string(20, 90, time_str, COLOR_TEXT);
    
    draw_rect(20, 110, 160, 6, COLOR_CASSETTE);
    int progress_w = total_sec > 0 ? ((current_sec * 160) / total_sec) : 0;
    if (progress_w > 160) progress_w = 160;
    draw_rect(20, 110, progress_w, 6, COLOR_ACCENT);
}

unsigned int read_u32_le(unsigned char *buf) {
    return (buf[0]) | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

unsigned short read_u16_le(unsigned char *buf) {
    return (buf[0]) | (buf[1] << 8);
}

void scan_songs() {
    int handle;
    char filename[256];
    unsigned long type;
    
    song_count = 0;
    
    if (sys_findfirst("*.mp3", &handle, filename, &type) == 0) {
        do {
            if (type == 1 && song_count < MAX_SONGS) {
                strcpy(playlist[song_count++], filename);
            }
        } while (sys_findnext(handle, filename, &type) == 0);
        sys_findclose(handle);
    }
    
    if (sys_findfirst("*.wav", &handle, filename, &type) == 0) {
        do {
            if (type == 1 && song_count < MAX_SONGS) {
                strcpy(playlist[song_count++], filename);
            }
        } while (sys_findnext(handle, filename, &type) == 0);
        sys_findclose(handle);
    }
}

// Returns 0 to exit, 1 for next, -1 for prev
int play_wav(FILE *fp, const char* filename) {
    unsigned char header[44];
    if (fread(header, 1, 44, fp) != 44) return 1; // skip if invalid

    if (strncmp((char*)header, "RIFF", 4) != 0 || strncmp((char*)header + 8, "WAVE", 4) != 0) {
        return 1;
    }

    int sample_rate = 44100;
    int channels = 2;
    int bytes_per_sample = 2;

    int offset = 12;
    while (offset < 44) {
        if (strncmp((char*)header + offset, "fmt ", 4) == 0) {
            channels = read_u16_le(header + offset + 10);
            sample_rate = read_u32_le(header + offset + 12);
            bytes_per_sample = read_u16_le(header + offset + 22) / 8;
            break;
        }
        offset++;
    }

    long data_size = 0;
    fseek(fp, 0, SEEK_SET);
    unsigned char chunk_head[8];
    fseek(fp, 12, SEEK_SET);
    while (fread(chunk_head, 1, 8, fp) == 8) {
        int c_size = read_u32_le(chunk_head + 4);
        if (strncmp((char*)chunk_head, "data", 4) == 0) {
            data_size = c_size;
            break;
        }
        fseek(fp, c_size, SEEK_CUR);
    }

    if (data_size == 0) {
        fseek(fp, 44, SEEK_SET);
        data_size = 99999999; 
    }

    pcm_init(sample_rate, channels);

    int bytes_per_sec = sample_rate * channels * bytes_per_sample;
    int total_seconds = data_size / bytes_per_sec;
    if (total_seconds <= 0 || total_seconds > 36000) total_seconds = 0;
    int current_seconds = 0;
    
    draw_walkman_ui_static("WAV Audio", filename);
    update_walkman_progress(current_seconds, total_seconds);

    static short pcm_buf[2048];
    int samples_to_read = 2048; 
    int sample_counter = 0;
    
    long data_start_offset = fseek(fp, 0, SEEK_CUR); // get current position

    while (1) {
        unsigned int key = sys_get_key();
        if (key == 0x01 || key == 0x02) return 0;
        if (key == 88) return 1; // KEY_RIGHT
        if (key == 87) return -1; // KEY_LEFT
        
        if (key == 77) { // KEY_UP (FF 10s)
            current_seconds += 10;
            if (current_seconds > total_seconds) current_seconds = total_seconds;
            fseek(fp, data_start_offset + current_seconds * bytes_per_sec, SEEK_SET);
            update_walkman_progress(current_seconds, total_seconds);
        }
        if (key == 86) { // KEY_DOWN (RW 10s)
            current_seconds -= 10;
            if (current_seconds < 0) current_seconds = 0;
            fseek(fp, data_start_offset + current_seconds * bytes_per_sec, SEEK_SET);
            update_walkman_progress(current_seconds, total_seconds);
        }

        int read_bytes = fread(pcm_buf, 1, samples_to_read * channels * bytes_per_sample, fp);
        if (read_bytes <= 0) break;

        int read_samples = read_bytes / (channels * bytes_per_sample);
        pcm_submit(pcm_buf, read_samples * channels);

        sample_counter += read_samples;
        if (sample_counter >= sample_rate) {
            current_seconds += sample_counter / sample_rate;
            sample_counter %= sample_rate;
            update_walkman_progress(current_seconds, total_seconds);
        }
    }
    return 1;
}

int play_mp3(FILE *fp, const char* filename) {
    static mp3dec_t mp3d;
    mp3dec_init(&mp3d);
    mp3dec_frame_info_t info;
    static unsigned char in_buf[IN_BUF_SIZE];
    static short pcm_buf[PCM_BUF_SIZE];

    int current_sample_rate = 44100;
    int current_channels = 2;
    pcm_init(current_sample_rate, current_channels);

    int bytes_left = 0, read_bytes = 0, current_seconds = 0, total_seconds = 0; 
    // MP3 duration is hard to estimate without full scan. 
    // We estimate using file size and an assumed 128kbps (16000 bytes/sec).
    fseek(fp, 0, SEEK_END);
    long file_size = fseek(fp, 0, SEEK_CUR);
    fseek(fp, 0, SEEK_SET);
    
    total_seconds = file_size / 16000;
    
    draw_walkman_ui_static("MP3 Audio", filename);
    update_walkman_progress(current_seconds, total_seconds);

    while (1) {
        unsigned int key = sys_get_key();
        if (key == 0x01 || key == 0x02) return 0;
        if (key == 88) return 1; // KEY_RIGHT
        if (key == 87) return -1; // KEY_LEFT
        
        if (key == 77) { // KEY_UP (FF 10s)
            current_seconds += 10;
            if (current_seconds > total_seconds) current_seconds = total_seconds;
            fseek(fp, current_seconds * 16000, SEEK_SET);
            bytes_left = 0; // reset decode buffer
            update_walkman_progress(current_seconds, total_seconds);
        }
        if (key == 86) { // KEY_DOWN (RW 10s)
            current_seconds -= 10;
            if (current_seconds < 0) current_seconds = 0;
            fseek(fp, current_seconds * 16000, SEEK_SET);
            bytes_left = 0; // reset decode buffer
            update_walkman_progress(current_seconds, total_seconds);
        }

        if (bytes_left < IN_BUF_SIZE) {
            read_bytes = fread(in_buf + bytes_left, 1, IN_BUF_SIZE - bytes_left, fp);
            bytes_left += read_bytes;
        }
        if (bytes_left == 0) break;

        int samples = mp3dec_decode_frame(&mp3d, in_buf, bytes_left, pcm_buf, &info);

        if (samples > 0) {
            if (info.hz != current_sample_rate || info.channels != current_channels) {
                current_sample_rate = info.hz;
                current_channels = info.channels;
                pcm_init(current_sample_rate, current_channels);
            }
            pcm_submit(pcm_buf, samples * info.channels);

            static int sample_counter = 0;
            sample_counter += samples;
            if (sample_counter >= current_sample_rate) {
                current_seconds++;
                sample_counter -= current_sample_rate;
                update_walkman_progress(current_seconds, total_seconds);
            }
            bytes_left -= info.frame_bytes;
            memmove(in_buf, in_buf + info.frame_bytes, bytes_left);
        } else {
            if (bytes_left > 0) {
                bytes_left--;
                memmove(in_buf, in_buf + 1, bytes_left);
            }
            if (read_bytes == 0 && bytes_left == 0) break;
        }
    }
    return 1;
}

int main() {
    graphics_init(528, 320, (void*)0xAC200000);
    
    scan_songs();
    
    if (song_count == 0) {
        graphics_clear(COLOR_BG);
        draw_string(50, 150, "ERROR: Khong tim thay bai hat nao trong thu muc goc!", COLOR_TEXT);
        while(sys_get_key() == 0);
        graphics_clear(0x0000);
        return -2;
    }

    current_song_index = 0;
    
    while (current_song_index >= 0 && current_song_index < song_count) {
        char *filename = playlist[current_song_index];
        FILE *fp = fopen(filename, "rb");
        int action = 1; // default next
        
        if (fp) {
            int len = strlen(filename);
            if (len > 4 && (strcmp(filename + len - 4, ".wav") == 0 || strcmp(filename + len - 4, ".WAV") == 0)) {
                action = play_wav(fp, filename);
            } else {
                action = play_mp3(fp, filename);
            }
            fclose(fp);
        } else {
            // failed to open, skip
            action = 1;
        }
        
        if (action == 0) {
            break;
        } else if (action == 1) {
            current_song_index++;
            if (current_song_index >= song_count) current_song_index = 0; // loop playlist
        } else if (action == -1) {
            current_song_index--;
            if (current_song_index < 0) current_song_index = song_count - 1; // loop playlist
        }
    }

    graphics_clear(0x0000);
    return 0;
}
