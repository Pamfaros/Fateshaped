# Fateshaped post-jam backlog

## Classes and progression

Restore the full starting choice once each option has a playable kit. **V → Berserker** emphasizes resilience, close-range combat, and a risk/reward sacrifice attack. **M → Sorcerer** uses mana-powered projectiles and ranged positioning. Extend the Swordsman with physical weapon techniques.

Ordered choices define distinct hybrids. Explore the remaining pairs when their mechanics can be designed and tested.

| Sequence | Hybrid |
| --- | --- |
| M → S | Battlemage |
| V → M | Blood Mage |
| M → V | Warlock |
| S → V | Reaver |
| V → S | Juggernaut |

## Dungeon and encounters

- Add reusable room variants, additional enemy archetypes, elite encounters, and bosses.
- Start with a rule-based encounter director that reads health, damage taken and time to clear a room. Adjust the next encounter or offer a recovery break within bounded difficulty. Keep the current room stable.
- Prototype procedural room graphs with Delaunay triangulation, a minimum spanning tree, and selected extra edges. Connect rooms with L- or T-shaped corridors and validate reachability and collisions.
- Explore adaptive enemy AI and a separate DQN/PPO director after a deterministic baseline. Give the director a bounded pool of rooms, enemies and rewards. Choose between rooms using player performance as input.

## Guild and campaign

- Expand the guild, portal, quest board, and fairy guide. The crafting area can appear as scenery before it becomes a system.
- Explore loot, inventory, trading, and crafting from dungeon rewards, including gear, potions and scrolls. Revisit lightweight appearance presets, longer progression, and a persistent campaign.
- Defer co-op, multiplayer, auctions, a player economy, accounts, and backend services until after the jam.
