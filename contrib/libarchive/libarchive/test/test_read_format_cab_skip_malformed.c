#include "test.h"

DEFINE_TEST(test_read_format_cab_skip_malformed)
{
    /* Reference to the malformed CAB file */
    const char *refname = "test_read_format_cab_skip_malformed.cab";
    struct archive *a;
    struct archive_entry *ae;
    void *buffer;
    size_t buffersize;

    /* Extract the reference file into the test sandbox */
    extract_reference_file(refname);

    /* Read the entire file into memory */
    buffer = slurpfile(&buffersize, "%s", refname);
    assert(buffer != NULL);

    /* Initialize the archive reader */
    assert((a = archive_read_new()) != NULL);
    assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
    assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));

    /* Read from memory (a prerequisite for triggering this specific bug) */
    assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buffer, buffersize));

    /* Simulate the parsing flow to trigger the implicit skip routine */
    while (archive_read_next_header(a, &ae) == ARCHIVE_OK) {
        const void *buff;
        size_t size_read;
        int64_t offset;
        while (archive_read_data_block(a, &buff, &size_read, &offset) == ARCHIVE_OK) {
            /* Consume data. This will fail quickly due to the malformed payload. */
        }
    }

    /* Clean up. If the patch is effective, the program reaches here safely. */
    assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
    assertEqualInt(ARCHIVE_OK, archive_read_free(a));
    free(buffer);
}