/* store.c: products, FIFO batches, import, price, promo, query, report. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "store.h"

void store_init(Store *s)
{
    s->products = NULL;
    s->carts = NULL;
    s->promo.active = 0;
    s->next_batch = 1;
    s->revenue = 0;
    s->cogs = 0;
    s->gift_cost = 0;
}

/* Frees all products, batches and carts. Afterwards the store is empty. */
void store_clear(Store *s)
{
    Product *p = s->products;
    while (p != NULL) {
        Product *next_p = p->next;
        Batch *b = p->head;
        while (b != NULL) {
            Batch *next_b = b->next;
            free(b);
            b = next_b;
        }
        free(p);
        p = next_p;
    }
    free_all_carts(s);
    store_init(s);
}

Product *find_product(const Store *s, const char *pid)
{
    Product *p;
    for (p = s->products; p != NULL; p = p->next) {
        int c = strcmp(p->id, pid);
        if (c == 0)
            return p;
        if (c > 0)          /* list is sorted: we are already past it */
            break;
    }
    return NULL;
}

/* Creates a product and inserts it so the list stays sorted by ID. */
Product *add_product(Store *s, const char *pid, const char *name)
{
    Product *p = malloc(sizeof *p);
    Product **link = &s->products;
    if (p == NULL)
        exit(1);
    strcpy(p->id, pid);
    strcpy(p->name, name);
    p->price = NO_PRICE;
    p->head = p->tail = NULL;
    while (*link != NULL && strcmp((*link)->id, pid) < 0)
        link = &(*link)->next;
    p->next = *link;
    *link = p;
    return p;
}

/* Appends a new batch at the tail (= newest end) of the product. */
Batch *append_batch(Product *p, int id, int qty, int unit_cost)
{
    Batch *b = malloc(sizeof *b);
    if (b == NULL)
        exit(1);
    b->id = id;
    b->qty = qty;
    b->unit_cost = unit_cost;
    b->next = NULL;
    if (p->tail == NULL)
        p->head = b;
    else
        p->tail->next = b;
    p->tail = b;
    return b;
}

int product_stock(const Product *p)
{
    int sum = 0;
    const Batch *b;
    for (b = p->head; b != NULL; b = b->next)
        sum += b->qty;
    return sum;
}

/* Takes qty units from the oldest batches first. Used-up batches are
 * removed and freed at once. The caller must make sure the stock is enough.
 * Returns the cost of the units taken. */
int take_fifo(Product *p, int qty)
{
    int cost = 0;
    while (qty > 0 && p->head != NULL) {
        Batch *b = p->head;
        int take = qty < b->qty ? qty : b->qty;
        cost += take * b->unit_cost;
        b->qty -= take;
        qty -= take;
        if (b->qty == 0) {
            p->head = b->next;
            if (p->head == NULL)      /* the only batch is gone: tail must go too */
                p->tail = NULL;
            free(b);
        }
    }
    return cost;
}

/* Removes a trailing "\n" and "\r" from a line. */
static void chomp(char *line)
{
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
        line[--n] = '\0';
}

void cmd_import(Store *s, const char *file)
{
    FILE *f = fopen(file, "r");
    char line[256];
    int count = 0;

    if (f == NULL) {
        printf("ERR FILE_OPEN\n");
        return;
    }
    while (fgets(line, sizeof line, f) != NULL) {
        /* line format: <pid>###<name>@@@<qty>&&&<unit_cost> */
        char *sep1, *sep2, *sep3;
        char pid[ID_LEN], name[NAME_LEN];
        size_t len;
        Product *p;

        chomp(line);
        if (line[0] == '\0')
            continue;
        sep1 = strstr(line, "###");
        if (sep1 == NULL)
            continue;
        sep2 = strstr(sep1 + 3, "@@@");
        if (sep2 == NULL)
            continue;
        sep3 = strstr(sep2 + 3, "&&&");
        if (sep3 == NULL)
            continue;

        len = (size_t)(sep1 - line);
        if (len >= ID_LEN)
            len = ID_LEN - 1;
        memcpy(pid, line, len);
        pid[len] = '\0';

        len = (size_t)(sep2 - (sep1 + 3));      /* name keeps all its spaces */
        if (len >= NAME_LEN)
            len = NAME_LEN - 1;
        memcpy(name, sep1 + 3, len);
        name[len] = '\0';

        p = find_product(s, pid);
        if (p == NULL)
            p = add_product(s, pid, name);
        append_batch(p, s->next_batch++, atoi(sep2 + 3), atoi(sep3 + 3));
        count++;
    }
    fclose(f);
    printf("OK IMPORT %d\n", count);
}

void cmd_price(Store *s, const char *pid, int price)
{
    Product *p = find_product(s, pid);
    if (p == NULL) {
        printf("ERR NO_PRODUCT\n");
        return;
    }
    p->price = price;
    printf("OK PRICE %s %d\n", pid, price);
}

void cmd_promo(Store *s, const char *a, int n, const char *b, int p)
{
    s->promo.active = 1;
    strcpy(s->promo.a, a);
    s->promo.n = n;
    strcpy(s->promo.b, b);
    s->promo.p = p;
    printf("OK PROMO\n");
}

void cmd_query(const Store *s, const char *pid)
{
    const Product *p = find_product(s, pid);
    const Batch *b;

    if (p == NULL) {
        printf("ERR NO_PRODUCT\n");
        return;
    }
    printf("QUERY %s %s price=", p->id, p->name);
    if (p->price == NO_PRICE)
        printf("none");
    else
        printf("%d", p->price);
    printf(" stock=%d\n", product_stock(p));
    for (b = p->head; b != NULL; b = b->next)
        printf("  #%d qty=%d cost=%d\n", b->id, b->qty, b->unit_cost);
}

void cmd_report(const Store *s)
{
    const Product *p;
    printf("REPORT\n");
    for (p = s->products; p != NULL; p = p->next) {
        int stock = product_stock(p);
        long long value = 0;
        const Batch *b;
        if (stock <= 0)
            continue;
        for (b = p->head; b != NULL; b = b->next)
            value += (long long)b->qty * b->unit_cost;
        printf("  %s stock=%d value=%lld\n", p->id, stock, value);
    }
    printf("REVENUE %lld\n", s->revenue);
    printf("COGS %lld\n", s->cogs);
    printf("GIFTCOST %lld\n", s->gift_cost);
    printf("PROFIT %lld\n", s->revenue - s->cogs - s->gift_cost);
}
