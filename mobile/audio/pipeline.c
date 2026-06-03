#include "pipeline.h"
#include <stdlib.h>
#include <string.h>

static int node_id_counter = 0;

audio_pipeline_t* pipe_init(void) {
    audio_pipeline_t *pipe = calloc(1, sizeof(audio_pipeline_t));
    if (!pipe) return NULL;

    // Create dummy head for double-linked list
    pipe->head = calloc(1, sizeof(audio_node_t));
    if (!pipe->head) {
        free(pipe);
        return NULL;
    }
    pipe->head->next = pipe->head;
    pipe->head->prev = pipe->head;

    // In a real implementation, we would create actual source, mixer, sink nodes here
    // For now, we just return the pipeline structure
    return pipe;
}

int pipe_add_source(audio_pipeline_t *pipe, const char *name, 
                   audio_format_t format, uint32_t rate, audio_channels_t channels) {
    if (!pipe || !name) return -1;

    audio_node_t *node = calloc(1, sizeof(audio_node_t));
    if (!node) return -1;

    strncpy(node->name, name, sizeof(node->name) - 1);
    node->name[sizeof(node->name) - 1] = '\0';
    node->id = node_id_counter++;

    // Insert at tail
    node->prev = pipe->head->prev;
    node->next = pipe->head;
    pipe->head->prev->next = node;
    pipe->head->prev = node;

    pipe->node_count++;
    return node->id;
}

int pipe_add_sink(audio_pipeline_t *pipe, const char *name) {
    if (!pipe || !name) return -1;

    audio_node_t *node = calloc(1, sizeof(audio_node_t));
    if (!node) return -1;

    strncpy(node->name, name, sizeof(node->name) - 1);
    node->name[sizeof(node->name) - 1] = '\0';
    node->id = node_id_counter++;

    // Insert at tail
    node->prev = pipe->head->prev;
    node->next = pipe->head;
    pipe->head->prev->next = node;
    pipe->head->prev = node;

    pipe->node_count++;
    return node->id;
}

int pipe_connect(audio_pipeline_t *pipe, int src_idx, int dst_idx) {
    // In a real implementation, we would set up the connection between nodes
    // For now, we just validate the indices exist
    if (!pipe) return -1;

    // Simple validation: we assume nodes exist if indices are non-negative and less than node_count
    // This is a simplification; in reality we'd traverse the list to find nodes by id
    if (src_idx < 0 || src_idx >= pipe->node_count ||
        dst_idx < 0 || dst_idx >= pipe->node_count) {
        return -1;
    }

    // Connection successful
    return 0;
}

int pipe_process(audio_pipeline_t *pipe, void *buffer, uint32_t frames) {
    // In a real implementation, we would process audio through the chain
    // For now, we just return success (no actual processing)
    if (!pipe || !buffer) return -1;
    // Dummy processing: do nothing
    return 0;
}

void pipe_free(audio_pipeline_t *pipe) {
    if (!pipe) return;

    // Free all nodes
    audio_node_t *current = pipe->head->next;
    while (current != pipe->head) {
        audio_node_t *next = current->next;
        free(current);
        current = next;
    }
    free(pipe->head);
    free(pipe);
}