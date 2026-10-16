# TEXTMODE 1.0

Switch between 80x25 and 80x50 DOS text without loading ANSI.SYS. Made for
using IRCjr in 50 lines while keeping the NuXT's existing Vidsync setup.
The assembly utility is 287 bytes and leaves nothing resident.

Copy `TEXTMODE.COM` and `IRC50.BAT` to `C:\NET\MTCP`, beside `IRCJR.EXE`.
Keep your existing mTCP configuration and packet driver setup.

```dos
IRC50 irc.example.net #channel
```

The batch selects 50 rows, launches IRCjr and returns to 25 rows when IRCjr
exits with Alt+X. To save a server/channel, replace `%1` through `%9` on its
IRCJR line with your usual arguments. Edit the paths if mTCP lives elsewhere.

For manual switching:

```dos
TEXTMODE 50
TEXTMODE 25
```

Requires a VGA-compatible BIOS. Each switch clears the screen. Remove the
ANSI.SYS entry if you added it only for row switching, then reboot; keep
Vidsync installed. Use **Binary** FTP transfers so BAT file line endings
arrive unchanged. Serial transfer also worked on the owner's NuXT.

Both modes use 400 scanlines. BIOS calls pass through Vidsync's existing hook;
this utility does not force Vidsync to recalibrate the LCD.

The owner confirmed the IRCjr wrapper works on the NuXT. Focused DOSBox-X
checks covered both row counts, font heights and invalid arguments.
See [README.TXT](README.TXT) for details and exit codes.

Build with `sh BUILD.SH` on Linux using GNU binutils. No Watcom required.
Original code, MIT licensed. BIOS and Vidsync references were consulted;
no third-party source was copied.
