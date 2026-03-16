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

/* ── Temporary main to verify compile ───────────────────────── */
int main(void) {
    printf("rank_of('A')=%d rank_of('T')=%d rank_of('2')=%d\n",
           rank_of('A'), rank_of('T'), rank_of('2'));
    printf("singular(11)=%s plural(14)=%s\n",
           rank_singular(11), rank_plural(14));
    return 0;
}
