#include <stdio.h>
#include "subdirA/unit-a.h"
#include "subdirB/unit-b.h"

int main() {
    printf("Hello from main1.c!\n");
    unit_a();
    unit_b();
    return 0;
}
