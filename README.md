# LeftTray
Move the system tray to the left of taskbar

* Similar to [opusman/ClockPositionRighteousifier](https://github.com/opusman/ClockPositionRighteousifier), uses hooks to inject subclass into taskbar and tray window, listening to position changes.
* When their position changes, sends a message back to the main window. The main window reposition the taskbar and tray.
* The position is calculated by enumerating all the child windows of the shell bar, taking account of hidden windows.
* Creates a tray icon.

## Limitations
* Only supports horizontal shell bar.
* Sometimes the position is not calculated correctly, where the right side of the tray is covered by the task bar.
