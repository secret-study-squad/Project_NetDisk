#include <my_header.h>
#include "hash.h"

int main(int argc, char* argv[]){                                  
    char hash[65] = {0};

    compute_file_sha256(argv[1], hash);

    printf("%s\n", hash);

    return 0;
}

