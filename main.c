#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>
#include <stdatomic.h>
#include "src/ed_25519.h"
#include "src/sha512.h"

_Atomic int found = 0;
_Atomic unsigned long attempts = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
unsigned char target_bytes[32];
size_t target_byte_len;
unsigned char last_mask = 0; // 0 if full bytes, 0x0F for suffix partial, 0xF0 for prefix partial
unsigned char found_pub[32];
unsigned char found_priv[32];
int is_prefix_mode = 0; // 0 for suffix (default), 1 for prefix

int hex_to_bytes(const char *hex, unsigned char *bytes) {
    size_t hex_len = strlen(hex);
    size_t byte_idx = 0;
    while (hex_len >= 2) {
        if (sscanf(hex, "%2hhx", &bytes[byte_idx]) != 1) {
            return -1;
        }
        byte_idx++;
        hex += 2;
        hex_len -= 2;
    }
    if (hex_len == 1) {
        unsigned char nibble;
        if (sscanf(hex, "%1hhx", &nibble) != 1) {
            return -1;
        }
        bytes[byte_idx] = nibble << 4;
        byte_idx++;
    }
    return byte_idx; // Return the number of bytes converted
}

void bytes_to_hex(const unsigned char *bytes, char *hex, size_t len) {
    for (size_t i = 0; i < len; i++) {
        snprintf(hex + 2 * i, 3, "%02x", bytes[i]);
    }
}

int is_valid_hex(const char *hex) {
    size_t len = strlen(hex);
    if (len == 0) return 0;
    for (size_t i = 0; i < len; i++) {
        if (!isxdigit((unsigned char)hex[i])) return 0; // Check valid hex chars
    }
    return 1;
}

int check_vanity(const unsigned char *public_key, const unsigned char *target_bytes, size_t target_byte_len, unsigned char last_mask, int is_prefix) {
    if (is_prefix) {
        // Prefix mode: compare start of public_key
        for (size_t i = 0; i < target_byte_len; i++) {
            unsigned char mask = (i == target_byte_len - 1 && last_mask != 0) ? last_mask : 0xFF;
            if ((public_key[i] & mask) != (target_bytes[i] & mask)) {
                return 0;
            }
        }
    } else {
        // Suffix mode: compare end of public_key
        for (size_t i = 0; i < target_byte_len; i++) {
            size_t pub_idx = 32 - target_byte_len + i;
            unsigned char mask = (i == target_byte_len - 1 && last_mask != 0) ? last_mask : 0xFF;
            if ((public_key[pub_idx] & mask) != (target_bytes[i] & mask)) {
                return 0;
            }
        }
    }
    return 1;
}

void *worker(void *arg) {
    (void)arg; // Unused thread ID
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        perror("Failed to open /dev/urandom");
        exit(1);
    }

    unsigned char private_key[32];
    unsigned char public_key[32];

    while (atomic_load_explicit(&found, memory_order_relaxed) == 0) {
        if (read(fd, private_key, 32) != 32) {
            perror("Failed to read from /dev/urandom");
            exit(1);
        }
        ed25519_derive_pub(public_key, private_key);
        atomic_fetch_add_explicit(&attempts, 1, memory_order_relaxed);

        if (check_vanity(public_key, target_bytes, target_byte_len, last_mask, is_prefix_mode)) {
            pthread_mutex_lock(&mutex);
            if (atomic_load_explicit(&found, memory_order_relaxed) == 0) {
                memcpy(found_pub, public_key, 32);
                memcpy(found_priv, private_key, 32);
                atomic_store_explicit(&found, 1, memory_order_relaxed);
            }
            pthread_mutex_unlock(&mutex);
        }
    }

    close(fd);
    return NULL;
}

void *reporter(void *arg) {
    (void)arg;
    while (atomic_load_explicit(&found, memory_order_relaxed) == 0) {
        sleep(1);
        printf("Attempts: %lu\r", atomic_load_explicit(&attempts, memory_order_relaxed));
        fflush(stdout);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    const char *target = NULL;
    int num_threads = 0;
    int arg_index = 1;

    // Parse command-line arguments
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Usage: %s [--i-know-what-im-doing] <hex_target> <num_threads>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "--i-know-what-im-doing") == 0) {
        is_prefix_mode = 1;
        if (argc != 4) {
            fprintf(stderr, "Usage: %s --i-know-what-im-doing <hex_prefix> <num_threads>\n", argv[0]);
            return 1;
        }
        target = argv[2];
        num_threads = atoi(argv[3]);
    } else {
        is_prefix_mode = 0;
        if (argc != 3) {
            fprintf(stderr, "Usage: %s <hex_suffix> <num_threads>\n", argv[0]);
            return 1;
        }
        target = argv[1];
        num_threads = atoi(argv[2]);
    }

    if (!is_valid_hex(target)) {
        fprintf(stderr, "Error: Invalid hex %s '%s'\n", is_prefix_mode ? "prefix" : "suffix", target);
        return 1;
    }

    int converted_len = hex_to_bytes(target, target_bytes);
    if (converted_len < 0) {
        fprintf(stderr, "Error: Invalid hex %s '%s'\n", is_prefix_mode ? "prefix" : "suffix", target);
        return 1;
    }
    target_byte_len = (size_t)converted_len;

    size_t hex_len = strlen(target);
    last_mask = (hex_len % 2 == 1) ? (is_prefix_mode ? 0xF0 : 0x0F) : 0;

    if (num_threads <= 0) {
        fprintf(stderr, "Error: Invalid number of threads '%s'\n", argv[argc - 1]);
        return 1;
    }

    pthread_t *workers = malloc(num_threads * sizeof(pthread_t));
    if (!workers) {
        perror("Failed to allocate worker threads");
        return 1;
    }

    if (is_prefix_mode == 1) {
        printf("\nWarning! You are calculating a vanity **prefix**. You are responsible for ensuring that your public address doesn't collide with any neighbors. See the README for more details.\nPress ENTER to continue.");
        getchar();
    }

    printf("Started looking for %s '%s' using %d threads...\n", 
           is_prefix_mode ? "prefix" : "suffix", target, num_threads);

    pthread_t report_thread;
    if (pthread_create(&report_thread, NULL, reporter, NULL) != 0) {
        perror("Failed to create reporter thread");
        return 1;
    }

    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&workers[i], NULL, worker, NULL) != 0) {
            perror("Failed to create worker thread");
            return 1;
        }
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_join(workers[i], NULL);
    }
    pthread_join(report_thread, NULL);

    char hex_pubkey[65];
    char hex_privkey[65];
    bytes_to_hex(found_pub, hex_pubkey, 32);
    bytes_to_hex(found_priv, hex_privkey, 32);

    printf("\rFound vanity public key after %lu attempts!\n", atomic_load_explicit(&attempts, memory_order_relaxed));
    printf("Identity: %s%s\n", hex_privkey, hex_pubkey);

    free(workers);
    return 0;
}