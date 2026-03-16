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

/* sort hand descending by rank (insertion sort on linked list) */
static void hand_sort(Hand *h) {
    if (!h->head || !h->head->next) return;
    Card *sorted = NULL;
    Card *cur = h->head;
    while (cur) {
        Card *next = cur->next;
        cur->next = NULL;
        /* insert cur into sorted list in descending rank order */
        if (!sorted || rank_of(cur->value) >= rank_of(sorted->value)) {
            cur->next = sorted;
            sorted = cur;
        } else {
            Card *s = sorted;
            while (s->next && rank_of(s->next->value) > rank_of(cur->value))
                s = s->next;
            cur->next = s->next;
            s->next = cur;
        }
        cur = next;
    }
    h->head = sorted;
}

/* evaluate a 5-card hand (must be sorted descending first).
   Returns HandResult with rank and kickers per spec. */
static HandResult hand_evaluate(const Hand *h) {
    int r[5];  /* ranks, descending */
    char s[5]; /* suits */
    int i = 0;
    for (Card *c = h->head; c && i < 5; c = c->next) {
        r[i] = rank_of(c->value);
        s[i] = c->suit;
        i++;
    }

    HandResult res = {0, {0,0,0,0,0}};

    /* flush? */
    int flush = (s[0]==s[1] && s[1]==s[2] && s[2]==s[3] && s[3]==s[4]);

    /* straight? (normal) */
    int straight = 0;
    int straight_top = r[0];
    if (r[0]-r[4]==4 && r[1]-r[2]==1 && r[2]-r[3]==1 && r[3]-r[4]==1
        && r[0]-r[1]==1) {
        straight = 1;
    }
    /* wheel: A-2-3-4-5 → sorted as 14,5,4,3,2 */
    if (r[0]==14 && r[1]==5 && r[2]==4 && r[3]==3 && r[4]==2) {
        straight = 1;
        straight_top = 5; /* wheel high is 5, not Ace */
    }

    /* count value frequencies */
    int freq[15] = {0}; /* index = rank */
    for (int j = 0; j < 5; j++) freq[r[j]]++;

    int four=0, three=0, pairs=0;
    int four_val=0, three_val=0, pair_vals[2]={0,0};
    for (int v = 14; v >= 2; v--) {
        if (freq[v]==4) { four=1; four_val=v; }
        else if (freq[v]==3) { three=1; three_val=v; }
        else if (freq[v]==2) { if (!pairs) pair_vals[0]=v; else pair_vals[1]=v; pairs++; }
    }

    /* classify */
    if (straight && flush) {
        res.rank = 8;
        res.kickers[0] = straight_top;
    } else if (four) {
        res.rank = 7;
        res.kickers[0] = four_val;
        /* kicker = the non-quad card */
        for (int j=0;j<5;j++) if(r[j]!=four_val){res.kickers[1]=r[j]; break;}
    } else if (three && pairs==1) {
        res.rank = 6;
        res.kickers[0] = three_val;
        res.kickers[1] = pair_vals[0];
    } else if (flush) {
        res.rank = 5;
        for (int j=0;j<5;j++) res.kickers[j]=r[j];
    } else if (straight) {
        res.rank = 4;
        res.kickers[0] = straight_top;
    } else if (three) {
        res.rank = 3;
        res.kickers[0] = three_val;
        int ki=1;
        for (int j=0;j<5;j++) if(r[j]!=three_val) res.kickers[ki++]=r[j];
    } else if (pairs==2) {
        res.rank = 2;
        res.kickers[0] = pair_vals[0]; /* higher pair */
        res.kickers[1] = pair_vals[1]; /* lower pair */
        for (int j=0;j<5;j++)
            if(r[j]!=pair_vals[0]&&r[j]!=pair_vals[1]){res.kickers[2]=r[j];break;}
    } else if (pairs==1) {
        res.rank = 1;
        res.kickers[0] = pair_vals[0];
        int ki=1;
        for (int j=0;j<5;j++) if(r[j]!=pair_vals[0]) res.kickers[ki++]=r[j];
    } else {
        res.rank = 0;
        for (int j=0;j<5;j++) res.kickers[j]=r[j];
    }
    return res;
}

/* compare two HandResults: >0 if a wins, <0 if b wins, 0 if tie */
static int hand_compare(const HandResult *a, const HandResult *b) {
    if (a->rank != b->rank) return a->rank - b->rank;
    for (int i = 0; i < 5; i++) {
        if (a->kickers[i] != b->kickers[i])
            return a->kickers[i] - b->kickers[i];
    }
    return 0;
}

/* helper: build a hand from a string like "AS KH QD JC TS" */
static Hand make_hand(const char *cards) {
    Hand h = {NULL, 0};
    const char *p = cards;
    while (*p) {
        Card *c = malloc(sizeof(Card));
        c->value = p[0]; c->suit = p[1]; c->next = NULL;
        hand_append(&h, c);
        p += 2;
        while (*p == ' ') p++;
    }
    return h;
}

static void free_hand(Hand *h) {
    Card *c = h->head, *nx;
    while (c) { nx=c->next; free(c); c=nx; }
    h->head=NULL; h->count=0;
}

static void check(const char *label, const char *cards, int expected_rank) {
    Hand h = make_hand(cards);
    hand_sort(&h);
    HandResult r = hand_evaluate(&h);
    printf("%-30s rank=%d %s\n", label, r.rank,
           r.rank==expected_rank ? "OK" : "FAIL");
    free_hand(&h);
}

/* Fills keep_mask[5] with 1=keep, 0=discard.
   Hand must be sorted descending before calling.
   Returns number of cards to discard. */
static int ai_discard(const Hand *h, int *keep_mask) {
    int r[5]; int i=0;
    for (Card *c=h->head; c&&i<5; c=c->next) r[i++]=rank_of(c->value);
    for (int j=0;j<5;j++) keep_mask[j]=1; /* default: keep all */

    HandResult hr = hand_evaluate(h);

    /* Straight Flush, Four of a Kind, Full House, Flush, Straight: keep all */
    if (hr.rank >= 4) return 0;

    /* Three of a Kind: keep the 3 matching, discard 2 */
    if (hr.rank == 3) {
        int tv = hr.kickers[0];
        int kept=0;
        for (int j=0;j<5;j++) {
            if (r[j]==tv && kept<3) { keep_mask[j]=1; kept++; }
            else keep_mask[j]=0;
        }
        return 2;
    }

    /* Two Pair: keep all 5 */
    if (hr.rank == 2) return 0;

    /* Pair: keep the 2 paired cards, discard 3 */
    if (hr.rank == 1) {
        int pv = hr.kickers[0];
        int kept=0;
        for (int j=0;j<5;j++) {
            if (r[j]==pv && kept<2) { keep_mask[j]=1; kept++; }
            else keep_mask[j]=0;
        }
        return 3;
    }

    /* No made hand: keep cards with rank >= 11 (Jack+), discard rest */
    int discard_count=0;
    for (int j=0;j<5;j++) {
        if (r[j]>=11) keep_mask[j]=1;
        else { keep_mask[j]=0; discard_count++; }
    }
    return discard_count;
}

static void check_ai(const char *label, const char *cards,
                     int expected_discards) {
    Hand h = make_hand(cards);
    hand_sort(&h);
    int mask[5];
    int nd = ai_discard(&h, mask);
    printf("%-30s discards=%d %s | keep:", label, nd,
           nd==expected_discards ? "OK" : "FAIL");
    int i=0;
    for (Card *c=h.head; c; c=c->next,i++)
        if (mask[i]) { printf(" "); print_card(c); }
    printf("\n");
    free_hand(&h);
}

/* Write human-readable hand name into buf (at least 64 bytes).
   Hand must already be sorted and evaluated. */
static void hand_name(const HandResult *hr, char *buf, size_t sz) {
    int k0=hr->kickers[0], k1=hr->kickers[1];
    switch (hr->rank) {
        case 8:
            if (k0==14) snprintf(buf,sz,"Royal Flush");
            else snprintf(buf,sz,"Straight Flush, %s high",rank_singular(k0));
            break;
        case 7:
            snprintf(buf,sz,"Four of a Kind, %s",rank_plural(k0));
            break;
        case 6:
            snprintf(buf,sz,"Full House, %s full of %s",
                     rank_plural(k0),rank_plural(k1));
            break;
        case 5:
            snprintf(buf,sz,"Flush, %s high",rank_singular(k0));
            break;
        case 4:
            snprintf(buf,sz,"Straight, %s high",rank_singular(k0));
            break;
        case 3:
            snprintf(buf,sz,"Three of a Kind, %s",rank_plural(k0));
            break;
        case 2:
            snprintf(buf,sz,"Two Pair, %s and %s",
                     rank_plural(k0),rank_plural(k1));
            break;
        case 1:
            snprintf(buf,sz,"Pair of %s",rank_plural(k0));
            break;
        default:
            snprintf(buf,sz,"High Card, %s high",rank_singular(k0));
            break;
    }
}

static void check_name(const char *cards, const char *expected) {
    Hand h = make_hand(cards);
    hand_sort(&h);
    HandResult hr = hand_evaluate(&h);
    char buf[64];
    hand_name(&hr, buf, sizeof(buf));
    printf("%s → \"%s\" %s\n", cards, buf,
           strcmp(buf,expected)==0 ? "OK" : "FAIL");
    free_hand(&h);
}

/* returns 1 if hand (sorted descending) is a wheel straight A-2-3-4-5 */
static int is_wheel(const Hand *h) {
    if (h->count != 5) return 0;
    int r[5]; int i=0;
    for (Card *c=h->head; c&&i<5; c=c->next) r[i++]=rank_of(c->value);
    return (r[0]==14 && r[1]==5 && r[2]==4 && r[3]==3 && r[4]==2);
}

/* print hand for display:
   - normal: already sorted descending, just call print_hand
   - wheel:  print 5,4,3,2,A (skip Ace at head, print it last) */
static void print_hand_display(const Hand *h) {
    if (!is_wheel(h)) {
        print_hand(h);
        return;
    }
    /* wheel: skip head (Ace), print rest, then Ace */
    int first=1;
    for (Card *c=h->head->next; c; c=c->next) {
        if (!first) printf(" ");
        print_card(c);
        first=0;
    }
    printf(" "); print_card(h->head);
}

int main(void) {
    check_name("AS KS QS JS TS", "Royal Flush");
    check_name("9H 8H 7H 6H 5H", "Straight Flush, Nine high");
    check_name("AS 5H 4D 3C 2S", "Straight, Five high");
    check_name("AS AH AD AC KS", "Four of a Kind, Aces");
    check_name("KS KH KD QS QH", "Full House, Kings full of Queens");
    check_name("AS QS 9S 6S 3S", "Flush, Ace high");
    check_name("9H 8S 7D 6C 5H", "Straight, Nine high");
    check_name("JS JH JD AS KH", "Three of a Kind, Jacks");
    check_name("AS AH KS KH QD", "Two Pair, Aces and Kings");
    check_name("AS AH 9S 7D 3C", "Pair of Aces");
    check_name("AS KH QD JC 9S", "High Card, Ace high");
    return 0;
}
