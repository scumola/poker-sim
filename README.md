# poker-sim

A fully-automated 5-card draw poker simulator written in C, using linked lists to represent the deck and player hands.

## Build

```bash
make
```

## Usage

```bash
./poker <num_players>
```

Players: 2–5.

## Example

```
$ ./poker 4
--- 5-Card Draw Poker Simulation ---
Players: 4

[Deal]
Player 1: AS QD JC TC 9S
Player 2: KH 7H 7D 3S 3C
Player 3: QS QH QC 5D 2H
Player 4: 9H 8H 7H 6H 5H

[Draw]
Player 1 discards 3, draws 3
Player 2 discards 0, draws 0
Player 3 discards 2, draws 2
Player 4 discards 0, draws 0

[Final Hands]
Player 1: AS KD QD TC 8S  [High Card, Ace high]
Player 2: KH 7H 7D 3S 3C  [Two Pair, Sevens and Threes]
Player 3: QS QH QC QD 4C  [Four of a Kind, Queens]
Player 4: 9H 8H 7H 6H 5H  [Straight Flush, Nine high]

Winner: Player 4 (Straight Flush, Nine high)
```

## How it works

- The deck is a singly-linked list of 52 `Card` nodes allocated once at startup
- Cards are moved between the deck, player hands, and a discard pile by relinking pointers — no copying
- The deck is shuffled with Fisher-Yates via a temporary pointer array
- Each player's hand is sorted descending by rank after dealing and after drawing
- AI discard strategy: keep made hands intact, keep pairs, discard down to high cards (J+) otherwise
- Hand evaluation covers all standard ranks from High Card through Straight Flush (Royal Flush reported by name)
- Ties are handled; multiple winners are announced with proper formatting
