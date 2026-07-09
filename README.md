# Waystone Helper

A PoeFixer plugin for **Path of Exile 2** that highlights **waystones** in every open
item window (inventory, stash, guild stash) so you can judge a map at a glance without
hovering it.

Unofficial third-party game tool. Maintainer: Ömer Faruk ARPA.

![Waystone Helper overlay](https://i.hizliresim.com/2xy76sf.png)
![Waystone Helper badges](https://i.hizliresim.com/4qynhkf.png)

## Features

- **Affix-count badge** (top-left): shows the waystone's explicit affix count once it
  reaches your target (0–8). Set the target low (0/1/2) to also flag white/magic maps.
- **Map-stat badges** (top-right): the tracked "final from map" stats, each with its
  own color and short code, shown with their value:
  - `E` Monster Effectiveness
  - `R` Item Rarity
  - `P` Monster Pack Size
  - `MR` Monster Rarity
  - `W` Waystone Drop Chance
- **Custom affix groups** (left-side badges): pick any of the waystone affix families;
  a group matches when at least *N* of its selected affixes are present, and draws a
  colored `matched/selected` badge.
- **Border filter**: outline a map (one color) when it meets every condition you set —
  a minimum value on any map stat (e.g. Item Rarity ≥ 30, Monster Effectiveness ≥ 20),
  a minimum tier, and/or the target affix count. Set a stat's min right next to its
  badge toggle; leave it 0 to keep the badge without a border condition.
- **Hover breakdown panel**: hover a waystone to see exactly which stats, groups and
  rules matched.

Everything is configurable per color, and the overlay can hide itself on the item under
the cursor so the game's own tooltip stays readable.

## Install

1. Build `WaystoneHelper.dll` (see below) or grab it from a release.
2. Copy it to `…\fixer\Plugins\WaystoneHelper\WaystoneHelper.dll`, keeping the bundled
   `config\waystone_data.json` alongside it under `…\Plugins\WaystoneHelper\config\`.
3. Enable **Waystone Helper** in the PoeFixer plugin list.

## Build

Requires MSVC (v14x toolset, C++20), x64. From the plugin folder:

```
MSBuild.exe WaystoneHelper.sln -p:Configuration=Release -p:Platform=x64 -m
```

Output: `bin\Release\WaystoneHelper.dll`. The `sdk/`, `imgui/` and `third_party/`
folders are vendored so the plugin builds standalone.

## Notes

Map-stat values are read from the game's aggregated item stats. If a game patch shifts
the internal stat keys and the numbers stop appearing, enable **Debug mode**, open a
stash with waystones, and the plugin writes `debug\waystone-dump.txt`; match a known
tooltip value to its key and set it under `stat_aggregate_keys` in
`config\settings.json`. If a waystone ever misbehaves, turn off **Read item mods** (or
set `"read_mods": false`).
