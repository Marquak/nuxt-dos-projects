# NuXT Menu 1.4

Organise and launch your DOS programs from menus. Browse the drive to add a
program, sort things into submenus, and star the ones you use most to put them
in Favourites. Use a keyboard, mouse or joystick to get around.

Settings cover colours and themes, text rows, joystick calibration, a 12/24-hour
clock and four simple screensavers. The menu remembers where you were when you
return from a program. F1 shows the controls without filling the main screen.

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

The owner reports this version working well on the NuXT so far. Development
checks and their limits are in [TESTING.TXT](TESTING.TXT).

Source and current binaries are included. See [../BUILDING.md](../BUILDING.md)
for build dependencies. Project-authored material is MIT licensed; see
[../LICENSE](../LICENSE) and [../THIRD_PARTY.md](../THIRD_PARTY.md) for runtime terms.

![Menu preview](PREVIEW.PNG)
