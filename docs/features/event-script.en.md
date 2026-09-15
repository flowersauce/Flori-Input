# Event Scripts

Event scripts run text, keyboard, mouse, wait, and loop operations in sequence. Supported commands are `text`, `stroke`,
`event`, `wait`, `repeat`, `move`, and `wheel`.

## Files and execution

Save a script as a UTF-8 file (the `.fsevent` extension is recommended) in the folder opened by **Open scripts folder**
in the app. For the portable package, this folder is next to the executable. For the installer,
see [Where settings and scripts are stored](../getting-started/installation.en.md#where-settings-and-scripts-are-stored).

The page lists `.fsevent` and `.txt` files in that folder only, not in subfolders. The app validates a file when you
select it. You can click **Edit** to change it in a text editor, then click **Refresh** after saving. Each file may be
at most 1 MiB; a UTF-8 BOM is accepted.

Keep the **Script** page selected, switch to the target window, and press the start hotkey (default `F8`). Press it
again to stop. Before starting, the app rereads and validates the file. If it finds an error, the status bar reports it
and the app does not execute part of the script. It also checks input pairing and display layout changes while running.

Each of the three input pages responds only to its own hotkey while selected. The Set and Info pages do not respond to
start hotkeys. Changing pages while running stops the current task first.

## Parameter units

Do not write units after parameter values. Each parameter has a fixed unit:

| Parameter                                         | Unit                                   | Example                                                                   |
|---------------------------------------------------|----------------------------------------|---------------------------------------------------------------------------|
| Press duration, character interval, wait duration | Integer milliseconds (ms)              | `250` means 250 ms; only press duration and character interval may be `0` |
| Mouse coordinates                                 | Physical pixels in the virtual desktop | `-1920, 100`                                                              |
| Wheel movement                                    | Integer steps                          | `-1` means one step down                                                  |
| Repeat count                                      | Positive integer, or `-1`              | `3` runs three times; `-1` repeats indefinitely                           |

Do not add `ms`, `s`, or `px`, and do not infer a unit from the size of the number. Time values must be integer
milliseconds up to `86400000`. Press duration and character interval may be `0`, but `wait(0)` is invalid: `wait`
requires at least `1`. Fractions such as `0.5` are invalid. Coordinates, wheel steps, and repeat counts must also be
integers.

## Command reference

These are all supported commands. Names are case-sensitive. Every command except a `repeat` block ends with `;`.

| Command                               | Purpose and parameters                                                                                                                                                                           |
|---------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `text("content", press, interval);`   | Types Unicode text character by character. Holds each character for `press` ms and waits `interval` ms between adjacent characters. Both may be `0`.                                             |
| `stroke(target[, target...], press);` | Presses one or more `Key.*`/`Mouse.*` targets in order, holds them for `press` ms, then releases them in reverse order. At most 32 targets; no duplicates.                                       |
| `event(target, Down \| Up);`          | Presses or releases one `Key.*`/`Mouse.*` target once. There is no duration; presses and releases must be paired, and a held target cannot be pressed twice.                                     |
| `wait(ms);`                           | Pauses for an integer number of milliseconds from `1` to `86400000`; `0` is not accepted.                                                                                                        |
| `repeat(count) { ... }`               | Runs the enclosed commands repeatedly. `count` is `1`–`1000000000`, or `-1` for an indefinite loop. Up to 16 nesting levels; an indefinite loop must advance time. No semicolon after the block. |
| `move(x, y);`                         | Moves the pointer to absolute physical pixel coordinates in the virtual desktop. Negative coordinates are allowed; the point must be on an actual display.                                       |
| `wheel(steps);`                       | Scrolls at the pointer's current position. Positive is up, negative is down; one command allows `-100` to `-1` or `1` to `100` steps.                                                            |

## Commands in detail

The following examples can be placed directly in a `.fsevent` file. Time parameters are integer milliseconds;
coordinates are physical pixels in the full virtual desktop.

### `text`: type text

Syntax: `text("content", press duration, character interval);`. It uses Unicode input character by character and does
not depend on the active input method. The interval applies only between adjacent characters in the same `text` command;
there is no wait before the first or after the last character.

```text
text("你好，Flori Input", 5, 20);
```

Both time values may be `0`. To include a double quote, write `""` inside the string. A backslash is not an escape
character, so `\n` does not insert a newline. To insert a newline, use `stroke(Key.Enter, 5);`.

### `stroke`: complete press-and-release action

Syntax: `stroke(target[, target...], press duration);`. It presses the specified keyboard or mouse targets in order,
holds them for the given duration, then releases them in reverse order. A single target is one keystroke or click;
multiple targets can form a key combination.

```text
stroke(Key.Ctrl, Key.A, 30);
stroke(Mouse.Left, 5);
```

Write targets as `Key.Name` or `Mouse.Name`. A command cannot repeat a target and may contain at most 32 targets. A
press duration of `0` sends the press and release immediately. The app does not restrict key combinations to a list of
predefined meaningful combinations.

### `event`: one press or release

Syntax: `event(target, Down);` or `event(target, Up);`. Each command performs one step without waiting or releasing
automatically, so you can insert other operations while an input is held.

```text
move(300, 300);
event(Mouse.Left, Down);
wait(100);
move(600, 400);
event(Mouse.Left, Up);
```

Every `Down` needs a matching `Up`. Pressing an already held target, releasing a target that is not held, or ending with
a target still held causes an error. The app attempts to release held inputs when execution stops or fails.

### `wait`: pause

Syntax: `wait(milliseconds);`. Add an explicit pause between operations. The value must be from `1` to `86400000`; `0`
and fractional values are not accepted.

```text
stroke(Key.Enter, 5);
wait(250);
text("Next step", 5, 10);
```

### `repeat`: repeat a block

Syntax: `repeat(count) { ... }`. The count must be `1`–`1000000000`; `-1` repeats until you press the start hotkey
again. Blocks may be nested up to 16 levels. Do not add a semicolon after the closing brace.

```text
repeat(3) {
    stroke(Mouse.Left, 5);
    wait(500);
}
```

An indefinite loop must contain an operation that takes time. An explicit `wait` is recommended so that the no-wait
action limit does not stop the script. The count cannot be `0`, and no negative number other than `-1` is valid.

### `move`: move the pointer

Syntax: `move(x, y);`. Coordinates refer to the full virtual desktop. A secondary display to the left or above the
primary display may have negative coordinates. Click **Pick point** on the Script page, move the pointer to the target,
then left-click. The app copies the point as `x, y` to the clipboard so you can paste it into `move(...)`. Right-click
or press `Esc` to cancel.

```text
move(400, 300);
stroke(Mouse.Left, 5);
```

Replace the example coordinates with a point you picked. The point must lie on an actual display. If it is in a gap
between displays, or the display layout changes while the script runs, the app reports an error instead of snapping to
another screen.

### `wheel`: scroll the mouse wheel

Syntax: `wheel(steps);`. Positive values scroll up and negative values scroll down. Each command accepts `-100` to `-1`
or `1` to `100`; `0` is invalid.

```text
wheel(2);
wait(100);
wheel(-1);
```

## Syntax and safety

Outside strings, you may use spaces, indentation, newlines, and `//` line comments. Command and target names are
case-sensitive; aliases are not supported. `text` cannot span lines or contain control characters. Use the corresponding
`Key.*` target for Enter, Tab, and similar input.

Keyboard targets include `A`–`Z`, `0`–`9`, `F1`–`F24`, `Ctrl`, `Shift`, `Alt`, `Win`, `Enter`, `Tab`, `Esc`, `Space`,
`Backspace`, `Delete`, `Insert`, `Home`, `End`, `PageUp`, `PageDown`, `ArrowLeft`, `ArrowRight`, `ArrowUp`, and
`ArrowDown`. Mouse targets are limited to `Left`, `Middle`, and `Right`.

To prevent unexpectedly long runs, a finite script is limited to 100,000 steps by default, and no more than 1,000
actions may run consecutively without a positive-time wait. If a limit is exceeded, the app stops and attempts to
release held inputs. For an indefinite loop, include a reasonable `wait` and make sure the start hotkey can stop it at
any time.
