NuXT Menu 1.2
Fast DOS program launcher, submenus and drive explorer

QUICK START
  Copy MENU.EXE and NUI.EXE into the same writable DOS folder, for example:
    C:\NUXMENU
  Run:
    C:\NUXMENU\MENU

UPDATING: replace MENU.EXE and NUI.EXE, but keep your NUXMENU.DAT and
NUXMENU.BAK files. Version 1.2 reads existing menus unchanged.

Both EXEs are required. No installation, TSR, sound driver or 8087 is needed.
You can start MENU from another directory or drive. It finds its own files
and restores your original working directory when you exit.

The first run creates empty Games, Demos and Utilities submenus. Open one,
press F2, browse to a program, then choose its display name and arguments.
The program's directory becomes its working directory automatically.

MAIN MENU
  Up/Down, Home/End, PgUp/PgDn  Select / scroll
  Enter                       Open submenu or launch program
  Right                       Open submenu
  Esc / Backspace / Left      Parent menu (Esc at Home exits)
  Letters                     Jump to an entry starting with that letter
  F1                          Help
  F2                          Browse and add a program here
  F3                          Create a submenu here
  F4                          Edit selected entry
  F5                          Screen layout, themes and colours
  F6                          Move selected entry to another submenu
  F7 / F8                     Move entry up / down in its current menu
  Delete                      Remove menu entry, never the program file
  F9                          DOS prompt; type EXIT to return
  F10                         Save and exit to DOS

With a DOS mouse driver: single-click a submenu or browser folder to open
it. Single-click a program to select it; double-click to launch/add it.
Right-click goes back. Main-menu F1-F6 labels can also be clicked.
Clicks take effect when you release the button. A second quick click after
opening a folder is ignored to avoid accidentally opening a child entry.
Editing fields and confirmation dialogs use the keyboard.

BROWSING
  Enter               Open folder or select EXE / COM / BAT
  Backspace           Parent directory
  F3                  Filter program names (directories remain visible)
  F4                  Toggle executable files / all files
  F5                  Refresh directory
  F6                  Select drive letter (A through Z)
  Esc                 Cancel and return to menu

Folders sort first, then names alphabetically. Hidden/system entries can
be browsed too. Non-program files are visible with F4 but cannot be added.
No files or directories are deleted, renamed or moved by the explorer.
Its current folder is remembered when you add a program.

PROGRAM SETTINGS
F4 edits the name, full program path, command-line arguments, working
folder, and whether to pause after the program finishes. Enable that pause
for tools whose printed output you want to read before returning.

Edit fields support arrows, Home/End, Delete, Backspace and Ctrl-U (clear).
Enter accepts; Esc cancels the edit. Submenus can be renamed or moved,
including their contents. A submenu must be empty before it can be removed.
Moves that would put a submenu inside itself or its children are excluded.

SCREEN AND COLOURS
F5 offers 80x25, 80x43 and 80x50. These are fast BIOS text modes; the latter
two require VGA. The Trident on the NuXT is the target. Standard VGA text
is generally 720x400 for 25/50 rows, and 720x350 for 43 rows; actual timing
can be adjusted by the installed video BIOS or a utility such as Vidsync.
There is no graphics-mode renderer or SVGA-specific resolution dependency.

Six presets: Classic blue, Amber terminal, Matrix green, Neon coast, Slate,
and Paper. Each of eight UI roles has its own 16-colour foreground and
background. In F5: up/down selects a setting; left/right changes layout,
theme or foreground ink; PgUp/PgDn changes background paper. Changes
preview immediately. S or F10 saves; Esc restores the previous appearance.

Command-line overrides (initial launch only; later F5 changes are kept):
  MENU /25       Start with 80x25
  MENU /43       Start with 80x43
  MENU /50       Start with 80x50
  MENU /SAFE     Readable Classic blue, 80x25; keeps your menu entries
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
NM-RUN.BAT is a temporary launch job, deleted after the program returns.
Do not use that filename for your own files inside the menu directory.

MEMORY AND COMPATIBILITY
8086/8088-compatible real-mode code, DOS 3.3+; tested under MS-DOS 6.22 and
DOSBox. VGA is recommended. No floating point or extended memory is used.
The interface uses direct text video writes, assembly word fills, cached
browser filtering, and sleeps between input polls. CRC checks use a small
lookup table; the checksum format and validation are unchanged.
The full interface exits before your program starts. MENU.EXE remains as a
small supervisor and uses COMMAND.COM to run the job, including batch files.
The test system measured about 30.5 KiB additional conventional-memory cost
versus direct launch, including the extra command interpreter. F10 releases
the supervisor too if a particular game needs every last bit of memory.
COMSPEC must point to your normal COMMAND.COM.

LIMITS
128 total entries (programs and submenus); 31-character labels; 79-character
paths and argument strings. The DOS command interpreter imposes its own
command-line limit as well. Use DOS 8.3 paths without spaces or percent
signs/shell control characters, and keep the menu installation path short
(e.g. C:\NUXMENU). The browser holds the first 384 entries in a directory
and flags that limit; split very large directories into subdirectories.
Only one instance should use a given menu directory at a time.

SOURCE / BUILD
Set WATCOM to Open Watcom, then run sh BUILD.SH on Linux. Build files stay
in a private temporary directory. sh TEST.SH builds and runs the data tests
in an isolated DOSBox directory. Source is included for future changes.
See TESTING.TXT for validation and REVIEW.TXT for the optimization review. No 86Box test was used.

Design references checked (no code or artwork copied):
  https://archive.org/details/3DMENUPLUSDOS
  https://github.com/zirkoni/zmenu
  https://github.com/megatron-uk/x86Launcher
Open Watcom runtime license: OWLIC.TXT.
