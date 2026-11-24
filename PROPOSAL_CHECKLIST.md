# Proposal alignment check

Source: `CASE ZERO - Project Proposal.pdf`, pp. 2-4.

The official proposal describes a **console-only, ISO C11** project. The C console build remains available with no GTK or Python. The extra desktop GUI uses C/GTK3, so its presence is an enhancement **outside** the proposal's original "no graphical framework" scope.

| # | Proposal feature | Implementation |
|---|---|---|
| 1 | Detective profile | Detective name and persistent named score in Detective Case Files |
| 2 | New investigation | Random case initializer |
| 3 | Save / continue | Binary detective save; original campaign save |
| 4 | Multiple cases | 4 randomized crimes plus V5 modes |
| 5 | Difficulty | Easy / Medium / Hard differing move limits |
| 6 | Random culprit | Seeded `rand()` culprit selection |
| 7 | Dynamic clues | All clue records derive from generated culprit and event |
| 8 | Crime scene search | Scene report |
| 9 | CCTV analysis | Fast-clock offset and corrected minute deduction |
| 10 | Suspect profiles | Named suspect records and verified location/time lines |
| 11 | Interrogation | Interview reports and witness/record descriptions (not branching dialogue) |
| 12 | Alibi verification | Independent location/time check |
| 13 | Evidence inventory | Collected evidence list |
| 14 | Evidence board | Clue inventory, suspect suspicion scores and review |
| 15 | Suspicion meter | Per-suspect 0-10 values |
| 16 | Fingerprint / access check | Partial fingerprint and badge check |
| 17 | Limited moves | Move counter depends on difficulty |
| 18 | Hint system | Hints with score penalty |
| 19 | Final accusation | Suspect + reason + corrected CCTV minute |
| 20 | Multiple outcomes | Solved, incomplete/wrong accusation, and limited-move state |
| 21 | Score calculation | Evidence, remaining moves and hints affect score |
| 22 | Detective rank | Rookie / Inspector / Senior Detective / Master Detective |
| 23 | Leaderboard | Persistent top 10 sorted by score |
| 24 | Case history | Saved solved/failed history |
| 25 | Input validation | Range-checked numeric prompts |
| 26 | Console interface | Existing original terminal menus; optional graphical frontend |

**Accuracy note:** For a strict reading of the proposal, suspect interviews are structured summaries rather than interactive multi-question dialogue, and the move-limit consequence is inability to collect additional evidence rather than an instant timeout screen. These aspects can be enhanced later without changing the C11 core architecture.
