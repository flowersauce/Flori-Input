# Quick Start

Flori Input has three input pages: **Trigger**, **Sim Paste**, and **Script**. All use `F8` as their default start
hotkey, but **only the current input page responds**. Changing pages while a task is running stops that task first.

## Try repeated mouse clicks

1. Open **Trigger** and leave the default Left button, Repeat action, and Free cursor settings unchanged.
2. Move the pointer to a safe place to click. Press `F8` to start, then press `F8` again to stop.
3. To change the speed, adjust Period and Press. Both fields use seconds.

## Try simulated paste

1. Open **Sim Paste** and enter a short phrase, or leave the text field empty to use the latest clipboard text.
2. Place the text cursor in Notepad and press `F8`. The task stops automatically when all text has been sent.

## Try an event script

1. Open **Script**, click **Open scripts folder**, and create `hello.fsevent`.
2. Save the following UTF-8 text in that file:

   ```text
   text("Hello, Flori Input", 5, 20);
   stroke(Key.Enter, 5);
   ```

3. Return to the app, click **Refresh**, and select the file. Focus Notepad's input area and press `F8`.
   See [Event Scripts](../features/event-script.en.md) for the complete syntax.

If the status bar shows an error, check the target window, hotkey, and script file. Do not first try automation in a
window with unsaved work or where an unintended action could be harmful.

