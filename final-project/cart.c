/* cart.c: shopping carts and checkout (pricing, FIFO, promotion, gifts). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "store.h"

Cart *find_cart(const Store *s, const char *cid)
{
    Cart *c;
    for (c = s->carts; c != NULL; c = c->next)
        if (strcmp(c->id, cid) == 0)
            return c;
    return NULL;
}

void free_cart_items(Cart *c)
{
    CartItem *it = c->items;
    while (it != NULL) {
        CartItem *next = it->next;
        free(it);
        it = next;
    }
    c->items = NULL;
}

void free_all_carts(Store *s)
{
    Cart *c = s->carts;
    while (c != NULL) {
        Cart *next = c->next;
        free_cart_items(c);
        free(c);
        c = next;
    }
    s->carts = NULL;
}

/* Removes a cart from the list and frees it. */
static void close_cart(Store *s, Cart *cart)
{
    Cart **link = &s->carts;
    while (*link != NULL && *link != cart)
        link = &(*link)->next;
    if (*link != NULL)
        *link = cart->next;
    free_cart_items(cart);
    free(cart);
}

void cmd_cart(Store *s, const char *cid)
{
    Cart *c = malloc(sizeof *c);
    if (c == NULL)
        exit(1);
    strcpy(c->id, cid);
    c->items = NULL;
    c->next = s->carts;
    s->carts = c;
    printf("OK CART %s\n", cid);
}

void cmd_add(Store *s, const char *cid, const char *pid, int qty)
{
    Cart *cart = find_cart(s, cid);    /* the cart is checked first */
    CartItem **link, *it;

    if (cart == NULL) {
        printf("ERR NO_CART\n");
        return;
    }
    if (find_product(s, pid) == NULL) {
        printf("ERR NO_PRODUCT\n");
        return;
    }
    link = &cart->items;               /* items stay sorted by product ID */
    while (*link != NULL && strcmp((*link)->pid, pid) < 0)
        link = &(*link)->next;
    if (*link != NULL && strcmp((*link)->pid, pid) == 0) {
        it = *link;
        it->qty += qty;
    } else {
        it = malloc(sizeof *it);
        if (it == NULL)
            exit(1);
        strcpy(it->pid, pid);
        it->qty = qty;
        it->next = *link;
        *link = it;
    }
    printf("OK ADD %s %s %d\n", cid, pid, it->qty);
}

/* Quantity of product pid in the cart (0 if absent). */
static int cart_qty(const Cart *cart, const char *pid)
{
    const CartItem *it;
    for (it = cart->items; it != NULL; it = it->next)
        if (strcmp(it->pid, pid) == 0)
            return it->qty;
    return 0;
}

/* Step 1: go through the items in ID order, first error wins.
 * Returns 1 if the checkout can go on, 0 if an error was printed. */
static int check_cart(const Store *s, const Cart *cart)
{
    const CartItem *it;
    for (it = cart->items; it != NULL; it = it->next) {
        const Product *p = find_product(s, it->pid);
        if (p->price == NO_PRICE) {
            printf("ERR NO_PRICE\n");
            return 0;
        }
        if (product_stock(p) < it->qty) {
            printf("ERR INSUFFICIENT %s\n", it->pid);
            return 0;
        }
    }
    return 1;
}

/* Step 4: picks the gift product: smallest (price - cost of the oldest batch),
 * ties go to the smallest ID (the list is sorted, so the first one wins). */
static Product *pick_gift(const Store *s)
{
    Product *p, *best = NULL;
    long long best_margin = 0;
    for (p = s->products; p != NULL; p = p->next) {
        long long margin;
        if (p->price == NO_PRICE || p->head == NULL)
            continue;
        margin = (long long)p->price - p->head->unit_cost;
        if (best == NULL || margin < best_margin) {
            best = p;
            best_margin = margin;
        }
    }
    return best;
}

void cmd_checkout(Store *s, const char *cid)
{
    Cart *cart = find_cart(s, cid);
    const CartItem *it;
    long long total = 0, cost = 0, gift_cost = 0;
    int gifts_due, gifts_given = 0;
    int promo_on = 0;

    if (cart == NULL) {
        printf("ERR NO_CART\n");
        return;
    }
    if (!check_cart(s, cart)) {
        close_cart(s, cart);           /* the cart is closed even on failure */
        return;
    }

    /* The promotion applies if the cart has at least N of product A. */
    if (s->promo.active && cart_qty(cart, s->promo.a) >= s->promo.n)
        promo_on = 1;

    /* Steps 2 and 3: price every line and take the stock (FIFO).
     * The check in step 1 passed, so nothing can fail any more and
     * the receipt can be printed while we go. */
    printf("RECEIPT %s\n", cart->id);
    for (it = cart->items; it != NULL; it = it->next) {
        Product *p = find_product(s, it->pid);
        int pct = 100;
        long long sub;
        if (promo_on && strcmp(it->pid, s->promo.b) == 0)
            pct = s->promo.p;
        sub = ((long long)p->price * it->qty * pct + 50) / 100;
        total += sub;
        cost += take_fifo(p, it->qty);
        if (pct != 100)
            printf("  %s x%d @%d *%d%% = %lld\n", p->id, it->qty, p->price, pct, sub);
        else
            printf("  %s x%d @%d = %lld\n", p->id, it->qty, p->price, sub);
    }
    printf("TOTAL %lld\n", total);
    printf("COST %lld\n", cost);

    /* Step 4: one gift per full 1000 dollars of TOTAL. */
    gifts_due = (int)(total / 1000);
    while (gifts_given < gifts_due) {
        Product *g = pick_gift(s);
        int unit;
        if (g == NULL)
            break;
        unit = g->head->unit_cost;
        take_fifo(g, 1);
        gift_cost += unit;
        gifts_given++;
        printf("GIFT %s @%d\n", g->id, unit);
    }
    if (gifts_given == 0)
        printf("GIFT none\n");
    if (gifts_given < gifts_due)
        printf("GIFT_SHORT %d\n", gifts_due - gifts_given);

    /* Step 5: totals. */
    printf("PROFIT %lld\n", total - cost - gift_cost);
    s->revenue += total;
    s->cogs += cost;
    s->gift_cost += gift_cost;
    close_cart(s, cart);
}
