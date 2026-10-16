@echo off
if not exist C:\NET\MTCP\TEXTMODE.COM goto missing
if not exist C:\NET\MTCP\IRCJR.EXE goto missing
C:\NET\MTCP\TEXTMODE.COM 50
if errorlevel 1 goto failed
C:\NET\MTCP\IRCJR.EXE %1 %2 %3 %4 %5 %6 %7 %8 %9
C:\NET\MTCP\TEXTMODE.COM 25
goto end
:failed
C:\NET\MTCP\TEXTMODE.COM 25
echo Could not select 80x50. IRCjr was not started.
goto end
:missing
echo Put TEXTMODE.COM and IRCJR.EXE in C:\NET\MTCP.
:end
