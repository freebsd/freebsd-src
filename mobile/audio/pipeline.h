#ifndef _AUDIO_PIPELINE_H_
#define _AUDIO_PIPELINE_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    FORMAT_S16_LE,
    FORMAT_S24_LE,
    FORMAT_S32_LE,
    FORMAT_FLOAT_LE
} audio_format_t;

typedef enum {
    CHANNEL_MONO = 1,
    CHANNEL_STEREO = 2,
    CHANNEL_5_1 = 6,
    CHANNEL_7_1 = 8
} audio_channels_t;

typedef struct audio_node {
    int id;
    char name[32];
    struct audio_node *next;
    struct audio_node *prev;
    // Node-specific data would be here in a real implementation
} audio_node_t;

typedef struct {
    audio_node_t *source;
    audio_node_t *mixer;
    audio_node_t *sink;
    audio_node_t *head; // Dummy head for double-linked list
    int node_count;
} audio_pipeline_t;

/**
 * Initialize audio pipeline with default source -> mixer -> sink
 * @return pointer to initialized pipeline or NULL on failure
 */
audio_pipeline_t* pipe_init(void);

/**
 * Add an audio source node to the pipeline
 * @param pipe pipeline to modify
 * @param name name of the source
 * @param format audio format
 * @param rate sample rate in Hz
 * @param channels channel configuration
 * @return index of the added node or -1 on failure
 */
int pipe_add_source(audio_pipeline_t *pipe, const char *name, 
                   audio_format_t format, uint32_t rate, audio_channels_t channels);

/**
 * Add an audio sink node to the pipeline
 * @param pipe pipeline to modify
 * @param name name of the sink
 * @return index of the added node or -1 on failure
 */
int pipe_add_sink(audio_pipeline_t *pipe, const char *name);

/**
 * Connect two nodes in the pipeline
 * @param pipe pipeline containing the nodes
 * @param src_idx index of source node
 * @param dst_idx index of destination node
 * @return 0 on success, -1 on failure
 */
int pipe_connect(audio_pipeline_t *pipe, int src_idx, int dst_idx);

/**
 * Process audio data through the pipeline
 * @param pipe pipeline to process with
 * @param buffer input/output buffer (in-place processing)
 * @param frames number of audio frames to process
 * @return 0 on success, -1 on failure
 */
int pipe_process(audio_pipeline_t *pipe, void *buffer, uint32_t frames);

/**
 * Free all resources associated with the pipeline
 * @param pipe pipeline to free
 */
void pipe_free(audio_pipeline_t *pipe);

#endif // _AUDIO_PIPELINE_H_