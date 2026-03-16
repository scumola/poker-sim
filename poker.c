#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── Data structures ─────────────────────────────────────────── */

typedef struct Card {
    char value; /* '2'-'9', 'T', 'J', 'Q', 'K', 'A' */
    char suit;  /* 'S', 'H', 'D', 'C' */
    struct Card *next;
} Card;

typedef struct {
    Card *head;
    int   count;
} Hand;

typedef struct {
    int rank;       /* 0=High Card … 8=Straight Flush */
    int kickers[5]; /* ordered by significance; unused = 0 */
} HandResult;

/* ── Card value → integer rank ───────────────────────────────── */

static int rank_of(char v) {
    if (v >= '2' && v <= '9') return v - '0';
    if (v == 'T') return 10;
    if (v == 'J') return 11;
    if (v == 'Q') return 12;
    if (v == 'K') return 13;
    if (v == 'A') return 14;
    return 0;
}

/* singular rank name used in "X high" strings */
static const char *rank_singular(int r) {
    switch (r) {
        case  2: return "Two";   case  3: return "Three";
        case  4: return "Four";  case  5: return "Five";
        case  6: return "Six";   case  7: return "Seven";
        case  8: return "Eight"; case  9: return "Nine";
        case 10: return "Ten";   case 11: return "Jack";
        case 12: return "Queen"; case 13: return "King";
        case 14: return "Ace";
        default: return "?";
    }
}

/* plural rank name used in hand descriptions */
static const char *rank_plural(int r) {
    switch (r) {
        case  2: return "Twos";   case  3: return "Threes";
        case  4: return "Fours";  case  5: return "Fives";
        case  6: return "Sixes";  case  7: return "Sevens";
        case  8: return "Eights"; case  9: return "Nines";
        case 10: return "Tens";   case 11: return "Jacks";
        case 12: return "Queens"; case 13: return "Kings";
        case 14: return "Aces";
        default: return "?";
    }
}

/* print a single card as two chars, e.g. "AS" */
static void print_card(const Card *c) {
    printf("%c%c", c->value, c->suit);
}

/* print all cards in a hand space-separated */
static void print_hand(const Hand *h) {
    for (Card *c = h->head; c; c = c->next) {
        print_card(c);
        if (c->next) printf(" ");
    }
}

/* ── Linked-list helpers ─────────────────────────────────────── */

/* append card to tail of hand */
static void hand_append(Hand *h, Card *c) {
    c->next = NULL;
    if (!h->head) {
        h->head = c;
    } else {
        Card *t = h->head;
        while (t->next) t = t->next;
        t->next = c;
    }
    h->count++;
}

/* remove and return head card; returns NULL if empty */
static Card *hand_pop(Hand *h) {
    if (!h->head) return NULL;
    Card *c = h->head;
    h->head = c->next;
    c->next = NULL;
    h->count--;
    return c;
}

/* ── Deck ────────────────────────────────────────────────────── */

static const char VALUES[] = "23456789TJQKA";
static const char SUITS[]  = "SHDC";

/* allocate all 52 Card nodes and return them as a Hand */
static Hand deck_init(void) {
    Hand deck = {NULL, 0};
    for (int s = 0; s < 4; s++) {
        for (int v = 0; v < 13; v++) {
            Card *c = malloc(sizeof(Card));
            if (!c) { fprintf(stderr, "malloc failed\n"); exit(1); }
            c->value = VALUES[v];
            c->suit  = SUITS[s];
            c->next  = NULL;
            hand_append(&deck, c);
        }
    }
    return deck;
}

/* Fisher-Yates shuffle: collect into temp array, shuffle, relink */
static void deck_shuffle(Hand *deck) {
    Card *arr[52];
    int n = 0;
    for (Card *c = deck->head; c; c = c->next)
        arr[n++] = c;
    if (n == 0) return;
    if (n > 52) { fprintf(stderr, "deck_shuffle: too many cards (%d)\n", n); exit(1); }
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        Card *tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp;
    }
    /* relink */
    deck->head = arr[0];
    for (int i = 0; i < n - 1; i++) arr[i]->next = arr[i+1];
    arr[n-1]->next = NULL;
    deck->count = n;
}

/* deal `n` cards from deck head to player hand */
static void deal(Hand *deck, Hand *player, int n) {
    for (int i = 0; i < n; i++) {
        Card *c = hand_pop(deck);
        if (!c) break;
        hand_append(player, c);
    }
}

/* move cards NOT in keep_mask[] to discard pile.
   keep_mask: array of `hand->count` booleans (1=keep, 0=discard).
   Returns number of cards discarded. */
static int discard_cards(Hand *player, Hand *discard, int *keep_mask) {
    Card *prev = NULL;
    Card *c = player->head;
    int discarded = 0;
    int idx = 0;
    while (c) {
        Card *next = c->next;
        if (!keep_mask[idx]) {
            /* unlink from player */
            if (prev) prev->next = next;
            else       player->head = next;
            player->count--;
            /* append to discard */
            hand_append(discard, c);
            discarded++;
        } else {
            prev = c;
        }
        c = next;
        idx++;
    }
    return discarded;
}

/* draw replacement cards from deck head into player hand */
static int draw_cards(Hand *deck, Hand *player, int n) {
    int drawn = 0;
    for (int i = 0; i < n; i++) {
        Card *c = hand_pop(deck);
        if (!c) break;
        hand_append(player, c);
        drawn++;
    }
    return drawn;
}

int main(void) {
    srand((unsigned)time(NULL));
    Hand deck = deck_init();
    deck_shuffle(&deck);
    Hand player = {NULL, 0};
    Hand discard = {NULL, 0};

    deal(&deck, &player, 5);
    printf("Dealt: "); print_hand(&player); printf("\n");
    printf("Deck remaining: %d\n", deck.count);

    /* discard cards at index 1 and 3 */
    int mask[5] = {1, 0, 1, 0, 1};
    int nd = discard_cards(&player, &discard, mask);
    printf("Discarded %d. Hand now: ", nd); print_hand(&player); printf("\n");

    int drawn = draw_cards(&deck, &player, nd);
    printf("Drew %d. Final hand: ", drawn); print_hand(&player); printf("\n");
    printf("Deck remaining: %d  Discard pile: %d\n",
           deck.count, discard.count);

    /* free all */
    Card *c, *nx;
    for (c = deck.head;    c; c = nx) { nx=c->next; free(c); }
    for (c = discard.head; c; c = nx) { nx=c->next; free(c); }
    for (c = player.head;  c; c = nx) { nx=c->next; free(c); }
    return 0;
}
