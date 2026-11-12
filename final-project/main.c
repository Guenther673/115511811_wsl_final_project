/* main.c: reads one command per line and calls the functions of the other files. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "store.h"

int main(void)
{
    Store store;
    char line[256];

    store_init(&store);
    while (fgets(line, sizeof line, stdin) != NULL) {
        char cmd[32] = "", a1[160] = "", a2[160] = "", a3[160] = "", a4[160] = "";
        int n = sscanf(line, "%31s %159s %159s %159s %159s", cmd, a1, a2, a3, a4);

        if (n < 1)
            continue;
        if (strcmp(cmd, "IMPORT") == 0)
            cmd_import(&store, a1);
        else if (strcmp(cmd, "PRICE") == 0)
            cmd_price(&store, a1, atoi(a2));
        else if (strcmp(cmd, "PROMO") == 0)
            cmd_promo(&store, a1, atoi(a2), a3, atoi(a4));
        else if (strcmp(cmd, "CART") == 0)
            cmd_cart(&store, a1);
        else if (strcmp(cmd, "ADD") == 0)
            cmd_add(&store, a1, a2, atoi(a3));
        else if (strcmp(cmd, "CHECKOUT") == 0)
            cmd_checkout(&store, a1);
        else if (strcmp(cmd, "QUERY") == 0)
            cmd_query(&store, a1);
        else if (strcmp(cmd, "REPORT") == 0)
            cmd_report(&store);
        else if (strcmp(cmd, "SAVE") == 0)
            cmd_save(&store, a1);
        else if (strcmp(cmd, "LOAD") == 0)
            cmd_load(&store, a1);
        else if (strcmp(cmd, "QUIT") == 0)
            break;
    }
    store_clear(&store);
    printf("BYE\n");
    return 0;
}
