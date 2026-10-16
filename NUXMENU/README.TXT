NuXT Menu 1.5
Fast DOS program launcher, submenus and drive explorer

QUICK START
  Copy MENU.EXE and NUI.EXE into the same writable DOS folder, for example:
    C:\NUXMENU
  Run:
    C:\NUXMENU\MENU

UPDATING: replace MENU.EXE and NUI.EXE, but keep your NUXMENU.DAT and
NUXMENU.BAK files. Version 1.5 keeps the 1.4 data format. Menus from older releases are upgraded
on the next save; versions before 1.4 cannot read that upgraded format.

Both EXEs are required. No installation, TSR, sound driver or 8087 is needed.
You can start MENU from another directory or drive. It finds its own files
and restores your original working directory when you exit.

The first run creates empty Games, Demos and Utilities submenus. Open one,
press F2, browse to a program, then choose its display name and arguments.
The program's directory becomes its working directory automatically.

NEW IN 1.5
  Scrollbars with draggable thumbs and clickable arrows/tracks.
  Mouse wheel scrolling where the DOS mouse driver supports it.
  A single F4 edit form with program and working-folder Browse buttons.
  Tab/Shift-Tab focus with highlighted buttons, and safer confirmations.
  The clock is centred vertically in the three-row top bar.
  Sort each menu by name A-Z or Z-A, optionally with folders first.
  Configuration format and the small relauncher are unchanged from 1.4.

MAIN MENU
  Up/Down, Home/End, PgUp/PgDn  Select / scroll
  Enter                       Open submenu or launch program
  Right                       Open submenu
  Esc / Backspace / Left      Parent menu (does nothing at Home)
  *                           Add/remove selected program from favourites
  Ctrl-F                      Favourites / return to Home
  Ctrl-D                      Duplicate selected program
  Ctrl-O                      Sort current menu
  Ctrl-S                      Save now
  Letters                     Jump to an entry starting with that letter
  F1                          Help
  F2                          Browse and add a program here
  F3                          Create a submenu here
  F4                          Edit selected entry
  F5                          Settings
  F6                          Move selected entry to another submenu
  F7 / F8                     Move entry up / down in its current menu
  Delete                      Remove menu entry, never the program file
  F9                          DOS prompt; type EXIT to return
  F10                         Save and exit to DOS

F10 (or clicking F10 Exit) is the only menu exit action. Esc never exits
the menu to DOS; it goes back or cancels a dialog.

The main footer keeps just F1 Help, F5 Settings, F9 DOS prompt and F10 Exit.
F1 lists editing shortcuts and provides clickable editing buttons. Back and
scroll buttons remain on the main screen. Each submenu remembers its selected
entry and scroll position, including after launching a program. After moving an entry, its destination
menu opens with that entry selected.

With a DOS mouse driver: single-click a submenu or browser folder to open
it. Single-click a program to select it; double-click to launch/add it.
Right-click goes back. Toolbar labels and bracketed buttons are clickable.
Back, arrow and page buttons navigate without using the keyboard.
Clicks take effect when you release the button. A second quick click after
opening a folder is ignored to avoid accidentally opening a child entry.
Tab and Shift-Tab move between fields/buttons with a highlighted focus.
Enter activates the focused button. Confirmations start on Cancel, including
removing entries and discarding unsaved edits. Editing still uses the keyboard.
F1 Help also has keyboard-focusable buttons, including Sort.

Scrollbars appear beside menus, file lists, move destinations and Settings.
Click arrows to scroll a row, click the track to page, or drag the thumb.
The selected row stays visible. A wheel scrolls three rows per detent over
the list under the pointer; it does nothing over unrelated panels.
Wheel input requires a mouse driver/device exposing the CuteMouse Wheel API.
It is detected automatically; regular two-button drivers still work normally.
There is no extra TSR supplied or required by the menu.

BROWSING
  Enter               Open folder or select EXE / COM / BAT
  Backspace           Parent directory
  F3                  Filter program names (directories remain visible)
  F4                  Toggle executable files / all files
  F5                  Refresh directory
  F6                  Select drive letter (A through Z)
  F2                  Use current folder (working-folder picker only)
  Esc                 Cancel and return to menu

Folders sort first, then names alphabetically. Hidden/system entries can
be browsed too. Non-program files are visible with F4 but cannot be added.
No files or directories are deleted, renamed or moved by the explorer.
Its current folder is remembered when you add a program.

FAVOURITES AND DUPLICATES
Star a program with * (or [*] in F1 Help). Home always contains a Favourites
submenu listing starred programs from all submenus, sorted by name. These
are references to the original entries, so edits stay in sync. Back returns
to Home; Ctrl-F also opens Favourites. It uses no entry slot and cannot be
renamed, moved or removed. Adding a program there puts it in Home and stars
it automatically. Launching from Favourites returns there afterward.

Ctrl-D or [Duplicate] in Help copies a program into its original submenu. Choose a
new name, then use F4 to change its arguments or working folder independently.
The copy keeps the original pause and favourite settings. No program files
are copied. Submenu trees are not duplicated.

PROGRAM SETTINGS
F4 opens a single form for name, full program path, arguments, working
folder, and pause after exit. Enable that pause for tools whose output you
want to read before returning. Browse beside Program selects an EXE/COM/BAT;
Browse beside Working folder selects a directory, including empty folders.
Browsing starts at the entry's current location when it is available.
F2 opens Browse when a path field or its Browse button has focus.
Selecting a different program offers to change its working folder too.
Other edited fields are retained while browsing. Ctrl-S or the Save button
commits the complete entry. Esc/Cancel asks before discarding changed fields.
The original entry is untouched until Save.

Edit fields support arrows, Home/End, Delete, Backspace and Ctrl-U (clear).
Enter accepts; Esc cancels the edit. Submenus can be renamed or moved,
including their contents. A submenu must be empty before it can be removed.
Moves that would put a submenu inside itself or its children are excluded.

SORTING
Ctrl-O or F1 Help -> Sort opens sorting for the current menu. Choose Name
A-Z or Name Z-A and toggle Folders first if wanted. Only that menu changes;
submenus retain their own order. Favourites stays alphabetical, with its
virtual entry first on Home. The resulting order saves in NUXMENU.DAT.
This is an arrange-on-request action: new entries still append, and F7/F8
still move entries manually. Sorting preserves the selected entry.

SETTINGS (F5)
Use arrows to select/change an option. Enter changes an option or starts
calibration/preview; the [Run] button does the same. S, Ctrl-S or F10 saves;
Esc/Cancel restores all previous settings. Changes preview immediately.

Screen layout: 80x25, 80x43 and 80x50. The latter two require VGA. These
are BIOS text modes, not graphics modes; Vidsync can still adjust timing.

Themes: Classic blue, Amber terminal, Matrix green, Neon coast, Slate,
Paper, Phosphor, Ice Station, Plum CRT and Red Alert. Each of eight colour
roles also has custom foreground/background choices. For colour rows,
left/right changes ink and PgUp/PgDn changes paper. Elsewhere PgUp/PgDn
scrolls the settings list. Home/End go to the first/last setting.

Clock: On/Off, and 12-hour or 24-hour time. Default is On, 12-hour with
AM/PM. Only hours and minutes appear at top right. It follows the DOS clock;
unchanged time causes no video writes. The moving-clock saver uses the same
format even if the corner clock is off.

Screensaver: Off (default), Blank screen, Roaming clock, Star drift or
Orbit frames. Timeout is adjustable from 1 to 60 minutes (default 5).
Preview runs the selected saver immediately. Keyboard, mouse movement/clicks
or an enabled calibrated joystick wakes it; the wake action is consumed.
Held joystick/mouse buttons must settle before normal menu actions resume.
Savers run only while waiting on the main menu or a submenu, not while
editing, calibrating or running another program. The whole interface,
including savers and clock, unloads before launching a game. This is no TSR.

Joystick: Off by default. Calibrate with centre, up-left and down-right,
pressing and releasing button 1 at each position. Successful calibration
enables it; Save keeps the calibration. Repeat speed: Slow, Normal or Fast.
The standard analog first joystick on port 201h is supported, including
compatible gamepads. Proprietary digital modes are not supported.
  Up/Down selects; Left goes back; Right opens submenus.
  Button 1 opens a submenu or launches the selected program.
  Button 2 goes back; at Home it does nothing.
Use keyboard/mouse for editing, Settings and dialogs. Recalibrate if the
controller/card adjustment changes. Reads are bounded; an unplugged stick
cannot hang the menu. Actual gameport timing needs a check on your NuXT.

Start in: Home (default) or Last-used menu. Returning from a game always
restores the launching submenu and selection, regardless of this setting.
Confirm exit: optional Yes/Cancel prompt before exiting to DOS; default Off.

Command-line overrides (initial launch only; later F5 changes are kept):
  MENU /25       Start with 80x25
  MENU /43       Start with 80x43
  MENU /50       Start with 80x50
  MENU /SAFE     Classic blue, 80x25, joystick/saver off; keeps menu entries
  MENU /?        Help

CONFIGURATION
NUXMENU.DAT lives beside the two EXEs, regardless of the launch directory.
Edits save automatically; navigation/selection also saves on launch/exit.
A complete new file is written and closed before replacing the old file.
NUXMENU.BAK retains the previous save; a CRC checks for corruption. A valid
backup is recovered if the primary file is damaged or missing. The first
save after recovery preserves that known-good backup. If neither
copy is valid, the program reports that instead of silently overwriting it.

Back up NUXMENU.DAT and NUXMENU.BAK. They are binary files, not INI text.
If the folder is write protected or full, the interface reports the error
and keeps unsaved edits in memory. Use a writable hard-disk directory.
NM-RUN.DAT and NM-RUN.BAT are temporary launch jobs, removed by the launcher.
Do not use those filenames for your own files inside the menu directory.

MEMORY AND COMPATIBILITY
8086/8088-compatible real-mode code, DOS 3.3+; tested under MS-DOS 6.22 and
DOSBox-X (earlier releases also tested in DOSBox). VGA is recommended. No floating point or extended memory is used.
The interface uses direct text video writes, assembly word fills, cached
browser filtering, and sleeps between input polls. A separate 8 KiB RAM
shadow lets it write only changed screen cells. Simple selection changes
rebuild just the old/new rows and item details. The shadow is freed with
the interface before a child starts. If allocation fails, full redraws
remain available as a fallback. CRC checks use a small
lookup table; the checksum format and validation are unchanged.
The full interface exits before your program starts. A small assembly
MENU.EXE stays behind, with a 512-byte stack and no C runtime. It runs
EXE/COM programs directly. BAT files and the DOS prompt use COMSPEC.

Controlled MS-DOS 6.22/QEMU memory probe, identical child executable:
  Direct DOS launch: 35,628 free paragraphs
  EXE from menu:     35,452 free paragraphs
  BAT from menu:     35,257 free paragraphs
Extra cost is 2,816 bytes (2.75 KiB) for direct EXE/COM launches and 5,936
bytes (about 5.8 KiB) for the batch path. The earlier launcher measured
31,184 bytes (about 30.5 KiB). These differences include environment and
DOS allocation overhead; your DOS/driver/environment setup may differ.
The menu UI can use additional memory while open; none of that stays
behind during the child program. F10 also releases the launcher.

COMSPEC must point to your normal COMMAND.COM for BAT files/DOS prompt.
Keep MENU.EXE and NUI.EXE from this release together: the launch-job format
changed in 1.4. EXE/COM arguments are passed directly; use a BAT wrapper for shell
redirection or command chaining. User programs receive their working directory;
the menu restores its original launch directory when you exit.

LIMITS
128 total entries (programs and submenus); 31-character labels; 79-character
paths and argument strings. The DOS command interpreter imposes its own
command-line limit as well. Use DOS 8.3 paths without spaces or percent
signs/shell control characters, and keep the menu installation path short
(e.g. C:\NUXMENU). The browser holds the first 384 entries in a directory
and flags that limit; split very large directories into subdirectories.
Only one instance should use a given menu directory at a time.

SOURCE / BUILD
Requires Open Watcom, GNU binutils and Python 3 on Linux. Set WATCOM to
Open Watcom, then run sh BUILD.SH. Build files stay
in a private temporary directory. sh TEST.SH builds and runs the data tests
in an isolated DOSBox-X directory, including UI regression checks. Source is included for future changes.
See TESTING.TXT for validation and REVIEW.TXT for the optimization review. No 86Box test was used.

Design references checked (no code or artwork copied):
  https://github.com/FDOS/mouse/blob/master/wheelapi.txt
  https://archive.org/details/3DMENUPLUSDOS
  https://github.com/zirkoni/zmenu
  https://github.com/megatron-uk/x86Launcher
  https://github.com/MobyGamer/total-dos-launcher
  https://github.com/FDOS/freecom
  https://fd.lod.bz/rbil/interrup/dos_kernel/214b.html
  https://www.pcjs.org/documents/books/mspl13/msdos/encyclopedia/section2/
Open Watcom runtime license: OWLIC.TXT.
