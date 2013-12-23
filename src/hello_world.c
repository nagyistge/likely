#include <stdio.h>
#include <likely.h>

int main()
{
    printf("Reading input image...\n");
    likely_mat lenna = likely_read("../data/misc/lenna.tiff");
    if (lenna) {
        printf("Width: %zu\nHeight: %zu\n", lenna->columns, lenna->rows);
    } else {
        printf("Failed to read!\n");
        return -1;
    }

    printf("Retrieving IR for expression...\n");
    likely_ir ir = likely_parse("divide{2}");
    if (!ir) {
        printf("Failed to interpret!\n");
        return -1;
    }

    printf("Compiling source code...\n");
    likely_function darken = likely_compile(ir);
    if (!darken) {
        printf("Failed to compile!\n");
        return -1;
    }

    printf("Calling compiled function...\n");
    likely_mat dark_lenna = darken(lenna);
    if (!dark_lenna) {
        printf("Failed to execute!\n");
        return -1;
    }

    printf("Writing output image...\n");
    likely_write(dark_lenna, "dark_lenna.png");

    printf("Releasing data...\n");
    likely_release(lenna);
    likely_release(dark_lenna);

    printf("Done!\n");
    return 0;
}
