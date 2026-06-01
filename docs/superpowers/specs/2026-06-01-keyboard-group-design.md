# wlr_keyboard_group Integration Design

## Problem

Treeland/Waylib currently supports only one keyboard per seat. When multiple physical keyboards are attached to a seat, the last one to attach wins (`WSeatPrivate::attachInputDevice` calls `handle()->set_keyboard(*keyboard)` for every keyboard). There is no synchronization of keyboard state (NumLock, CapsLock, layout, modifiers) across keyboards.

## Solution

Embed a `wlr_keyboard_group` into `WSeat`. All keyboard devices attached to a seat are added to the group. The group's internal `wlr_keyboard` is set on the `wlr_seat`. wlroots handles all synchronization automatically.

## Architecture

```
Physical Keyboard 1 --+
                       +--> wlr_keyboard_group --> group.keyboard --> wlr_seat::set_keyboard()
Physical Keyboard 2 --+        (auto-sync)      |
                                           +----> notify_key / notify_modifiers -> Wayland clients
```

**Synchronization (handled by wlroots):**
- Modifiers (Shift, Ctrl, Alt, etc.) — group aggregates modifiers from all member keyboards
- NumLock/CapsLock (LEDs) — group syncs LED state to all member keyboards
- Keymap — group manages a unified keymap; updates all members on change
- Keys — group handles key passthrough via `enter`/`leave` signals

## Design: Waylib Layer

### WSeatPrivate additions

New members:
```cpp
qw_keyboard_group *m_keyboardGroup = nullptr;
```

### WSeat public API additions

```cpp
// Set keymap on the keyboard group using XKB rules
bool setKeyboardKeymap(const struct xkb_rule_names &rules);
// Set keymap on the keyboard group using pre-built xkb_keymap
bool setKeyboardKeymap(struct xkb_keymap *keymap);
// Set repeat info on the keyboard group
void setKeyboardRepeatInfo(int32_t rate_hz, int32_t delay_ms);
```

### Lifecycle

1. **WSeat::create()** — create `qw_keyboard_group`, connect `enter`/`leave` signals
2. **WSeat::attachInputDevice(keyboard)** — call `m_keyboardGroup->add_keyboard(keyboard)`, set `set_keyboard(group->keyboard)` on seat (only needs to happen once, not per-keyboard)
3. **WSeat::detachInputDevice(keyboard)** — call `m_keyboardGroup->remove_keyboard(keyboard)`
4. **WSeat::keyboard()** — return a `WInputDevice` wrapping the group keyboard (may need a sentinel device or lookup mechanism)
5. **WSeat::destroy()** — destroy keyboard group

### `enter`/`leave` signal handling

The group emits `enter` when a keyboard joins with keys already pressed, and `leave` when it leaves with keys still pressed. These events carry a `wl_array` of key codes. The compositor must update its internal `keyModifiers` state from the group keyboard's xkb_state, but must NOT trigger key bindings or notify Wayland surfaces.

### qwkeyboardgroup.cpp

Create the missing `.cpp` implementation file for `qw_keyboard_group`, following the standard QW type pattern used by other types (constructor, `fromHandle`, destroy).

## Design: Treeland Layer

Treeland's responsibilities:
1. Call `seat->setKeyboardKeymap(rules)` with system XKB configuration after WSeat is created
2. Handle `enter`/`leave` signals from the keyboard group to update internal key modifier state
3. React to XKB setting changes (layout switch, options change) by calling `setKeyboardKeymap(newRules)` to atomically update all keyboards

No manual synchronization logic is needed — wlroots handles it internally.

## Files to Modify

### New files:
- `qwlroots/src/types/qwkeyboardgroup.cpp` — QW wrapper implementation

### Modified files (waylib):
- `waylib/src/server/kernel/wseat.h` — add keyboard group API declarations
- `waylib/src/server/kernel/wseat.cpp` — implement keyboard group lifecycle, modify attachInputDevice/detachInputDevice

### Modified files (treeland):
- Treeland compositor code where WSeat is used — configure keymap, handle enter/leave signals (specific file TBD during implementation)

## Open Questions

None — design is complete pending implementation.
