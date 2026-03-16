#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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

static void free_hand_nodes(Hand *h) {
    Card *c = h->head, *nx;
    while (c) { nx=c->next; free(c); c=nx; }
    h->head=NULL; h->count=0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <num_players>\n", argv[0]);
        return 1;
    }
    int n = atoi(argv[1]);
    if (n < 2 || n > 5) {
        fprintf(stderr, "Error: num_players must be 2-5 (got %d)\n", n);
        return 1;
    }

    srand((unsigned)time(NULL) ^ (unsigned)getpid());

    Hand deck    = deck_init();
    Hand discard = {NULL, 0};
    Hand players[5];
    for (int i=0; i<n; i++) players[i] = (Hand){NULL, 0};

    /* ── Deal ──────────────────────────────────────────────── */
    printf("--- 5-Card Draw Poker Simulation ---\n");
    printf("Players: %d\n\n", n);
    deck_shuffle(&deck);
    for (int i=0; i<n; i++) deal(&deck, &players[i], 5);

    printf("[Deal]\n");
    for (int i=0; i<n; i++) {
        hand_sort(&players[i]);
        printf("Player %d: ", i+1);
        print_hand_display(&players[i]);
        printf("\n");
    }
    printf("\n");

    /* ── Draw ──────────────────────────────────────────────── */
    printf("[Draw]\n");
    for (int i=0; i<n; i++) {
        int mask[5];
        ai_discard(&players[i], mask);
        int actual_discards = discard_cards(&players[i], &discard, mask);
        int drawn = draw_cards(&deck, &players[i], actual_discards);
        if (drawn < actual_discards) {
            printf("(deck ran out — Player %d receives %d cards)\n", i+1, drawn);
        }
        printf("Player %d discards %d, draws %d\n", i+1, actual_discards, drawn);
        hand_sort(&players[i]);
    }
    printf("\n");

    /* ── Evaluate ──────────────────────────────────────────── */
    HandResult results[5];
    char names[5][64];
    for (int i=0; i<n; i++) {
        results[i] = hand_evaluate(&players[i]);
        hand_name(&results[i], names[i], 64);
    }

    printf("[Final Hands]\n");
    for (int i=0; i<n; i++) {
        printf("Player %d: ", i+1);
        print_hand_display(&players[i]);
        printf("  [%s]\n", names[i]);
    }
    printf("\n");

    /* ── Winner ────────────────────────────────────────────── */
    /* find best rank */
    int best = 0;
    for (int i=1; i<n; i++)
        if (hand_compare(&results[i], &results[best]) > 0) best=i;

    /* collect all tied winners (ascending order) */
    int winners[5], nw=0;
    for (int i=0; i<n; i++)
        if (hand_compare(&results[i], &results[best]) == 0) winners[nw++]=i;

    if (nw == 1) {
        printf("Winner: Player %d (%s)\n", winners[0]+1, names[winners[0]]);
    } else {
        printf("Winners: ");
        for (int i=0; i<nw; i++) {
            if (i>0 && i==nw-1 && nw==2) printf(" and ");
            else if (i>0 && i==nw-1)     printf(", and ");
            else if (i>0)                printf(", ");
            printf("Player %d", winners[i]+1);
        }
        printf(" (%s)\n", names[winners[0]]);
    }

    /* ── Cleanup ───────────────────────────────────────────── */
    free_hand_nodes(&deck);
    free_hand_nodes(&discard);
    for (int i=0; i<n; i++) free_hand_nodes(&players[i]);

    return 0;
}
