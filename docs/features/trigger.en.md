# Trigger

Trigger is for repeated clicks or keystrokes, or temporarily holding a key. Choose the input, switch to the target
window, and press the start hotkey to begin. Press the same hotkey again to stop. The default is `F8`, and it works only
while the **Trigger** page is selected.

## Choose the input

- **Button**: Select the left, middle, or right mouse button, or click **Pick** to capture a keyboard key or wheel
  direction. A custom input key cannot be the same as this page's start hotkey.
- **Action**: **Repeat** presses and releases the input repeatedly; **Hold** keeps it pressed until you stop. The wheel
  supports Repeat only.
- **Cursor**: **Free** performs mouse input at the pointer's current position; **Lock** uses a specified coordinate. To
  pick a coordinate, move the pointer to the target, left-click to confirm, or right-click or press `Esc` to cancel.
  Coordinates span all monitors, so a screen to the left or above the primary display can have negative coordinates.

## Repeat timing

The UI uses **seconds** with up to three decimal places. For example, `0.010` means 10 milliseconds.

- **Period**: Target interval between the starts of two inputs; defaults to `0.010` seconds.
- **Press**: How long each input stays down; defaults to `0.005` seconds. `0.000` sends the press and release
  immediately. Hold and wheel input do not use this setting.
- **Jitter**: Adds random variation around the target period. The range is `0%`–`50%`, and the default is `0%`.

When Press is above zero, the minimum Period is Press plus `0.001` seconds, leaving time to release the input. When
Press is zero, the minimum Period is `0.001` seconds. Actual speed also depends on Windows scheduling, the target app,
and system load; the configured values do not guarantee an exact clicks-per-second rate.

## Stopping and status

Press the start hotkey again while running to stop. The status bar at the bottom of the page shows running and error
states. On stop, the app attempts to release any held inputs. Confirm the hotkey and input behavior in a safe window
before using them in a game or another app.
