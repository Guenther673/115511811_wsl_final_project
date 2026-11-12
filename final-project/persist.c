/* persist.c: SAVE and LOAD (format of Appendix A). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "store.h"

void cmd_save(const Store *s, const char *file)
{
    FILE *f = fopen(file, "w");
    const Product *p;
    int product_count = 0, batch_count = 0;

    if (f == NULL) {
        printf("ERR FILE_OPEN\n");
        return;
    }
    for (p = s->products; p != NULL; p = p->next) {
        const Batch *b;
        product_count++;
        for (b = p->head; b != NULL; b = b->next)
            batch_count++;
    }

    fprintf(f, "NEXT_BATCH %d\n", s->next_batch);
    fprintf(f, "PRODUCTS %d\n", product_count);
    for (p = s->products; p != NULL; p = p->next) {
        fprintf(f, "%s %d\n", p->id, p->price);
        fprintf(f, "%s\n", p->name);
    }
    if (s->promo.active) {
        fprintf(f, "PROMO 1\n");
        fprintf(f, "%s %d %s %d\n", s->promo.a, s->promo.n, s->promo.b, s->promo.p);
    } else {
        fprintf(f, "PROMO 0\n");
    }
    fprintf(f, "TOTALS %lld %lld %lld\n", s->revenue, s->cogs, s->gift_cost);
    fprintf(f, "BATCHES %d\n", batch_count);
    for (p = s->products; p != NULL; p = p->next) {
        const Batch *b;
        for (b = p->head; b != NULL; b = b->next)
            fprintf(f, "%s %d %d %d\n", p->id, b->id, b->qty, b->unit_cost);
    }
    fclose(f);
    printf("OK SAVE\n");
}

/* Reads one line and removes only the line ending (names keep their spaces). */
static int read_line(FILE *f, char *line, int size)
{
    size_t n;
    if (fgets(line, size, f) == NULL)
        return 0;
    n = strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
        line[--n] = '\0';
    return 1;
}

/* The new data is read into a temporary Store. Only when the whole file is
 * read, it replaces the current data, so a failure changes nothing. */
void cmd_load(Store *s, const char *file)
{
    FILE *f = fopen(file, "r");
    Store t;
    char line[256], pid[ID_LEN], a[ID_LEN], b[ID_LEN];
    int count, i, id, qty, cost, n, pct;
    long long rev, cg, gc;

    if (f == NULL) {
        printf("ERR FILE_OPEN\n");
        return;
    }
    store_init(&t);

    if (read_line(f, line, sizeof line))
        sscanf(line, "NEXT_BATCH %d", &t.next_batch);

    count = 0;
    if (read_line(f, line, sizeof line))
        sscanf(line, "PRODUCTS %d", &count);
    for (i = 0; i < count; i++) {
        int price = NO_PRICE;
        Product *p;
        if (!read_line(f, line, sizeof line))
            break;
        sscanf(line, "%15s %d", pid, &price);
        if (!read_line(f, line, sizeof line))
            line[0] = '\0';
        line[NAME_LEN - 1] = '\0';
        p = add_product(&t, pid, line);
        p->price = price;
    }

    count = 0;
    if (read_line(f, line, sizeof line))
        sscanf(line, "PROMO %d", &count);
    if (count == 1 && read_line(f, line, sizeof line)) {
        if (sscanf(line, "%15s %d %15s %d", a, &n, b, &pct) == 4) {
            t.promo.active = 1;
            strcpy(t.promo.a, a);
            t.promo.n = n;
            strcpy(t.promo.b, b);
            t.promo.p = pct;
        }
    }

    if (read_line(f, line, sizeof line) &&
        sscanf(line, "TOTALS %lld %lld %lld", &rev, &cg, &gc) == 3) {
        t.revenue = rev;
        t.cogs = cg;
        t.gift_cost = gc;
    }

    count = 0;
    if (read_line(f, line, sizeof line))
        sscanf(line, "BATCHES %d", &count);
    for (i = 0; i < count; i++) {
        Product *p;
        if (!read_line(f, line, sizeof line))
            break;
        if (sscanf(line, "%15s %d %d %d", pid, &id, &qty, &cost) != 4)
            continue;
        p = find_product(&t, pid);
        if (p != NULL)
            append_batch(p, id, qty, cost);   /* file order = FIFO order */
    }
    fclose(f);

    store_clear(s);       /* frees the old data and closes all carts */
    *s = t;               /* take over the new lists */
    printf("OK LOAD\n");
}
