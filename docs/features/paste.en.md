# Simulated Paste

Some text fields do not allow ordinary paste. Simulated paste sends text to the current target window one character at a
time, making it useful for short text in such fields. It is not clipboard paste, and some apps or protected input fields
may still reject simulated input.

## How to use it

1. Stay on the **Sim Paste** page and enter the text to send. If the text field is empty, the app reads the latest
   clipboard text when you start.
2. Choose an input strategy and adjust Press and Interval if needed.
3. Focus the target text field and press the start hotkey (default `F8`). The task stops automatically when finished;
   press the hotkey again to stop it early.

You can change this page's hotkey independently or use the same key as Trigger or Script. Hotkeys respond only on the
current input page. The status bar at the bottom shows results or errors.

## Input strategies

- **Simulate keys**: Sends keyboard-representable characters as key events and falls back to Unicode input for
  characters that cannot be mapped. Better suited to apps that rely on keyboard events.
- **Unicode text**: Sends every character using Unicode input. Useful for Chinese and other non-ASCII text, though some
  games and specialized controls may not accept it.

Try the output in Notepad before using it in the target app. If one strategy is ignored, try the other.

## Input timing

The UI uses **seconds** with up to three decimal places. Press defaults to `0.005` seconds and controls how long each
character stays down. Interval defaults to `0.010` seconds and applies only between adjacent characters; there is no
extra wait before or after the text. Both fields allow `0.000`.

For example, with `ABC` and an Interval of `0.100` seconds, the app waits once between A and B and once between B and C,
but not after C.

