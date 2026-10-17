# NuXT Menu 1.5

Keep your DOS games and programs in one menu. Browse the drive to add them,
arrange them in submenus, and mark the ones you use most as Favourites. Use a
keyboard, mouse or joystick to get around.

Choose your colours, add a clock or screensaver, and pick up where you left off
when a program closes. The main screen stays simple, with the extra controls
in F1 Help.

- **Run:** copy `MENU.EXE` and `NUI.EXE` together to DOS and start `MENU`.
- **Update:** replace both executables together. Keep your `NUXMENU.DAT` and backup.
- **Exit:** F10 exits; Esc goes back or cancels and does nothing at Home.
  F9 opens a DOS prompt; type `EXIT` to return.
- **Hardware:** 8088/8086 or later, DOS 3.3+, text-mode display. Mouse input
  needs a DOS mouse driver; joystick input uses a standard analog gameport.
- **Instructions:** [README.TXT](README.TXT).
- **Build:** `WATCOM=/path/to/watcom sh BUILD.SH`; also needs GNU binutils and Python 3.

The menu UI unloads while your program runs. A small assembly launcher stays
behind to bring it back. Measured overhead under MS-DOS 6.22 was 2.75 KiB for
an EXE and about 5.8 KiB for a batch file, down from about 30.5 KiB with the old
launcher. DOS version, environment size and drivers affect these figures.

Version 1.5 adds scrollbars, mouse-wheel support where the driver provides it,
a file browser in the edit form, Tab navigation and sorting for each menu.
[TESTING.TXT](TESTING.TXT) has the test results.

Source and current binaries are included. See [../BUILDING.md](../BUILDING.md)
for build dependencies. Project-authored material is MIT licensed; see
[../LICENSE](../LICENSE) and [../THIRD_PARTY.md](../THIRD_PARTY.md) for runtime terms.

![Menu preview](PREVIEW.PNG)
