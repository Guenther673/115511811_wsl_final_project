#ifndef STORE_H
#define STORE_H

#include <stdio.h>

#define ID_LEN   16   /* 15 characters + '\0' */
#define NAME_LEN 32   /* 31 characters + '\0' */
#define NO_PRICE (-1)

/* One batch of a product. Batches of a product form a FIFO list:
 * the head is the oldest batch, new batches are appended at the tail. */
typedef struct Batch {
    int id;
    int qty;
    int unit_cost;
    struct Batch *next;
} Batch;

/* One product. Products form a list sorted by ID (ASCII order). */
typedef struct Product {
    char id[ID_LEN];
    char name[NAME_LEN];
    int price;                 /* NO_PRICE if not set yet */
    Batch *head, *tail;        /* oldest / newest batch */
    struct Product *next;
} Product;

/* One line of a cart. Items form a list sorted by product ID. */
typedef struct CartItem {
    char pid[ID_LEN];
    int qty;
    struct CartItem *next;
} CartItem;

/* One open cart. */
typedef struct Cart {
    char id[ID_LEN];
    CartItem *items;
    struct Cart *next;
} Cart;

/* The current promotion: if the cart has at least n of a, b costs p%. */
typedef struct {
    int active;
    char a[ID_LEN];
    int n;
    char b[ID_LEN];
    int p;
} Promo;

/* The whole system. No global variables: every function gets a Store *. */
typedef struct {
    Product *products;
    Cart *carts;
    Promo promo;
    int next_batch;            /* batch ID of the next imported line */
    long long revenue;
    long long cogs;
    long long gift_cost;
} Store;

/* ---- store.c : products, batches, import, price, promo, query, report ---- */
void store_init(Store *s);
void store_clear(Store *s);               /* frees everything, back to the start state */
Product *find_product(const Store *s, const char *pid);
Product *add_product(Store *s, const char *pid, const char *name); /* keeps list sorted */
Batch *append_batch(Product *p, int id, int qty, int unit_cost);
int product_stock(const Product *p);
int take_fifo(Product *p, int qty);       /* removes qty units, returns their cost */

void cmd_import(Store *s, const char *file);
void cmd_price(Store *s, const char *pid, int price);
void cmd_promo(Store *s, const char *a, int n, const char *b, int p);
void cmd_query(const Store *s, const char *pid);
void cmd_report(const Store *s);

/* ---- cart.c : carts and checkout ---- */
Cart *find_cart(const Store *s, const char *cid);
void free_cart_items(Cart *c);
void free_all_carts(Store *s);
void cmd_cart(Store *s, const char *cid);
void cmd_add(Store *s, const char *cid, const char *pid, int qty);
void cmd_checkout(Store *s, const char *cid);

/* ---- persist.c : save and load ---- */
void cmd_save(const Store *s, const char *file);
void cmd_load(Store *s, const char *file);

#endif
